// What a host needs from TrueType and OpenType files to offer installed or user fonts to the
// engine: names, weight, outline format, character coverage, and single faces taken out of font
// collections (.ttc). The engine draws TrueType (glyf) and CFF outlines, though not CFF2, from a
// plain sfnt whose table directory is sorted, and rejects a `kern` table in Apple's format.

/** Random access to a font file, so large collections are not read whole to list them. */
export interface FontFile {
    readonly size: number;
    read(offset: number, length: number): Promise<Uint8Array>;
}

export function memoryFile(data: ArrayBuffer | Uint8Array): FontFile {
    const bytes = data instanceof Uint8Array ? data : new Uint8Array(data);
    return {
        size: bytes.byteLength,
        read: async (offset, length) => {
            if (offset < 0 || offset + length > bytes.byteLength) throw new RangeError("Read past the end of the font");
            return bytes.subarray(offset, offset + length);
        },
    };
}

export interface TableRecord {
    readonly tag: string;
    readonly checksum: number;
    readonly offset: number;
    readonly length: number;
}

export type Outlines = "truetype" | "cff" | "other";

export interface FontFace {
    /** The face's place in a collection; 0 in a single font. */
    readonly index: number;
    readonly collection: boolean;
    /** The typographic family, else the family, in English where the font has it. */
    readonly family: string;
    readonly subfamily: string;
    readonly fullName: string;
    readonly postscriptName: string;
    /** Every family, full and PostScript name, in every language the font gives. */
    readonly names: readonly string[];
    /** OS/2 weight class: 400 is regular, 700 bold. */
    readonly weight: number;
    readonly italic: boolean;
    /** The font says its glyphs share one width, in `post` or its PANOSE class, as coding fonts do. */
    readonly fixed: boolean;
    /** The engine draws "truetype" (glyf) and "cff" outlines; "other" includes CFF2. */
    readonly outlines: Outlines;
    readonly tables: readonly TableRecord[];
}

const TTCF = 0x74746366, TRUE = 0x74727565, OTTO = 0x4f54544f, SFNT = 0x00010000;

const view = (bytes: Uint8Array) => new DataView(bytes.buffer, bytes.byteOffset, bytes.byteLength);

async function readAt(file: FontFile, offset: number, length: number): Promise<DataView> {
    if (offset < 0 || length < 0 || offset + length > file.size) throw new Error("Not a font: a table lies outside the file");
    return view(await file.read(offset, length));
}

const tagOf = (data: DataView, at: number) =>
    String.fromCharCode(data.getUint8(at), data.getUint8(at + 1), data.getUint8(at + 2), data.getUint8(at + 3));

async function readDirectory(file: FontFile, offset: number): Promise<TableRecord[]> {
    const header = await readAt(file, offset, 12);
    const version = header.getUint32(0);
    if (version !== SFNT && version !== TRUE && version !== OTTO) throw new Error("Not a TrueType or OpenType font");
    const count = header.getUint16(4);
    const records = await readAt(file, offset + 12, count * 16);
    const tables: TableRecord[] = [];
    for (let i = 0; i < count; i++) {
        const at = i * 16;
        const table = {tag: tagOf(records, at), checksum: records.getUint32(at + 4),
            offset: records.getUint32(at + 8), length: records.getUint32(at + 12)};
        if (table.offset + table.length > file.size) throw new Error(`Not a font: the ${table.tag} table lies outside the file`);
        tables.push(table);
    }
    return tables;
}

function decode(bytes: Uint8Array, platform: number, encoding: number): string | null {
    // Unicode, or Windows symbol, BMP and full repertoire: UTF-16BE.
    if (platform === 0 || (platform === 3 && (encoding === 0 || encoding === 1 || encoding === 10))) {
        let text = "";
        for (let i = 0; i + 1 < bytes.length; i += 2) text += String.fromCharCode(bytes[i]! << 8 | bytes[i + 1]!);
        return text;
    }
    // Mac Roman, which names keep to ASCII in practice.
    if (platform === 1 && encoding === 0) return String.fromCharCode(...bytes);
    return null;
}

