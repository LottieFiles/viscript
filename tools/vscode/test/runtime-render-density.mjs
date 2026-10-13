import assert from "node:assert/strict";
import fs from "node:fs";
import path from "node:path";
import {fileURLToPath} from "node:url";
import {createVis} from "../runtime/client.js";

const extensionRoot = path.dirname(path.dirname(fileURLToPath(import.meta.url)));
const wasmBinary = fs.readFileSync(path.join(extensionRoot, "runtime", "vis-wasm.wasm"));

globalThis.ImageData = class ImageData {
    constructor(data, width, height) {
        this.data = data;
        this.width = width;
        this.height = height;
    }
};

let image;
const canvas = {
    width: 0,
    height: 0,
    getContext() {
        return {
            putImageData(value) {
                image = value;
            },
        };
    },
};
const runtime = await createVis(`
local scene = vis.scene {width = 160, height = 90}
scene:circle {center = {80, 45}, radius = 20, fill = "#ffffff"}
return scene
`, "render-density.lua", {wasmBinary});

runtime.draw(canvas, 0, 2);
assert.equal(canvas.width, 320);
assert.equal(canvas.height, 180);
assert.equal(image.width, 320);
assert.equal(image.height, 180);
assert.equal(image.data.length, 320 * 180 * 4);
assert.deepEqual(runtime.size(), [160, 90]);
assert.throws(() => runtime.draw(canvas, 0, 0), /Pixel ratio/);
runtime.destroy();

console.log("adaptive canvas render density passed");
