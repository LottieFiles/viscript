// The render server: owns the WASM runtime, one session per player, and the shared instances.
// It runs in the render worker (worker.ts) or, when a page cannot hand canvases to a worker, on
// the main thread; either way it only talks through ClientMessage and ServerMessage.
import type {HostThemePalette, SceneBuilder, VisModule, VisScene} from "vis-wasm-bundle";
import {fontType} from "../fonts/provider.ts";
import type {
    AssetRef, ClientMessage, ErrorInfo, ErrorPhase, FrameRequest, FrameResult, SceneInfo, SceneRequest, ServerMessage,
} from "../protocol.ts";
import {InstancePool, type FontAnswer, type Lease} from "./pool.ts";
import {type Canvas, Surface} from "./surface.ts";

/** The parts of the runtime bundle (client.js) the server calls. */
export interface RuntimeApi {
    createVisModule(options: Record<string, unknown>): Promise<VisModule>;
    createVis(source: string, name: string, options: Record<string, unknown>): Promise<VisScene>;
    evaluateScene(source: string, name: string): SceneBuilder;
}

export interface ServerEnvironment {
    readonly runtime: RuntimeApi;
    readonly post: (message: ServerMessage, transfer?: Transferable[]) => void;
    /** A canvas for intermediate rasters, never shown. */
    readonly scratch: () => Canvas;
    readonly idleMs?: number;
}

const BOOT = "local s = vis.scene {width = 16, height = 16}\nreturn s";
const STOPPED = "The viscript runtime stopped after an error; load the scene again";
/** Fonts one load asks for at most, and the longest name asked for. */
const MAX_FONTS = 16, MAX_FONT_NAME = 256;

// Fields every input event carries; a host sends only what differs.
const INPUT = {x: 0, y: 0, dx: 0, dy: 0, wheelX: 0, wheelY: 0, button: -1, buttons: 0, modifiers: 0, pointerId: 0};

/** A failure to fetch bytes, as opposed to a failure inside the runtime. */
class ResourceError extends Error {}

function errorText(error: unknown): string {
    return error instanceof Error ? error.message : String(error);
}

function describe(engine: VisScene): SceneInfo {
    const [width, height] = engine.size();
    return {
        duration: engine.duration,
        width,
        height,
        loop: engine.loop,
        retained: engine.retainedLua,
        input: typeof engine.input === "function",
        runtime: typeof engine.advanceRuntime === "function",
        adaptiveTheme: engine.adaptiveTheme,
        cameraMode: engine.cameraMode,
        cameraView: engine.cameraView,
    };
}

class Session {
    readonly id: number;
    readonly surface: Surface;
    lease: Lease | null = null;
    /** The lease's engine holds a scene the player loaded, not the placeholder. */
    loaded = false;
    /** The newest load asked for; an older one still waiting on bytes gives way to it. */
    latest = 0;
    /** The retained runtime stepped since the last input frame began. */
    consumed = true;
    /** Operations run one at a time, so a frame never draws a scene that is half loaded. */
    queue: Promise<void> = Promise.resolve();
    closed = false;

    constructor(id: number, surface: Surface) {
        this.id = id;
        this.surface = surface;
    }
}

export class RenderServer {
    readonly #env: ServerEnvironment;
    readonly #pool: InstancePool;
    readonly #sessions = new Map<number, Session>();
    readonly #bytes = new Map<string, Promise<ArrayBuffer>>();
    readonly #requests = new Map<number, {resolve(data: ArrayBuffer): void; reject(error: Error): void}>();
    /** The host's answer for each font name, until its fonts change. */
    readonly #fonts = new Map<string, Promise<FontAnswer | null>>();
    readonly #fontRequests = new Map<number, {resolve(font: FontAnswer | null): void; reject(error: Error): void}>();
    #nextRequest = 1;
    #compiled!: Promise<WebAssembly.Module>;
    #compiledState: "waiting" | "compiling" | "ready" | "failed" = "waiting";
    #settleCompiled: {resolve(module: WebAssembly.Module): void; reject(error: Error): void} | null = null;

