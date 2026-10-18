// The main thread's side of the render server: it starts the server (in a worker, or inline when
// the browser cannot hand a canvas to a worker), forwards each player's requests, and keeps the
// worker honest. Frames are coalesced per session so a slow server is never sent a backlog, and a
// worker that stops answering is replaced; its players get new canvases and load again.
import {fontType, type FontProvider} from "../fonts/provider.ts";
import {
    INLINE_SERVER, type ClientMessage, type ErrorInfo, type FrameRequest, type FrameResult, type SceneInfo,
    type SceneRequest, type ServerMessage,
} from "../protocol.ts";

export interface AssetProvider {
    /** The bytes of an image a scene loads, by the id its SceneResources gave. */
    bytes(id: string): Promise<ArrayBuffer>;
}

/** The server bundle (player-worker.js): where it is served from, or its source text. */
export type ServerScript = {readonly url: string | URL} | {readonly code: string};

export type HostMode = "worker" | "inline";

/** Turns a JavaScript scene into the Lua it builds, as createSandboxCompiler does. */
export interface SceneCompiler {
    /** The scene's Lua; rejects with a SceneError when the scene's code fails or runs too long. */
    compile(source: string, name: string): Promise<string>;
    dispose(): void;
}

export interface RenderHostOptions {
    readonly server: ServerScript;
    /** The WASM binary (vis-wasm.wasm). Asked for once, and again only after it failed. */
    readonly wasm: () => Promise<ArrayBuffer>;
    /** The images scenes load; without it, a scene that loads one fails. */
    readonly assets?: AssetProvider;
    /**
     * The fonts scenes set text in, by name, as ThorVG's FontProvider: asked for each name a scene
     * uses that the render server has no font for yet. Text in a font it has none for, or none
     * given, is drawn in the engine's default font. `runtimeFonts` gives the runtime's code font.
     */
    readonly fonts?: FontProvider;
    /** Milliseconds a font may take to arrive before its text is drawn in the default font. */
    readonly fontTimeout?: number;
    /**
     * "auto" (the default) renders in a worker when the browser can transfer a canvas to one,
     * and on the main thread otherwise or when the worker cannot start.
     */
    readonly mode?: "auto" | HostMode;
    /** Milliseconds a busy worker may go without answering before it is replaced. */
    readonly hangTimeout?: number;
    /**
     * Whether JavaScript scenes may run on the main thread, where they can do all the page can.
     * A host that shows scenes it did not write, such as notes from others, can keep them to the
     * worker. True unless given; with a compiler, scene code runs in neither.
     */
    readonly inlineJavaScript?: boolean;
    /**
     * Compiles JavaScript scenes before they reach the render server, which is then sent only the
     * Lua they build: their code runs neither there nor on the page, in whichever mode. A host that
     * shows scenes it did not write gives createSandboxCompiler, which also keeps them off the
     * network and apart from each other. The host disposes it with itself.
     */
    readonly compiler?: SceneCompiler;
}

/** Why a JavaScript scene was not run on the main thread. */
export const INLINE_JAVASCRIPT_REFUSED = "This JavaScript scene was not run: JavaScript scenes run only in the render worker, and it is not running";

/** Why a session lost its canvas: its worker was replaced, or this session's scene stalled it. */
export type LostReason = "restart" | "timeout";

export interface SessionHandlers {
    framed(result: FrameResult): void;
    failed(error: ErrorInfo): void;
    /** The canvas went with its worker. Open a new session on a new canvas. */
    lost(reason: LostReason): void;
}

export interface HostSession {
    /** Loads a scene; rejects with SupersededError when a later load replaces it first. */
    load(scene: SceneRequest): Promise<SceneInfo>;
    /** Asks for a frame; while one is being drawn, later requests merge into one. */
    frame(frame: FrameRequest): void;
    /** Frees the scene's engine but keeps the canvas, for a player out of view. */
    release(): void;
    close(): void;
}

export interface RenderHost {
    readonly mode: HostMode;
    open(canvas: HTMLCanvasElement, handlers: SessionHandlers): HostSession;
    /**
     * Tells the render server that the provider's fonts changed. A name keeps the first font it
     * was given in an engine, so scenes get the new fonts the next time they load, in new engines.
     */
    refreshFonts(): void;
    dispose(): void;
}

