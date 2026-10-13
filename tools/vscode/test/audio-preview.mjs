import assert from "node:assert/strict";
import fs from "node:fs";
import path from "node:path";
import {fileURLToPath} from "node:url";

const extensionRoot = path.dirname(path.dirname(fileURLToPath(import.meta.url)));
const webview = fs.readFileSync(path.join(extensionRoot, "media", "main.js"), "utf8");
const panel = fs.readFileSync(path.join(extensionRoot, "src", "previewPanel.ts"), "utf8");

assert.match(panel, /case "\.wav": return "audio\/wav"/,
    "Preview asset discovery must preserve the WAV MIME family");
assert.match(webview,
    /asset\.mime\.startsWith\("audio\/"\)[\s\S]*candidate\.audio\?\.load\(asset\.name, bytes\)/,
    "Audio assets must load through the optional Audio sidecar rather than image assets");
assert.match(webview,
    /function startRuntimeAudio\(\)[\s\S]*runtime\.audio\.start\(\)[\s\S]*syncRuntimeAudio\(true\)/,
    "Audio must start and seek only through the user-gesture path");
assert.match(webview,
    /function playRuntimeSounds\(step\)[\s\S]*step\.sounds[\s\S]*runtime\.audio\.play\(sound\.asset/,
    "Retained sound events must reach optional immediate playback");
assert.match(webview,
    /function syncRuntimeAudio\(seek\)[\s\S]*runtime\.audio\.transport\([\s\S]*time: runtimeAudioTime\(\)[\s\S]*playing[\s\S]*seek/,
    "Authored cues must follow retained or Scene playback transport");
assert.match(webview, /document\.addEventListener\("pointerdown",[\s\S]*startRuntimeAudio\(\)/,
    "Pointer activation must unlock browser audio");
assert.match(webview, /window\.addEventListener\("keydown", startRuntimeAudio, \{capture: true\}\)/,
    "Keyboard activation must unlock browser audio without coupling it to game actions");
assert.match(webview,
    /const wasAudioStarted = audioStarted[\s\S]*if \(wasAudioStarted\) startRuntimeAudio\(\)/,
    "A user-unlocked Preview must restore Audio when a hot reload replaces its runtime");

console.log("audio preview routing passed");
