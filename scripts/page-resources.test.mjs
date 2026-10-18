// Which fonts and images the page gives a scene (docs/page/resources.js).
// Usage: node --test scripts/page-resources.test.mjs
import assert from "node:assert/strict";
import {existsSync} from "node:fs";
import test from "node:test";
import {ASSETS, assetsFor, FONTS, FULL_PRETENDARD, needsFullPretendard} from "../docs/page/resources.js";

const page = new URL("../docs/page/", import.meta.url);

test("the page has a font for each name the engine's themes set text in", () => {
    for (const name of ["Pretendard", "JetBrainsMono", "Source Serif 4", "IBM Plex Sans KR"]) assert.ok(FONTS.has(name), name);
});

test("every font and image the page names ships with it, under the page", () => {
    for (const path of [...FONTS.values(), FULL_PRETENDARD, ...[...ASSETS.values()].map(({path}) => path)]) {
        assert.ok(!path.startsWith("/"), path);
        assert.ok(existsSync(new URL(path, page)), path);
    }
});

test("Hangul, CJK and character escapes need the full Pretendard", () => {
    for (const source of ['text = "한글"', 'text = "。"', 'text = "\\u{AC00}"', "utf8.char(44032)", "String.fromCodePoint(44032)"]) {
        assert.ok(needsFullPretendard(source), source);
    }
    assert.ok(!needsFullPretendard('TEXT {text = "hi → θ", role = "code"}'));
});

test("a scene gets the images it names", () => {
    assert.deepEqual(assetsFor('picture {asset = "earth.png"}'), ["earth.png"]);
    assert.deepEqual(assetsFor('text = "earth"'), []);
});
