import assert from "node:assert/strict";
import fs from "node:fs";
import path from "node:path";
import {fileURLToPath, pathToFileURL} from "node:url";

const extensionRoot = path.dirname(path.dirname(fileURLToPath(import.meta.url)));
const runtimeRoot = process.env.VIS_RUNTIME_ROOT
    ? path.resolve(process.env.VIS_RUNTIME_ROOT)
    : path.join(extensionRoot, "runtime");
const {createVis} = await import(pathToFileURL(path.join(runtimeRoot, "client.js")));
const wasmBinary = fs.readFileSync(path.join(runtimeRoot, "vis-wasm.wasm"));
const runtime = await createVis(`
local scene = vis.scene {
    width = 160, height = 90,
    camera = {view = "2d", target = {0, 0}, height = 9},
}
local subject = scene:circle {
    center = {0, 0}, radius = 1, fill = "accent", id = "autonomous-subject",
}
vis.runtime(scene, {
    fixed_step = 0.05,
    max_steps = 2,
    update = function(ctx, dt, time, tick)
        ctx:update(subject, {shift = {tick, 0, 0}, opacity = 0.8})
    end,
})
return scene
`, "runtime-retained-autonomous.lua", {wasmBinary});

try {
    assert.equal(runtime.retainedLua, true);
    assert.equal(typeof runtime.advanceRuntime, "function");
    const before = runtime.bounds("autonomous-subject", 0);
    const step = runtime.advanceRuntime(1);
    assert.equal(step.steps, 2);
    assert.equal(step.tick, 2);
    assert(step.dropped > 0.8);
    const after = runtime.bounds("autonomous-subject", 0);
    assert(after.x > before.x);
    assert.equal(runtime.render(0, true).length, 160 * 90 * 4);
} finally {
    runtime.destroy();
}

console.log("autonomous retained Lua runtime passed");
