import assert from "node:assert/strict";
import test, {afterEach} from "node:test";
import type {AssetRef, ClientMessage, SceneRequest, ServerMessage} from "../src/protocol.ts";
import {type RuntimeApi, RenderServer} from "../src/server/server.ts";
import type {Canvas} from "../src/server/surface.ts";

// The smallest valid WebAssembly binary: it compiles and instantiates with no imports.
const WASM = new Uint8Array([0x00, 0x61, 0x73, 0x6d, 0x01, 0x00, 0x00, 0x00]).buffer;

class FakeCanvas {
    width = 0;
    height = 0;
    readonly calls: string[] = [];
    readonly #context = {
        imageSmoothingQuality: "low",
        globalAlpha: 1,
        clearRect: () => this.calls.push("clear"),
        drawImage: () => this.calls.push("image"),
        getImageData: (_x: number, _y: number, width: number, height: number) => ({data: new Uint8ClampedArray(width * height * 4)}),
    };

    getContext() {
        return this.#context;
    }
}

// A scene is JSON naming its size, duration, the fonts its text is in and how it misbehaves.
interface FakeScene {
    width?: number;
    height?: number;
    duration?: number;
    live?: boolean;
    fonts?: string[];
    /** Sounds the retained runtime starts at each step. */
    sounds?: string[];
    fail?: "load" | "trapLoad" | "runtime" | "trapRuntime" | "trapDraw";
}

function fakeRuntime() {
    const log: string[] = [];
    let modules = 0;
    // Fonts go into an instance, and every engine in it has them.
    const registered = new Map<number, Set<string>>();
    const api: RuntimeApi = {
        createVisModule: (options) => new Promise((resolve) => {
            const instantiate = options.instantiateWasm as (imports: WebAssembly.Imports,
                receive: (instance: WebAssembly.Instance, module: WebAssembly.Module) => void) => unknown;
            const id = ++modules;
            instantiate({}, () => resolve({id} as never));
        }),
        createVis: async (options) => {
            const module = (options.module as {id: number}).id;
            let scene: FakeScene = {width: 16, height: 16};
            let runtimeTime = 0;
            let view = "2d";
            const engine = {
                get duration() { return scene.duration ?? 0; },
                loop: false,
                get retainedLua() { return scene.live ?? false; },
                adaptiveTheme: false,
                cameraMode: "interactive",
                get cameraView() { return view; },
                cameraInput(action: string, x: number, y: number) {
                    log.push(`camera ${action} ${x} ${y}`);
                    if (action === "orbit" && view === "2d") throw new Error("only a 3D camera orbits");
                    if (action === "view3d" || action === "view2d") view = action.slice(4);
                    return true;
                },
                render(time: number) {
                    log.push(`render ${time}`);
                    return new Uint8ClampedArray((scene.width ?? 16) * (scene.height ?? 16) * 4);
                },
                audio: {
                    load: (name: string) => log.push(`sound ${name}`),
                    start: () => log.push("audio start"),
                    transport: ({time, rate, playing, seek}: {time: number; rate: number; playing: boolean; seek: boolean}) =>
                        log.push(`transport ${time} ${rate} ${playing} ${seek}`),
                    play: (name: string) => log.push(`play ${name}`),
                },
                get runtimeTime() { return runtimeTime; },
                size: () => [scene.width ?? 16, scene.height ?? 16],
                font: (name: string, _data: ArrayBuffer, type: string) => {
                    log.push(`font ${module} ${name} ${type}`);
                    registered.set(module, (registered.get(module) ?? new Set()).add(name));
                },
                missingFonts: () => (scene.fonts ?? []).filter((name) => !registered.get(module)?.has(name)),
                asset: (name: string) => log.push(`asset ${module} ${name}`),
                hostTheme: () => log.push("theme"),
                load(input: string | {source: string}) {
                    const next = JSON.parse(typeof input === "string" ? input : input.source) as FakeScene;
                    if (next.fail === "load") throw new Error("bad scene");
                    if (next.fail === "trapLoad") throw new WebAssembly.RuntimeError("unreachable");
                    scene = next;
                    log.push(`load ${module} ${JSON.stringify(next)}`);
                },
                draw(canvas: FakeCanvas, time: number, ratio: number) {
                    if (scene.fail === "trapDraw") throw new WebAssembly.RuntimeError("unreachable");
                    canvas.width = (scene.width ?? 16) * ratio;
                    canvas.height = (scene.height ?? 16) * ratio;
                    canvas.calls.push(`draw ${time}`);
                },
                input: (event: {type: string; key?: string; time: number; button?: number}) =>
                    log.push(`input ${event.type}${event.key ? ` ${event.key}` : ""} at ${event.time}`
                        + (event.button === undefined ? "" : ` button ${event.button}`)),
                beginInputFrame: (time: number) => log.push(`begin ${time}`),
                releaseInput: (time: number) => log.push(`release ${time}`),
                advanceRuntime(dt: number) {
                    if (scene.fail === "runtime") throw new Error("runtime failed");
                    if (scene.fail === "trapRuntime") throw new WebAssembly.RuntimeError("unreachable");
                    runtimeTime += dt;
                    log.push(`advance ${dt}`);
                    return {steps: 1, sounds: (scene.sounds ?? []).map((asset) => ({asset, bus: "effect", gain: 1, rate: 1, loop: false}))};
                },
                destroy: () => log.push(`destroy ${module}`),
            };
            return engine as never;
        },
        evaluateScene: (source) => ({source}) as never,
    };
    return {api, log, modules: () => modules};
}

