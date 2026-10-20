import assert from "node:assert/strict";
import test from "node:test";
import {fenceOptions, parseOptions} from "../src/options.ts";
import {DEFAULT_SETTINGS, readSettings} from "../src/settings.ts";

test("an embed's alias gives options, and other words a label", () => {
    assert.deepEqual(parseOptions("600|loop"), {width: 600, end: "loop"});
    assert.deepEqual(parseOptions("640x360 center paused"), {width: 640, align: "center", autoplay: false});
    assert.deepEqual(parseOptions("Orbit of the moon | once"), {end: "stop", label: "Orbit of the moon"});
    assert.deepEqual(parseOptions("interactive nocontrols"), {interactive: true, controls: false});
    // A plain embed's alt is its file name, which is not a label.
    assert.deepEqual(parseOptions("intro.lua", "intro.lua"), {});
    // Later words win.
    assert.deepEqual(parseOptions("loop stop"), {end: "stop"});
});

test("a code block's options follow its language", () => {
    assert.deepEqual(fenceOptions("```viscript loop 480"), {end: "loop", width: 480});
    assert.deepEqual(fenceOptions("~~~~viscript-js   right"), {align: "right"});
    assert.deepEqual(fenceOptions("```viscript"), {});
    assert.deepEqual(fenceOptions(""), {});
});

test("stored settings are checked one by one", () => {
    assert.deepEqual(readSettings(undefined), DEFAULT_SETTINGS);
    assert.deepEqual(readSettings("nonsense"), DEFAULT_SETTINGS);
    const read = readSettings({
        autoplay: false, end: "loop", controls: "sometimes", maxFps: -5, quality: "high", textFont: "custom",
        textFontName: "Inter", downloadFonts: "yes", fallbackFont: "auto", fontFolder: "fonts", unknown: true,
    });
    assert.equal(read.autoplay, false);
    assert.equal(read.end, "loop");
    assert.equal(read.controls, DEFAULT_SETTINGS.controls);
    assert.equal(read.maxFps, DEFAULT_SETTINGS.maxFps);
    assert.equal(read.quality, "high");
    assert.equal(read.textFont, "custom");
    assert.equal(read.textFontName, "Inter");
    assert.equal(read.downloadFonts, DEFAULT_SETTINGS.downloadFonts);
    assert.equal(read.fontFolder, "fonts");
    assert.ok(!("unknown" in read) && !("fallbackFont" in read));
    // Font choices and settings earlier versions had are the default, or dropped.
    assert.equal(readSettings({textFont: "bundled", codeFont: "bundled"}).codeFont, "default");
    assert.equal(readSettings({textFont: "obsidian"}).textFont, "default");
    assert.ok(!("installedFonts" in readSettings({installedFonts: true})));
    assert.equal(readSettings({maxFps: 30}).maxFps, 30);
    assert.equal(readSettings({maxFps: Number.POSITIVE_INFINITY}).maxFps, 0);
});

test("removed replay modes fall back to replay at once", () => {
    assert.equal(readSettings({end: "auto"}).end, "loop");
    assert.equal(readSettings({end: "crossfade"}).end, "loop");
    assert.equal(readSettings({end: "stop"}).end, "stop");
});
