export function normalizeTime(value, duration) {
    if (!Number.isFinite(value) || !Number.isFinite(duration) || duration < 0) return 0;
    return Math.max(0, Math.min(duration, value));
}

export function frameElapsed(now, previous, maximum = 0.1) {
    if (!Number.isFinite(now) || !Number.isFinite(previous)
        || !Number.isFinite(maximum) || maximum < 0) {
        return 0;
    }
    return Math.max(0, Math.min((now - previous) / 1000, maximum));
}
