import assert from "node:assert/strict";
import {
    automaticWidePlayerWidth,
    cascadingMenuPlacement,
    floatingMenuPlacement,
    floatingPointMenuPlacement,
    manualStageHeight,
    normalStageMaxHeight,
    responsiveViewerLayout,
    WIDE_LAYOUT_EXIT_WIDTH,
    WIDE_LAYOUT_REFERENCE_WIDTH,
    widePlayerWidth,
} from "../media/viewer-layout.mjs";

assert.equal(WIDE_LAYOUT_REFERENCE_WIDTH, 600);
assert.equal(WIDE_LAYOUT_EXIT_WIDTH, 567);
assert.equal(responsiveViewerLayout(599, "compact"), "compact");
assert.equal(responsiveViewerLayout(600, "compact"), "wide");
assert.equal(responsiveViewerLayout(567, "wide"), "wide");
assert.equal(responsiveViewerLayout(566, "wide"), "compact");
assert.equal(responsiveViewerLayout(1200, "wide", true), "compact");
assert.equal(responsiveViewerLayout(1200, "compact", true), "compact");
assert.equal(responsiveViewerLayout(Number.NaN, "wide"), "compact");

assert.equal(widePlayerWidth(700, 1200), 700);
assert.equal(widePlayerWidth(1100, 1200), 913);
assert.equal(widePlayerWidth(50, 1200), 280);
assert.equal(widePlayerWidth(200, 300), 280);
assert.equal(automaticWidePlayerWidth(1200), 813);
assert.equal(automaticWidePlayerWidth(1000), 673);
assert.equal(automaticWidePlayerWidth(600), 313);

assert.equal(normalStageMaxHeight(900, 38, 7), 855);
assert.equal(normalStageMaxHeight(600, 38, 7), 555);
assert.equal(normalStageMaxHeight(360, 38, 7), 315);
assert.equal(normalStageMaxHeight(120, 38, 7), 75);
assert.equal(normalStageMaxHeight(90, 38, 7), 45);
assert.equal(normalStageMaxHeight(Number.NaN, 38), 260);
assert.equal(manualStageHeight(700, 900, 38), 700);
assert.equal(manualStageHeight(900, 900, 38), 855);
assert.equal(manualStageHeight(10, 900, 38), 64);
assert.equal(manualStageHeight(10, 90, 38), 45);
assert.equal(manualStageHeight(900, 120, 38, 7), 75);
assert.equal(manualStageHeight(10, 120, 38, 7), 64);
assert.equal(manualStageHeight(Number.NaN, 900, 38), 260);

// Automatic sizing uses all space above Playback while respecting Scene FIT.
assert.equal(normalStageMaxHeight(900, 38, 7, 1_600, 960, 540), 855);
assert.equal(normalStageMaxHeight(900, 38, 7, 1_200, 960, 540), 675);
assert.equal(normalStageMaxHeight(900, 38, 7, 1_000, 960, 540), 562);

assert.deepEqual(
    floatingMenuPlacement(800, 600, {top: 500, right: 790, bottom: 528}),
    {top: null, bottom: 107, right: 10, maxHeight: 493},
);
assert.deepEqual(
    floatingMenuPlacement(800, 600, {top: 100, right: 790, bottom: 128}),
    {top: 135, bottom: null, right: 10, maxHeight: 465},
);
assert.deepEqual(
    floatingMenuPlacement(320, 160, {top: 70, right: 320, bottom: 98}),
    {top: null, bottom: 97, right: 0, maxHeight: 63},
);
assert.deepEqual(
    floatingPointMenuPlacement(800, 600, 200, 100, 330),
    {top: 107, bottom: null, right: 600, maxHeight: 493, left: 207},
);
assert.deepEqual(
    floatingPointMenuPlacement(800, 600, 780, 580, 330),
    {top: null, bottom: 27, right: 20, maxHeight: 573, left: 443},
);
assert.deepEqual(
    floatingPointMenuPlacement(320, 160, 318, 4, 304),
    {top: 11, bottom: null, right: 2, maxHeight: 149, left: 8},
);
assert.deepEqual(
    cascadingMenuPlacement(800, 600, {top: 120, left: 100, right: 274}, 152, 118),
    {left: 276, top: 120, maxHeight: 592},
);
assert.deepEqual(
    cascadingMenuPlacement(320, 160, {top: 120, left: 142, right: 316}, 152, 118),
    {left: 4, top: 38, maxHeight: 152},
);

console.log("viewer height allocation passed");
