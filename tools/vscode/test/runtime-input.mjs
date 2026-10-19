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
local follower = scene:rectangle {
    center = { 0, 0 },
    size = { 1.2, 0.3 },
    id = "pointer-follower",
}
local mover = scene:circle {
    center = { 0, -1 },
    radius = 0.2,
    id = "key-mover",
}
local input = vis.input.controller(scene)
input:pointer_follow {
    target = follower,
    region = { 0, 0, 200, 100 },
    map_origin = { -4, 2, 0 },
    map_x = { 8, 0, 0 },
    map_y = { 0, -4, 0 },
    period = 0.2,
    rotate = true,
    reset_on_leave = true,
}
input:key_move {
    target = mover,
    key = "ArrowRight",
    shift = { 1, 0, 0 },
    period = 0.12,
}
input:key_move {
    target = mover,
    key = "A",
    shift = { -1, 0, 0 },
    period = 0.12,
}
if vis.ui then
    local ui_face = scene:rectangle {
        center = { -3.6, -1.7 }, size = { 0.2, 0.2 }, id = "ui-overlap-face",
    }
    local ui_target = scene:circle {
        center = { 3.6, -1.7 }, radius = 0.1, id = "ui-overlap-target",
    }
    local panel = vis.ui.panel(scene)
    panel:button {
        visual = ui_face,
        region = { 0, 0, 20, 20 },
        target = ui_target,
        transform = {},
    }
end
scene:wait(2)
return scene
`;

const runtime = await createVis(source, "runtime-input.lua", {wasmBinary});
const center = (box) => ({
    x: box.x + box.width / 2,
    y: box.y + box.height / 2,
});
const event = (type, fields = {}) => runtime.input({
    type,
    x: 0,
    y: 0,
    dx: 0,
    dy: 0,
    wheelX: 0,
    wheelY: 0,
    button: -1,
    buttons: 0,
    modifiers: 0,
    pointerId: 1,
    time: 0,
    ...fields,
});

try {
    assert.equal(typeof runtime.input, "function");
    assert.equal(typeof runtime.beginInputFrame, "function");
    assert.equal(typeof runtime.releaseInput, "function");

    runtime.beginInputFrame(0);
    const pointerInitial = center(runtime.bounds("pointer-follower", 0));
    const pointer = event("pointermove", {x: 150, y: 25, dx: 5, dy: -3});
    assert.deepEqual(pointer, {
        handled: true,
        redraw: true,
        capture: false,
        release: false,
    });
    const pointerMiddle = center(runtime.bounds("pointer-follower", 0.1));
    const pointerEnd = center(runtime.bounds("pointer-follower", 0.2));
    assert(pointerMiddle.x > pointerInitial.x && pointerMiddle.x < pointerEnd.x);
    assert(pointerMiddle.y < pointerInitial.y && pointerMiddle.y > pointerEnd.y);
    assert(Math.abs(pointerEnd.x - 150) < 0.1);
    assert(Math.abs(pointerEnd.y - 25) < 0.1);
    assert.equal(event("wheel", {
        x: 150,
        y: 25,
        wheelX: 2,
        wheelY: -4,
        time: 0.21,
    }).handled, false);
    runtime.beginInputFrame(0.25);

    assert.equal(event("pointercancel", {x: 150, y: 25, time: 0.3}).handled, true);
    const pointerReset = center(runtime.bounds("pointer-follower", 0.5));
    assert(Math.abs(pointerReset.x - pointerInitial.x) < 0.1);
    assert(Math.abs(pointerReset.y - pointerInitial.y) < 0.1);

    const keyInitial = center(runtime.bounds("key-mover", 0.6));
    const down = event("keydown", {
        key: "ArrowRight",
        button: 1,
        repeat: false,
        time: 0.6,
    });
    assert.equal(down.handled, true);
    assert.equal(down.redraw, true);
    const keyMiddle = center(runtime.bounds("key-mover", 0.66));
    const keyFirstPeriod = center(runtime.bounds("key-mover", 0.72));
    const keyHeld = center(runtime.bounds("key-mover", 0.84));
    assert(keyMiddle.x > keyInitial.x && keyMiddle.x < keyFirstPeriod.x);
    assert(Math.abs(keyFirstPeriod.x - keyInitial.x - 25) < 0.1);
    assert(Math.abs(keyHeld.x - keyInitial.x - 50) < 0.1);

    const repeat = event("keydown", {
        key: "ArrowRight",
        button: 1,
        repeat: true,
        time: 0.9,
    });
    assert.equal(repeat.handled, true);
    assert.equal(repeat.redraw, false);
    const keyAfterRepeat = center(runtime.bounds("key-mover", 0.96));
    assert(Math.abs(keyAfterRepeat.x - keyInitial.x - 75) < 0.1);

    const up = event("keyup", {key: "ArrowRight", button: 1, time: 0.96});
    assert.equal(up.handled, true);
    assert.equal(up.redraw, true);
    const keyStopped = center(runtime.bounds("key-mover", 1.08));
    assert(Math.abs(keyStopped.x - keyAfterRepeat.x) < 0.1);
    assert.equal(event("keydown", {key: "A", time: 1.08}).handled, true);
    const keyReturned = center(runtime.bounds("key-mover", 1.2));
    assert(Math.abs(keyReturned.x - keyInitial.x - 50) < 0.1);
    assert.equal(event("keyup", {key: "A", time: 1.2}).handled, true);

    const shortcut = event("keydown", {
        key: "ArrowRight",
        modifiers: 2,
        time: 1.3,
    });
    assert.equal(shortcut.handled, false);
    assert.equal(shortcut.redraw, false);
    const afterShortcut = center(runtime.bounds("key-mover", 1.42));
    assert(Math.abs(afterShortcut.x - keyReturned.x) < 0.1);

    // Keys and buttons the scene does not bind reach the retained Input state unhandled, and
    // releasing them all reports no error.
    runtime.beginInputFrame(1.5);
    assert.equal(event("keydown", {key: "W", time: 1.51}).handled, false);
    assert.equal(event("keydown", {key: "D", time: 1.52}).handled, false);
    assert.equal(event("pointerdown", {
        x: 150,
        y: 50,
        button: 0,
        buttons: 1,
        time: 1.53,
    }).handled, false);
    assert.equal(runtime.releaseInput(1.61).error, undefined);
    assert.throws(() => runtime.beginInputFrame(-1), /finite and non-negative/);
    assert.throws(() => runtime.releaseInput(Number.NaN), /finite and non-negative/);

    assert.throws(
        () => event("keydown", {key: "F12", time: 1}),
        /Unsupported input key/,
    );
} finally {
    runtime.destroy();
}

console.log("runtime Input bridge passed");
