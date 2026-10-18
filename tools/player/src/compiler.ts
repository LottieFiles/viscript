// The compiler bundle: runs one JavaScript scene in a worker the sandbox starts for it, and answers
// with the Lua the scene builds. The sandbox's policy refuses every request this code could make;
// asked to check, the bundle tells whether that policy reaches its worker on this system.
import {compileScene, evaluateScene} from "vis-wasm-bindings";

interface CompilerScope {
    onmessage: ((event: MessageEvent<{readonly check?: boolean; readonly source?: string; readonly name?: string}>) => void) | null;
    postMessage(message: unknown): void;
}

const scope = globalThis as unknown as CompilerScope;

scope.onmessage = ({data}) => {
    if (data.check) {
        // Only the policy refuses a data: URL, so an answer means the policy does not hold here.
        fetch("data:,").then(() => scope.postMessage({sealed: false}), () => scope.postMessage({sealed: true}));
        return;
    }
    try {
        scope.postMessage({lua: compileScene(evaluateScene(data.source ?? "", data.name))});
    } catch (error) {
        scope.postMessage({error: error instanceof Error ? error.message : String(error)});
    }
};
