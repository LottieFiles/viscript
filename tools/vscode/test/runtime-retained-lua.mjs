import assert from "node:assert/strict";
import fs from "node:fs";
import path from "node:path";
import {fileURLToPath, pathToFileURL} from "node:url";

const extensionRoot = path.dirname(path.dirname(fileURLToPath(import.meta.url)));
const codeFont = fs.readFileSync(path.resolve(extensionRoot, "../../src/examples/assets/JetBrainsMono.ttf"));
const runtimeRoot = process.env.VIS_RUNTIME_ROOT
    ? path.resolve(process.env.VIS_RUNTIME_ROOT)
    : path.join(extensionRoot, "runtime");
const {createVis} = await import(pathToFileURL(path.join(runtimeRoot, "client.js")));
const wasmBinary = fs.readFileSync(path.join(runtimeRoot, "vis-wasm.wasm"));
const sourceSerif = fs.readFileSync(path.join(
    extensionRoot, "runtime", "SourceSerif4-Semibold.ttf",
));
const pretendard = fs.readFileSync(path.join(extensionRoot, "runtime", "Pretendard.ttf"));
const koreanSans = fs.readFileSync(path.join(
    extensionRoot, "runtime", "IBMPlexSansKR-SemiBold.ttf",
));

const source = `
local scene = vis.scene {
    width = 320, height = 180,
    camera = {view = "2d", target = {0, 0}, height = 18},
}
local subject = scene:rectangle {
    center = {-2, 0}, size = {2, 2}, fill = "accent", id = "runtime-subject",
}
local marker = scene:circle {
    center = {2, 0}, radius = 0.5, fill = "result", id = "runtime-marker",
}
local x, presses, pointer_x = 0, 0, 0
local runtime = vis.runtime(scene, {
    fixed_step = 0.1,
    max_steps = 4,
    update = function(ctx, dt, elapsed, tick)
        local move = ctx:action("Move")
        local key = ctx:key("D")
        local place = ctx:action("Place")
        if move.pressed and key.pressed then presses = presses + 1 end
        if place.pressed then
            local pointer = ctx:pointer(1)
            if pointer then pointer_x = pointer.position.x / 100 end
        end
        if tick == 1 then
            assert(ctx:sound("test-shot.wav", {gain = 0.5, rate = 1.25}))
        end
        x = x + move.value.x * dt
        ctx:update(subject, {shift = {x, presses, 0}})
        ctx:update(marker, {shift = {pointer_x, 0, 0}})
    end,
})
runtime:bind_key("Move", "D", {1, 0})
runtime:bind_pointer("Place", 0, {1, 0}, 1)
return scene
`;

const center = (box) => ({
    x: box.x + box.width / 2,
    y: box.y + box.height / 2,
});

const runtime = await createVis(source, "runtime-retained-lua.lua", {wasmBinary});
const event = (type, fields = {}) => runtime.input({type, pointerId: 1, time: runtime.runtimeTime, ...fields});

try {
    runtime.font("Source Serif 4", sourceSerif, "ttf");
    runtime.font("Pretendard", pretendard, "ttf");
    runtime.font("JetBrainsMono", codeFont, "ttf");
    runtime.font("IBM Plex Sans KR", koreanSans, "ttf");
    assert.equal(typeof runtime.advanceRuntime, "function");
    assert.equal(runtime.retainedLua, true);
    assert.equal(runtime.runtimeTime, 0);

    const initial = center(runtime.bounds("runtime-subject", 0));
    runtime.beginInputFrame(0);
    assert.equal(event("keydown", {key: "D"}).handled, true);
    const first = runtime.advanceRuntime(0.2);
    assert.equal(first.steps, 2);
    assert.equal(first.tick, 2);
    assert(Math.abs(first.time - 0.2) < 1e-5);
    assert.deepEqual(first.sounds, [{
        asset: "test-shot.wav", bus: "effect", gain: 0.5, rate: 1.25, loop: false,
    }]);
    const moved = center(runtime.bounds("runtime-subject", 0));
    assert(moved.x > initial.x);
    assert(moved.y < initial.y, "Pressed must be visible to the first catch-up step only");

    runtime.beginInputFrame(first.time);
    const held = runtime.advanceRuntime(0.1);
    assert.equal(held.steps, 1);
    assert.deepEqual(held.sounds, []);
    const heldCenter = center(runtime.bounds("runtime-subject", 0));
    assert(heldCenter.x > moved.x);
    assert(Math.abs(heldCenter.y - moved.y) < 0.1);

    runtime.beginInputFrame(held.time);
    assert.equal(event("keyup", {key: "D"}).handled, true);
    runtime.advanceRuntime(0.1);
    const stopped = center(runtime.bounds("runtime-subject", 0));
    assert(Math.abs(stopped.x - heldCenter.x) < 0.1);

    runtime.beginInputFrame(runtime.runtimeTime);
    const pointerDown = event("pointerdown", {
        x: 150, y: 70, button: 0, buttons: 1,
    });
    assert.equal(pointerDown.handled, true);
    assert.equal(pointerDown.capture, true);
    const markerInitial = center(runtime.bounds("runtime-marker", 0));
    const pointerStep = runtime.advanceRuntime(0.1);
    const markerMoved = center(runtime.bounds("runtime-marker", 0));
    assert.equal(pointerStep.steps, 1);
    assert(markerMoved.x > markerInitial.x);
    runtime.beginInputFrame(pointerStep.time);
    const pointerUp = event("pointerup", {x: 150, y: 70, button: 0, buttons: 0});
    assert.equal(pointerUp.handled, true);
    assert.equal(pointerUp.release, true);

    runtime.load("return vis.scene {width = 32, height = 18}", "ordinary.lua");
    assert.equal(runtime.retainedLua, false);
    assert.equal(runtime.runtimeTime, 0);
    assert.equal(runtime.advanceRuntime(0.1), null);

    runtime.load(`
local scene = vis.scene {
    width = 320, height = 180,
    camera = {view = "2d", target = {0, 0}, height = 18},
}
local subject = scene:rectangle {
    center = {0, 0}, size = {2, 2}, id = "atomic-subject",
}
vis.runtime(scene, {
    fixed_step = 0.1,
    update = function(ctx, dt, time, tick)
        ctx:update(subject, {shift = {tick == 2 and 40 or tick, 0, 0}})
        if tick == 2 then scene:circle {center = {0, 0}, radius = 1} end
    end,
})
return scene
`, "runtime-authoring-lock.lua");
    runtime.advanceRuntime(0.1);
    const committed = runtime.bounds("atomic-subject", 0);
    assert.throws(
        () => runtime.advanceRuntime(0.1),
        /scene authoring is unavailable after loading/,
    );
    assert.deepEqual(runtime.bounds("atomic-subject", 0), committed);
    assert.throws(() => runtime.advanceRuntime(0.1), /scene authoring is unavailable after loading/);

    runtime.load(`
local scene = vis.scene {width = 64, height = 36}
local diagram = vis.diagram(scene, {id = "runtime-lock"})
vis.runtime(scene, {
    fixed_step = 0.1,
    update = function() diagram:node {id = "late-node"} end,
})
return scene
`, "runtime-diagram-lock.lua");
    assert.throws(
        () => runtime.advanceRuntime(0.1),
        /diagram authoring is unavailable after loading/,
    );
} finally {
    runtime.destroy();
}

console.log("retained Lua fixed-step, input, rollback, and authoring lock passed");
