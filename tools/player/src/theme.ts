// Colors for scenes that follow the host's theme (theme = "adaptive_vscode" and the like), read
// from the host's CSS custom properties with a fallback for each.
import type {HostThemePalette} from "vis-wasm-bundle";

type Color = Exclude<keyof HostThemePalette, "dark" | "objects">;

export const LIGHT_PALETTE: HostThemePalette = Object.freeze({
    dark: false,
    background: "#ffffff",
    foreground: "#202124",
    muted: "#555b64",
    accent: "#005fb8",
    secondary: "#6750a4",
    success: "#2e7d32",
    warning: "#8a5d00",
    danger: "#b3261e",
    info: "#00639b",
    surface: "#f5f6f8",
    line: "#d8dadd",
    result: "#8a6500",
    focus: "#315f8c",
    objects: Object.freeze(["#b45f06", "#b63a3c", "#2e7f7a", "#815681"]),
});

export const DARK_PALETTE: HostThemePalette = Object.freeze({
    dark: true,
    background: "#1e1e1e",
    foreground: "#cccccc",
    muted: "#9d9d9d",
    accent: "#4daafc",
    secondary: "#c8a7ff",
    success: "#7ee787",
    warning: "#e3b341",
    danger: "#ff7b72",
    info: "#79c0ff",
    surface: "#252526",
    line: "#454545",
    result: "#edc948",
    focus: "#6ea8fe",
    objects: Object.freeze(["#f28e2b", "#ff6b6b", "#7bc96f", "#c29bc0"]),
});

/** For each color, the custom properties to try in order. */
export type PaletteVariables = {readonly [K in Color]?: readonly string[]} & {
    /** One list per object color; the palette keeps four. */
    readonly objects?: readonly (readonly string[])[];
};

const COLORS: readonly Color[] = [
    "background", "foreground", "muted", "accent", "secondary", "success", "warning", "danger", "info",
    "surface", "line", "result", "focus",
];

/**
 * The palette from `read` (a custom property's value, or empty), taking the light or dark
 * fallback for any color none of its properties give.
 */
export function resolvePalette(dark: boolean, read: (name: string) => string | undefined,
    variables: PaletteVariables): HostThemePalette {
    const fallback = dark ? DARK_PALETTE : LIGHT_PALETTE;
    const first = (names: readonly string[] | undefined, otherwise: string) => {
        for (const name of names ?? []) {
            const value = read(name)?.trim();
            if (value) return value;
        }
        return otherwise;
    };
    const palette: Record<string, unknown> = {dark};
    for (const color of COLORS) palette[color] = first(variables[color], fallback[color]);
    palette.objects = Object.freeze(fallback.objects.map((color, i) => first(variables.objects?.[i], color)));
    return Object.freeze(palette) as unknown as HostThemePalette;
}

const HEX = /^#(?:[0-9a-f]{3,4}|[0-9a-f]{6}|[0-9a-f]{8})$/i;
let probe: CanvasRenderingContext2D | OffscreenCanvasRenderingContext2D | null = null;
const byte = (value: number) => value.toString(16).padStart(2, "0");

/**
 * A CSS color as the engine reads colors, #rrggbb or #rrggbbaa, or undefined for a value that is
 * not a color. Hosts define colors with hsl(), color-mix() and the like; a canvas draws the color
 * and the pixel is read back.
 */
export function cssColor(value: string): string | undefined {
    const color = value.trim();
    if (!color || HEX.test(color)) return color || undefined;
    probe ??= (typeof OffscreenCanvas === "function" ? new OffscreenCanvas(1, 1) : document.createElement("canvas"))
        .getContext("2d", {willReadFrequently: true}) as CanvasRenderingContext2D | OffscreenCanvasRenderingContext2D | null;
    if (!probe) return undefined;
    // A value the canvas cannot parse leaves fillStyle as it was, whichever color that is.
    probe.fillStyle = "#000";
    probe.fillStyle = color;
    const onBlack = probe.fillStyle;
    probe.fillStyle = "#fff";
    probe.fillStyle = color;
    if (probe.fillStyle !== onBlack) return undefined;
    probe.clearRect(0, 0, 1, 1);
    probe.fillRect(0, 0, 1, 1);
    const [red = 0, green = 0, blue = 0, alpha = 0] = probe.getImageData(0, 0, 1, 1).data;
    return `#${byte(red)}${byte(green)}${byte(blue)}${alpha === 255 ? "" : byte(alpha)}`;
}

/** The palette from an element's computed style. */
export function readPalette(element: Element, dark: boolean, variables: PaletteVariables): HostThemePalette {
    const style = getComputedStyle(element);
    return resolvePalette(dark, (name) => cssColor(style.getPropertyValue(name)), variables);
}

/** Equal for equal palettes, so a host reloads scenes only when the colors changed. */
export function paletteKey(palette: HostThemePalette): string {
    return [palette.dark, ...COLORS.map((color) => palette[color]), ...palette.objects].join("\u0000");
}
