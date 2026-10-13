import {playbackBounds} from "../media/playback-range.mjs";

export const FRAME_LIMIT = 3600;
export const PIXEL_LIMIT = 8294400;
export const PIXEL_SAMPLE_LIMIT = 268435456;

/** FPS is an integer in 1..60. Export accepts at most 8294400 pixels per frame,
 * 3600 frames, and 268435456 sampled pixels across the selected range. */
export function exportPlan(scene, fps, range = null) {
    if (!Number.isInteger(fps) || fps < 1 || fps > 60) {
        throw new Error("FPS must be an integer from 1 to 60");
    }
    const [width, height] = scene.size();
    const {start, end} = playbackBounds(range, scene.duration);
    const duration = end - start;
    const samples = duration * fps;
    const integer = Math.round(samples);
    const snapped = Math.abs(samples - integer) <= Math.max(1, samples) * 1e-6 ? integer : samples;
    const frames = Math.max(1, Math.ceil(snapped));
    if (width * height > PIXEL_LIMIT || frames > FRAME_LIMIT || width * height * frames > PIXEL_SAMPLE_LIMIT) {
        throw new Error("Export is limited to 3840×2160, 3600 frames, and 268 million sampled pixels");
    }
    return {width, height, start, end, duration, fps, frames};
}

export function frameTime(plan, frame) {
    return Math.min(plan.start + frame / plan.fps, plan.end);
}

export function gifDelay(frame, fps) {
    const begin = Math.round((frame * 100) / fps);
    const end = Math.round(((frame + 1) * 100) / fps);
    return Math.max(1, end - begin) * 10;
}
