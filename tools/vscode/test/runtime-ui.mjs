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
const source = `
local scene = vis.scene {
    width = 200,
    height = 100,
    camera = { mode = "interactive", view = "2d", target = { 0, 0 }, height = 4 },
}
local gradient = scene:rectangle {
    center = { 0, 0 },
    size = { 4, 2 },
    fill = "#ff0000",
    gradient = "#0000ff",
    id = "sample-gradient",
}
local target = scene:rectangle {
    center = { 1, 0 },
    size = { 1, 1 },
    id = "target",
}
local idle = scene:rectangle { center = { -1, 0 }, size = { 0.8, 0.8 }, fill = "#e5484d" }
local hover = scene:rectangle { center = { -1, 0 }, size = { 0.8, 0.8 }, fill = "#30a46c" }
local pressed = scene:rectangle { center = { -1, 0 }, size = { 0.8, 0.8 }, fill = "#3e63dd" }
local transition_visual = scene:rectangle { center = { 0, -1.5 }, size = { 0.8, 0.4 } }
local toggle_visual = scene:rectangle { center = { 2, 1.5 }, size = { 1.6, 0.4 } }
local toggle_target = scene:circle {
    center = { -1.5, -1.5 },
    radius = 0.15,
    id = "toggle-target",
}
local panel = vis.ui.panel(scene)
panel:sample_area {
    targets = { gradient },
    region = { 0, 0, 200, 100 },
}
panel:button {
    visual = idle,
    hover_visual = hover,
    pressed_visual = pressed,
    region = { 120, 30, 50, 40 },
    camera = "reset",
}
panel:button {
    visual = transition_visual,
    region = { 120, 70, 50, 20 },
    target = target,
    transform = { shift = { 1, 0, 0 } },
    duration = 1,
}
panel:toggle_button {
    visual = toggle_visual,
    region = { 120, 0, 50, 20 },
    target = toggle_target,
    off = { shift = { -0.5, 0, 0 } },
    on = { shift = { 0.5, 0, 0 } },
    value = false,
    duration = 0.4,
}
panel:slider {
    visual = target,
    target = target,
    region = { 0, 0, 100, 20 },
    value = 0,
    from = { shift = { 0, 0, 0 } },
    to = { shift = { 1, 0, 0 } },
}
scene:wait(2)
return scene
`;

