import assert from "node:assert/strict";
import {exportPlan, frameTime, gifDelay} from "../webview/export-plan.mjs";

const scene = {size: () => [320, 180], duration: 1.25};
assert.deepEqual(exportPlan(scene, 24), {
    width: 320,
    height: 180,
    start: 0,
    end: 1.25,
    duration: 1.25,
    fps: 24,
    frames: 30,
});
assert.equal(frameTime(exportPlan(scene, 24), 29), 29 / 24);
const rangePlan = exportPlan(scene, 10, {start: 0.25, end: 0.75});
assert.deepEqual(rangePlan, {
    width: 320,
    height: 180,
    start: 0.25,
    end: 0.75,
    duration: 0.5,
    fps: 10,
    frames: 5,
});
assert.equal(frameTime(rangePlan, 0), 0.25);
assert.equal(frameTime(rangePlan, 4), 0.65);
assert.equal(gifDelay(0, 30), 30);
assert.equal(gifDelay(1, 30), 40);
assert.throws(() => exportPlan(scene, 0), /FPS/);
assert.throws(() => exportPlan({size: () => [3840, 2160], duration: 100}, 60), /limited/);

console.log("media export planning passed");
