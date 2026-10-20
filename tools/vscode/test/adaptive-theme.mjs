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
    font: "Source Serif 4, Pretendard",
});

// A color the theme does not define stays unset for the runtime to fill.
const unset = resolveAdaptiveVscodeTheme({dark: false, read: (name) => name === "--vscode-charts-red" ? "#ef6464" : ""});
assert.deepEqual(unset, {
    dark: false,
    ...Object.fromEntries(Object.keys(dark).filter((key) => !["dark", "objects", "font"].includes(key)).map((key) => [key, undefined])),
    objects: [undefined, "#ef6464", undefined, undefined],
    font: "Source Serif 4, Pretendard",
});
assert.notEqual(adaptiveVscodeThemeKey(dark), adaptiveVscodeThemeKey(unset));
assert.notEqual(adaptiveVscodeThemeKey(unset), adaptiveVscodeThemeKey({...unset, dark: true}));
assert.equal(adaptiveVscodeThemeKey(dark), adaptiveVscodeThemeKey({...dark}));

console.log("Adaptive VS Code palette resolution passed");
