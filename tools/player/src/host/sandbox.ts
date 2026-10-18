// JavaScript scenes compiled where their code can reach nothing: a frame with an opaque origin,
// under a policy that refuses every request, starts a worker for each scene, which runs the scene's
// code and answers with the Lua it builds. The page, its storage and the render worker are out of
// the code's reach, and so are the network and the other scenes; a scene that runs too long is
// stopped without holding up any other.
import {SceneError, type SceneCompiler} from "./host.ts";

export interface SandboxOptions {
    /** The compiler bundle's source (player-compiler.js). */
    readonly code: string;
    /** Milliseconds a scene's code may run before it is stopped; 10000 unless given, as the render host waits for a hung worker. */
    readonly timeout?: number;
    /** The document the frame goes in: the page's unless given. */
    readonly document?: Document;
}

/** The most Lua a scene may build, in UTF-16 code units. */
const MAX_LUA = 64 * 1024 * 1024;
/** Milliseconds the frame has to start, and its first worker to check the policy. */
const START_TIMEOUT = 5000;
const POLICY = "default-src 'none'; script-src 'unsafe-inline' 'unsafe-eval' blob:; worker-src blob:";

const NOT_RUN = "This JavaScript scene was not run, since its sandbox cannot work here";
const CLOSED = "The sandbox for JavaScript scenes was closed";

// The frame's script. It keeps the compiler as a blob of the frame's own origin, checks once that
// its workers are held to the policy, and runs each scene in a new worker, ended when it answers or
// its time is up. What goes back to the page is picked field by field, never what a worker sent.
const RELAY = `
let compiler = null;
let sealed = null;
const send = (message) => parent.postMessage(message, "*");
const reason = (error) => String(error && error.message || error);
function check() {
  return sealed ??= new Promise((resolve) => {
    let worker;
    try { worker = new Worker(compiler); } catch (error) { resolve(reason(error)); return; }
    const finish = (result) => { clearTimeout(timer); worker.terminate(); resolve(result); };
    const timer = setTimeout(() => finish("its workers do not answer"), ${START_TIMEOUT});
    worker.onmessage = (event) => finish(event.data && event.data.sealed === true ? true : "its workers are not kept off the network");
    worker.onerror = (event) => { event.preventDefault(); finish(event.message || "its workers do not start"); };
    worker.postMessage({check: true});
  });
}
onmessage = async (event) => {
  if (event.source !== parent) return;
  const message = event.data;
  if (message.type === "code") {
    compiler = URL.createObjectURL(new Blob([message.code], {type: "text/javascript"}));
    return;
  }
  if (message.type !== "compile") return;
  const {id, source, name, timeout} = message;
  const ready = await check();
  if (ready !== true) { send({type: "compiled", id, unavailable: ready}); return; }
  let worker;
  try { worker = new Worker(compiler); } catch (error) { send({type: "compiled", id, unavailable: reason(error)}); return; }
  let done = false;
  const finish = (reply) => {
    if (done) return;
    done = true;
    clearTimeout(timer);
    worker.terminate();
    send({type: "compiled", id, ...reply});
  };
  const timer = setTimeout(() => finish({timeout: true}), timeout);
  worker.onmessage = (event) => {
    const data = event.data || {};
    finish(typeof data.lua === "string" ? {lua: data.lua} : {error: String(data.error)});
  };
  worker.onerror = (event) => { event.preventDefault(); finish({error: event.message || "The scene's code failed"}); };
  worker.postMessage({source, name});
};
send({type: "ready"});
`;

interface Pending {
    resolve(lua: string): void;
    reject(error: Error): void;
}

class SandboxCompiler implements SceneCompiler {
    readonly #code: string;
    readonly #timeout: number;
    readonly #document: Document;
    #frame: HTMLIFrameElement | null = null;
    #ready: Promise<Window> | null = null;
    #started: {resolve(target: Window): void; reject(error: Error): void} | null = null;
    readonly #pending = new Map<number, Pending>();
    #next = 1;
    #disposed = false;
    readonly #receive = (event: MessageEvent) => this.#message(event);

    constructor(options: SandboxOptions) {
        this.#code = options.code;
        this.#timeout = options.timeout ?? 10000;
        this.#document = options.document ?? document;
    }

