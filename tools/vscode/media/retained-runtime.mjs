export function sceneCanPlay(runtime) {
    return Boolean(runtime && (runtime.duration > 0 || runtime.retainedLua));
}

export function advanceRetainedRuntime(runtime, elapsed) {
    if (!runtime?.retainedLua || typeof runtime.advanceRuntime !== "function") return null;
    const step = runtime.advanceRuntime(elapsed);
    if (step?.steps > 0 && typeof runtime.beginInputFrame === "function") {
        runtime.beginInputFrame(step.time);
    }
    return step;
}
