import assert from "node:assert/strict";
import {frameElapsed, normalizeTime} from "../media/time.mjs";

assert.equal(normalizeTime(Number.NaN, 10), 0);
assert.equal(normalizeTime(Number.POSITIVE_INFINITY, 10), 0);
assert.equal(normalizeTime(-0.001, 10), 0);
assert.equal(normalizeTime(3.25, 10), 3.25);
assert.equal(normalizeTime(10.001, 10), 10);
assert.equal(normalizeTime(2, Number.NaN), 0);

assert.equal(frameElapsed(1100, 1000), 0.1);
assert.equal(frameElapsed(1000, 1000.5), 0);
assert.equal(frameElapsed(Number.NaN, 1000), 0);
assert.equal(frameElapsed(1200, 1000), 0.1);

console.log("time normalization passed");
