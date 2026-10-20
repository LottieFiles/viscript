export function normalizeTime(value, duration) {
    if (!Number.isFinite(value) || !Number.isFinite(duration) || duration < 0) return 0;
    return Math.max(0, Math.min(duration, value));
}
