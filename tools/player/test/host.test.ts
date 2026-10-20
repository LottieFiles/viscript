import assert from "node:assert/strict";
import test, {afterEach} from "node:test";
import {
    createRenderHost, INLINE_JAVASCRIPT_REFUSED, merge, SceneError, SupersededError, type RenderHost, type SceneCompiler,
    type SessionHandlers,
} from "../src/host/host.ts";
import type {FontProvider} from "../src/fonts/provider.ts";
import type {ClientMessage, FrameRequest, SceneRequest, ServerMessage} from "../src/protocol.ts";

const PLAN = {width: 640, height: 360, ratio: 1};
const SCENE: SceneRequest = {source: "return vis.scene {}", language: "lua", name: "scene.lua", resources: {assets: []}};
const INFO = {duration: 4, width: 640, height: 360, loop: false, retained: false, input: false, runtime: false,
    adaptiveTheme: false, cameraMode: "fixed", cameraView: "2d"} as const;
const WASM = new ArrayBuffer(8);

const tick = () => new Promise((resolve) => setTimeout(resolve, 0));

const hosts: RenderHost[] = [];
afterEach(() => {
    for (const host of hosts.splice(0)) host.dispose();
    delete (globalThis as Record<string, unknown>).__fakeServer;
});

// An inline server the test answers for: every message it gets is kept, and `post` replies.
interface FakeServer {
    readonly received: ClientMessage[];
    post(message: ServerMessage): void;
}

interface InlineOptions {
    wasm?: () => Promise<ArrayBuffer>;
    bytes?: (id: string) => Promise<ArrayBuffer>;
    fonts?: FontProvider;
    fontTimeout?: number;
    inlineJavaScript?: boolean;
    compiler?: SceneCompiler;
}

function inlineHost(options: InlineOptions = {}) {
    const server: FakeServer = {received: [], post: () => undefined};
    (globalThis as Record<string, unknown>).__fakeServer = (post: (message: ServerMessage) => void) => {
        server.post = post;
        return {handle: (message: ClientMessage) => server.received.push(message), dispose: () => undefined};
    };
    const host = createRenderHost({
        // Each test gets its own source text, so the inline server is loaded anew.
        server: {code: `globalThis.__viscriptRenderServer = globalThis.__fakeServer; // ${Math.random()}`},
        wasm: options.wasm ?? (async () => WASM),
        assets: {bytes: options.bytes ?? (async () => new ArrayBuffer(4))},
        ...options.fonts ? {fonts: options.fonts} : {},
        ...options.fontTimeout ? {fontTimeout: options.fontTimeout} : {},
        mode: "inline",
        ...options.inlineJavaScript === undefined ? {} : {inlineJavaScript: options.inlineJavaScript},
        ...options.compiler ? {compiler: options.compiler} : {},
    });
    hosts.push(host);
    return {host, server};
}

function handlers() {
    const events: string[] = [];
    const handler: SessionHandlers = {
        framed: (result) => events.push(`framed ${JSON.stringify(result)}`),
        failed: (error) => events.push(`failed ${error.phase}`),
        lost: (reason) => events.push(`lost ${reason}`),
    };
    return {events, handler};
}

const sent = <T extends ClientMessage["type"]>(server: FakeServer, type: T) =>
    server.received.filter((message): message is Extract<ClientMessage, {type: T}> => message.type === type);

test("merging keeps the newest time and plan and queues input and runtime steps", () => {
    const older: FrameRequest = {time: 1, plan: PLAN, input: [{type: "keydown", key: "A"}], advance: 0.1};
    const newer: FrameRequest = {time: 2, plan: {...PLAN, width: 320}, input: [{type: "keyup", key: "A"}], advance: 0.2};
    assert.deepEqual(merge(older, newer), {time: 2, plan: {...PLAN, width: 320},
        input: [{type: "keydown", key: "A"}, {type: "keyup", key: "A"}], advance: 0.1 + 0.2});
    assert.deepEqual(merge(null, newer), newer);
});

test("merging queues camera moves, and keeps a jump in the sound for the frame that follows it", () => {
    const merged = merge({time: 1, plan: PLAN, camera: [{action: "pan", x: 0.1, y: 0}], audio: {playing: true, rate: 1, seek: true}},
        {time: 2, plan: PLAN, camera: [{action: "zoom", x: 0, y: -0.12}], audio: {playing: false, rate: 1, seek: false}});
    assert.deepEqual(merged.camera, [{action: "pan", x: 0.1, y: 0}, {action: "zoom", x: 0, y: -0.12}]);
    assert.deepEqual(merged.audio, {playing: false, rate: 1, seek: true});
    assert.deepEqual(merge({time: 1, plan: PLAN, audio: {playing: true, rate: 2, seek: false}}, {time: 2, plan: PLAN}).audio,
        {playing: true, rate: 2, seek: false});
});

