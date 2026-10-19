import assert from "node:assert/strict";
import {existsSync, readFileSync} from "node:fs";
import {open} from "node:fs/promises";
import test from "node:test";
import {createVis} from "../../skill/vis-skills/assets/wasm/client.js";
import {FontLibrary, type FontLocation} from "../src/fonts/library.ts";
import {extractFace, memoryFile, readCharset, readFaces, type FontFile} from "../src/fonts/sfnt.ts";

const bundle = new URL("../../skill/vis-skills/assets/wasm/", import.meta.url);
const wasmBinary = readFileSync(new URL("vis-wasm.wasm", bundle));
const MONO = new Uint8Array(readFileSync(new URL("JetBrainsMono.ttf", bundle)));
const STD = new Uint8Array(readFileSync(new URL("../../../src/examples/assets/PretendardStd-Regular.ttf", import.meta.url)));
const FULL = new Uint8Array(readFileSync(new URL("Pretendard.ttf", bundle)));

// Draws a line of text in the font registered as "Probe".
async function render(font: Uint8Array, text = "Probe AaGg 0123"): Promise<Buffer> {
    const engine = await createVis({wasmBinary});
    try {
        engine.font("Probe", font, "ttf");
        engine.load(`local s = vis.scene {width = 320, height = 60, theme = "pro_white"}
s:text {text = ${JSON.stringify(text)}, point = {0, 0}, font = "Probe", size = 20}
return s`, "probe.lua");
        return Buffer.from(engine.render(0));
    } finally {
        engine.destroy();
    }
}

// A collection of whole fonts, each face's tables moved to where its font landed.
function collection(fonts: Uint8Array[]): Uint8Array {
    let size = 12 + fonts.length * 4;
    const bases = fonts.map((font) => {
        const at = size;
        size += (font.length + 3) & ~3;
        return at;
    });
    const out = new Uint8Array(size);
    const data = new DataView(out.buffer);
    data.setUint32(0, 0x74746366);
    data.setUint16(4, 1);
    data.setUint32(8, fonts.length);
    fonts.forEach((font, i) => {
        const base = bases[i]!;
        data.setUint32(12 + i * 4, base);
        out.set(font, base);
        for (let t = 0; t < data.getUint16(base + 4); t++) {
            const at = base + 12 + t * 16 + 8;
            data.setUint32(at, data.getUint32(at) + base);
        }
    });
    return out;
}

// A font with `extra` tables added, as written by a tool that keeps whatever it is given.
async function withTables(font: Uint8Array, extra: Record<string, Uint8Array>): Promise<Uint8Array> {
    const [face] = await readFaces(memoryFile(font));
    const tables = new Map(face!.tables.map((table) => [table.tag, font.subarray(table.offset, table.offset + table.length)]));
    for (const [tag, bytes] of Object.entries(extra)) tables.set(tag, bytes);
    const tags = [...tables.keys()].sort();
    let size = 12 + tags.length * 16;
    const offsets = tags.map((tag) => {
        const at = size;
        size += (tables.get(tag)!.length + 3) & ~3;
        return at;
    });
    const out = new Uint8Array(size);
    const data = new DataView(out.buffer);
    data.setUint32(0, 0x00010000);
    data.setUint16(4, tags.length);
    tags.forEach((tag, i) => {
        for (let c = 0; c < 4; c++) out[12 + i * 16 + c] = tag.charCodeAt(c);
        data.setUint32(12 + i * 16 + 8, offsets[i]!);
        data.setUint32(12 + i * 16 + 12, tables.get(tag)!.length);
        out.set(tables.get(tag)!, offsets[i]!);
    });
    return out;
}

async function diskFile(path: string): Promise<FontFile & {close(): Promise<void>}> {
    const handle = await open(path, "r");
    const {size} = await handle.stat();
    return {
        size,
        read: async (offset, length) => {
            const bytes = new Uint8Array(length);
            await handle.read(bytes, 0, length, offset);
            return bytes;
        },
        close: () => handle.close(),
    };
}