/** A load that a later load on the same session replaced, or whose session closed. */
export class SupersededError extends Error {
    constructor() {
        super("A later load replaced this one");
        this.name = "SupersededError";
    }
}

/** A load that failed in the scene or the runtime. */
export class SceneError extends Error {
    readonly phase: ErrorInfo["phase"];

    constructor(info: ErrorInfo) {
        super(info.message);
        this.name = "SceneError";
        this.phase = info.phase;
    }
}

interface Transport {
    post(message: ClientMessage, transfer?: Transferable[]): void;
    terminate(): void;
}

interface Outstanding {
    readonly session: number;
    readonly sentAt: number;
}

interface SessionState {
    readonly id: number;
    readonly handlers: SessionHandlers;
    readonly loads: Map<number, {resolve(info: SceneInfo): void; reject(error: Error): void}>;
    /** Loads asked for, so a scene still compiling gives way to a later load. */
    asked: number;
    /** The frame the server is drawing, by sequence number. */
    inflight: number;
    /** Requests that arrived while one was in flight, merged. */
    pending: FrameRequest | null;
}

const canTransfer = (): boolean => typeof Worker === "function" && typeof OffscreenCanvas === "function"
    && typeof HTMLCanvasElement !== "undefined" && "transferControlToOffscreen" in HTMLCanvasElement.prototype;

// The newest time and plan win; input queues up, and so does the time a live scene steps by. A
// release lets go of everything before it, so input queued before one is dropped.
export function merge(older: FrameRequest | null, newer: FrameRequest): FrameRequest {
    if (!older) return newer;
    const live = older.advance !== undefined || newer.advance !== undefined;
    const input = [...newer.releaseInput ? [] : older.input ?? [], ...newer.input ?? []];
    return {
        time: newer.time,
        plan: newer.plan,
        ...(input.length ? {input} : {}),
        ...(live ? {advance: (older.advance ?? 0) + (newer.advance ?? 0)} : {}),
        ...(older.releaseInput || newer.releaseInput ? {releaseInput: true} : {}),
        ...(newer.crossfade ?? older.crossfade ? {crossfade: newer.crossfade ?? older.crossfade} : {}),
    };
}

const copy = (data: ArrayBuffer): ArrayBuffer => data.slice(0);
const bytesOf = (data: Uint8Array | ArrayBuffer): ArrayBuffer => data instanceof ArrayBuffer ? data
    : data.buffer.slice(data.byteOffset, data.byteOffset + data.byteLength) as ArrayBuffer;
const errorText = (error: unknown) => error instanceof Error ? error.message : String(error);

class Host implements RenderHost {
    readonly #options: RenderHostOptions;
    readonly #hangTimeout: number;
    #mode: HostMode;
    #transport: Transport | null = null;
    /** The worker answered at least once, so it started. */
    #started = false;
    #wasm: Promise<ArrayBuffer> | null = null;
    readonly #sessions = new Map<number, SessionState>();
    /** Requests the server has not answered, oldest first. */
    readonly #outstanding = new Map<number, Outstanding>();
    #nextSession = 1;
    #nextSeq = 1;
    #lastHeard = 0;
    /** When the unanswered ping was sent, or 0. */
    #pingedAt = 0;
    /** The request whose scene code the server said it is running. */
    #running: Outstanding & {readonly seq: number} | null = null;
    #watchdog: ReturnType<typeof setInterval> | undefined;
    #disposed = false;
    /**
     * The images loads have named. Scene code runs where the server does and can post as it, so
     * the server is sent only images a load gave it.
     */
    readonly #offered = new Set<string>();

    constructor(options: RenderHostOptions) {
        this.#options = options;
        this.#hangTimeout = options.hangTimeout ?? 10000;
        const mode = options.mode ?? "auto";
        this.#mode = mode === "inline" || (mode === "auto" && !canTransfer()) ? "inline" : "worker";
    }

    get mode(): HostMode {
        return this.#mode;
    }

