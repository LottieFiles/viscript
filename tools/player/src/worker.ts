// The render server bundle. In a dedicated worker it answers the host's messages; loaded on the
// main thread, it leaves a factory for the host's inline fallback instead. One bundle serves both,
// so a page ships the runtime once.
import {createVis, createVisModule, evaluateScene} from "vis-wasm-bundle";
import {INLINE_SERVER, type ClientMessage, type ServerMessage} from "./protocol.ts";
import {type RuntimeApi, RenderServer} from "./server/server.ts";
import type {Canvas} from "./server/surface.ts";

const runtime: RuntimeApi = {createVis, createVisModule, evaluateScene};
const scratch = (): Canvas => typeof OffscreenCanvas === "function"
    ? new OffscreenCanvas(1, 1) : document.createElement("canvas");

interface WorkerScope {
    onmessage: ((event: MessageEvent<ClientMessage>) => void) | null;
    postMessage(message: ServerMessage, transfer: Transferable[]): void;
}

// A JavaScript scene runs in the worker's global scope. Some hosts give workers more than the web
// platform: Obsidian gives them Node, which would let any note's scene read and write every file.
// The server needs none of it, nor the means to start a worker that would have it again, nor the
// storage the worker shares with the host's page.
const HOST_GLOBALS = [
    "require", "process", "module", "exports", "Buffer", "global", "setImmediate", "clearImmediate",
    "__filename", "__dirname", "Worker", "SharedWorker", "indexedDB", "caches",
];

const scope = globalThis as typeof globalThis & {WorkerGlobalScope?: abstract new () => object};
if (typeof scope.WorkerGlobalScope === "function" && globalThis instanceof scope.WorkerGlobalScope) {
    // Gone from the prototypes too, where the web platform defines its attributes.
    for (let target: object | null = globalThis; target; target = Object.getPrototypeOf(target)) {
        for (const name of HOST_GLOBALS) Reflect.deleteProperty(target, name);
    }
    const worker = globalThis as unknown as WorkerScope;
    const server = new RenderServer({runtime, scratch, post: (message, transfer = []) => worker.postMessage(message, transfer)});
    worker.onmessage = (event) => server.handle(event.data);
} else {
    (globalThis as Record<string, unknown>)[INLINE_SERVER] = (post: (message: ServerMessage) => void) =>
        new RenderServer({runtime, scratch, post});
}
