import assert from "node:assert/strict";
import test from "node:test";
import {audioMime, audioNames, imageMime, imageNames, looksLikeScene, scanSource} from "../src/scan.ts";

test("Lua literals are read past comments, escapes decoded", () => {
    const {literals, code} = scanSource(String.raw`
-- s:text {text = "commented out"}
--[==[ a block comment with "quotes" ]==]
local a = "tab\there"
local b = 'it''s'
local c = [[long
string]]
local d = [=[with ]] inside]=]
local e = "\u{AC00}\x41\66\z
           C"`, "lua");
    assert.deepEqual(literals, ["tab\there", "it", "s", "long\nstring", "with ]] inside", "가ABC"]);
    assert.ok(!code.includes("commented out") && !code.includes("block comment") && !code.includes("tab"));
});

test("Lua escapes are bytes, and the text is their UTF-8", () => {
    assert.deepEqual(scanSource(String.raw`x = "\234\176\128" .. "\xEA\xB0\x80" .. "\u{1F600}"`, "lua").literals,
        ["가", "가", "😀"]);
});

test("JavaScript template literals nest", () => {
    const {literals, code} = scanSource("// \"gone\"\nconst a = `outer ${f(`in ${'deep'} er`)} \\u{AC00}`; /* 'x' */ const b = \"\\uAC01\";",
        "js");
    assert.deepEqual(literals, ["outer ", "in ", "deep", " er", " 가", "각"]);
    assert.ok(!code.includes("gone") && !code.includes("'x'"));
});

test("the comment-free code keeps the source's length and lines", () => {
    const source = "-- a\nlocal x = 1 --[[ b\nc ]] return x";
    const {code} = scanSource(source, "lua");
    assert.equal(code.length, source.length);
    assert.equal(code.split("\n").length, source.split("\n").length);
});

test("images are literals naming a PNG or SVG file, the types the runtime decodes", () => {
    const scanned = scanSource(`s:picture {asset = "earth.png"}
s:picture {asset = 'img/LOGO.SVG'}
s:picture {asset = "photo.jpg"}
s:picture {asset = "img/photo.JPEG"}
s:picture {asset = "photo.webp"}
s:picture {asset = "anim.gif"}
s:text {text = "not an image.png\\nreally"}`, "lua");
    assert.deepEqual(imageNames(scanned), ["earth.png", "img/LOGO.SVG"]);
    assert.equal(imageMime("img/LOGO.SVG"), "svg");
    assert.equal(imageMime("earth.png"), "png");
});

test("sounds are WAV literals in Lua cues and retained runtime calls", () => {
    const scanned = scanSource(String.raw`
-- runtime:sound("comment.wav")
vis.audio.cue(scene, {asset = "tone.wav"})
runtime:sound("sounds/CLICK.WAV", {gain = 0.25})
runtime:sound("tone.wav")
runtime:sound("music.mp3")
runtime:sound("music.flac")
runtime:sound("music.ogg")
scene:text {text = "not a sound.wav\nreally"}`, "lua");
    assert.deepEqual(audioNames(scanned), ["tone.wav", "sounds/CLICK.WAV"]);
    assert.equal(audioMime("sounds/CLICK.WAV"), "audio/wav");
    assert.equal(audioMime("music.mp3"), "");
});

test("JavaScript cue audio names decode escaped paths and skip comments", () => {
    const scanned = scanSource(String.raw`
// scene.sound({asset: "comment.wav"});
scene.sound({asset: "sounds/\u0074one.wav"});
scene.sound({asset: 'sounds/tone.wav'});
` + "scene.sound({asset: `click.wav`});", "js");
    assert.deepEqual(audioNames(scanned), ["sounds/tone.wav", "click.wav"]);
});

test("a scene calls into vis outside comments", () => {
    assert.ok(looksLikeScene(scanSource("return vis.scene {}", "lua")));
    assert.ok(looksLikeScene(scanSource("return vis . scene({})", "js")));
    assert.ok(!looksLikeScene(scanSource("-- vis.scene\nprint(1)", "lua")));
    assert.ok(!looksLikeScene(scanSource("print('vis.scene')", "lua")));
});