const servers: RenderServer[] = [];
afterEach(() => {
    for (const server of servers.splice(0)) server.dispose();
});

function fixture() {
    const runtime = fakeRuntime();
    const messages: ServerMessage[] = [];
    const server = new RenderServer({runtime: runtime.api, post: (message) => messages.push(message),
        scratch: () => new FakeCanvas() as unknown as Canvas});
    servers.push(server);
    const send = (message: ClientMessage) => server.handle(message);
    const canvas = () => {
        const target = new FakeCanvas();
        return target;
    };
    // Waits for the first message matching `match` that was not taken before.
    const taken = new Set<ServerMessage>();
    const next = async <T extends ServerMessage["type"]>(type: T, match: (message: Extract<ServerMessage, {type: T}>) => boolean = () => true) => {
        for (let i = 0; i < 200; i++) {
            const found = messages.find((message): message is Extract<ServerMessage, {type: T}> =>
                message.type === type && !taken.has(message) && match(message as Extract<ServerMessage, {type: T}>));
            if (found) {
                taken.add(found);
                return found;
            }
            await new Promise((resolve) => setTimeout(resolve, 1));
        }
        throw new Error(`No ${type} message`);
    };
    // Answers every image request with bytes, and every font request with a font unless the
    // name is in `none`.
    const serve = (none: readonly string[] = []) => {
        const answered = new Set<number>();
        const timer = setInterval(() => {
            for (const message of messages) {
                if ((message.type !== "fetch" && message.type !== "font") || answered.has(message.request)) continue;
                answered.add(message.request);
                if (message.type === "fetch") send({type: "bytes", request: message.request, data: new ArrayBuffer(8)});
                else if (none.includes(message.name)) send({type: "font", request: message.request});
                else send({type: "font", request: message.request, data: new ArrayBuffer(8)});
            }
        }, 1);
        return () => clearInterval(timer);
    };
    return {runtime, messages, send, canvas, next, serve};
}

const THEME = ["Pretendard", "JetBrainsMono"];
const scene = (spec: FakeScene, assets: AssetRef[] = []): SceneRequest =>
    ({source: JSON.stringify({width: 640, height: 360, duration: 4, fonts: THEME, ...spec}), language: "lua",
        name: "scene.lua", resources: {assets}});
const PLAN = {width: 640, height: 360, ratio: 1};
const loads = (log: string[]) => log.filter((line) => line.startsWith("load"));

