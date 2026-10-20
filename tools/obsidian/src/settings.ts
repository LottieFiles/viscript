// The plugin's settings, as stored in data.json.
import type {EndBehavior} from "../../player/src/player/player.ts";
import type {Quality} from "../../player/src/policy.ts";
import type {FontSettings} from "./fonts.ts";

export type ControlsMode = "hover" | "always" | "never";

export interface ViscriptSettings extends FontSettings {
    readonly autoplay: boolean;
    readonly end: EndBehavior;
    readonly controls: ControlsMode;
    /** Show the read-only script below players in notes. */
    readonly showSource: boolean;
    /** Frames per second at most; 0 for the default, 60 or 30 on phones. */
    readonly maxFps: number;
    readonly quality: Quality;
    /** Render in a worker when the device allows it, rather than on the main thread. */
    readonly worker: boolean;
    /** Open .lua files as scenes; read when the plugin loads. */
    readonly openFiles: boolean;
    /** A vault folder of fonts scenes may name, which syncs with the vault; empty for none. */
    readonly fontFolder: string;
}

export const DEFAULT_SETTINGS: ViscriptSettings = {
    autoplay: true,
    end: "auto",
    controls: "hover",
    showSource: false,
    maxFps: 0,
    quality: "auto",
    worker: true,
    openFiles: true,
    textFont: "default",
    textFontName: "",
    codeFont: "default",
    codeFontName: "",
    downloadFonts: true,
    fontFolder: "",
};

const CHOICES: {readonly [K in keyof ViscriptSettings]?: readonly ViscriptSettings[K][]} = {
    end: ["auto", "loop", "crossfade", "stop"],
    controls: ["hover", "always", "never"],
    quality: ["auto", "high"],
    textFont: ["default", "custom"],
    codeFont: ["default", "custom"],
};

/**
 * Stored settings over the defaults, each value checked: data.json may be old or hand-edited. Font
 * choices earlier versions had, "bundled" and "obsidian", are read as the default.
 */
export function readSettings(stored: unknown): ViscriptSettings {
    const data = stored && typeof stored === "object" ? stored as Record<string, unknown> : {};
    const settings: Record<string, unknown> = {...DEFAULT_SETTINGS};
    for (const [key, fallback] of Object.entries(DEFAULT_SETTINGS)) {
        const value = data[key];
        const choices = CHOICES[key as keyof ViscriptSettings] as readonly unknown[] | undefined;
        if (typeof value !== typeof fallback || (choices && !choices.includes(value))) continue;
        if (typeof value === "number" && !(Number.isFinite(value) && value >= 0)) continue;
        settings[key] = value;
    }
    return settings as unknown as ViscriptSettings;
}
