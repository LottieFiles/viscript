import assert from "node:assert/strict";
import fs from "node:fs";
import path from "node:path";
import {fileURLToPath} from "node:url";

const extensionRoot = path.dirname(path.dirname(fileURLToPath(import.meta.url)));
const manifest = JSON.parse(fs.readFileSync(path.join(extensionRoot, "package.json"), "utf8"));
const extension = fs.readFileSync(path.join(extensionRoot, "src", "extension.ts"), "utf8");
const previewPanel = fs.readFileSync(path.join(extensionRoot, "src", "previewPanel.ts"), "utf8");
const visualizationProvider = fs.readFileSync(path.join(extensionRoot, "src", "visualizationProvider.ts"), "utf8");
const webview = fs.readFileSync(path.join(extensionRoot, "media", "main.js"), "utf8");

assert.ok(manifest.activationEvents.includes("onCommand:visPreview.openViewer"));
assert.ok(manifest.contributes.commands.some(
    (command) => command.command === "visPreview.openViewer" && command.title === "viscript: Open Viewer",
));
assert.ok(!manifest.activationEvents.includes("onCommand:visPreview.open"));
assert.ok(!manifest.contributes.commands.some((command) => command.command === "visPreview.open"));
assert.doesNotMatch(extension, /registerCommand\("visPreview\.open"/);
assert.ok(manifest.contributes.commands.some((command) => command.command === "visPreview.openToSide"));
assert.ok(manifest.contributes.menus.commandPalette.some(
    (item) => item.command === "visPreview.openToSide" && item.when === "false",
));
const viewerOpenCommandIds = new Set([
    "visPreview.openViewer",
    "visPreview.open",
    "visPreview.openToSide",
]);
const visibleOpenCommands = manifest.contributes.commands
    .filter((command) => viewerOpenCommandIds.has(command.command))
    .filter((command) => !manifest.contributes.menus.commandPalette.some(
        (item) => item.command === command.command && item.when === "false",
    ));
assert.deepEqual(visibleOpenCommands.map((command) => command.command), ["visPreview.openViewer"]);
assert.match(extension, /registerCommand\("visPreview\.openViewer"[\s\S]*PreviewPanel\.openViewer\(/);
assert.match(extension, /visualizations\.previewSeriesCatalog\(\)/);
assert.match(previewPanel, /static async openViewer\(/);
assert.match(previewPanel, /this\.createPanel\(context, diagnostics, column, "viscript Viewer"/);
assert.match(previewPanel, /private static createPanel\([\s\S]*createWebviewPanel\(/);
assert.match(previewPanel, /setSeriesCatalog\(/);
assert.match(visualizationProvider, /previewSeriesCatalog:/);
assert.match(visualizationProvider, /getWorkspaceFolder\(vscode\.window\.activeTextEditor\.document\.uri\)[\s\S]*\?\? vscode\.workspace\.workspaceFolders\?\.\[0\]/);
assert.match(webview, /event\.data\?\.type === "series"[\s\S]*syncSeries\(event\.data\.series\)/);

console.log("empty viewer command passed");

// Passive editor activity and hover requests must never launch the Viewer.
assert.doesNotMatch(visualizationProvider, /scheduleAutoOpen|thumbnail/i);
assert.doesNotMatch(visualizationProvider, /onDidChangeTextEditorSelection|onDidChangeActiveTextEditor/);
assert.equal((visualizationProvider.match(/this\.openReference\(/g) ?? []).length, 1);
assert.ok(!("visPreview.codeVisualizations.autoOpen" in manifest.contributes.configuration.properties));
assert.ok(!("visPreview.codeVisualizations.autoOpenDelay" in manifest.contributes.configuration.properties));
assert.ok(!("visPreview.codeVisualizations.hoverThumbnails" in manifest.contributes.configuration.properties));