const runtime = await createVis(source, "runtime-ui.lua", {wasmBinary});
try {
    assert.equal(typeof runtime.input, "function");
    const event = (type, x, y, fields = {}) => runtime.input({type, x, y, pointerId: 6, time: 0, ...fields});
    const sampleDown = event("pointerdown", 60, 50, {button: 0, buttons: 1, pointerId: 5});
    assert.equal(sampleDown.sample, undefined, "sampling resolves only after a completed click");
    const sampleUp = event("pointerup", 60, 50, {button: 0, pointerId: 5});
    assert.equal(sampleUp.handled, true);
    assert.equal(sampleUp.redraw, true);
    assert.equal(sampleUp.release, true);
    assert.deepEqual(sampleUp.sample.position, {x: 60, y: 50});
    assert(Math.abs(sampleUp.sample.value.x - 0.3) < 1e-6);
    assert.equal(sampleUp.sample.value.y, 0.5);
    assert(sampleUp.sample.rgba[0] > sampleUp.sample.rgba[2]);
    assert.equal(sampleUp.sample.rgba[1], 0);
    assert.equal(sampleUp.sample.rgba[3], 0xff);
    assert(sampleUp.sample.object.handle > 0);
    assert.equal(sampleUp.sample.object.id, "sample-gradient");
    assert.equal(sampleUp.sample.object.type, "rectangle");
    assert(Object.isFrozen(sampleUp));
    assert(Object.isFrozen(sampleUp.sample));
    assert(Object.isFrozen(sampleUp.sample.position));
    assert(Object.isFrozen(sampleUp.sample.value));
    assert(Object.isFrozen(sampleUp.sample.rgba));
    assert(Object.isFrozen(sampleUp.sample.object));

    const facePixel = () => {
        const pixels = runtime.render(0);
        const offset = (50 * 200 + 75) * 4;
        return [...pixels.slice(offset, offset + 4)];
    };
    assert.deepEqual(facePixel(), [0xe5, 0x48, 0x4d, 0xff]);
    assert.deepEqual(event("pointermove", 130, 40), {
        handled: false,
        redraw: true,
        capture: false,
        release: false,
    });
    assert.deepEqual(facePixel(), [0x30, 0xa4, 0x6c, 0xff]);
    assert.equal(event("pointerdown", 130, 40, {button: 0, buttons: 1}).capture, true);
    assert.deepEqual(facePixel(), [0x3e, 0x63, 0xdd, 0xff]);
    event("pointermove", 190, 90, {buttons: 1});
    assert.deepEqual(facePixel(), [0xe5, 0x48, 0x4d, 0xff]);
    event("pointermove", 130, 40, {buttons: 1});
    assert.deepEqual(facePixel(), [0x3e, 0x63, 0xdd, 0xff]);
    assert.equal(event("pointerup", 130, 40, {button: 0}).release, true);
    assert.deepEqual(facePixel(), [0x30, 0xa4, 0x6c, 0xff]);
    event("pointercancel", 130, 40);
    assert.deepEqual(facePixel(), [0xe5, 0x48, 0x4d, 0xff]);

    const center = (box) => box.x + box.width / 2;
    const toggleInitial = runtime.bounds("toggle-target", 0.2);
    event("pointerdown", 145, 10, {button: 0, buttons: 1, pointerId: 9, time: 0.2});
    event("pointerup", 145, 10, {button: 0, pointerId: 9, time: 0.2});
    const toggleStart = runtime.bounds("toggle-target", 0.2);
    const toggleMiddle = runtime.bounds("toggle-target", 0.4);
    const toggleOn = runtime.bounds("toggle-target", 0.6);
    assert.ok(Math.abs(center(toggleStart) - center(toggleInitial)) < 0.1);
    assert.ok(center(toggleMiddle) > center(toggleStart));
    assert.ok(center(toggleMiddle) < center(toggleOn));
    assert.ok(Math.abs(center(toggleOn) - center(toggleStart) - 25) < 0.1);
    event("pointerdown", 145, 10, {button: 0, buttons: 1, pointerId: 10, time: 1});
    event("pointerup", 145, 10, {button: 0, pointerId: 10, time: 1});
    const toggleOff = runtime.bounds("toggle-target", 1.4);
    assert.ok(Math.abs(center(toggleOff) - center(toggleInitial)) < 0.1);

    const before = runtime.bounds("target", 0);
    const down = runtime.input({type: "pointerdown", x: 100, y: 10, button: 0, buttons: 1, pointerId: 7, time: 0});
    assert.deepEqual(down, {handled: true, redraw: true, capture: true, release: false});
    const up = runtime.input({type: "pointerup", x: 100, y: 10, button: 0, pointerId: 7, time: 0});
    assert.equal(up.handled, true);
    assert.equal(up.release, true);
    const after = runtime.bounds("target", 0);
    assert.ok(Math.abs((after.x + after.width / 2) - (before.x + before.width / 2) - 25) < 0.1);

    event("pointerdown", 145, 80, {button: 0, buttons: 1, pointerId: 8, time: 0});
    event("pointerup", 145, 80, {button: 0, pointerId: 8, time: 0});
    const transitionStart = runtime.bounds("target", 0);
    const transitionMid = runtime.bounds("target", 0.5);
    const transitionEnd = runtime.bounds("target", 1);
    assert.ok(Math.abs(center(transitionStart) - center(after)) < 0.1);
    assert.ok(center(transitionMid) > center(transitionStart));
    assert.ok(center(transitionMid) < center(transitionEnd));
    assert.ok(Math.abs(center(transitionEnd) - center(transitionStart) - 25) < 0.1);

    runtime.load(`
local scene = vis.scene {width = 200, height = 100}
scene:rectangle {id = "target", size = {1, 1}}
local panel = vis.ui.panel(scene)
panel:camera("pan", {0.2, 0})
vis.runtime(scene, {fixed_step = 0.1, update = function()
    panel:camera("reset")
end})
return scene
`, "runtime-ui-camera.lua");
    assert.throws(() => runtime.advanceRuntime(0.1), /UI authoring is unavailable after loading/);
} finally {
    runtime.destroy();
}

console.log("runtime UI bridge passed");