test("names, weight, outlines and coverage of the bundled fonts", async () => {
    const [mono] = await readFaces(memoryFile(MONO));
    assert.equal(mono!.family, "JetBrains Mono NL");
    assert.equal(mono!.postscriptName, "JetBrainsMonoNL-Regular");
    assert.equal(mono!.weight, 400);
    assert.equal(mono!.italic, false);
    assert.equal(mono!.fixed, true);
    assert.equal(mono!.outlines, "truetype");
    assert.equal(mono!.collection, false);
    for (const [font, hangul] of [[STD, false], [FULL, true]] as const) {
        const file = memoryFile(font);
        const [face] = await readFaces(file);
        const has = await readCharset(file, face!);
        assert.equal(face!.fixed, false);
        assert.equal(has(0x41), true);
        assert.equal(has(0xac00), hangul);
        assert.equal(has(0x10ffff), false);
    }
});

test("a face is fixed-pitch when its post table or its PANOSE class says so", async () => {
    const fixed = async (font: Uint8Array) => (await readFaces(memoryFile(font)))[0]!.fixed;
    // The font with one byte of a table changed.
    const patched = async (font: Uint8Array, tag: string, at: number, write: (data: DataView, at: number) => void) => {
        const table = (await readFaces(memoryFile(font)))[0]!.tables.find((entry) => entry.tag === tag)!;
        const bytes = font.slice(table.offset, table.offset + table.length);
        write(new DataView(bytes.buffer), at);
        return withTables(font, {[tag]: bytes});
    };
    const pitch = (value: number) => (data: DataView, at: number) => data.setUint32(at, value);
    assert.equal(await fixed(await patched(STD, "post", 12, pitch(1))), true);
    const proportionalPost = await patched(MONO, "post", 12, pitch(0));
    assert.equal(await fixed(proportionalPost), true, "JetBrains Mono's PANOSE class is monospaced");
    assert.equal(await fixed(await patched(proportionalPost, "OS/2", 35, (data, at) => data.setUint8(at, 3))), false);
});

test("a file that is not a font is refused", async () => {
    await assert.rejects(readFaces(memoryFile(new TextEncoder().encode("<html>not a font</html>"))), /Not a/);
    await assert.rejects(readFaces(memoryFile(new Uint8Array(4))), /too short/);
});

test("a face taken out of a collection draws exactly like the font it came from", async () => {
    const file = memoryFile(collection([MONO, STD]));
    const faces = await readFaces(file);
    assert.deepEqual(faces.map((face) => [face.index, face.collection, face.family]),
        [[0, true, "JetBrains Mono NL"], [1, true, "Pretendard Std"]]);
    const extracted = await extractFace(file, faces[1]!);
    const [face] = await readFaces(memoryFile(extracted));
    assert.equal(face!.family, "Pretendard Std");
    const tags = face!.tables.map((table) => table.tag);
    assert.deepEqual(tags, [...tags].sort(), "the engine binary-searches the directory");
    assert.ok(!tags.includes("GPOS") && !tags.includes("GSUB"), "tables the engine does not read are left out");
    assert.ok(extracted.length < STD.length);
    const [original, copy, other] = await Promise.all([render(STD), render(extracted), render(await extractFace(file, faces[0]!))]);
    assert.ok(copy.equals(original));
    assert.ok(!other.equals(original), "the font drawn is the one registered");
});

test("an Apple-format kern table, which the engine rejects, is left out", async () => {
    // Version 1.0 as a 32-bit number, then no subtables.
    const apple = await withTables(STD, {kern: new Uint8Array([0, 1, 0, 0, 0, 0, 0, 0])});
    await assert.rejects(render(apple), /Font|font|Invalid/);
    const [face] = await readFaces(memoryFile(apple));
    const fixed = await extractFace(memoryFile(apple), face!);
    assert.ok(!(await readFaces(memoryFile(fixed)))[0]!.tables.some((table) => table.tag === "kern"));
    assert.ok((await render(fixed)).equals(await render(STD)));
    // The OpenType format stays.
    const open = await withTables(STD, {kern: new Uint8Array([0, 0, 0, 0])});
    const [openFace] = await readFaces(memoryFile(open));
    const kept = await extractFace(memoryFile(open), openFace!);
    assert.ok((await readFaces(memoryFile(kept)))[0]!.tables.some((table) => table.tag === "kern"));
});