test("sessions load once the binary arrives; the fonts a scene uses are asked for once per server", async () => {
    const {send, canvas, next, messages, serve, runtime} = fixture();
    send({type: "open", session: 1, canvas: canvas() as never});
    send({type: "load", session: 1, seq: 1, scene: scene({})});
    const stop = serve();
    send({type: "init", wasm: WASM});
    const loaded = await next("loaded");
    assert.deepEqual({width: loaded.info.width, height: loaded.info.height, duration: loaded.info.duration}, {width: 640, height: 360, duration: 4});
    // Loaded in the default font, then again in the scene's fonts.
    assert.equal(loads(runtime.log).length, 2);
    send({type: "open", session: 2, canvas: canvas() as never});
    send({type: "load", session: 2, seq: 2, scene: scene({})});
    await next("loaded", (message) => message.session === 2);
    stop();
    assert.equal(loads(runtime.log).length, 3, "a scene whose fonts are in loads once");
    assert.deepEqual(messages.flatMap((message) => message.type === "font" ? [message.name] : []), THEME);
    assert.equal(runtime.modules(), 1);
    assert.deepEqual(runtime.log.filter((line) => line.startsWith("font")), ["font 1 Pretendard ttf", "font 1 JetBrainsMono ttf"]);
});

test("a font the host has none for leaves its text in the default font, and is not asked for again", async () => {
    const {send, canvas, next, messages, serve, runtime} = fixture();
    send({type: "init", wasm: WASM});
    send({type: "open", session: 1, canvas: canvas() as never});
    const stop = serve(["Missing Family"]);
    send({type: "load", session: 1, seq: 1, scene: scene({fonts: ["Missing Family"]})});
    await next("loaded");
    send({type: "load", session: 1, seq: 2, scene: scene({fonts: ["Missing Family"]})});
    await next("loaded");
    stop();
    assert.equal(loads(runtime.log).length, 2, "no font came, so neither load ran twice");
    assert.equal(messages.filter((message) => message.type === "font").length, 1);
});

test("a font that does not arrive leaves its text in the default font, and the next load asks again", async () => {
    const {send, canvas, next, messages, runtime} = fixture();
    send({type: "init", wasm: WASM});
    send({type: "open", session: 1, canvas: canvas() as never});
    send({type: "load", session: 1, seq: 1, scene: scene({fonts: ["Remote"]})});
    const first = await next("font");
    send({type: "font", request: first.request, error: "Fontsource answered 503"});
    await next("loaded");
    send({type: "load", session: 1, seq: 2, scene: scene({fonts: ["Remote"]})});
    const second = await next("font");
    assert.notEqual(second.request, first.request);
    // The type comes from the bytes when the host does not say: these are a CFF font's.
    send({type: "font", request: second.request, data: new Uint8Array([0x4f, 0x54, 0x54, 0x4f]).buffer});
    await next("loaded");
    assert.equal(messages.filter((message) => message.type === "font").length, 2);
    assert.deepEqual(runtime.log.filter((line) => line.startsWith("font")), ["font 1 Remote otf"]);
    assert.equal(loads(runtime.log).length, 3);
});

test("after the host's fonts change, a session's next load moves to a new instance that asks again", async () => {
    const {send, canvas, next, messages, serve, runtime} = fixture();
    send({type: "init", wasm: WASM});
    send({type: "open", session: 1, canvas: canvas() as never});
    const stop = serve();
    send({type: "load", session: 1, seq: 1, scene: scene({})});
    await next("loaded");
    send({type: "fonts"});
    send({type: "frame", session: 1, seq: 2, frame: {time: 1, plan: PLAN}});
    await next("framed");
    assert.ok(!runtime.log.includes("destroy 1"), "the retired instance still draws its scene");
    send({type: "load", session: 1, seq: 3, scene: scene({})});
    await next("loaded");
    stop();
    assert.equal(runtime.modules(), 2);
    assert.equal(messages.filter((message) => message.type === "font").length, 4);
    assert.ok(runtime.log.includes("destroy 1"), "the retired instance goes once its session leaves");
});

test("a load that a newer one overtakes is skipped", async () => {
    const {send, canvas, next, serve} = fixture();
    send({type: "init", wasm: WASM});
    send({type: "open", session: 1, canvas: canvas() as never});
    send({type: "load", session: 1, seq: 1, scene: scene({duration: 1})});
    send({type: "load", session: 1, seq: 2, scene: scene({duration: 2})});
    const stop = serve();
    assert.equal((await next("skipped")).seq, 1);
    assert.equal((await next("loaded")).info.duration, 2);
    stop();
});

