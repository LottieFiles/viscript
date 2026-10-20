// Fonts by name, as ThorVG's web players load them. The engine draws text in the font registered
// under the name a scene sets it in, and in its own default font, "default" (Latin, Greek and math
// signs), when no font is; a font never fails a scene. The render server asks the host's
// FontProvider for each name a loaded scene uses that is not registered yet, registers the font it
// answers with, and loads the scene once more, so the scene shows in its fonts from its first
// frame. A provider has the shape of WebCanvas's (thorvg.web), and may answer null for a name it
// has no font for.

export type FontType = "ttf" | "otf";

/** A font's bytes, as WebCanvas's FontProviderResult. */
export interface FontProviderResult {
    readonly data: Uint8Array | ArrayBuffer;
    /** Read from the bytes when left out. */
    readonly type?: FontType;
}

export interface FontProvider {
    /**
     * The font to register under `name`, or null to leave text in that font to the engine's
     * default one. The render host asks once per name, until its fonts change (see
     * RenderHost.refreshFonts); a rejection is forgotten, so a later load asks again. Scenes
     * choose the names, so a provider must be safe to ask for any.
     */
    fetch(name: string, options?: Readonly<Record<string, unknown>>): Promise<FontProviderResult | null>;
}

/** The families the engine's themes set text in, which a host's provider answers for. */
export const THEME_FONTS = {
    /** Text and headings in every theme but adaptive_vscode, and the Korean letters of
     * adaptive_vscode's text, which Source Serif 4 lacks. */
    text: "Pretendard",
    /** Code, and the code text role, in every theme. */
    code: "JetBrainsMono",
    /** Text in adaptive_vscode. */
    serif: "Source Serif 4",
} as const;

/** "otf" for a font with CFF outlines, "ttf" for any other, from the font's first bytes. */
export function fontType(data: Uint8Array | ArrayBuffer): FontType {
    const bytes = data instanceof Uint8Array ? data : new Uint8Array(data);
    return bytes.length >= 4 && bytes[0] === 0x4f && bytes[1] === 0x54 && bytes[2] === 0x54 && bytes[3] === 0x4f ? "otf" : "ttf";
}

/** A font's URL, or a function that gives its bytes, or null when it has none after all. */
export type FontSource = string | URL | (() => Promise<Uint8Array | ArrayBuffer | FontProviderResult | null>);

async function download(url: string | URL): Promise<ArrayBuffer> {
    const response = await fetch(url);
    if (!response.ok) throw new Error(`Could not load the font ${url} (${response.status})`);
    return response.arrayBuffer();
}

function result(value: Uint8Array | ArrayBuffer | FontProviderResult | null): FontProviderResult | null {
    if (!value) return null;
    return value instanceof Uint8Array || value instanceof ArrayBuffer ? {data: value} : value;
}

/** The fonts of a fixed table, name → source, each fetched once, when a scene first needs it. */
export function fontTable(table: Readonly<Record<string, FontSource>>): FontProvider {
    const cache = new Map<string, Promise<FontProviderResult | null>>();
    return {
        fetch(name) {
            if (!Object.hasOwn(table, name)) return Promise.resolve(null);
            let pending = cache.get(name);
            if (!pending) {
                const source = table[name]!;
                pending = (typeof source === "function" ? source() : download(source)).then(result);
                cache.set(name, pending);
                // A failed download is tried again when the font is next asked for.
                pending.catch(() => { if (cache.get(name) === pending) cache.delete(name); });
            }
            return pending;
        },
    };
}

/**
 * Asks each provider in turn and answers with the first font one has. One that fails is passed
 * over, so the ones after it still answer; when none has a font, the first failure is the answer.
 */
export function chainFonts(...providers: readonly (FontProvider | null | undefined | false)[]): FontProvider {
    const chain = providers.filter((provider): provider is FontProvider => !!provider);
    return {
        async fetch(name, options) {
            let failure: unknown = null;
            for (const provider of chain) {
                try {
                    const found = await provider.fetch(name, options);
                    if (found) return found;
                } catch (error) {
                    failure ??= error;
                }
            }
            if (failure) throw failure;
            return null;
        },
    };
}

/**
 * The fonts of a runtime bundle, as scripts/sync-wasm.mjs writes it, from the folder `base`
 * (relative to the page): JetBrains Mono for code. Text the host gives no font for is drawn in the
 * engine's default font.
 */
export function runtimeFonts(base: string | URL): FontProvider {
    const folder = new URL(String(base).replace(/\/?$/, "/"), globalThis.location?.href);
    return fontTable({[THEME_FONTS.code]: new URL("JetBrainsMono.ttf", folder)});
}