test("a release lets go of input queued before it", () => {
    const merged = merge({time: 1, plan: PLAN, input: [{type: "keydown", key: "A"}], advance: 0},
        {time: 1, plan: PLAN, releaseInput: true, input: [{type: "pointercancel"}], advance: 0});
    assert.deepEqual(merged.input, [{type: "pointercancel"}]);
    assert.equal(merged.releaseInput, true);
});

test("the binary goes to the server, and frames wait while one is drawn", async () => {
    const {host, server} = inlineHost();
    const {events, handler} = handlers();
    const session = host.open({} as HTMLCanvasElement, handler);
    session.frame({time: 1, plan: PLAN});
    session.frame({time: 2, plan: PLAN});
    session.frame({time: 3, plan: PLAN, input: [{type: "keydown", key: "B"}]});
    await tick();
    // The binary may arrive after the session opened; the server waits for it.
    assert.equal(sent(server, "init")[0]?.wasm?.byteLength, 8);
    const frames = sent(server, "frame");
    assert.deepEqual(frames.map((message) => message.frame.time), [1]);
    server.post({type: "framed", session: frames[0]!.session, seq: frames[0]!.seq, result: {}});
    await tick();
    assert.deepEqual(sent(server, "frame").map((message) => message.frame.time), [1, 3]);
    assert.deepEqual(sent(server, "frame")[1]!.frame.input, [{type: "keydown", key: "B"}]);
    assert.deepEqual(events, ["framed {}"]);
});

test("a load the server skips rejects as superseded; the newer one resolves", async () => {
    const {host, server} = inlineHost();
    const session = host.open({} as HTMLCanvasElement, handlers().handler);
    const first = session.load(SCENE);
    const second = session.load(SCENE);
    await tick();
    const [a, b] = sent(server, "load");
    server.post({type: "skipped", session: a!.session, seq: a!.seq});
    server.post({type: "loaded", session: b!.session, seq: b!.seq, info: INFO});
    await assert.rejects(first, SupersededError);
    assert.deepEqual(await second, INFO);
});

test("closing a session rejects its loads and ignores late answers", async () => {
    const {host, server} = inlineHost();
    const {events, handler} = handlers();
    const session = host.open({} as HTMLCanvasElement, handler);
    const load = session.load(SCENE);
    session.frame({time: 0, plan: PLAN});
    session.close();
    await assert.rejects(load, SupersededError);
    await tick();
    const frame = sent(server, "frame")[0]!;
    server.post({type: "framed", session: frame.session, seq: frame.seq, result: {}});
    await tick();
    assert.deepEqual(events, []);
    assert.equal(sent(server, "close").length, 1);
});

test("a capture resolves with the frame the server drew, and rejects when it fails or its session closes", async () => {
    const {host, server} = inlineHost();
    const session = host.open({} as HTMLCanvasElement, handlers().handler);
    const drawn = session.capture({time: 1, ratio: 2});
    const failing = session.capture({time: 2, ratio: 1});
    const closing = session.capture({time: 3, ratio: 1});
    await tick();
    const [first, second] = sent(server, "capture");
    assert.deepEqual(first?.capture, {time: 1, ratio: 2});
    const data = new ArrayBuffer(16);
    server.post({type: "captured", session: first!.session, seq: first!.seq, frame: {width: 2, height: 2, data}});
    server.post({type: "failed", session: second!.session, seq: second!.seq, error: {phase: "render", message: "No scene"}});
    assert.equal((await drawn).data, data);
    await assert.rejects(failing, SceneError);
    session.close();
    await assert.rejects(closing, SupersededError);
});

test("the server's requests for images are answered from the assets", async () => {
    const asked: string[] = [];
    const {host, server} = inlineHost({bytes: async (id) => {
        asked.push(id);
        if (id === "missing") throw new Error("Could not load resource: missing (404)");
        return new ArrayBuffer(3);
    }});
    const session = host.open({} as HTMLCanvasElement, handlers().handler);
    session.load({...SCENE, resources: {assets: [{id: "earth", name: "earth.png", mime: "image/png"},
        {id: "missing", name: "missing.png", mime: "image/png"}]}}).catch(() => undefined);
    await tick();
    server.post({type: "fetch", request: 7, id: "earth"});
    server.post({type: "fetch", request: 8, id: "missing"});
    await tick();
    await tick();
    assert.deepEqual(asked, ["earth", "missing"]);
    const answers = sent(server, "bytes");
    assert.equal(answers.find((message) => message.request === 7)?.data?.byteLength, 3);
    assert.equal(answers.find((message) => message.request === 8)?.error, "Could not load resource: missing (404)");
});

