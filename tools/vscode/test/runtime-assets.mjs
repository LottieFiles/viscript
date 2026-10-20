import assert from "node:assert/strict";
import fs from "node:fs";
import {createVis} from "../runtime/client.js";

// The runtime takes PNG and SVG images, by extension or MIME type, and names any other type it refuses.
const engine = await createVis({wasmBinary: fs.readFileSync(new URL("../runtime/vis-wasm.wasm", import.meta.url))});
const png = fs.readFileSync(new URL("../media/icon.png", import.meta.url));
const svg = fs.readFileSync(new URL("../../../src/examples/assets/pixels.svg", import.meta.url));
for (const [name, mime, bytes] of [["a.png", "png", png], ["b.png", "image/png", png], ["c.svg", "svg", svg], ["d.svg", "image/SVG+xml", svg]]) {
    engine.asset(name, bytes, mime);
}
engine.load(`local s = vis.scene {width = 64, height = 32}
for i, name in ipairs {"a.png", "b.png", "c.svg", "d.svg"} do s:picture {asset = name, center = {i - 2.5, 0}, width = 1} end
return s`, "images.lua");
for (const [name, mime] of [["photo.jpg", "jpg"], ["photo.webp", "image/webp"], ["anim.gif", "gif"]]) {
    assert.throws(() => engine.asset(name, png, mime),
        {name: "TypeError", message: `Unsupported image type '${mime}' for asset '${name}'; the runtime decodes PNG and SVG`});
}
engine.destroy();

console.log("runtime image assets passed");
