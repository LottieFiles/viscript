import assert from "node:assert/strict";
import gifenc from "gifenc";

const {GIFEncoder, applyPalette, quantize} = gifenc;

const pixels = new Uint8Array([
    255, 0, 0, 255,
    0, 255, 0, 255,
    0, 0, 255, 255,
    255, 255, 255, 255,
]);
const palette = quantize(pixels, 256);
const encoder = GIFEncoder();
encoder.writeFrame(applyPalette(pixels, palette), 2, 2, {palette, delay: 40, repeat: 0});
encoder.finish();
const bytes = encoder.bytes();
assert.equal(new TextDecoder().decode(bytes.slice(0, 6)), "GIF89a");
assert.ok(bytes.length > 20);

console.log("GIF encoding passed");
