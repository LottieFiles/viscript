import {normalizeTime} from "./time.mjs";

function sceneDuration(duration) {
    return Number.isFinite(duration) && duration >= 0 ? duration : 0;
}

export function normalizePlaybackRange(range, duration) {
    const limit = sceneDuration(duration);
    if (!range || !Number.isFinite(range.start) || !Number.isFinite(range.end)) return null;
    if (range.start < 0 || range.start >= range.end || range.end > limit) return null;
    return {start: range.start, end: range.end};
}

export function clampPlaybackRange(range, duration) {
    const limit = sceneDuration(duration);
    if (!range || !Number.isFinite(range.start) || !Number.isFinite(range.end)) return null;
    const start = normalizeTime(range.start, limit);
    const end = normalizeTime(range.end, limit);
    return start < end ? {start, end} : null;
}

export function playbackBounds(range, duration) {
    return normalizePlaybackRange(range, duration) ?? {start: 0, end: sceneDuration(duration)};
}

export function setPlaybackStart(range, time, duration) {
    const limit = sceneDuration(duration);
    const start = normalizeTime(time, limit);
    const current = normalizePlaybackRange(range, limit);
    if (start >= limit) return current;
    const end = current && current.end > start ? current.end : limit;
    return {start, end};
}

export function setPlaybackEnd(range, time, duration) {
    const limit = sceneDuration(duration);
    const end = normalizeTime(time, limit);
    const current = normalizePlaybackRange(range, limit);
    if (end <= 0) return current;
    const start = current && current.start < end ? current.start : 0;
    return {start, end};
}

export function advancePlaybackTime(time, elapsed, duration, range, looping) {
    const {start, end} = playbackBounds(range, duration);
    if (end <= start) return {time: start, ended: true};
    if (Number.isFinite(time) && time >= end && !looping) return {time: end, ended: true};
    const base = Number.isFinite(time) && time >= start && time < end ? time : start;
    const next = base + (Number.isFinite(elapsed) ? Math.max(0, elapsed) : 0);
    if (next < end) return {time: next, ended: false};
    if (!looping) return {time: end, ended: true};
    return {time: start + ((next - start) % (end - start)), ended: false};
}
