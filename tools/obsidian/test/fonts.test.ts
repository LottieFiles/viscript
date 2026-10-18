import assert from "node:assert/strict";
import test from "node:test";
import {fontFileName, fontProvider, namedFile, type FontSettings, type FontSources, type ThemeRole} from "../src/fonts.ts";

const SETTINGS: FontSettings = {textFont: "default", textFontName: "", codeFont: "default", codeFontName: "", downloadFonts: true};
const THEME = ["Pretendard", "JetBrainsMono", "Source Serif 4", "IBM Plex Sans KR"];

interface Fixture {
    folder?: readonly string[];
    /** Families Fontsource has; "Offline" fails to arrive. */
    fontsource?: readonly string[];
}

// Sources whose fonts are their names, in bytes, so an answer says where it came from.
function sources({folder = [], fontsource = []}: Fixture = {}) {
    const asked: string[] = [];
    const font = (source: string) => ({data: new TextEncoder().encode(source)});
    const sources: FontSources = {
        folder: async (name) => folder.includes(name) ? font(`folder:${name}`) : null,
        download: async (name) => {
            asked.push(name);
            if (name === "Offline") throw new Error("Fontsource answered 503");
            return fontsource.includes(name) ? font(`fontsource:${name}`) : null;
        },
        bundled: async (role: ThemeRole) => font(`bundled:${role}`),
    };
    return {sources, asked};
}

async function answers(settings: Partial<FontSettings>, fixture: Fixture, names: readonly string[]) {
    const {sources: from, asked} = sources(fixture);
    const provider = fontProvider(() => ({...SETTINGS, ...settings}), from);
    const found: Record<string, string> = {};
    for (const name of names) {
        const answer = await provider.fetch(name);
        found[name] = answer ? new TextDecoder().decode(answer.data as Uint8Array) : "none";
    }
    return {found, downloaded: asked};
}

test("the theme's names take the bundled fonts, whatever else the vault or Fontsource has", async () => {
    const {found, downloaded} = await answers({}, {folder: THEME, fontsource: THEME}, THEME);
    assert.deepEqual(found, {
        Pretendard: "bundled:text", JetBrainsMono: "bundled:code",
        "Source Serif 4": "bundled:serif", "IBM Plex Sans KR": "bundled:serifKorean",
    });
    assert.deepEqual(downloaded, []);
});

test("a font named in the settings is looked for like one a scene names, else the bundled one stays", async () => {
    const fixture = {folder: ["Fira Code"], fontsource: ["Noto Sans KR"]};
    const {found} = await answers({textFont: "custom", textFontName: " Noto Sans KR ", codeFont: "custom", codeFontName: "Fira Code"},
        fixture, THEME);
    // adaptive_vscode's fonts are not the text font's to replace.
    assert.deepEqual(found, {
        Pretendard: "fontsource:Noto Sans KR", JetBrainsMono: "folder:Fira Code",
        "Source Serif 4": "bundled:serif", "IBM Plex Sans KR": "bundled:serifKorean",
    });
    const missing = await answers({codeFont: "custom", codeFontName: "Nowhere Mono"}, fixture, ["JetBrainsMono"]);
    assert.equal(missing.found.JetBrainsMono, "bundled:code");
    // An empty name is the default.
    assert.equal((await answers({textFont: "custom"}, fixture, ["Pretendard"])).found.Pretendard, "bundled:text");
});

test("a font a scene names comes from the folder, then Fontsource", async () => {
    const fixture = {folder: ["Noto Serif KR"], fontsource: ["Noto Serif KR", "Lobster"]};
    const {found, downloaded} = await answers({}, fixture, ["Noto Serif KR", "Lobster"]);
    assert.deepEqual(found, {"Noto Serif KR": "folder:Noto Serif KR", Lobster: "fontsource:Lobster"});
    assert.deepEqual(downloaded, ["Lobster"]);
});

test("a font that is nowhere is drawn in the text font, or the code font when it reads as monospaced", async () => {
    const {found} = await answers({}, {}, ["No Such Sans", "No Such Mono", "Fira Code"]);
    assert.deepEqual(found, {"No Such Sans": "bundled:text", "No Such Mono": "bundled:code", "Fira Code": "bundled:code"});
    // The text font as set.
    const inter = await answers({textFont: "custom", textFontName: "Inter"}, {folder: ["Inter"]}, ["No Such Sans"]);
    assert.equal(inter.found["No Such Sans"], "folder:Inter");
    // With downloads off, Fontsource is not asked.
    const off = await answers({downloadFonts: false}, {fontsource: ["Lobster"]}, ["Lobster"]);
    assert.deepEqual(off, {found: {Lobster: "bundled:text"}, downloaded: []});
});

test("a download that fails leaves the text in the text font", async (t) => {
    t.mock.method(console, "warn", () => undefined);
    const {found} = await answers({textFont: "custom", textFontName: "Offline"}, {}, ["Offline", "Pretendard"]);
    assert.deepEqual(found, {Offline: "bundled:text", Pretendard: "bundled:text"});
});

test("a downloaded font is saved under its name, and found by its file's name", () => {
    assert.equal(fontFileName("Noto Sans KR"), "Noto Sans KR.ttf");
    assert.equal(fontFileName("../a/b:c"), "a-b-c.ttf");
    assert.equal(fontFileName("..."), "font.ttf");
    assert.ok(namedFile("Fonts/Noto Sans KR.ttf", "noto sans kr"));
    assert.ok(namedFile("Fonts/sub/NotoSans-KR.TTF", "Noto Sans KR"));
    assert.ok(!namedFile("Fonts/NotoSansKR-Regular.ttf", "Noto Sans KR"));
    assert.ok(!namedFile("Fonts/.ttf", ""));
});
