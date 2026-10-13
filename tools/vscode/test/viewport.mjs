import assert from "node:assert/strict";
import {
    MAX_PREVIEW_SCALE,
    MAX_RENDER_PIXEL_RATIO,
    MIN_PREVIEW_SCALE,
    clampPreviewScale,
    fitPreviewScale,
    previewAnchorTranslation,
    previewRenderPixelRatio,
    stepPreviewScale,
    wheelPreviewScale,
} from "../media/viewport.mjs";

assert.equal(clampPreviewScale(Number.NaN), 1);
assert.equal(clampPreviewScale(0.001), MIN_PREVIEW_SCALE);
assert.equal(clampPreviewScale(20), MAX_PREVIEW_SCALE);
assert.equal(fitPreviewScale(320, 200, 1000, 700), 3.125);
assert.equal(fitPreviewScale(10_000, 5_000, 1000, 500), 0.1);
assert.equal(fitPreviewScale(1_000_000, 500_000, 1000, 500), MIN_PREVIEW_SCALE);
assert.equal(stepPreviewScale(1, 1), 1.1);
assert.equal(stepPreviewScale(1, -1), 1 / 1.1);
assert.ok(Math.abs(wheelPreviewScale(1, -1) - 1.0015) < 0.00001);
assert.ok(Math.abs(wheelPreviewScale(1, 100) - Math.exp(-0.15)) < 0.00001);
assert.equal(wheelPreviewScale(1, 1000), wheelPreviewScale(1, 100));
assert.equal(wheelPreviewScale(0.8, 100, 0.75), 0.75);
assert.equal(wheelPreviewScale(0.75, 100, 0.75), 0.75);
assert.ok(wheelPreviewScale(0.75, -100, 0.75) > 0.75);
assert.equal(previewRenderPixelRatio(800, 800, 1, 2), 2);
assert.equal(previewRenderPixelRatio(800, 800, 1.01, 2), 3);
assert.equal(previewRenderPixelRatio(320, 180, 8, 2), MAX_RENDER_PIXEL_RATIO);
assert.equal(previewRenderPixelRatio(1920, 1080, 4, 2), 2);
assert.equal(previewRenderPixelRatio(4000, 3000, 2, 2), 1);
assert.equal(previewRenderPixelRatio(100, 100, 2, 2, 20_000), 1);
assert.equal(previewRenderPixelRatio(0, 100, 2, 2), 1);
assert.deepEqual(
    previewAnchorTranslation(
        {left: 100, top: 50, width: 400, height: 200},
        {left: 100, top: 50, width: 800, height: 400},
        200,
        100,
    ),
    {x: -100, y: -50},
);
assert.deepEqual(
    previewAnchorTranslation(
        {left: 100, top: 50, width: 400, height: 200},
        {left: 100, top: 50, width: 800, height: 400},
        50,
        300,
    ),
    {x: -50, y: -150},
);
assert.deepEqual(previewAnchorTranslation({}, {}, 0, 0), {x: 0, y: 0});

console.log("preview viewport scaling passed");
