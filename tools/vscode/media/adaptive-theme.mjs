// The host theme from the workbench theme's colors. A color the theme leaves undefined stays
// unset, and the runtime takes its own light or dark one.
const firstColor = (read, names) => names.map((name) => read(name)?.trim()).find(Boolean);

export function resolveAdaptiveVscodeTheme({dark, read}) {
    return Object.freeze({
        dark,
        background: firstColor(read, ["--vscode-editor-background"]),
        foreground: firstColor(read, ["--vscode-editor-foreground", "--vscode-foreground"]),
        muted: firstColor(read, ["--vscode-descriptionForeground", "--vscode-foreground"]),
        accent: firstColor(read, ["--vscode-textLink-foreground", "--vscode-focusBorder"]),
        secondary: firstColor(read, ["--vscode-charts-purple", "--vscode-symbolIcon-variableForeground"]),
        success: firstColor(read, ["--vscode-testing-iconPassed", "--vscode-charts-green"]),
        warning: firstColor(read, ["--vscode-editorWarning-foreground", "--vscode-charts-yellow"]),
        danger: firstColor(read, ["--vscode-editorError-foreground", "--vscode-errorForeground"]),
        info: firstColor(read, ["--vscode-editorInfo-foreground", "--vscode-charts-blue"]),
        surface: firstColor(read, ["--vscode-editorWidget-background", "--vscode-sideBar-background"]),
        line: firstColor(read, [
            "--vscode-contrastBorder", "--vscode-panel-border", "--vscode-editorWidget-border", "--vscode-editorRuler-foreground",
        ]),
        result: firstColor(read, ["--vscode-charts-yellow"]),
        focus: firstColor(read, ["--vscode-focusBorder", "--vscode-textLink-foreground", "--vscode-charts-blue"]),
        objects: Object.freeze([
            firstColor(read, ["--vscode-charts-orange"]),
            firstColor(read, ["--vscode-charts-red"]),
            firstColor(read, ["--vscode-charts-green"]),
            firstColor(read, ["--vscode-charts-purple"]),
        ]),
    });
}

export function readAdaptiveVscodeTheme(body = document.body) {
    const style = getComputedStyle(body);
    const dark = body.classList.contains("vscode-dark") || body.classList.contains("vscode-high-contrast");
    return resolveAdaptiveVscodeTheme({dark, read: (name) => style.getPropertyValue(name)});
}

// Equal for equal themes: resolveAdaptiveVscodeTheme gives every theme the same key order.
export function adaptiveVscodeThemeKey(theme) {
    return JSON.stringify(theme);
}
