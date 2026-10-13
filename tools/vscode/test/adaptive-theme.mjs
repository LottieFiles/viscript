import assert from "node:assert/strict";
import {adaptiveVscodeThemeKey, resolveAdaptiveVscodeTheme} from "../media/adaptive-theme.mjs";

const values = new Map([
    ["--vscode-editor-background", " #101820 "],
    ["--vscode-editor-foreground", "#e8edf2"],
    ["--vscode-descriptionForeground", "#a8b1bb"],
    ["--vscode-textLink-foreground", "#62b0ff"],
    ["--vscode-charts-purple", "#c8a7ff"],
    ["--vscode-testing-iconPassed", "#7ee787"],
    ["--vscode-editorWarning-foreground", "#e3b341"],
    ["--vscode-editorError-foreground", "#ff7b72"],
    ["--vscode-editorInfo-foreground", "#79c0ff"],
    ["--vscode-editorWidget-background", "#252526"],
    ["--vscode-panel-border", "#39434d"],
    ["--vscode-charts-yellow", "#d9b949"],
    ["--vscode-focusBorder", "#4f9dde"],
    ["--vscode-charts-orange", "#e89432"],
    ["--vscode-charts-red", "#ef6464"],
    ["--vscode-charts-green", "#72bd78"],
    ["--vscode-charts-purple", "#b390d4"],
]);
const dark = resolveAdaptiveVscodeTheme({dark: true, read: (name) => values.get(name) ?? ""});
assert.deepEqual(dark, {
    dark: true,
    background: "#101820",
    foreground: "#e8edf2",
    muted: "#a8b1bb",
    accent: "#62b0ff",
    secondary: "#b390d4",
    success: "#7ee787",
    warning: "#e3b341",
    danger: "#ff7b72",
    info: "#79c0ff",
    surface: "#252526",
    line: "#39434d",
    result: "#d9b949",
    focus: "#4f9dde",
    objects: ["#e89432", "#ef6464", "#72bd78", "#b390d4"],
});

const fallback = resolveAdaptiveVscodeTheme({dark: false, read: () => ""});
assert.equal(fallback.background, "#ffffff");
assert.equal(fallback.foreground, "#202124");
assert.deepEqual(fallback.objects, ["#b45f06", "#b63a3c", "#2e7f7a", "#815681"]);
assert.notEqual(adaptiveVscodeThemeKey(dark), adaptiveVscodeThemeKey(fallback));
assert.equal(adaptiveVscodeThemeKey(dark), adaptiveVscodeThemeKey({...dark}));

console.log("Adaptive VS Code palette resolution passed");