interface NameRecord {
    readonly id: number;
    readonly text: string;
    /** Lower is preferred: Windows US English, other Windows, Unicode, Mac English. */
    readonly rank: number;
}

function readNames(data: DataView): NameRecord[] {
    const count = data.getUint16(2), storage = data.getUint16(4);
    const records: NameRecord[] = [];
    for (let i = 0; i < count; i++) {
        const at = 6 + i * 12;
        if (at + 12 > data.byteLength) break;
        const platform = data.getUint16(at), encoding = data.getUint16(at + 2), language = data.getUint16(at + 4);
        const id = data.getUint16(at + 6), length = data.getUint16(at + 8), offset = data.getUint16(at + 10);
        if (![1, 2, 4, 6, 16, 17].includes(id) || storage + offset + length > data.byteLength) continue;
        const text = decode(new Uint8Array(data.buffer, data.byteOffset + storage + offset, length), platform, encoding);
        if (!text) continue;
        const rank = platform === 3 ? (language === 0x409 ? 0 : 1) : platform === 0 ? 2 : language === 0 ? 3 : 4;
        records.push({id, text: text.replace(/\0/g, "").trim(), rank});
    }
    return records;
}

function preferred(names: readonly NameRecord[], ...ids: number[]): string {
    for (const id of ids) {
        let best: NameRecord | undefined;
        for (const name of names) if (name.id === id && name.text && (!best || name.rank < best.rank)) best = name;
        if (best) return best.text;
    }
    return "";
}

async function readFace(file: FontFile, offset: number, index: number, collection: boolean): Promise<FontFace> {
    const tables = await readDirectory(file, offset);
    const find = (tag: string) => tables.find((table) => table.tag === tag);
    const name = find("name"), os2 = find("OS/2"), head = find("head"), post = find("post");
    const names = name ? readNames(await readAt(file, name.offset, name.length)) : [];
    let weight = 400, italic = false;
    // post.isFixedPitch, or a Latin text face whose PANOSE proportion is monospaced.
    let fixed = post !== undefined && post.length >= 16 && (await readAt(file, post.offset + 12, 4)).getUint32(0) !== 0;
    if (os2 && os2.length >= 64) {
        const data = await readAt(file, os2.offset, 64);
        weight = data.getUint16(4) || 400;
        italic = (data.getUint16(62) & 1) !== 0;
        fixed ||= data.getUint8(32) === 2 && data.getUint8(35) === 9;
    } else if (head && head.length >= 46) {
        const style = (await readAt(file, head.offset + 44, 2)).getUint16(0);
        weight = style & 1 ? 700 : 400;
        italic = (style & 2) !== 0;
    }
    const outlines: Outlines = find("glyf") && find("loca") ? "truetype" : find("CFF ") ? "cff" : "other";
    const family = preferred(names, 16, 1);
    return {
        index, collection, family,
        subfamily: preferred(names, 17, 2),
        fullName: preferred(names, 4) || family,
        postscriptName: preferred(names, 6),
        names: [...new Set(names.filter((record) => record.id !== 2 && record.id !== 17).map((record) => record.text))],
        weight, italic, fixed, outlines, tables,
    };
}

/** The faces in a font or font collection. Throws for a file that is not one. */
export async function readFaces(file: FontFile): Promise<FontFace[]> {
    if (file.size < 12) throw new Error("Not a font: the file is too short");
    const header = await readAt(file, 0, 12);
    if (header.getUint32(0) !== TTCF) return [await readFace(file, 0, 0, false)];
    const count = header.getUint32(8);
    const offsets = await readAt(file, 12, count * 4);
    const faces: FontFace[] = [];
    for (let i = 0; i < count; i++) faces.push(await readFace(file, offsets.getUint32(i * 4), i, true));
    return faces;
}

