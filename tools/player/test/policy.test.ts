import assert from "node:assert/strict";
import test from "node:test";
import {fitPlan, frameInterval} from "../src/policy.ts";

const scene = {width: 356, logicalWidth: 960, logicalHeight: 540, dpr: 3, mobile: true};

test("phones cap the device pixel ratio at 1.5", () => assert.deepEqual(fitPlan(scene), {width: 534, height: 300, ratio: 1}));
test("phones never rasterize at 2x by accident", () => assert.equal(fitPlan({...scene, width: 700}).ratio, 1));
test("high quality keeps the original supersampling", () =>
    assert.deepEqual(fitPlan({...scene, quality: "high"}), {width: 1068, height: 601, ratio: 2}));
test("desktops cap the device pixel ratio at 2", () => assert.equal(fitPlan({...scene, mobile: false}).width, 712));
test("a zero CSS width still gives a drawable size", () => assert.equal(fitPlan({...scene, width: 0}).width, 1));
test("an invalid device pixel ratio counts as 1", () => assert.equal(fitPlan({...scene, dpr: Number.NaN}).width, 356));
test("the ratio stays a positive integer under an invalid limit", () => assert.equal(fitPlan({...scene, maxRatio: 0}).ratio, 1));
test("phones draw 30 frames a second", () => assert.equal(frameInterval(true), 1000 / 30));
test("desktops draw 60 frames a second", () => assert.equal(frameInterval(false), 1000 / 60));
test("a scene can ask for 20 frames a second", () => assert.equal(frameInterval(true, 20), 50));
test("an invalid rate takes the default", () => assert.equal(frameInterval(true, -1), 1000 / 30));
test("the rate is held to 120", () => assert.equal(frameInterval(false, 1000), 1000 / 120));
