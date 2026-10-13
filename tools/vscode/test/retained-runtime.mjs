import assert from "node:assert/strict";

import {advanceRetainedRuntime, sceneCanPlay} from "../media/retained-runtime.mjs";

assert.equal(sceneCanPlay(undefined), false);
assert.equal(sceneCanPlay({duration: 0, retainedLua: false}), false);
assert.equal(sceneCanPlay({duration: 1, retainedLua: false}), true);
assert.equal(sceneCanPlay({duration: 0, retainedLua: true}), true);

const frames = [];
const inputFrames = [];
const runtime = {
    duration: 0,
    retainedLua: true,
    advanceRuntime(elapsed) {
        frames.push(elapsed);
        return Object.freeze({time: 0.1, dropped: 0, tick: 1, steps: 1, interpolation: 0});
    },
    beginInputFrame(time) {
        inputFrames.push(time);
    },
};
assert.deepEqual(advanceRetainedRuntime(runtime, 0.1), {
    time: 0.1,
    dropped: 0,
    tick: 1,
    steps: 1,
    interpolation: 0,
});
assert.deepEqual(frames, [0.1]);
assert.deepEqual(inputFrames, [0.1]);

runtime.advanceRuntime = () => ({time: 0.11, steps: 0});
advanceRetainedRuntime(runtime, 0.01);
assert.deepEqual(inputFrames, [0.1], "unconsumed edges must survive accumulator-only frames");
assert.equal(advanceRetainedRuntime({retainedLua: false}, 0.1), null);

console.log("retained runtime host scheduling passed");