// The tables the engine reads, and small ones that keep the file identifiable.
const RENDER_TABLES = new Set(["CFF ", "cmap", "glyf", "head", "hhea", "hmtx", "kern", "loca", "maxp", "name", "OS/2", "post"]);

/** The largest font file the engine registers. */
export const MAX_FONT_BYTES = 32 * 1024 * 1024;

/** About how large extractFace() makes `face`, before table padding and fixes. */
export function extractedSize(face: FontFace, {renderOnly = true}: ExtractOptions = {}): number {
    const tables = face.tables.filter((table) => !renderOnly || RENDER_TABLES.has(table.tag));
    return tables.reduce((size, table) => size + table.length, 12 + tables.length * 16);
}

export interface ExtractOptions {
    /** Keep only the tables the engine reads, which can make CJK fonts several MB smaller. True unless given. */
    readonly renderOnly?: boolean;
}

/** The Unicode map to read: format 12 when the font has one, else format 4. */
function unicodeSubtable(data: DataView): {at: number; format: 4 | 12} | null {
    const count = data.getUint16(2);
    let best: {at: number; format: 4 | 12} | null = null;
    for (let i = 0; i < count && 4 + i * 8 + 8 <= data.byteLength; i++) {
        const platform = data.getUint16(4 + i * 8), encoding = data.getUint16(6 + i * 8), at = data.getUint32(8 + i * 8);
        if (at + 4 > data.byteLength) continue;
        const unicode = platform === 0 || (platform === 3 && (encoding === 1 || encoding === 10));
        const format = data.getUint16(at);
        if (!unicode || (format !== 4 && format !== 12)) continue;
        if (!best || (format === 12 && best.format === 4)) best = {at, format};
    }
    return best;
}

// The engine looks for its Unicode map under (3,1) or (0,3) for format 4 and (3,10) or (0,4) for
// format 12 only, so a font that labels it otherwise, as the Unicode 1.1 (0,1) of some Apple
// fonts, draws nothing. The extracted font keeps that one map, labeled as the engine expects.
function engineCmap(bytes: Uint8Array): Uint8Array {
    const data = view(bytes);
    if (bytes.byteLength < 4) return bytes;
    const chosen = unicodeSubtable(data);
    if (!chosen) return bytes;
    // A format 4 length is 16 bits and overflows in large fonts, so the rest of the table goes.
    const length = chosen.format === 12 ? Math.min(data.getUint32(chosen.at + 4), bytes.byteLength - chosen.at)
        : bytes.byteLength - chosen.at;
    const out = new Uint8Array(12 + length);
    const header = view(out);
    header.setUint16(2, 1);
    header.setUint16(4, 3);
    header.setUint16(6, chosen.format === 12 ? 10 : 1);
    header.setUint32(8, 12);
    out.set(bytes.subarray(chosen.at, chosen.at + length), 12);
    return out;
}

function checksum(bytes: Uint8Array): number {
    const data = view(bytes);
    let sum = 0, at = 0;
    for (; at + 4 <= bytes.byteLength; at += 4) sum = (sum + data.getUint32(at)) >>> 0;
    let last = 0;
    for (let shift = 24; at < bytes.byteLength; at++, shift -= 8) last |= bytes[at]! << shift;
    return (sum + (last >>> 0)) >>> 0;
}

/**
 * `face` as a standalone font file the engine can load: out of its collection, with a sorted
 * table directory, its Unicode map where the engine looks for it, and without an Apple-format
 * `kern` table, which the engine would reject.
 */
