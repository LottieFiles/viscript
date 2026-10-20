// Which font a scene gets under each name it sets text in, as ThorVG's web players ask a
// FontProvider. The fonts the engine's themes set text in ship with the plugin, the files VS Code
// ships, so a scene draws the same on every computer, phone and tablet: "Pretendard" and
// "JetBrainsMono" take the text and code fonts the settings choose, the bundled ones unless another
// is named. Any other name a scene sets text in, as `font = "Noto Serif KR"`, is looked for in the
// vault's font folder, which syncs with the vault, then on Fontsource, which saves it in the
// folder; failing both, its text is drawn in the text font, or in the code font for a name that
// reads as a monospaced font's. Fonts installed on a device are never used, as they differ from one
// to the next.
import {THEME_FONTS, type FontProvider, type FontProviderResult} from "../../player/src/fonts/provider.ts";

/** The font the plugin ships (Pretendard or JetBrains Mono), or another one by name. */
export type FontChoice = "default" | "custom";

export interface FontSettings {
    readonly textFont: FontChoice;
    readonly textFontName: string;
    readonly codeFont: FontChoice;
    readonly codeFontName: string;
    /** Fetch fonts scenes name that are not here from Fontsource, into the font folder. */
    readonly downloadFonts: boolean;
}

/** A font the plugin ships, by the role THEME_FONTS gives its name. */
export type ThemeRole = keyof typeof THEME_FONTS;

/** Where fonts come from. Each answers null for a font it does not have. */
export interface FontSources {
    /** A face from the vault's font folder, by name or by its file's name. */
    folder(name: string): Promise<FontProviderResult | null>;
    /** A family from Fontsource, saved in the font folder; rejects when Fontsource cannot be reached. */
    download(name: string): Promise<FontProviderResult | null>;
    /** The font the plugin ships for the role. */
    bundled(role: ThemeRole): Promise<FontProviderResult>;
}

const THEME_ROLES: ReadonlyMap<string, ThemeRole> =
    new Map(Object.entries(THEME_FONTS).map(([role, name]) => [name, role as ThemeRole]));

const MONOSPACED = /mono|code|consol|courier|menlo|typewriter|terminal|fixed/i;

/** The name a downloaded font is saved under in the font folder, safe on every system. */
export function fontFileName(name: string): string {
    const base = name.replace(/[\\/:*?"<>|\0-\x1f]+/g, "-").replace(/^[\s.-]+|[\s.]+$/g, "").slice(0, 100);
    return `${base || "font"}.ttf`;
}

/** Whether the font file at `path` is named for `name`, as a downloaded font is: "Fonts/Noto Sans KR.ttf". */
export function namedFile(path: string, name: string): boolean {
    const file = path.slice(path.lastIndexOf("/") + 1).replace(/\.(ttf|ttc|otf|otc)$/i, "");
    const normal = (text: string) => text.toLowerCase().replace(/[\s_-]+/g, "");
    return normal(file) !== "" && normal(file) === normal(fontFileName(name).slice(0, -4));
}

export function fontProvider(settings: () => FontSettings, sources: FontSources): FontProvider {
    // A font that cannot be downloaded now is left to the fallback; looking for fonts again tries anew.
    const download = async (name: string) => {
        if (!settings().downloadFonts) return null;
        try {
            return await sources.download(name);
        } catch (error) {
            console.warn(`viscript: could not download the font "${name}" from Fontsource:`, error instanceof Error ? error.message : error);
            return null;
        }
    };
    const named = async (name: string) => await sources.folder(name) ?? await download(name);

    // The font the settings name for text or code, where it can be had, else the bundled one.
    const font = async (role: "text" | "code"): Promise<FontProviderResult> => {
        const current = settings();
        const choice = role === "text" ? current.textFont : current.codeFont;
        const name = (role === "text" ? current.textFontName : current.codeFontName).trim();
        return (choice === "custom" && name ? await named(name) : null) ?? sources.bundled(role);
    };

    return {
        async fetch(name) {
            const role = THEME_ROLES.get(name);
            if (role === "text" || role === "code") return font(role);
            if (role) return sources.bundled(role);
            return await named(name) ?? font(MONOSPACED.test(name) ? "code" : "text");
        },
    };
}
