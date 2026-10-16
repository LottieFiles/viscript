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
if not vis.states then error("states module missing") end
if not vis.runtime then error("runtime module missing") end
local scene = vis.scene {
    width = 200, height = 100,
    camera = {view = "2d", target = {0, 0}, height = 10},
}
local subject = scene:rectangle {
    center = {0, 0}, size = {2, 1}, fill = "accent", id = "states-subject",
}
local states = vis.states(scene)
states:define(subject, "idle", {})
states:define(subject, "right", {shift = {2, 0}})
states:define(subject, "up", {shift = {1, 2}})
states:transition(subject, "right", 1, "linear")
vis.runtime(scene, {
    fixed_step = 0.25,
    max_steps = 4,
    update = function(ctx, dt, time, tick)
        states:advance(dt)
        if tick == 2 then
            local visible = states:sample(subject)
            assert(math.abs(visible.shift[1] - 1) < 0.0001)
            states:transition(subject, "up", 1, "linear")
            assert(states:event_count() == 2)
            assert(states:event(1).transaction == "1")
            assert(states:event(2).transaction == "2")
        end
    end,
})
return scene
`, "runtime-states.lua", {wasmBinary});

try {
    const initial = center(runtime.bounds("states-subject", 0));
    assert.equal(runtime.advanceRuntime(0.5).steps, 2);
    const interrupted = center(runtime.bounds("states-subject", 0));
    assert(interrupted.x > initial.x);
    assert(Math.abs(interrupted.y - initial.y) < 0.1);

    assert.equal(runtime.advanceRuntime(0.5).steps, 2);
    const midway = center(runtime.bounds("states-subject", 0));
    assert(Math.abs(midway.x - interrupted.x) < 0.1);
    assert(midway.y < interrupted.y);

    assert.equal(runtime.advanceRuntime(0.5).steps, 2);
    const settled = center(runtime.bounds("states-subject", 0));
    assert(Math.abs(settled.x - interrupted.x) < 0.1);
    assert(settled.y < midway.y);
    assert.equal(runtime.render(0, true).length, 200 * 100 * 4);

    runtime.loadLua(`
if not vis.states then error("states module missing") end
local scene = vis.scene {
    width = 100, height = 50,
    camera = {view = "2d", target = {0, 0}, height = 5},
}
local subject = scene:circle {center = {0, 0}, radius = 0.5, id = "states-static"}
local states = vis.states(scene)
states:define(subject, "visible", {shift = {1, 0}})
states:transition(subject, "visible", 0)
assert(states:sample(subject).shift[1] == 1)
return scene
`, "ordinary-states.lua");
    assert.equal(runtime.retainedLua, false);
    assert(center(runtime.bounds("states-static", 0)).x > 50);

    runtime.loadLua(`
local scene = vis.scene {width = 100, height = 50}
local a = scene:circle {id = "a"}
local b = scene:circle {id = "b"}
local states = vis.states(scene)
states:define(a, "right", {shift = {1, 0}})
states:define(b, "up", {shift = {0, 1}})
states:transition({{object = a, state = "right"}, {object = b, state = "up"}}, {duration = 1, curve = "linear"})
assert(states:event(1).transaction == states:event(2).transaction)
states:advance(0.5)
assert(math.abs(states:sample(a).shift[1] - 0.5) < 0.0001)
assert(math.abs(states:sample(b).shift[2] - 0.5) < 0.0001)
local target = scene:rectangle {id = "morphed"}
scene:morph(a, target, {duration = 1})
return scene
`, "batch-states.lua");
    assert.equal(runtime.duration, 1);
    assert.ok(runtime.bounds("morphed", 1).width > 0);
    assert.equal(runtime.render(1, true).length, 100 * 50 * 4);
} finally {
    runtime.destroy();
}

console.log("WASM Lua semantic States and retained two-clock integration passed");
