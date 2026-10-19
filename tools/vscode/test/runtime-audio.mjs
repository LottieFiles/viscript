import assert from "node:assert/strict";
import {createRuntimeAudio, loadSceneAsset} from "../media/runtime-audio.mjs";

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