test("a Unicode map labeled where the engine does not look is relabeled", async () => {
    // Only the format 4 map, labeled Unicode 1.1 as in some Apple fonts.
    const [face] = await readFaces(memoryFile(STD));
    const table = face!.tables.find((entry) => entry.tag === "cmap")!;
    const cmap = new DataView(STD.buffer, STD.byteOffset + table.offset, table.length);
    let at = 0;
    for (let i = 0; i < cmap.getUint16(2) && !at; i++) {
        const offset = cmap.getUint32(8 + i * 8);
        if (cmap.getUint16(offset) === 4) at = offset;
    }
    assert.ok(at);
    const map = STD.subarray(table.offset + at, table.offset + table.length);
    const relabeled = new Uint8Array(12 + map.length);
    const header = new DataView(relabeled.buffer);
    header.setUint16(2, 1);
    header.setUint16(6, 1);
    header.setUint32(8, 12);
    relabeled.set(map, 12);
    const apple = await withTables(STD, {cmap: relabeled});
    const [unfixed, blank] = await Promise.all([render(apple), render(STD, " ")]);
    assert.ok(unfixed.equals(blank), "the engine finds no glyphs");
    const [appleFace] = await readFaces(memoryFile(apple));
    assert.equal((await readCharset(memoryFile(apple), appleFace!))(0x41), true);
    const fixed = await extractFace(memoryFile(apple), appleFace!);
    assert.ok((await render(fixed)).equals(await render(STD)));
});

test("the library finds faces by family, full or PostScript name, the regular face first", async () => {
    const memory = (id: string, bytes: Uint8Array): FontLocation => ({id, open: async () => memoryFile(bytes)});
    const library = new FontLibrary();
    await library.add([memory("both.ttc", collection([MONO, STD])), memory("full.ttf", FULL),
        memory("broken.ttf", new Uint8Array(64))]);
    assert.equal(library.size, 3);
    assert.deepEqual(library.families(), ["JetBrains Mono NL", "Pretendard", "Pretendard Std"]);
    assert.equal(library.find("jetbrains mono nl")?.id, "both.ttc#0");
    assert.equal(library.find("PretendardStd-Regular")?.id, "both.ttc#1");
    assert.equal(library.find("Pretendard")?.id, "full.ttf");
    assert.equal(library.find("Nope"), undefined);
    assert.equal(await library.covers("full.ttf", [0xac00, 0x41]), true);
    assert.equal(await library.covers("both.ttc#1", [0xac00]), false);
    const bytes = new Uint8Array(await library.bytes("both.ttc#1"));
    assert.equal((await readFaces(memoryFile(bytes)))[0]!.family, "Pretendard Std");
});

const SYSTEM = ["/System/Library/Fonts/Menlo.ttc", "/System/Library/Fonts/Supplemental/AppleGothic.ttf",
    "/System/Library/Fonts/HelveticaNeue.ttc"];

test("installed fonts: collection faces and Apple fonts load in the engine once extracted", {skip: !SYSTEM.every(existsSync)}, async () => {
    const library = new FontLibrary();
    await library.add(SYSTEM.map((path) => ({id: path, open: () => diskFile(path)})));
    const menlo = library.find("Menlo")!;
    assert.equal(menlo.face.postscriptName, "Menlo-Regular");
    const gothic = library.find("AppleGothic")!;
    assert.equal(await library.covers(gothic.id, [0xac00]), true);
    // Helvetica Neue labels its Unicode map (0,1), which the engine does not read unless relabeled.
    const neue = library.find("Helvetica Neue")!;
    assert.equal(neue.face.postscriptName, "HelveticaNeue");
    // All load, and draw differently from each other and from a bundled font.
    const drawn = await Promise.all([menlo, gothic, neue].map(async (entry) => render(new Uint8Array(await library.bytes(entry.id)), "Menlo 가나다")));
    const others = [...drawn, await render(STD, "Menlo 가나다"), await render(STD, " ")];
    for (const [i, a] of others.entries()) for (const b of others.slice(i + 1)) assert.ok(!a.equals(b));
});