    constructor(env: ServerEnvironment) {
        this.#env = env;
        this.#pool = new InstancePool({
            createModule: (onAbort) => this.#createModule(onAbort),
            createEngine: (module) => env.runtime.createVis(BOOT, "boot.lua", {renderEngine: "cpu", module}),
            font: (name: string) => this.#font(name),
            assetBytes: (asset: AssetRef) => this.#fetch(asset.id),
        }, {idleMs: env.idleMs});
        this.#awaitWasm();
    }

    handle(message: ClientMessage): void {
        switch (message.type) {
            case "init":
                this.#init(message.wasm, message.error);
                break;
            case "open":
                this.#sessions.get(message.session)?.surface.dispose();
                this.#sessions.set(message.session, new Session(message.session, new Surface(message.canvas, this.#env.scratch)));
                break;
            case "load": {
                const session = this.#sessions.get(message.session);
                if (!session) break;
                session.latest = message.seq;
                this.#enqueue(session, () => this.#load(session, message.seq, message.scene));
                break;
            }
            case "frame": {
                const session = this.#sessions.get(message.session);
                if (session) this.#enqueue(session, () => this.#frame(session, message.seq, message.frame));
                break;
            }
            case "release": {
                const session = this.#sessions.get(message.session);
                if (session) this.#enqueue(session, () => this.#release(session));
                break;
            }
            case "close": {
                const session = this.#sessions.get(message.session);
                if (!session) break;
                this.#sessions.delete(message.session);
                session.closed = true;
                this.#enqueue(session, () => {
                    this.#release(session);
                    session.surface.dispose();
                });
                break;
            }
            case "bytes": {
                const request = this.#requests.get(message.request);
                this.#requests.delete(message.request);
                if (!request) break;
                if (message.data) request.resolve(message.data);
                else request.reject(new ResourceError(message.error ?? "The resource could not be loaded"));
                break;
            }
            case "font": {
                const request = this.#fontRequests.get(message.request);
                this.#fontRequests.delete(message.request);
                if (!request) break;
                if (message.data) request.resolve({data: message.data, type: message.fontType ?? fontType(message.data)});
                else if (message.error) request.reject(new Error(message.error));
                else request.resolve(null);
                break;
            }
            case "fonts":
                this.#fonts.clear();
                this.#pool.retire();
                break;
            case "ping":
                this.#env.post({type: "pong"});
                break;
        }
    }

    dispose(): void {
        for (const session of this.#sessions.values()) {
            session.closed = true;
            this.#release(session);
            session.surface.dispose();
        }
        this.#sessions.clear();
        this.#pool.dispose();
    }

    // Sessions may open and load before the binary arrives; instances wait for it. It is compiled
    // once, and every instance instantiates the same module.
    #awaitWasm(): void {
        this.#compiledState = "waiting";
        this.#compiled = new Promise((resolve, reject) => { this.#settleCompiled = {resolve, reject}; });
        this.#compiled.catch(() => undefined);
    }

    #init(wasm: ArrayBuffer | undefined, error: string | undefined): void {
        if (this.#compiledState === "ready" || this.#compiledState === "compiling") return;
        if (this.#compiledState === "failed") this.#awaitWasm();
        const settle = this.#settleCompiled!;
        const fail = (reason: string) => {
            this.#compiledState = "failed";
            settle.reject(new Error(reason));
            this.#env.post({type: "fatal", error: {phase: "engine", message: reason}});
        };
        if (!wasm) {
            fail(error ?? "The viscript runtime was not loaded");
            return;
        }
        this.#compiledState = "compiling";
        WebAssembly.compile(wasm).then((module) => {
            this.#compiledState = "ready";
            settle.resolve(module);
        }, (reason: unknown) => fail(`The viscript runtime did not compile: ${errorText(reason)}`));
    }

    #enqueue(session: Session, step: () => void | Promise<void>): void {
        session.queue = session.queue.then(step).catch((error: unknown) => {
            // Each step reports its own failures; reaching here is a bug in the server.
            console.error("viscript render server:", error);
        });
    }

    async #createModule(onAbort: () => void): Promise<VisModule> {
        const compiled = await this.#compiled;
        // The runtime's loader waits for instantiateWasm to call back and has no way to fail,
        // so a failed instantiation rejects through this promise instead.
        let fail: (error: unknown) => void = () => undefined;
        const failed = new Promise<never>((_, reject) => { fail = reject; });
        const module = this.#env.runtime.createVisModule({
            instantiateWasm: (imports: WebAssembly.Imports,
                receive: (instance: WebAssembly.Instance, module: WebAssembly.Module) => void) => {
                WebAssembly.instantiate(compiled, imports).then((instance) => receive(instance, compiled), fail);
                return {};
            },
            onAbort,
        });
        return Promise.race([module, failed]);
    }

    // An image's bytes are asked of the host once per server, and kept for the engines that load
    // it later. A failed fetch is forgotten, so the next load asks again.
    #fetch(id: string): Promise<ArrayBuffer> {
        let pending = this.#bytes.get(id);
        if (!pending) {
            const request = this.#nextRequest++;
            pending = new Promise<ArrayBuffer>((resolve, reject) => this.#requests.set(request, {resolve, reject}));
            this.#bytes.set(id, pending);
            pending.catch(() => this.#bytes.delete(id));
            this.#env.post({type: "fetch", request, id});
        }
        return pending;
    }

    // A font is asked for by name once, and its answer kept for every instance, until the host's
    // fonts change. A failed fetch is forgotten, so a later load asks again.
    #font(name: string): Promise<FontAnswer | null> {
        let pending = this.#fonts.get(name);
        if (!pending) {
            const request = this.#nextRequest++;
            const asked = new Promise<FontAnswer | null>((resolve, reject) => this.#fontRequests.set(request, {resolve, reject}));
            this.#fonts.set(name, asked);
            asked.catch(() => { if (this.#fonts.get(name) === asked) this.#fonts.delete(name); });
            this.#env.post({type: "font", request, name});
            pending = asked;
        }
        return pending;
    }

    #fail(session: Session, seq: number, phase: ErrorPhase, error: unknown): void {
        const info: ErrorInfo = {phase, message: errorText(error)};
        this.#env.post({type: "failed", session: session.id, seq, error: info});
    }

    async #load(session: Session, seq: number, scene: SceneRequest): Promise<void> {
        const stale = () => session.closed || seq !== session.latest;
        if (stale()) {
            this.#env.post({type: "skipped", session: session.id, seq});
            return;
        }
        let lease = session.lease;
        try {
            if (lease && !this.#pool.fits(lease)) {
                this.#release(session);
                lease = null;
            }
            lease ??= session.lease = await this.#pool.acquire();
            await this.#pool.ensureAssets(lease, scene.resources.assets);
        } catch (error) {
            if (lease) this.#pool.report(lease, error);
            this.#fail(session, seq, error instanceof ResourceError ? "resource" : "engine", error);
            return;
        }
        if (stale()) {
            this.#env.post({type: "skipped", session: session.id, seq});
            return;
        }
        this.#env.post({type: "running", session: session.id, seq});
        const engine = lease.engine;
        let input: string | SceneBuilder;
        try {
            if (scene.theme) engine.hostTheme(scene.theme as HostThemePalette);
            input = scene.language === "js" ? this.#env.runtime.evaluateScene(scene.source, scene.name) : scene.source;
            // A failed load keeps the scene before it, so a player can keep showing it.
            engine.load(input, scene.name);
            session.loaded = true;
            session.consumed = true;
        } catch (error) {
            this.#pool.report(lease, error);
            this.#fail(session, seq, "load", error);
            return;
        }
        // Text in a font the instance has not been given was set in the default font. The host is
        // asked for those fonts, and the scene loads once more with the ones it gives.
        const missing = (engine.missingFonts?.() ?? []).filter((name) => name.length <= MAX_FONT_NAME).slice(0, MAX_FONTS);
        if (missing.length) {
            let added: boolean;
            try {
                added = await this.#pool.registerFonts(lease, missing);
            } catch (error) {
                this.#fail(session, seq, "engine", error);
                return;
            }
            if (stale()) {
                this.#env.post({type: "skipped", session: session.id, seq});
                return;
            }
            if (added) {
                this.#env.post({type: "running", session: session.id, seq});
                try {
                    engine.load(input, scene.name);
                } catch (error) {
                    this.#pool.report(lease, error);
                    this.#fail(session, seq, "load", error);
                    return;
                }
            }
        }
        this.#env.post({type: "loaded", session: session.id, seq, info: describe(engine)});
    }

    #frame(session: Session, seq: number, frame: FrameRequest): void {
        const lease = session.lease;
        // Released, or nothing loaded yet: the player loads again before it needs a frame.
        if (!lease || !session.loaded || session.closed) {
            this.#env.post({type: "framed", session: session.id, seq, result: {}});
            return;
        }
        if (!this.#pool.usable(lease)) {
            this.#fail(session, seq, "engine", STOPPED);
            return;
        }
        const engine = lease.engine;
        let result: FrameResult = {};
        const live = frame.advance !== undefined || frame.input?.length || frame.releaseInput;
        if (live) {
            try {
                if (session.consumed) engine.beginInputFrame?.(engine.runtimeTime);
                if (frame.releaseInput) engine.releaseInput?.(engine.runtimeTime);
                for (const event of frame.input ?? []) engine.input?.({...INPUT, ...event, time: engine.runtimeTime});
                const step = frame.advance !== undefined ? engine.advanceRuntime?.(frame.advance) : null;
                session.consumed = !step || step.steps > 0;
                result = {runtimeTime: engine.runtimeTime, ...(step?.sounds.length ? {sounds: step.sounds} : {})};
            } catch (error) {
                this.#pool.report(lease, error);
                // After a trap there is nothing left to draw with, so the scene stops here; any
                // other runtime error leaves the scene drawable without its runtime.
                if (!this.#pool.usable(lease)) {
                    this.#fail(session, seq, "runtime", error);
                    return;
                }
                result = {runtimeError: {phase: "runtime", message: errorText(error)}};
            }
        }
        try {
            session.surface.present(engine, frame.time, frame.plan, frame.crossfade);
        } catch (error) {
            this.#pool.report(lease, error);
            this.#fail(session, seq, "render", error);
            return;
        }
        this.#env.post({type: "framed", session: session.id, seq, result});
    }

    #release(session: Session): void {
        const lease = session.lease;
        session.lease = null;
        session.loaded = false;
        if (lease) this.#pool.release(lease);
    }
}
