import assert from "node:assert/strict";
import fs from "node:fs";
import path from "node:path";
import {fileURLToPath} from "node:url";
import {createVis} from "../runtime/client.js";

const extensionRoot = path.dirname(path.dirname(fileURLToPath(import.meta.url)));
const wasmBinary = fs.readFileSync(path.join(extensionRoot, "runtime", "vis-wasm.wasm"));

const interactive = await createVis(`
local scene = vis.scene {
    width = 160,
    height = 90,
    camera = {mode = "interactive", view = "2d", target = {0, 0}, height = 6},
}
local space = scene:space {x = {-3, 3, 1}, y = {-2, 2, 1}}
space:circle {center = {0, 0}, radius = 1}
return scene
`, "interactive-camera.lua", {wasmBinary});

assert.equal(interactive.cameraMode, "interactive");
assert.equal(interactive.cameraView, "2d");
assert.equal(interactive.camera, undefined);
assert.throws(() => interactive.cameraInput("unknown"), TypeError);
assert.throws(() => interactive.cameraInput("pan", NaN, 0), TypeError);
assert.equal(interactive.cameraInput("pan", 0.1, -0.1), true);
assert.equal(interactive.cameraInput("zoom", 0, -0.1), true);
assert.equal(interactive.cameraInput("view3d"), true);
assert.equal(interactive.cameraView, "3d");
assert.equal(interactive.cameraInput("orbit", 0.1, 0.1), true);
assert.equal(interactive.cameraInput("view2d"), true);
assert.equal(interactive.cameraView, "2d");
assert.equal(interactive.cameraInput("reset"), true);
if (interactive.input) {
    const baseInput = {
        x: 0,
        y: 0,
        dx: 0,
        dy: 0,
        wheelX: 0,
        wheelY: 0,
        button: 0,
        buttons: 0,
        modifiers: 0,
        pointerId: 1,
        time: 0,
    };
    assert.deepEqual(interactive.input({...baseInput, type: "pointerdown", buttons: 1}), {
        handled: false,
        redraw: false,
        capture: false,
        release: false,
    });
    assert.deepEqual(interactive.input({...baseInput, type: "keydown", key: "ArrowLeft"}), {
        handled: false,
        redraw: false,
        capture: false,
        release: false,
    });
}
assert.doesNotThrow(() => interactive.render(0));
interactive.destroy();

const fixed = await createVis("return vis.scene {width = 16, height = 16}", "fixed-camera.lua", {wasmBinary});
assert.equal(fixed.cameraMode, "fixed");
assert.equal(fixed.cameraInput("pan", 0.1, 0), false);
fixed.destroy();

console.log("interactive camera controls passed");