// Scene code can post as the server: it may ask for a note by name, which must not be read.
test("images no load named are refused without asking the assets", async () => {
    const asked: string[] = [];
    const {host, server} = inlineHost({bytes: async (id) => {
        asked.push(id);
        return new ArrayBuffer(3);
    }});
    const session = host.open({} as HTMLCanvasElement, handlers().handler);
    session.load(SCENE).catch(() => undefined);
    await tick();
    server.post({type: "fetch", request: 1, id: "Private/diary.md@0"});
    await tick();
    assert.deepEqual(asked, []);
    assert.deepEqual(sent(server, "bytes").map((message) => message.error), ["No scene was given the image Private/diary.md@0"]);
});

test("the server's requests for fonts are answered by the provider, none when it has none", async () => {
    const asked: string[] = [];
    const cff = new Uint8Array([0x4f, 0x54, 0x54, 0x4f, 0, 1]);
    const {host, server} = inlineHost({fonts: {
        async fetch(name) {
            asked.push(name);
            if (name === "Serif") return {data: cff};
            if (name === "Offline") throw new Error("Fontsource answered 503");
            if (name === "Slow") return new Promise(() => undefined);
            return null;
        },
    }, fontTimeout: 20});
    const session = host.open({} as HTMLCanvasElement, handlers().handler);
    session.load(SCENE).catch(() => undefined);
    await tick();
    for (const [request, name] of ["Serif", "Unknown", "Offline", "Slow"].entries()) server.post({type: "font", request, name});
    await new Promise((resolve) => setTimeout(resolve, 40));
    assert.deepEqual(asked, ["Serif", "Unknown", "Offline", "Slow"]);
    const answers = new Map(sent(server, "font").map((message) => [message.request, message]));
    assert.equal(answers.get(0)?.fontType, "otf");
    assert.deepEqual([...new Uint8Array(answers.get(0)!.data!)], [...cff]);
    assert.deepEqual(answers.get(1), {type: "font", request: 1});
    assert.equal(answers.get(2)?.error, "Fontsource answered 503");
    assert.match(answers.get(3)?.error ?? "", /took too long/);

    // Without a provider, every font is none.
    const bare = inlineHost();
    bare.host.open({} as HTMLCanvasElement, handlers().handler).load(SCENE).catch(() => undefined);
    await tick();
    bare.server.post({type: "font", request: 1, name: "Pretendard"});
    await tick();
    assert.deepEqual(sent(bare.server, "font"), [{type: "font", request: 1}]);
});

test("refreshing the fonts tells the server", async () => {
    const {host, server} = inlineHost();
    host.refreshFonts();
    host.open({} as HTMLCanvasElement, handlers().handler);
    host.refreshFonts();
    await tick();
    assert.equal(sent(server, "fonts").length, 1, "a host with no server yet has nothing to tell");
});

test("JavaScript scenes stay off the main thread when the host asks", async () => {
    const js = {...SCENE, source: "return vis.scene({});", language: "js", name: "scene.js"} as const;
    const kept = inlineHost({inlineJavaScript: false});
    const session = kept.host.open({} as HTMLCanvasElement, handlers().handler);
    await assert.rejects(session.load(js), (error) => error instanceof SceneError && error.message === INLINE_JAVASCRIPT_REFUSED);
    // Lua scenes run on the main thread all the same, and so does JavaScript unless asked.
    session.load(SCENE).catch(() => undefined);
    await tick();
    assert.deepEqual(sent(kept.server, "load").map((message) => message.scene.language), ["lua"]);
    const open = inlineHost();
    open.host.open({} as HTMLCanvasElement, handlers().handler).load(js).catch(() => undefined);
    await tick();
    assert.deepEqual(sent(open.server, "load").map((message) => message.scene.language), ["js"]);
});

