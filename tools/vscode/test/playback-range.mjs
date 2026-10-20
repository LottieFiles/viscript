import assert from "node:assert/strict";
import {
    clampPlaybackRange,
    normalizePlaybackRange,
    playbackBounds,
    setPlaybackEnd,
    setPlaybackStart,
} from "../media/playback-range.mjs";

assert.deepEqual(normalizePlaybackRange({start: 2, end: 5}, 10), {start: 2, end: 5});
assert.equal(normalizePlaybackRange({start: 5, end: 2}, 10), null);
assert.equal(normalizePlaybackRange({start: 2, end: 11}, 10), null);
assert.deepEqual(clampPlaybackRange({start: 2, end: 11}, 8), {start: 2, end: 8});
assert.equal(clampPlaybackRange({start: 9, end: 11}, 8), null);
assert.deepEqual(playbackBounds(null, 10), {start: 0, end: 10});

assert.deepEqual(setPlaybackStart(null, 2.5, 10), {start: 2.5, end: 10});
assert.deepEqual(setPlaybackEnd({start: 2.5, end: 10}, 7, 10), {start: 2.5, end: 7});
assert.deepEqual(setPlaybackStart({start: 2.5, end: 7}, 8, 10), {start: 8, end: 10});
assert.deepEqual(setPlaybackEnd({start: 8, end: 10}, 4, 10), {start: 0, end: 4});

console.log("playback range behavior passed");
