// The runtime, the render server, the compiler and the bundled fonts, unpacked from main.js on
// first use.
import * as embedded from "viscript:embedded";
import type {ThemeRole} from "./fonts.ts";

async function inflate(base64: string): Promise<ArrayBuffer> {
    const binary = atob(base64);
    const bytes = new Uint8Array(binary.length);
    for (let i = 0; i < binary.length; i++) bytes[i] = binary.charCodeAt(i);
    const stream = new Blob([bytes]).stream().pipeThrough(new DecompressionStream("gzip"));
    return new Response(stream).arrayBuffer();
}

const FONTS: Readonly<Record<ThemeRole, string>> = {
    text: embedded.textFont,
    code: embedded.codeFont,
};

let wasm: Promise<ArrayBuffer> | null = null;
let server: Promise<string> | null = null;
let compiler: Promise<string> | null = null;
const fonts = new Map<ThemeRole, Promise<ArrayBuffer>>();

const text = (bytes: ArrayBuffer) => new TextDecoder().decode(bytes);

// Each is kept: the render host copies the binary and the fonts before handing them to a worker,
// and asks again only when a worker is replaced or the fonts change.
export const runtimeWasm = (): Promise<ArrayBuffer> => wasm ??= inflate(embedded.wasm);
export const serverCode = (): Promise<string> => server ??= inflate(embedded.server).then(text);
export const compilerCode = (): Promise<string> => compiler ??= inflate(embedded.compiler).then(text);

/** The font the plugin ships for the role, unpacked when a scene first sets text in it. */
export function bundledFont(role: ThemeRole): Promise<ArrayBuffer> {
    let font = fonts.get(role);
    if (!font) fonts.set(role, font = inflate(FONTS[role]));
    return font;
}
