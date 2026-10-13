import assert from "node:assert/strict";
import fs from "node:fs";
import path from "node:path";
import {fileURLToPath} from "node:url";
import {normalStageMaxHeight} from "../media/viewer-layout.mjs";

const extensionRoot = path.resolve(path.dirname(fileURLToPath(import.meta.url)), "..");
const source = fs.readFileSync(path.join(extensionRoot, "media", "main.js"), "utf8");
const start = source.indexOf("async function loadScene(");
const end = source.indexOf("\nfunction scheduleAdaptiveThemeRefresh", start);
assert.ok(start >= 0 && end > start, "loadScene implementation must be present");
const loadScene = source.slice(start, end);

// A stale wide-scene cap is much smaller than the cap required by the incoming
// portrait scene. Fitting once with each value produces the visible shrink/grow.
const staleWideCap = normalStageMaxHeight(900, 38, 7, 1_600, 3_840, 720);
const portraitCap = normalStageMaxHeight(900, 38, 7, 1_600, 450, 800);
assert.equal(staleWideCap, 300);
assert.equal(portraitCap, 855);

const runtimeGate = loadScene.indexOf("if (!createVis");
assert.ok(runtimeGate > 0, "runtime readiness gate must be present");
assert.doesNotMatch(
    loadScene.slice(0, runtimeGate),
    /requestAnimationFrame\(syncNormalLayoutLimit\)/,
    "scene switches must not calculate a new stage cap with the previous Scene dimensions",
);
assert.match(
    loadScene,
    /sceneWidth = width;[\s\S]*sceneHeight = height;[\s\S]*syncNormalLayoutLimit\(\);[\s\S]*if \(previewFits\) fitPreview\(\);/,
    "the incoming Scene dimensions and stage cap must commit before the first FIT",
);

console.log("scene switch layout remains stable");