// A compiler the test answers for: each scene it is given waits until the test settles it.
function testCompiler() {
    const scenes: {source: string; name: string; resolve(lua: string): void; reject(error: Error): void}[] = [];
    let disposed = false;
    const compiler: SceneCompiler = {
        compile: (source, name) => new Promise((resolve, reject) => scenes.push({source, name, resolve, reject})),
        dispose: () => { disposed = true; },
    };
    return {compiler, scenes, disposed: () => disposed};
}

const JS: SceneRequest = {...SCENE, source: "return vis.scene({});", language: "js", name: "scene.js"};

test("with a compiler, JavaScript scenes reach the server as the Lua they build, on the main thread too", async () => {
    const compiling = testCompiler();
    const {host, server} = inlineHost({inlineJavaScript: false, compiler: compiling.compiler});
    const session = host.open({} as HTMLCanvasElement, handlers().handler);
    const load = session.load(JS);
    await tick();
    assert.deepEqual(compiling.scenes.map(({source, name}) => [source, name]), [[JS.source, "scene.js"]]);
    assert.deepEqual(sent(server, "load"), [], "nothing reaches the server before the scene compiles");
    compiling.scenes[0]!.resolve("return vis.scene {width = 8, height = 8}");
    await tick();
    const [message] = sent(server, "load");
    assert.deepEqual([message?.scene.language, message?.scene.source, message?.scene.name],
        ["lua", "return vis.scene {width = 8, height = 8}", "scene.js"]);
    server.post({type: "loaded", session: message!.session, seq: message!.seq, info: INFO});
    assert.deepEqual(await load, INFO);
    host.dispose();
    assert.equal(compiling.disposed(), true, "the host disposes its compiler");
});

test("a load asked for while a scene compiles takes over, whether the compile succeeds or fails", async () => {
    const compiling = testCompiler();
    const {host, server} = inlineHost({compiler: compiling.compiler});
    const session = host.open({} as HTMLCanvasElement, handlers().handler);
    const first = session.load(JS);
    const second = session.load(JS);
    const third = session.load(SCENE);
    await tick();
    compiling.scenes[0]!.resolve("return vis.scene {}");
    compiling.scenes[1]!.reject(new SceneError({phase: "load", message: "broken"}));
    await assert.rejects(first, SupersededError);
    await assert.rejects(second, SupersededError);
    assert.deepEqual(sent(server, "load").map((message) => message.scene.source), [SCENE.source]);
    void third.catch(() => undefined);
});

test("a scene whose code fails or runs too long fails its load with the compiler's error", async () => {
    const compiling = testCompiler();
    const {host, server} = inlineHost({compiler: compiling.compiler});
    const session = host.open({} as HTMLCanvasElement, handlers().handler);
    const load = session.load(JS);
    await tick();
    compiling.scenes[0]!.reject(new SceneError({phase: "timeout", message: "ran too long"}));
    await assert.rejects(load, (error) => error instanceof SceneError && error.phase === "timeout" && error.message === "ran too long");
    assert.deepEqual(sent(server, "load"), []);
});

test("a binary that fails to download is reported, and asked for again by the next load", async () => {
    let attempts = 0;
    const {host, server} = inlineHost({wasm: async () => {
        if (++attempts === 1) throw new Error("offline");
        return WASM;
    }});
    const session = host.open({} as HTMLCanvasElement, handlers().handler);
    await tick();
    assert.match(sent(server, "init")[0]?.error ?? "", /offline/);
    void session.load(SCENE).catch(() => undefined);
    await tick();
    await tick();
    assert.equal(attempts, 2);
    assert.equal(sent(server, "init")[1]?.wasm?.byteLength, 8);
});

// A dedicated worker the test controls, with the canvas transfer the host needs to choose it.
class FakeWorker {
    static instances: FakeWorker[] = [];
    static answering = true;
    onmessage: ((event: {data: ServerMessage}) => void) | null = null;
    onerror: ((event: {message: string; preventDefault(): void}) => void) | null = null;
    readonly received: ClientMessage[] = [];
    terminated = false;

    constructor() {
        FakeWorker.instances.push(this);
    }

    postMessage(message: ClientMessage) {
        this.received.push(message);
        if (message.type === "ping" && FakeWorker.answering) setTimeout(() => this.onmessage?.({data: {type: "pong"}}), 0);
    }

    terminate() {
        this.terminated = true;
    }
}

