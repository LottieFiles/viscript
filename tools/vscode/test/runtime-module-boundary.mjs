import assert from "node:assert/strict";
import fs from "node:fs";
import path from "node:path";
import {fileURLToPath, pathToFileURL} from "node:url";

const extensionRoot = path.dirname(path.dirname(fileURLToPath(import.meta.url)));
const runtimeRoot = process.env.VIS_RUNTIME_ROOT
    ? path.resolve(process.env.VIS_RUNTIME_ROOT)
    : path.join(extensionRoot, "runtime");
// The interactive build option enables UI, Input, States, and the retained Lua runtime together.
const expectedInteractive = process.env.VIS_EXPECT_INTERACTIVE !== "0";
const {createVis} = await import(pathToFileURL(path.join(runtimeRoot, "client.js")));
const wasmBinary = fs.readFileSync(path.join(runtimeRoot, "vis-wasm.wasm"));
const source = `
local expected = ${expectedInteractive}
if (vis.ui ~= nil) ~= expected then error("unexpected UI namespace") end
if (vis.input ~= nil) ~= expected then error("unexpected Input namespace") end
if (vis.runtime ~= nil) ~= expected then error("unexpected Lua Runtime namespace") end
if (vis.states ~= nil) ~= expected then error("unexpected States namespace") end
return vis.scene {
    width = 32,
    height = 18,
    camera = {mode = "interactive", view = "2d", target = {0, 0}, height = 4},
}
`;

const runtime = await createVis(source, "runtime-module-boundary.lua", {wasmBinary});
try {
    assert.equal(typeof runtime.input === "function", expectedInteractive);
    for (const name of ["beginInputFrame", "releaseInput", "keyState", "pointerState", "actionMap"]) {
        assert.equal(typeof runtime[name] === "function", expectedInteractive);
    }
    assert.equal(typeof runtime.advanceRuntime === "function", expectedInteractive);
    assert.equal(runtime.retainedLua, false);
    assert.equal(runtime.runtimeTime, 0);
    assert.equal(runtime.cameraInput("pan", 0.01, 0), expectedInteractive);
    assert.doesNotThrow(() => runtime.render(0));
} finally {
    runtime.destroy();
}

console.log(`runtime module boundary passed (interactive=${expectedInteractive})`);
