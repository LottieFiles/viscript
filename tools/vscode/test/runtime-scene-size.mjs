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
runtime.destroy();

console.log("default scene size passed");