test("frames before a load or after a release draw nothing", async () => {
    const {send, canvas, next, serve} = fixture();
    const target = canvas();
    send({type: "init", wasm: WASM});
    send({type: "open", session: 1, canvas: target as never});
    send({type: "frame", session: 1, seq: 1, frame: {time: 0, plan: PLAN}});
    assert.deepEqual((await next("framed")).result, {});
    const stop = serve();
    send({type: "load", session: 1, seq: 2, scene: scene({})});
    await next("loaded");
    stop();
    send({type: "frame", session: 1, seq: 3, frame: {time: 1.5, plan: PLAN}});
    await next("framed", (message) => message.seq === 3);
    send({type: "release", session: 1});
    send({type: "frame", session: 1, seq: 4, frame: {time: 2, plan: PLAN}});
    await next("framed", (message) => message.seq === 4);
    assert.deepEqual(target.calls, ["draw 1.5"]);
});

test("a live frame begins input, lets go, applies input at the runtime time, then steps", async () => {
    const {send, canvas, next, serve, runtime} = fixture();
    send({type: "init", wasm: WASM});
    send({type: "open", session: 1, canvas: canvas() as never});
    const stop = serve();
    send({type: "load", session: 1, seq: 1, scene: scene({live: true})});
    const loaded = await next("loaded");
    stop();
    assert.equal(loaded.info.retained, true);
    assert.equal(loaded.info.input, true);
    send({type: "frame", session: 1, seq: 2, frame: {time: 0, plan: PLAN, advance: 0.5}});
    send({type: "frame", session: 1, seq: 3, frame: {time: 0, plan: PLAN, advance: 0.25, releaseInput: true,
        input: [{type: "keydown", key: "A"}, {type: "pointerdown", x: 1, y: 2, button: 0}]}});
    const framed = await next("framed", (message) => message.seq === 3);
    assert.equal(framed.result.runtimeTime, 0.75);
    assert.deepEqual(runtime.log.filter((line) => /^(begin|release|input|advance)/.test(line)), [
        "begin 0", "advance 0.5",
        "begin 0.5", "release 0.5", "input keydown A at 0.5", "input pointerdown at 0.5 button 0", "advance 0.25",
    ]);
});

test("a scene without a retained runtime takes input on the timeline", async () => {
    const {send, canvas, next, serve, runtime} = fixture();
    send({type: "init", wasm: WASM});
    send({type: "open", session: 1, canvas: canvas() as never});
    const stop = serve();
    send({type: "load", session: 1, seq: 1, scene: scene({})});
    await next("loaded");
    stop();
    send({type: "frame", session: 1, seq: 2, frame: {time: 2.5, plan: PLAN, advance: 0.1, input: [{type: "pointerdown", x: 1, y: 2, button: 0}]}});
    await next("framed", (message) => message.seq === 2);
    assert.deepEqual(runtime.log.filter((line) => /^(begin|input)/.test(line)), ["begin 2.5", "input pointerdown at 2.5 button 0"]);
});

test("a runtime error stops the runtime but not the drawing", async () => {
    const {send, canvas, next, serve} = fixture();
    const target = canvas();
    send({type: "init", wasm: WASM});
    send({type: "open", session: 1, canvas: target as never});
    const stop = serve();
    send({type: "load", session: 1, seq: 1, scene: scene({live: true, fail: "runtime"})});
    await next("loaded");
    stop();
    send({type: "frame", session: 1, seq: 2, frame: {time: 0, plan: PLAN, advance: 0.1}});
    const framed = await next("framed");
    assert.deepEqual(framed.result.runtimeError, {phase: "runtime", message: "runtime failed"});
    assert.deepEqual(target.calls, ["draw 0"]);
});