    open(canvas: HTMLCanvasElement, handlers: SessionHandlers): HostSession {
        if (this.#disposed) throw new Error("This render host was disposed");
        const transport = this.#connect();
        const state: SessionState = {id: this.#nextSession++, handlers, loads: new Map(), asked: 0, inflight: 0, pending: null};
        this.#sessions.set(state.id, state);
        if (this.#mode === "worker") {
            const offscreen = canvas.transferControlToOffscreen();
            transport.post({type: "open", session: state.id, canvas: offscreen}, [offscreen]);
        } else {
            transport.post({type: "open", session: state.id, canvas});
        }
        return {
            load: (scene) => this.#load(state, scene),
            frame: (frame) => this.#frame(state, frame),
            release: () => {
                if (this.#sessions.get(state.id) === state) this.#transport?.post({type: "release", session: state.id});
            },
            close: () => this.#close(state),
        };
    }

    refreshFonts(): void {
        this.#transport?.post({type: "fonts"});
    }

    dispose(): void {
        if (this.#disposed) return;
        this.#disposed = true;
        for (const state of this.#sessions.values()) this.#close(state);
        this.#stopWatchdog();
        this.#transport?.terminate();
        this.#transport = null;
        this.#options.compiler?.dispose();
    }

    #connect(): Transport {
        if (this.#transport) return this.#transport;
        const receive = (message: ServerMessage) => this.#receive(transport, message);
        const transport = this.#mode === "worker"
            ? this.#startWorker(receive)
            : this.#startInline(receive);
        this.#transport = transport;
        this.#started = this.#mode === "inline";
        this.#sendWasm(transport);
        return transport;
    }

    #startWorker(receive: (message: ServerMessage) => void): Transport {
        const script = this.#options.server;
        const url = "url" in script ? script.url
            : URL.createObjectURL(new Blob([script.code], {type: "text/javascript"}));
        const worker = new Worker(url);
        worker.onmessage = (event: MessageEvent<ServerMessage>) => receive(event.data);
        worker.onerror = (event) => {
            // A worker that never answered could not start (a blocked or missing script): render
            // on the main thread instead. A later error is the server's own and is reported per
            // request.
            if (this.#transport !== transport || this.#started) return;
            event.preventDefault();
            console.warn("viscript: the render worker did not start; rendering on the main thread", event.message);
            this.#mode = "inline";
            this.#restart(null);
        };
        const transport: Transport = {
            post: (message, transfer = []) => worker.postMessage(message, transfer),
            terminate: () => {
                worker.terminate();
                if (!("url" in script)) URL.revokeObjectURL(url as string);
            },
        };
        // The first answer tells that the worker runs.
        worker.postMessage({type: "ping"} satisfies ClientMessage);
        return transport;
    }

    #startInline(receive: (message: ServerMessage) => void): Transport {
        type Factory = (post: (message: ServerMessage) => void) => {handle(message: ClientMessage): void; dispose(): void};
        let stopped = false;
        const post = (message: ServerMessage) => queueMicrotask(() => { if (!stopped) receive(message); });
        const server = loadInlineServer(this.#options.server).then((factory: Factory) => factory(post));
        server.catch((error: unknown) => {
            for (const state of this.#sessions.values()) {
                for (const {reject} of state.loads.values()) reject(new SceneError({phase: "engine", message: String(error)}));
                state.loads.clear();
            }
        });
        return {
            post: (message) => { void server.then((target) => { if (!stopped) target.handle(message); }); },
            terminate: () => {
                stopped = true;
                void server.then((target) => target.dispose(), () => undefined);
            },
        };
    }

    #sendWasm(transport: Transport): void {
        const wasm = this.#wasm ??= this.#options.wasm();
        wasm.then((data) => {
            if (this.#transport !== transport) return;
            const bytes = copy(data);
            transport.post({type: "init", wasm: bytes}, [bytes]);
        }, (error: unknown) => {
            // Asked for again by the next load.
            if (this.#wasm === wasm) this.#wasm = null;
            if (this.#transport === transport) {
                transport.post({type: "init", error: `Could not load the viscript runtime: ${error instanceof Error ? error.message : String(error)}`});
            }
        });
    }

    #load(state: SessionState, scene: SceneRequest): Promise<SceneInfo> {
        if (this.#sessions.get(state.id) !== state || !this.#transport) return Promise.reject(new SupersededError());
        const asked = ++state.asked;
        const compiler = this.#options.compiler;
        if (scene.language === "js" && compiler) {
            // A later load, or the session closing, takes over from a scene still compiling.
            const current = () => state.asked === asked && this.#sessions.get(state.id) === state;
            return compiler.compile(scene.source, scene.name).then(
                (source) => current() ? this.#load(state, {...scene, source, language: "lua"}) : Promise.reject(new SupersededError()),
                (error: unknown) => Promise.reject(current() ? error : new SupersededError()));
        }
        if (scene.language === "js" && this.#mode === "inline" && this.#options.inlineJavaScript === false) {
            return Promise.reject(new SceneError({phase: "load", message: INLINE_JAVASCRIPT_REFUSED}));
        }
        for (const asset of scene.resources.assets) this.#offered.add(asset.id);
        // A runtime that failed to download is fetched again for the next load.
        if (!this.#wasm) this.#sendWasm(this.#transport);
        const seq = this.#nextSeq++;
        return new Promise<SceneInfo>((resolve, reject) => {
            state.loads.set(seq, {resolve, reject});
            this.#send({type: "load", session: state.id, seq, scene});
        });
    }

    #frame(state: SessionState, frame: FrameRequest): void {
        if (this.#sessions.get(state.id) !== state) return;
        if (state.inflight) {
            state.pending = merge(state.pending, frame);
            return;
        }
        const seq = this.#nextSeq++;
        state.inflight = seq;
        this.#send({type: "frame", session: state.id, seq, frame});
    }

    #close(state: SessionState): void {
        if (this.#sessions.get(state.id) !== state) return;
        this.#sessions.delete(state.id);
        for (const {reject} of state.loads.values()) reject(new SupersededError());
        state.loads.clear();
        state.pending = null;
        for (const [seq, request] of this.#outstanding) if (request.session === state.id) this.#outstanding.delete(seq);
        this.#transport?.post({type: "close", session: state.id});
    }

    #send(message: ClientMessage & {session: number; seq: number}): void {
        if (!this.#outstanding.size) this.#lastHeard = performance.now();
        this.#outstanding.set(message.seq, {session: message.session, sentAt: performance.now()});
        this.#transport!.post(message);
        if (this.#mode === "worker") this.#startWatchdog();
    }

    #receive(transport: Transport, message: ServerMessage): void {
        if (this.#transport !== transport) return;
        this.#started = true;
        this.#lastHeard = performance.now();
        this.#pingedAt = 0;
        switch (message.type) {
            case "running":
                this.#running = {session: message.session, seq: message.seq, sentAt: performance.now()};
                return;
            case "loaded":
            case "skipped":
            case "failed":
            case "framed": {
                this.#outstanding.delete(message.seq);
                if (this.#running?.seq === message.seq) this.#running = null;
                const state = this.#sessions.get(message.session);
                if (!state) return;
                const load = state.loads.get(message.seq);
                if (load) {
                    state.loads.delete(message.seq);
                    if (message.type === "loaded") load.resolve(message.info);
                    else if (message.type === "failed") load.reject(new SceneError(message.error));
                    else load.reject(new SupersededError());
                    return;
                }
                if (state.inflight !== message.seq) return;
                state.inflight = 0;
                if (message.type === "framed") state.handlers.framed(message.result);
                else if (message.type === "failed") state.handlers.failed(message.error);
                if (state.pending && this.#sessions.get(state.id) === state) {
                    const next = state.pending;
                    state.pending = null;
                    this.#frame(state, next);
                }
                return;
            }
            case "fetch": {
                const {request, id} = message;
                const assets = this.#options.assets;
                if (!this.#offered.has(id) || !assets) {
                    transport.post({type: "bytes", request, error: `No scene was given the image ${id}`});
                    return;
                }
                assets.bytes(id).then((data) => {
                    if (this.#transport !== transport) return;
                    const bytes = this.#mode === "worker" ? copy(data) : data;
                    transport.post({type: "bytes", request, data: bytes}, this.#mode === "worker" ? [bytes] : []);
                }, (error: unknown) => {
                    if (this.#transport !== transport) return;
                    transport.post({type: "bytes", request, error: errorText(error)});
                });
                return;
            }
            case "font":
                this.#font(transport, message.request, message.name);
                return;
            case "fatal":
                console.error(`viscript: ${message.error.message}`);
                return;
            case "pong":
                return;
        }
    }

    // A provider that fails, or takes longer than the font timeout, leaves the text in the default
    // font for this load; the server asks again on a later one.
    #font(transport: Transport, request: number, name: string): void {
        const provider = this.#options.fonts;
        if (!provider) {
            transport.post({type: "font", request});
            return;
        }
        let timer: ReturnType<typeof setTimeout> | undefined;
        const timeout = new Promise<never>((_, reject) => {
            timer = setTimeout(() => reject(new Error(`The font ${name} took too long to arrive`)), this.#options.fontTimeout ?? 15000);
        });
        Promise.race([provider.fetch(name), timeout]).finally(() => clearTimeout(timer)).then((found) => {
            if (this.#transport !== transport) return;
            if (!found) {
                transport.post({type: "font", request});
                return;
            }
            const data = this.#mode === "worker" ? copy(bytesOf(found.data)) : bytesOf(found.data);
            transport.post({type: "font", request, data, fontType: found.type ?? fontType(data)}, this.#mode === "worker" ? [data] : []);
        }, (error: unknown) => {
            console.warn(`viscript: no font for "${name}":`, errorText(error));
            if (this.#transport === transport) transport.post({type: "font", request, error: errorText(error)});
        });
    }

    // Pings go out after a second of silence, or sooner under a short hang timeout.
    get #pingAfter(): number {
        return Math.min(1000, this.#hangTimeout / 2);
    }

    #startWatchdog(): void {
        this.#watchdog ??= setInterval(() => this.#check(), this.#pingAfter / 2);
    }

    #stopWatchdog(): void {
        clearInterval(this.#watchdog);
        this.#watchdog = undefined;
        this.#pingedAt = 0;
    }

    // A worker busy or waiting on bytes still answers pings; one stuck in a scene's code does
    // not. The scene it said it was running is the one it is stuck in; failing that, the session
    // whose request has waited longest.
    #check(): void {
        const oldest = this.#outstanding.values().next().value;
        if (!oldest || !this.#transport) {
            this.#stopWatchdog();
            return;
        }
        const now = performance.now();
        if (this.#pingedAt) {
            const culprit = this.#running ?? oldest;
            if (now - this.#pingedAt >= this.#hangTimeout && now - culprit.sentAt >= this.#hangTimeout) this.#restart(culprit.session);
        } else if (now - this.#lastHeard >= this.#pingAfter) {
            this.#pingedAt = now;
            this.#transport.post({type: "ping"});
        }
    }

    // Every canvas went with the worker, so each session is closed and its player told to open
    // a new one; the next open starts a new worker.
    #restart(culprit: number | null): void {
        this.#stopWatchdog();
        this.#transport?.terminate();
        this.#transport = null;
        this.#outstanding.clear();
        this.#running = null;
        const sessions = [...this.#sessions.values()];
        this.#sessions.clear();
        for (const state of sessions) {
            for (const {reject} of state.loads.values()) reject(new SupersededError());
            state.loads.clear();
            state.pending = null;
            state.inflight = 0;
        }
        for (const state of sessions) state.handlers.lost(state.id === culprit ? "timeout" : "restart");
    }
}

const inlineServers = new Map<string, Promise<unknown>>();

// The server bundle leaves its factory on globalThis when it is not in a worker.
function loadInlineServer(script: ServerScript): Promise<any> {
    const key = "url" in script ? String(script.url) : script.code;
    let pending = inlineServers.get(key);
    if (!pending) {
        pending = new Promise<void>((resolve, reject) => {
            if ("code" in script) {
                new Function(script.code)();
                resolve();
                return;
            }
            const element = document.createElement("script");
            element.src = String(script.url);
            element.onload = () => resolve();
            element.onerror = () => reject(new Error(`Could not load the viscript render server from ${script.url}`));
            document.head.append(element);
        }).then(() => {
            const factory = (globalThis as Record<string, unknown>)[INLINE_SERVER];
            if (typeof factory !== "function") throw new Error("The viscript render server did not start");
            return factory;
        });
        pending.catch(() => inlineServers.delete(key));
        inlineServers.set(key, pending);
    }
    return pending;
}

export function createRenderHost(options: RenderHostOptions): RenderHost {
    return new Host(options);
}
