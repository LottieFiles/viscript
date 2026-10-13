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

const center = (bounds) => ({
    x: bounds.x + bounds.width / 2,
    y: bounds.y + bounds.height / 2,
});

const runtime = await createVis(`
if not vis.motion then error("motion module missing") end
if not vis.runtime then error("runtime module missing") end
local scene = vis.scene {
    width = 200, height = 100,
    camera = {view = "2d", target = {0, 0}, height = 10},
}
local subject = scene:rectangle {
    center = {0, 0}, size = {2, 1}, fill = "accent", id = "motion-subject",
}
local motion = vis.motion(scene)
motion:define(subject, "idle", {})
motion:define(subject, "right", {shift = {2, 0}})
motion:define(subject, "up", {shift = {1, 2}})
motion:transition(subject, "right", 1, "linear")
vis.runtime(scene, {
    fixed_step = 0.25,
    max_steps = 4,
    update = function(ctx, dt, time, tick)
        motion:advance(dt)
        if tick == 2 then
            local visible = motion:sample(subject)
            assert(math.abs(visible.shift[1] - 1) < 0.0001)
            motion:transition(subject, "up", 1, "linear")
            assert(motion:event_count() == 2)
            assert(motion:event(1).transaction == "1")
            assert(motion:event(2).transaction == "2")
        end
    end,
})
return scene
`, "runtime-motion.lua", {wasmBinary});

try {
    const initial = center(runtime.bounds("motion-subject", 0));
    assert.equal(runtime.advanceRuntime(0.5).steps, 2);
    const interrupted = center(runtime.bounds("motion-subject", 0));
    assert(interrupted.x > initial.x);
    assert(Math.abs(interrupted.y - initial.y) < 0.1);

    assert.equal(runtime.advanceRuntime(0.5).steps, 2);
    const midway = center(runtime.bounds("motion-subject", 0));
    assert(Math.abs(midway.x - interrupted.x) < 0.1);
    assert(midway.y < interrupted.y);

    assert.equal(runtime.advanceRuntime(0.5).steps, 2);
    const settled = center(runtime.bounds("motion-subject", 0));
    assert(Math.abs(settled.x - interrupted.x) < 0.1);
    assert(settled.y < midway.y);
    assert.equal(runtime.render(0, true).length, 200 * 100 * 4);

    runtime.loadLua(`
if not vis.motion then error("motion module missing") end
local scene = vis.scene {
    width = 100, height = 50,
    camera = {view = "2d", target = {0, 0}, height = 5},
}
local subject = scene:circle {center = {0, 0}, radius = 0.5, id = "motion-static"}
local motion = vis.motion(scene)
motion:define(subject, "visible", {shift = {1, 0}})
motion:transition(subject, "visible", 0)
assert(motion:sample(subject).shift[1] == 1)
return scene
`, "ordinary-motion.lua");
    assert.equal(runtime.retainedLua, false);
    assert(center(runtime.bounds("motion-static", 0)).x > 50);
} finally {
    runtime.destroy();
}

console.log("WASM Lua semantic Motion and retained two-clock integration passed");