test("a trap stops the scene; the instance is not called into again, and the next load gets a new one", async () => {
    const {send, canvas, next, serve, runtime} = fixture();
    const target = canvas();
    send({type: "init", wasm: WASM});
    send({type: "open", session: 1, canvas: target as never});
    const stop = serve();
    send({type: "load", session: 1, seq: 1, scene: scene({live: true, fail: "trapRuntime"})});
    await next("loaded");
    send({type: "frame", session: 1, seq: 2, frame: {time: 0, plan: PLAN, advance: 0.1}});
    assert.equal((await next("failed")).error.phase, "runtime");
    send({type: "frame", session: 1, seq: 3, frame: {time: 0, plan: PLAN}});
    assert.equal((await next("failed")).error.phase, "engine");
    assert.deepEqual(target.calls, []);
    send({type: "load", session: 1, seq: 4, scene: scene({})});
    await next("loaded");
    stop();
    assert.equal(runtime.modules(), 2);
    assert.ok(!runtime.log.includes("destroy 1"), "the trapped instance's engines are left alone");
});

test("a failed load keeps the scene before it drawable", async () => {
    const {send, canvas, next, serve} = fixture();
    const target = canvas();
    send({type: "init", wasm: WASM});
    send({type: "open", session: 1, canvas: target as never});
    const stop = serve();
    send({type: "load", session: 1, seq: 1, scene: scene({})});
    await next("loaded");
    send({type: "load", session: 1, seq: 2, scene: scene({fail: "load"})});
    const failed = await next("failed");
    stop();
    assert.deepEqual(failed.error, {phase: "load", message: "bad scene"});
    send({type: "frame", session: 1, seq: 3, frame: {time: 1, plan: PLAN}});
    await next("framed");
    assert.deepEqual(target.calls, ["draw 1"]);
});

test("an image that cannot be fetched fails the load as a resource, and the next load asks again", async () => {
    const {send, canvas, next, messages} = fixture();
    const earth: AssetRef = {id: "earth.png", name: "earth.png", mime: "png"};
    send({type: "init", wasm: WASM});
    send({type: "open", session: 1, canvas: canvas() as never});
    send({type: "load", session: 1, seq: 1, scene: scene({fonts: []}, [earth])});
    const first = await next("fetch");
    send({type: "bytes", request: first.request, error: "Could not load resource (404)"});
    assert.deepEqual((await next("failed")).error, {phase: "resource", message: "Could not load resource (404)"});
    send({type: "load", session: 1, seq: 2, scene: scene({fonts: []}, [earth])});
    const second = await next("fetch");
    assert.notEqual(second.request, first.request);
    send({type: "bytes", request: second.request, data: new ArrayBuffer(8)});
    await next("loaded");
    assert.equal(messages.filter((message) => message.type === "fetch").length, 2);
});

test("a binary that could not be fetched is fatal until one arrives", async () => {
    const {send, canvas, next, serve} = fixture();
    send({type: "open", session: 1, canvas: canvas() as never});
    send({type: "init", error: "Could not load the viscript runtime: 404"});
    assert.equal((await next("fatal")).error.message, "Could not load the viscript runtime: 404");
    const stop = serve();
    send({type: "load", session: 1, seq: 1, scene: scene({})});
    assert.equal((await next("failed")).error.phase, "engine");
    send({type: "init", wasm: WASM});
    send({type: "load", session: 1, seq: 2, scene: scene({})});
    await next("loaded");
    stop();
});

test("a crossfade keeps the frame on screen and dissolves it into the new ones", async () => {
    const {send, canvas, next, serve} = fixture();
    const target = canvas();
    send({type: "init", wasm: WASM});
    send({type: "open", session: 1, canvas: target as never});
    const stop = serve();
    send({type: "load", session: 1, seq: 1, scene: scene({})});
    await next("loaded");
    stop();
    send({type: "frame", session: 1, seq: 2, frame: {time: 3.9, plan: PLAN}});
    send({type: "frame", session: 1, seq: 3, frame: {time: 0, plan: PLAN, crossfade: 40}});
    await next("framed", (message) => message.seq === 3);
    // The last frame is copied out, the new one drawn aside, and both composited on the canvas.
    assert.deepEqual(target.calls, ["draw 3.9", "clear", "image", "image"]);
    await new Promise((resolve) => setTimeout(resolve, 80));
    assert.deepEqual(target.calls.slice(-2), ["clear", "image"], "the fade finishes on its own");
});

