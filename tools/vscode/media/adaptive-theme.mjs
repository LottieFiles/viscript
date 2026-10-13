const LIGHT_FALLBACK = Object.freeze({
    background: "#ffffff",
    foreground: "#202124",
    muted: "#555b64",
    accent: "#005fb8",
    secondary: "#6750a4",
    success: "#2e7d32",
    warning: "#8a5d00",
    danger: "#b3261e",
    info: "#00639b",
    surface: "#f5f6f8",
    line: "#d8dadd",
    result: "#8a6500",
    focus: "#315f8c",
    objects: Object.freeze(["#b45f06", "#b63a3c", "#2e7f7a", "#815681"]),
});

const DARK_FALLBACK = Object.freeze({
    background: "#1e1e1e",
    foreground: "#cccccc",
    muted: "#9d9d9d",
    accent: "#4daafc",
    secondary: "#c8a7ff",
    success: "#7ee787",
    warning: "#e3b341",
    danger: "#ff7b72",
    info: "#79c0ff",
    surface: "#252526",
    line: "#454545",
    result: "#edc948",
    focus: "#6ea8fe",
    objects: Object.freeze(["#f28e2b", "#ff6b6b", "#7bc96f", "#c29bc0"]),
});

function firstColor(read, names, fallback) {
    for (const name of names) {
        const value = read(name)?.trim();
        if (value) return value;
    }
    return fallback;
}

export function resolveAdaptiveVscodeTheme({dark, read}) {
    const fallback = dark ? DARK_FALLBACK : LIGHT_FALLBACK;
    return Object.freeze({
        dark,
        background: firstColor(read, ["--vscode-editor-background"], fallback.background),
        foreground: firstColor(read, ["--vscode-editor-foreground", "--vscode-foreground"], fallback.foreground),
        muted: firstColor(read, ["--vscode-descriptionForeground", "--vscode-foreground"], fallback.muted),
        accent: firstColor(read, ["--vscode-textLink-foreground", "--vscode-focusBorder"], fallback.accent),
        secondary: firstColor(read, ["--vscode-charts-purple", "--vscode-symbolIcon-variableForeground"], fallback.secondary),
        success: firstColor(read, ["--vscode-testing-iconPassed", "--vscode-charts-green"], fallback.success),
        warning: firstColor(read, ["--vscode-editorWarning-foreground", "--vscode-charts-yellow"], fallback.warning),
        danger: firstColor(read, ["--vscode-editorError-foreground", "--vscode-errorForeground"], fallback.danger),
        info: firstColor(read, ["--vscode-editorInfo-foreground", "--vscode-charts-blue"], fallback.info),
        surface: firstColor(read, ["--vscode-editorWidget-background", "--vscode-sideBar-background"], fallback.surface),
        line: firstColor(
            read,
            ["--vscode-contrastBorder", "--vscode-panel-border", "--vscode-editorWidget-border", "--vscode-editorRuler-foreground"],
            fallback.line,
        ),
        result: firstColor(read, ["--vscode-charts-yellow"], fallback.result),
        focus: firstColor(
            read,
            ["--vscode-focusBorder", "--vscode-textLink-foreground", "--vscode-charts-blue"],
            fallback.focus,
        ),
        objects: Object.freeze([
            firstColor(read, ["--vscode-charts-orange"], fallback.objects[0]),
            firstColor(read, ["--vscode-charts-red"], fallback.objects[1]),
            firstColor(read, ["--vscode-charts-green"], fallback.objects[2]),
            firstColor(read, ["--vscode-charts-purple"], fallback.objects[3]),
        ]),
    });
}

export function readAdaptiveVscodeTheme(body = document.body) {
    const style = getComputedStyle(body);
    const dark = body.classList.contains("vscode-dark") || body.classList.contains("vscode-high-contrast");
    return resolveAdaptiveVscodeTheme({dark, read: (name) => style.getPropertyValue(name)});
}

export function adaptiveVscodeThemeKey(theme) {
    return [
        theme.dark,
        theme.background,
        theme.foreground,
        theme.muted,
        theme.accent,
        theme.secondary,
        theme.success,
        theme.warning,
        theme.danger,
        theme.info,
        theme.surface,
        theme.line,
        theme.result,
        theme.focus,
        ...theme.objects,
    ].join("\u0000");
}
