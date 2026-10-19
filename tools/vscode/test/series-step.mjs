import assert from "node:assert/strict";
import {advanceSeriesPlayback, autoAdvanceSeriesTarget, seriesStepTarget} from "../media/series-step.mjs";

const series = {
    activeGroupIndex: 1,
    groups: [
        {label: "Other", activeIndex: 0, items: ["Ignored"]},
        {label: "Raster", activeIndex: 1, items: ["Dispatch", "Clip", "Blend"]},
    ],
};

assert.deepEqual(seriesStepTarget(series, -1), {
    groupIndex: 1,
    itemIndex: 0,
    groupLabel: "Raster",
    itemLabel: "Dispatch",
});
assert.deepEqual(seriesStepTarget(series, 1), {
    groupIndex: 1,
    itemIndex: 2,
    groupLabel: "Raster",
    itemLabel: "Blend",
});
assert.equal(seriesStepTarget({...series, groups: [series.groups[0], {...series.groups[1], activeIndex: 0}]}, -1), null);
assert.equal(seriesStepTarget({...series, groups: [series.groups[0], {...series.groups[1], activeIndex: 2}]}, 1), null);
assert.equal(seriesStepTarget(undefined, 1), null);
assert.equal(seriesStepTarget(series, 0), null);

assert.deepEqual(autoAdvanceSeriesTarget(series, true), {
    groupIndex: 1,
    itemIndex: 2,
    groupLabel: "Raster",
    itemLabel: "Blend",
});
assert.equal(autoAdvanceSeriesTarget(series, false), null);
assert.deepEqual(
    autoAdvanceSeriesTarget({...series, groups: [series.groups[0], {...series.groups[1], activeIndex: 2}]}, true),
    {groupIndex: 1, itemIndex: 0, groupLabel: "Raster", itemLabel: "Dispatch"},
);
assert.deepEqual(
    autoAdvanceSeriesTarget({activeGroupIndex: 0, groups: [{label: "Solo", activeIndex: 0, items: ["Only"]}]}, true),
    {groupIndex: 0, itemIndex: 0, groupLabel: "Solo", itemLabel: "Only"},
);
assert.equal(autoAdvanceSeriesTarget(undefined, true), null);

// Auto-play-next plays a looping scene once, then opens the next item.
const playback = {series, autoPlayNext: true, looping: true};
assert.deepEqual(advanceSeriesPlayback(playback, 1.5, 0.25, 2, null), {time: 1.75, ended: false});
assert.deepEqual(advanceSeriesPlayback(playback, 1.75, 0.5, 2, null), {
    time: 2,
    ended: true,
    open: {groupIndex: 1, itemIndex: 2, groupLabel: "Raster", itemLabel: "Blend"},
});
assert.deepEqual(advanceSeriesPlayback({...playback, autoPlayNext: false}, 1.75, 0.5, 2, null), {time: 0.25, ended: false});
assert.deepEqual(advanceSeriesPlayback({...playback, looping: false, series: undefined}, 1.75, 0.5, 2, null), {time: 2, ended: true});

console.log("series step navigation passed");
