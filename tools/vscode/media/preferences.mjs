// The player menu's preferences. The webview's own saved state wins; a new webview starts from
// the preferences the extension stored for all of them.
export const SOURCE_CONTEXT_LINE_COUNTS = new Set([5, 10, 20]);

export function previewPreferences(state, config) {
    const flag = (name) => typeof state?.[name] === "boolean" ? state[name] : config[name] === true;
    const lines = state?.sourceContextLineCount ?? config.sourceContextLineCount;
    return {
        autoPlayNext: flag("autoPlayNext"),
        forceCompactLayout: flag("forceCompactLayout"),
        sourceContextLineCount: SOURCE_CONTEXT_LINE_COUNTS.has(lines) ? lines : 5,
        // Stats are a per-webview choice the extension does not store.
        showStats: state?.showStats === true,
    };
}
