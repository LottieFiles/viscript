// What a scene's source says about the images it loads, and whether it is a scene at all. Scenes
// name images in string literals, so the literals are read out of the code, past comments; nothing
// is run.
import type {SceneLanguage} from "../../player/src/protocol.ts";

export interface ScannedSource {
    /** Each string literal's text, escapes decoded. */
    readonly literals: readonly string[];
    /** The source with comments and the literals' text blanked out, the same length as the source. */
    readonly code: string;
}

// What a scan collects, in either language.
class Scan {
    readonly #literals: string[] = [];
    readonly #code: string[];

    constructor(source: string) {
        this.#code = source.split("");
    }

    /** Blanks `from` to `to`, keeping line breaks. */
    blank(from: number, to: number): void {
        for (let i = from; i < to; i++) if (this.#code[i] !== "\n") this.#code[i] = " ";
    }

    /** A literal whose opening delimiter is at `start`, its text running to `end`. */
    literal(text: string, start: number, end: number): void {
        this.#literals.push(text);
        this.blank(start + 1, end);
    }

    result(): ScannedSource {
        return {literals: this.#literals, code: this.#code.join("")};
    }
}

type Escaped = {text: string; end: number};

const SIMPLE: Readonly<Record<string, string>> = {
    a: "\x07", b: "\b", f: "\f", n: "\n", r: "\r", t: "\t", v: "\v", "0": "\0", "\\": "\\", "\"": "\"", "'": "'",
    "`": "`",
};

const point = (value: number): string => value <= 0x10ffff ? String.fromCodePoint(value) : "";
const hexAt = (source: string, at: number, length: number) =>
    new RegExp(`^[0-9a-fA-F]{${length}}`).exec(source.slice(at, at + length))?.[0];
const bracedAt = (source: string, at: number) => /^\{([0-9a-fA-F]{1,8})\}/.exec(source.slice(at, at + 10));

// \xHH, \uHHHH and \u{H...}; a backslash before a line break continues the line.
function jsEscape(source: string, at: number): Escaped {
    const next = source[at + 1] ?? "";
    if (next === "x") {
        const hex = hexAt(source, at + 2, 2);
        if (hex) return {text: String.fromCharCode(Number.parseInt(hex, 16)), end: at + 4};
    } else if (next === "u") {
        const braced = bracedAt(source, at + 2);
        if (braced) return {text: point(Number.parseInt(braced[1]!, 16)), end: at + 2 + braced[0].length};
        const hex = hexAt(source, at + 2, 4);
        if (hex) return {text: String.fromCharCode(Number.parseInt(hex, 16)), end: at + 6};
    } else if (next === "\r" || next === "\n") {
        return {text: "", end: at + (source.startsWith("\r\n", at + 1) ? 3 : 2)};
    }
    return {text: SIMPLE[next] ?? next, end: at + 2};
}

function quoted(source: string, at: number): Escaped {
    const quote = source[at];
    let text = "", i = at + 1;
    while (i < source.length && source[i] !== quote && source[i] !== "\n") {
        if (source[i] === "\\") {
            const escaped = jsEscape(source, i);
            text += escaped.text;
            i = escaped.end;
        } else {
            text += source[i++];
        }
    }
    return {text, end: Math.min(i + 1, source.length)};
}

const utf8 = new TextEncoder(), fromUtf8 = new TextDecoder();

// \xHH and \ddd are one byte each, \u{H...} is a code point's UTF-8, \z skips white space and a
// backslash before a line break is a line break.
function luaEscape(source: string, at: number): {bytes: ArrayLike<number>; end: number} {
    const next = source[at + 1] ?? "";
    if (next === "x") {
        const hex = hexAt(source, at + 2, 2);
        if (hex) return {bytes: [Number.parseInt(hex, 16)], end: at + 4};
    } else if (next === "u") {
        const braced = bracedAt(source, at + 2);
        if (braced) return {bytes: utf8.encode(point(Number.parseInt(braced[1]!, 16))), end: at + 2 + braced[0].length};
    } else if (/[0-9]/.test(next)) {
        const digits = /^[0-9]{1,3}/.exec(source.slice(at + 1, at + 4))![0];
        return {bytes: [Number(digits) & 0xff], end: at + 1 + digits.length};
    } else if (next === "z") {
        let end = at + 2;
        while (/\s/.test(source[end] ?? "")) end++;
        return {bytes: [], end};
    } else if (next === "\r" || next === "\n") {
        const pair = source[at + 2] !== next && (source[at + 2] === "\r" || source[at + 2] === "\n");
        return {bytes: [0x0a], end: at + (pair ? 3 : 2)};
    }
    return {bytes: utf8.encode(SIMPLE[next] ?? next), end: at + 2};
}

// Lua strings are bytes, which scenes draw as UTF-8: "\234\176\128" is 가.
function luaQuoted(source: string, at: number): Escaped {
    const quote = source[at];
    const bytes: number[] = [];
    let i = at + 1;
    while (i < source.length && source[i] !== quote && source[i] !== "\n") {
        if (source[i] === "\\") {
            const escaped = luaEscape(source, i);
            for (let k = 0; k < escaped.bytes.length; k++) bytes.push(escaped.bytes[k]!);
            i = escaped.end;
        } else {
            const character = String.fromCodePoint(source.codePointAt(i)!);
            for (const byte of utf8.encode(character)) bytes.push(byte);
            i += character.length;
        }
    }
    return {text: fromUtf8.decode(Uint8Array.from(bytes)), end: Math.min(i + 1, source.length)};
}

// The level of a Lua long bracket opening at `at` ("[[" is 0, "[==[" is 2), or -1.
function longLevel(source: string, at: number): number {
    if (source[at] !== "[") return -1;
    let level = 0;
    while (source[at + 1 + level] === "=") level++;
    return source[at + 1 + level] === "[" ? level : -1;
}

function longString(source: string, at: number, level: number): Escaped {
    const open = at + level + 2, close = `]${"=".repeat(level)}]`;
    const found = source.indexOf(close, open);
    const end = found < 0 ? source.length : found;
    // A newline right after the opening bracket is not part of the string.
    return {text: source.slice(open, end).replace(/^\r?\n/, ""), end: found < 0 ? end : end + close.length};
}

function scanLua(source: string): ScannedSource {
    const scan = new Scan(source);
    let i = 0;
    while (i < source.length) {
        const c = source[i];
        if (c === "-" && source[i + 1] === "-") {
            const level = longLevel(source, i + 2);
            const end = level >= 0 ? longString(source, i + 2, level).end : (source.indexOf("\n", i) + 1 || source.length);
            scan.blank(i, end);
            i = end;
        } else if (c === "[" && longLevel(source, i) >= 0) {
            const {text, end} = longString(source, i, longLevel(source, i));
            scan.literal(text, i, end);
            i = end;
        } else if (c === "\"" || c === "'") {
            const {text, end} = luaQuoted(source, i);
            scan.literal(text, i, end);
            i = end;
        } else {
            i++;
        }
    }
    return scan.result();
}

function scanJs(source: string): ScannedSource {
    const scan = new Scan(source);
    // Template literals nest through ${...}; each open template keeps the brace depth it resumes at.
    const templates: number[] = [];
    let depth = 0, i = 0;
    // `from` is just past the backquote, or the brace that closes an interpolation.
    const template = (from: number) => {
        let text = "", at = from;
        while (at < source.length && source[at] !== "`") {
            if (source[at] === "\\") {
                const escaped = jsEscape(source, at);
                text += escaped.text;
                at = escaped.end;
            } else if (source[at] === "$" && source[at + 1] === "{") {
                scan.literal(text, from - 1, at);
                templates.push(depth);
                depth++;
                return at + 2;
            } else {
                text += source[at++];
            }
        }
        scan.literal(text, from - 1, at);
        return Math.min(at + 1, source.length);
    };
    while (i < source.length) {
        const c = source[i];
        if (c === "/" && source[i + 1] === "/") {
            const end = source.indexOf("\n", i) + 1 || source.length;
            scan.blank(i, end);
            i = end;
        } else if (c === "/" && source[i + 1] === "*") {
            const found = source.indexOf("*/", i + 2);
            const end = found < 0 ? source.length : found + 2;
            scan.blank(i, end);
            i = end;
        } else if (c === "\"" || c === "'") {
            const {text, end} = quoted(source, i);
            scan.literal(text, i, end);
            i = end;
        } else if (c === "`") {
            i = template(i + 1);
        } else if (c === "{") {
            depth++;
            i++;
        } else if (c === "}") {
            depth--;
            i++;
            if (templates.length && depth === templates[templates.length - 1]) {
                templates.pop();
                i = template(i);
            }
        } else {
            i++;
        }
    }
    return scan.result();
}

export function scanSource(source: string, language: SceneLanguage): ScannedSource {
    return language === "js" ? scanJs(source) : scanLua(source);
}

// The image types the runtime decodes.
const IMAGE = /\.(png|svg)$/i;

/** Images the scene loads by name: literals that are a file name or path ending in .png or .svg. */
export function imageNames(scanned: ScannedSource): string[] {
    return [...new Set(scanned.literals.filter((literal) => IMAGE.test(literal) && !/[\n"<>|]/.test(literal)))];
}

/** The runtime's type name for an image file, from its extension. */
export function imageMime(name: string): string {
    return IMAGE.exec(name)?.[1]?.toLowerCase() ?? "";
}

/** Whether the source looks like a viscript scene at all: it calls into `vis`. */
export function looksLikeScene(scanned: ScannedSource): boolean {
    return /\bvis\s*\.\s*[A-Za-z_]/.test(scanned.code);
}