export async function extractFace(file: FontFile, face: FontFace, {renderOnly = true}: ExtractOptions = {}): Promise<Uint8Array> {
    const chosen = face.tables.filter((table) => !renderOnly || RENDER_TABLES.has(table.tag));
    const loaded = await Promise.all(chosen.map(async (table) => {
        const data = await file.read(table.offset, table.length);
        return {table, data: table.tag === "cmap" ? engineCmap(data) : data};
    }));
    const tables = loaded
        .filter(({table, data}) => table.tag !== "kern" || (data.byteLength >= 2 && view(data).getUint16(0) === 0))
        // Sorted as big-endian tags, which the engine binary-searches.
        .sort((a, b) => a.table.tag < b.table.tag ? -1 : a.table.tag > b.table.tag ? 1 : 0);
    const count = tables.length;
    let size = 12 + count * 16;
    const offsets = tables.map(({data}) => {
        const at = size;
        size += (data.byteLength + 3) & ~3;
        return at;
    });
    const out = new Uint8Array(size);
    const data = view(out);
    const selector = Math.floor(Math.log2(Math.max(1, count)));
    data.setUint32(0, face.outlines === "cff" ? OTTO : SFNT);
    data.setUint16(4, count);
    data.setUint16(6, (1 << selector) * 16);
    data.setUint16(8, selector);
    data.setUint16(10, count * 16 - (1 << selector) * 16);
    tables.forEach(({table, data: bytes}, i) => {
        const at = 12 + i * 16;
        for (let c = 0; c < 4; c++) data.setUint8(at + c, table.tag.charCodeAt(c));
        data.setUint32(at + 4, checksum(bytes));
        data.setUint32(at + 8, offsets[i]!);
        data.setUint32(at + 12, bytes.byteLength);
        out.set(bytes, offsets[i]!);
    });
    return out;
}

/** Tells whether the face maps a code point to a glyph. */
export type Charset = (codePoint: number) => boolean;

function format12(data: DataView, at: number): Charset {
    const groups = Math.min(data.getUint32(at + 12), Math.floor((data.byteLength - at - 16) / 12));
    return (codePoint) => {
        let low = 0, high = groups - 1;
        while (low <= high) {
            const mid = (low + high) >> 1, group = at + 16 + mid * 12;
            if (codePoint < data.getUint32(group)) high = mid - 1;
            else if (codePoint > data.getUint32(group + 4)) low = mid + 1;
            else return data.getUint32(group + 8) + codePoint - data.getUint32(group) !== 0;
        }
        return false;
    };
}

function format4(data: DataView, at: number): Charset {
    const segments = data.getUint16(at + 6) / 2;
    const ends = at + 14, starts = ends + segments * 2 + 2, deltas = starts + segments * 2, ranges = deltas + segments * 2;
    if (ranges + segments * 2 > data.byteLength) return () => false;
    return (codePoint) => {
        if (codePoint > 0xffff) return false;
        let low = 0, high = segments - 1;
        while (low <= high) {
            const mid = (low + high) >> 1;
            if (codePoint > data.getUint16(ends + mid * 2)) low = mid + 1;
            else if (codePoint < data.getUint16(starts + mid * 2)) high = mid - 1;
            else {
                const range = data.getUint16(ranges + mid * 2);
                const delta = data.getUint16(deltas + mid * 2);
                if (range === 0) return ((codePoint + delta) & 0xffff) !== 0;
                const glyph = ranges + mid * 2 + range + (codePoint - data.getUint16(starts + mid * 2)) * 2;
                if (glyph + 2 > data.byteLength) return false;
                const id = data.getUint16(glyph);
                return id !== 0 && ((id + delta) & 0xffff) !== 0;
            }
        }
        return false;
    };
}

/** The face's Unicode coverage, from its cmap (formats 4 and 12, as the engine reads it). */
export async function readCharset(file: FontFile, face: FontFace): Promise<Charset> {
    const cmap = face.tables.find((table) => table.tag === "cmap");
    if (!cmap) return () => false;
    const data = await readAt(file, cmap.offset, cmap.length);
    if (data.byteLength < 4) return () => false;
    const best = unicodeSubtable(data);
    if (!best) return () => false;
    return best.format === 12 ? format12(data, best.at) : format4(data, best.at);
}
