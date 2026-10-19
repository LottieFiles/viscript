import assert from "node:assert/strict";
import fs from "node:fs";
import {createRuntimeAudio, loadSceneAsset} from "../media/runtime-audio.mjs";
import {createVis} from "../runtime/client.js";

const calls = [];
const audio = {
    load: (name, bytes) => calls.push(["load", name, bytes.length]),
    start: () => calls.push(["start"]),
    transport: (state) => calls.push(["transport", state]),
    play: (asset, options) => calls.push(["play", asset, options]),
};
const runtime = {audio, asset: (name, bytes, mime) => calls.push(["asset", name, mime])};

// WAV assets reach the audio, images the scene; a runtime without audio skips sounds.
loadSceneAsset(runtime, {name: "hit.wav", mime: "audio/wav"}, new Uint8Array(3));
loadSceneAsset(runtime, {name: "logo.png", mime: "image/png"}, new Uint8Array(2));
loadSceneAsset({asset: runtime.asset}, {name: "hit.wav", mime: "audio/wav"}, new Uint8Array(3));
assert.deepEqual(calls.splice(0), [["load", "hit.wav", 3], ["asset", "logo.png", "image/png"]]);

// The runtime takes PNG and SVG images, by extension or MIME type, and names any other type it refuses.
const engine = await createVis({wasmBinary: fs.readFileSync(new URL("../runtime/vis-wasm.wasm", import.meta.url))});
const png = fs.readFileSync(new URL("../media/icon.png", import.meta.url));
const svg = fs.readFileSync(new URL("../../../src/examples/assets/pixels.svg", import.meta.url));
for (const [name, mime, bytes] of [["a.png", "png", png], ["b.png", "image/png", png], ["c.svg", "svg", svg], ["d.svg", "image/SVG+xml", svg]]) {
    loadSceneAsset(engine, {name, mime}, bytes);
}
engine.load(`local s = vis.scene {width = 64, height = 32}
for i, name in ipairs {"a.png", "b.png", "c.svg", "d.svg"} do s:picture {asset = name, center = {i - 2.5, 0}, width = 1} end
return s`, "images.lua");
for (const [name, mime] of [["photo.jpg", "jpg"], ["photo.webp", "image/webp"], ["anim.gif", "gif"]]) {
    assert.throws(() => loadSceneAsset(engine, {name, mime}, png),
        {name: "TypeError", message: `Unsupported image type '${mime}' for asset '${name}'; the runtime decodes PNG and SVG`});
}
engine.destroy();

let current = runtime;
let playing = false;
const sounds = {sounds: [{asset: "hit.wav", bus: "sfx", gain: 0.5, rate: 1, loop: false}]};
const runtimeAudio = createRuntimeAudio({
    runtime: () => current,
    time: () => 1.5,
    rate: () => 2,
    playing: () => playing,
});

// Nothing plays or follows the transport until a user gesture starts the audio.
runtimeAudio.sync(true);
runtimeAudio.play(sounds);
assert.equal(runtimeAudio.started, false);
assert.deepEqual(calls.splice(0), []);

runtimeAudio.start();
runtimeAudio.start();
assert.equal(runtimeAudio.started, true);
assert.deepEqual(calls.splice(0), [
    ["start"],
    ["transport", {time: 1.5, rate: 2, playing: false, seek: true}],
]);

playing = true;
runtimeAudio.sync(false);
runtimeAudio.play(sounds);
assert.deepEqual(calls.splice(0), [
    ["transport", {time: 1.5, rate: 2, playing: true, seek: false}],
    ["play", "hit.wav", {bus: "sfx", gain: 0.5, rate: 1, loop: false}],
]);

// A dropped sound does not stop the others.
audio.play = (asset) => {
    if (asset === "busy.wav") throw new Error("voice pool full");
    calls.push(["play", asset]);
};
runtimeAudio.play({sounds: [{asset: "busy.wav"}, {asset: "hit.wav"}]});
assert.deepEqual(calls.splice(0), [["play", "hit.wav"]]);

// A failing transport stops the audio until a gesture starts it again; a new runtime's has not started.
audio.transport = () => {
    throw new Error("suspended");
};
runtimeAudio.sync(false);
assert.equal(runtimeAudio.started, false);
audio.transport = (state) => calls.push(["transport", state]);
runtimeAudio.start();
assert.equal(runtimeAudio.started, true);
runtimeAudio.reset();
assert.equal(runtimeAudio.started, false);
calls.length = 0;

// Starting does nothing when the audio stays locked or the runtime has none.
audio.start = () => {
    throw new Error("locked");
};
runtimeAudio.start();
assert.equal(runtimeAudio.started, false);
current = {};
runtimeAudio.start();
assert.equal(runtimeAudio.started, false);
assert.deepEqual(calls, []);

console.log("runtime audio passed");