function withWorkers() {
    const scope = globalThis as Record<string, unknown>;
    const saved = {Worker: scope.Worker, OffscreenCanvas: scope.OffscreenCanvas, HTMLCanvasElement: scope.HTMLCanvasElement};
    class Canvas {
        transferControlToOffscreen() {
            return {};
        }
    }
    Object.assign(scope, {Worker: FakeWorker, OffscreenCanvas: class {}, HTMLCanvasElement: Canvas});
    FakeWorker.instances = [];
    FakeWorker.answering = true;
    return {
        canvas: () => new Canvas() as unknown as HTMLCanvasElement,
        restore: () => Object.assign(scope, saved),
    };
}

test("a worker stuck in a scene is replaced: that session is told it timed out, the others to restart", async () => {
    const workers = withWorkers();
    try {
        const host = createRenderHost({server: {url: "https://viscript.invalid/player-worker.js"}, wasm: async () => WASM,
            assets: {bytes: async () => new ArrayBuffer(0)}, hangTimeout: 50});
        hosts.push(host);
        assert.equal(host.mode, "worker");
        const stuck = handlers(), other = handlers();
        const session = host.open(workers.canvas(), stuck.handler);
        host.open(workers.canvas(), other.handler);
        await tick();
        FakeWorker.answering = false;
        const load = session.load(SCENE);
        await assert.rejects(load, SupersededError);
        assert.deepEqual(stuck.events, ["lost timeout"]);
        assert.deepEqual(other.events, ["lost restart"]);
        assert.equal(FakeWorker.instances[0]!.terminated, true);
        // The next session starts a new worker.
        FakeWorker.answering = true;
        host.open(workers.canvas(), handlers().handler);
        assert.equal(FakeWorker.instances.length, 2);
    } finally {
        workers.restore();
    }
});

test("the scene the worker said it was running is blamed, not an older request waiting for it", async () => {
    const workers = withWorkers();
    try {
        const host = createRenderHost({server: {url: "https://viscript.invalid/player-worker.js"}, wasm: async () => WASM,
            assets: {bytes: async () => new ArrayBuffer(0)}, hangTimeout: 50});
        hosts.push(host);
        const waiting = handlers(), looping = handlers();
        const first = host.open(workers.canvas(), waiting.handler);
        const second = host.open(workers.canvas(), looping.handler);
        await tick();
        void first.load(SCENE).catch(() => undefined);
        void second.load(SCENE).catch(() => undefined);
        await tick();
        const worker = FakeWorker.instances[0]!;
        const load = worker.received.filter((message) => message.type === "load")[1] as Extract<ClientMessage, {type: "load"}>;
        FakeWorker.answering = false;
        worker.onmessage?.({data: {type: "running", session: load.session, seq: load.seq}});
        await new Promise((resolve) => setTimeout(resolve, 200));
        assert.deepEqual(waiting.events, ["lost restart"]);
        assert.deepEqual(looping.events, ["lost timeout"]);
    } finally {
        workers.restore();
    }
});

test("a worker that answers pings is not replaced while it waits", async () => {
    const workers = withWorkers();
    try {
        const host = createRenderHost({server: {url: "https://viscript.invalid/player-worker.js"}, wasm: async () => WASM,
            assets: {bytes: async () => new ArrayBuffer(0)}, hangTimeout: 50});
        hosts.push(host);
        const waiting = handlers();
        const session = host.open(workers.canvas(), waiting.handler);
        let settled = false;
        session.load(SCENE).then(() => { settled = true; }, () => { settled = true; });
        await new Promise((resolve) => setTimeout(resolve, 300));
        assert.equal(settled, false);
        assert.deepEqual(waiting.events, []);
        assert.ok(FakeWorker.instances[0]!.received.filter((message) => message.type === "ping").length >= 2);
    } finally {
        workers.restore();
    }
});

test("a worker that fails to start hands its sessions to the main thread", async () => {
    const workers = withWorkers();
    const scope = globalThis as Record<string, unknown>;
    scope.__fakeServer = () => ({handle: () => undefined, dispose: () => undefined});
    const warn = console.warn;
    console.warn = () => undefined;
    try {
        const host = createRenderHost({server: {code: `globalThis.__viscriptRenderServer = globalThis.__fakeServer; // fallback`},
            wasm: async () => WASM, assets: {bytes: async () => new ArrayBuffer(0)}});
        hosts.push(host);
        const {events, handler} = handlers();
        host.open(workers.canvas(), handler);
        FakeWorker.instances[0]!.onerror?.({message: "blocked", preventDefault: () => undefined});
        assert.equal(host.mode, "inline");
        assert.deepEqual(events, ["lost restart"]);
        assert.equal(FakeWorker.instances[0]!.terminated, true);
    } finally {
        console.warn = warn;
        workers.restore();
    }
});
