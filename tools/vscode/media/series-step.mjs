import {advancePlaybackTime} from "./playback-range.mjs";

export function seriesStepTarget(series, offset) {
    if (offset !== -1 && offset !== 1) return null;
    const groups = Array.isArray(series?.groups) ? series.groups : [];
    if (!Number.isInteger(series?.activeGroupIndex)) return null;
    const groupIndex = series.activeGroupIndex;
    const group = groups[groupIndex];
    if (!group || !Array.isArray(group.items) || !Number.isInteger(group.activeIndex)) return null;
    const itemIndex = group.activeIndex + offset;
    if (itemIndex < 0 || itemIndex >= group.items.length) return null;
    return {groupIndex, itemIndex, groupLabel: group.label, itemLabel: group.items[itemIndex]};
}

export function autoAdvanceSeriesTarget(series, enabled) {
    if (!enabled) return null;
    const next = seriesStepTarget(series, 1);
    if (next) return next;
    const groups = Array.isArray(series?.groups) ? series.groups : [];
    if (!Number.isInteger(series?.activeGroupIndex)) return null;
    const groupIndex = series.activeGroupIndex;
    const group = groups[groupIndex];
    if (!group || !Array.isArray(group.items) || group.items.length === 0
        || group.activeIndex !== group.items.length - 1) return null;
    return {groupIndex, itemIndex: 0, groupLabel: group.label, itemLabel: group.items[0]};
}

// Advances playback by a frame. With auto-play-next on and an item to go to, the scene plays once
// instead of looping, and ends with the item to open.
export function advanceSeriesPlayback({series, autoPlayNext, looping}, time, elapsed, duration, range) {
    const target = autoAdvanceSeriesTarget(series, autoPlayNext);
    const next = advancePlaybackTime(time, elapsed, duration, range, looping && !target);
    return next.ended && target ? {...next, open: target} : next;
}
