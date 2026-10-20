// Colors for scenes that follow the host's theme, as scenes that name no other do, read from the
// host's CSS custom properties. A color none of its properties give stays unset, and the runtime
// takes its own light or dark one.
import type {HostThemePalette} from "vis-wasm-bundle";

type Color = Exclude<keyof HostThemePalette, "dark" | "objects" | "font">;

/** For each color, the custom properties to try in order. */
export type PaletteVariables = {readonly [K in Color]?: readonly string[]} & {
    /** One list per object color; the palette keeps four. */
    readonly objects?: readonly (readonly string[])[];
};

const COLORS: readonly Color[] = [
    "background", "foreground", "muted", "accent", "secondary", "success", "warning", "danger", "info",
    "surface", "line", "result", "focus",
];

/** The palette from `read` (a custom property's value, or empty). */
export function resolvePalette(dark: boolean, read: (name: string) => string | undefined,
    variables: PaletteVariables): HostThemePalette {
    const first = (names: readonly string[] = []) => names.map((name) => read(name)?.trim()).find(Boolean);
    const palette: Record<string, unknown> = {dark};
    for (const color of COLORS) palette[color] = first(variables[color]);
    palette.objects = Object.freeze(Array.from({length: 4}, (_, i) => first(variables.objects?.[i])));
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
    return [palette.dark, ...COLORS.map((color) => palette[color]), ...palette.objects ?? []].join("\u0000");
}
