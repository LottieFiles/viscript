import assert from "node:assert/strict";
import fs from "node:fs";
import path from "node:path";
import {fileURLToPath} from "node:url";
import {createVis, vis} from "../runtime/client.js";

const extensionRoot = path.dirname(path.dirname(fileURLToPath(import.meta.url)));
const wasmBinary = fs.readFileSync(path.join(extensionRoot, "runtime", "vis-wasm.wasm"));

// A scene that names no size is 960 by 540, in Lua and in JavaScript, and a side it names keeps the
// other side's default.
const runtime = await createVis({wasmBinary});
runtime.load("return vis.scene {}", "bare.lua");
assert.deepEqual(runtime.size(), [960, 540]);
runtime.load(vis.scene(), "bare.js");
assert.deepEqual(runtime.size(), [960, 540]);
runtime.load("return vis.scene {width = 640}", "width.lua");
assert.deepEqual(runtime.size(), [640, 540]);
runtime.load(vis.scene({height: 360}), "height.js");
assert.deepEqual(runtime.size(), [960, 360]);

// A camera that names no height shows 6 world units, in Lua and in JavaScript: a circle is as tall
// as under a camera that names 6, and its 2 units span 45 more of the 540 pixels than under 8.
const dotHeight = (source, name) => {
    runtime.load(source, name);
    return runtime.bounds("dot").height;
};
const lua = (config) => `local s = vis.scene {${config}}\ns:circle {id = "dot", fill = "accent"}\nreturn s`;
const six = dotHeight(lua("camera = {height = 6}"), "six.lua");
assert.equal(dotHeight(lua(""), "camera.lua"), six);
const scene = vis.scene();
scene.circle({id: "dot", fill: "accent"});
assert.equal(dotHeight(scene, "camera.js"), six);
const eight = dotHeight(lua("camera = {height = 8}"), "eight.lua");
assert.ok(Math.abs(six - eight - (2 * 540 / 6 - 2 * 540 / 8)) < 0.01, `${six} and ${eight} differ by other than 45`);
runtime.destroy();

console.log("default scene size and camera height passed");