test("camera moves apply in order before the frame is drawn; one the camera does not make changes nothing", async () => {
    const {send, canvas, next, serve, runtime} = fixture();
    const target = canvas();
    send({type: "init", wasm: WASM});
    send({type: "open", session: 1, canvas: target as never});
    const stop = serve();
    send({type: "load", session: 1, seq: 1, scene: scene({})});
    await next("loaded");
    stop();
    send({type: "frame", session: 1, seq: 2, frame: {time: 1, plan: PLAN, camera: [
        {action: "orbit", x: 0.1, y: 0}, {action: "view3d", x: 0, y: 0}, {action: "orbit", x: 0.1, y: 0.2},
    ]}});
    assert.equal((await next("framed")).result.cameraView, "3d");
    assert.deepEqual(runtime.log.filter((line) => line.startsWith("camera")),
        ["camera orbit 0.1 0", "camera view3d 0 0", "camera orbit 0.1 0.2"]);
    assert.deepEqual(target.calls, ["draw 1"]);
});

test("a capture draws a frame apart from the canvas; with nothing loaded there is none", async () => {
    const {send, canvas, next, serve, runtime} = fixture();
    const target = canvas();
    send({type: "init", wasm: WASM});
    send({type: "open", session: 1, canvas: target as never});
    const stop = serve();
    send({type: "load", session: 1, seq: 1, scene: scene({width: 4, height: 2})});
    await next("loaded");
    stop();
    send({type: "capture", session: 1, seq: 2, capture: {time: 1.5, ratio: 1}});
    const own = (await next("captured")).frame;
    assert.deepEqual([own.width, own.height, own.data.byteLength], [4, 2, 32]);
    assert.ok(runtime.log.includes("render 1.5"));
    send({type: "capture", session: 1, seq: 3, capture: {time: 2, ratio: 3}});
    const larger = (await next("captured")).frame;
    assert.deepEqual([larger.width, larger.height, larger.data.byteLength], [12, 6, 288]);
    assert.deepEqual(target.calls, []);
    send({type: "release", session: 1});
    send({type: "capture", session: 1, seq: 4, capture: {time: 0, ratio: 1}});
    assert.equal((await next("failed")).error.phase, "render");
});

test("a scene given its sounds plays them with the player's transport, from the first frame that says where it is", async () => {
    const {send, canvas, next, serve, runtime} = fixture();
    send({type: "init", wasm: WASM});
    send({type: "open", session: 1, canvas: canvas() as never});
    const stop = serve();
    const chime = {id: "chime.wav", name: "chime.wav", mime: "audio/wav"};
    send({type: "load", session: 1, seq: 1, scene: {...scene({live: true, sounds: ["chime.wav"]}, [chime]), audio: true}});
    await next("loaded");
    const step = (seq: number, audio?: {playing: boolean; rate: number; seek: boolean}) =>
        send({type: "frame", session: 1, seq, frame: {time: 0, plan: PLAN, advance: 0.25, ...audio ? {audio} : {}}});
    step(2);
    step(3, {playing: true, rate: 2, seek: false});
    step(4, {playing: false, rate: 2, seek: false});
    await next("framed", (message) => message.seq === 4);
    const sound = () => runtime.log.filter((line) => /^(sound|audio|transport|play)/.test(line));
    // A retained scene's sound follows its runtime's clock.
    assert.deepEqual(sound(), ["sound chime.wav", "audio start", "transport 0.5 2 true true", "play chime.wav",
        "transport 0.75 2 false false", "play chime.wav"]);
    // A scene loaded without its sounds plays none.
    send({type: "load", session: 1, seq: 5, scene: scene({live: true, sounds: ["chime.wav"]}, [chime])});
    await next("loaded");
    stop();
    const before = sound().length;
    step(6, {playing: true, rate: 1, seek: true});
    await next("framed", (message) => message.seq === 6);
    assert.equal(sound().length, before);
});