    compile(source: string, name: string): Promise<string> {
        if (this.#disposed) return Promise.reject(new SceneError({phase: "load", message: CLOSED}));
        return this.#start().then((target) => new Promise<string>((resolve, reject) => {
            const id = this.#next++;
            this.#pending.set(id, {resolve, reject});
            target.postMessage({type: "compile", id, source, name, timeout: this.#timeout}, "*");
        }));
    }

    dispose(): void {
        if (this.#disposed) return;
        this.#disposed = true;
        this.#stop(new SceneError({phase: "load", message: CLOSED}));
    }

    // One frame serves every scene. It is made again when it left the document or did not start.
    #start(): Promise<Window> {
        if (this.#ready && this.#frame?.isConnected) return this.#ready;
        this.#stop(new SceneError({phase: "load", message: CLOSED}));
        const view = this.#document.defaultView;
        if (!view) return Promise.reject(new SceneError({phase: "load", message: `${NOT_RUN}: the page has no window`}));
        const frame = this.#document.createElement("iframe");
        frame.setAttribute("sandbox", "allow-scripts");
        frame.setAttribute("aria-hidden", "true");
        frame.tabIndex = -1;
        frame.style.display = "none";
        frame.srcdoc = `<!doctype html><meta http-equiv="Content-Security-Policy" content="${POLICY}"><script>${RELAY}</script>`;
        const ready = new Promise<Window>((resolve, reject) => { this.#started = {resolve, reject}; });
        const timer = setTimeout(() => {
            if (this.#ready === ready) this.#stop(new SceneError({phase: "load", message: `${NOT_RUN}: it did not start`}));
        }, START_TIMEOUT);
        ready.then(() => clearTimeout(timer), () => clearTimeout(timer));
        this.#frame = frame;
        this.#ready = ready;
        view.addEventListener("message", this.#receive);
        (this.#document.body ?? this.#document.documentElement).append(frame);
        return ready;
    }

    #stop(error: Error): void {
        this.#document.defaultView?.removeEventListener("message", this.#receive);
        this.#frame?.remove();
        this.#frame = null;
        this.#ready = null;
        this.#started?.reject(error);
        this.#started = null;
        for (const {reject} of this.#pending.values()) reject(error);
        this.#pending.clear();
    }

    #message(event: MessageEvent): void {
        const target = this.#frame?.contentWindow;
        if (!target || event.source !== target || typeof event.data !== "object" || !event.data) return;
        const message = event.data as Record<string, unknown>;
        if (message.type === "ready") {
            target.postMessage({type: "code", code: this.#code}, "*");
            this.#started?.resolve(target);
            this.#started = null;
            return;
        }
        if (message.type !== "compiled" || typeof message.id !== "number") return;
        const pending = this.#pending.get(message.id);
        if (!pending) return;
        this.#pending.delete(message.id);
        if (typeof message.lua === "string" && message.lua.length <= MAX_LUA) {
            pending.resolve(message.lua);
        } else if (typeof message.lua === "string") {
            pending.reject(new SceneError({phase: "load", message: "This scene's JavaScript built more Lua than a scene may have"}));
        } else if (message.timeout === true) {
            const seconds = Number((this.#timeout / 1000).toFixed(1));
            pending.reject(new SceneError({phase: "timeout",
                message: `This scene's JavaScript ran for more than ${seconds} second${seconds === 1 ? "" : "s"}, so it was stopped`}));
        } else if (typeof message.unavailable === "string") {
            pending.reject(new SceneError({phase: "load", message: `${NOT_RUN}: ${message.unavailable}`}));
        } else {
            pending.reject(new SceneError({phase: "load", message: typeof message.error === "string" ? message.error : "The scene's code failed"}));
        }
    }
}

/**
 * A compiler for createRenderHost that runs each JavaScript scene in a sandbox: a frame of its own
 * with no origin and no network, and a new worker there for each scene. Where a system cannot keep
 * the frame's workers off the network, it runs no scene.
 */
export function createSandboxCompiler(options: SandboxOptions): SceneCompiler {
    return new SandboxCompiler(options);
}
