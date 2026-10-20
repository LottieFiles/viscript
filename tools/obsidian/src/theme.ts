// Colors for scenes that follow the host's theme, as scenes that name no other do, from the
// Obsidian theme's CSS variables, so a scene matches the note it is in, light or dark, in any
// theme. Their text takes the text font, as the plugin names no font of its own for the theme.
import type {HostThemePalette} from "vis-wasm-bundle";
import {readPalette, type PaletteVariables} from "../../player/src/theme.ts";

export const OBSIDIAN_PALETTE: PaletteVariables = {
    background: ["--background-primary"],
    foreground: ["--text-normal"],
    muted: ["--text-muted"],
    accent: ["--color-accent", "--interactive-accent", "--text-accent"],
    secondary: ["--color-purple"],
    success: ["--color-green", "--text-success"],
    warning: ["--text-warning", "--color-orange"],
    danger: ["--text-error", "--color-red"],
    info: ["--color-cyan", "--color-blue"],
    surface: ["--background-secondary", "--background-primary-alt"],
    line: ["--background-modifier-border"],
    result: ["--color-yellow"],
    focus: ["--background-modifier-border-focus", "--interactive-accent"],
    objects: [["--color-orange"], ["--color-red"], ["--color-green"], ["--color-purple"]],
};

/** The palette where `element` is: a popover or a canvas card can set its own colors. */
export function obsidianPalette(element: Element): HostThemePalette {
    return readPalette(element, element.ownerDocument.body.classList.contains("theme-dark"), OBSIDIAN_PALETTE);
}
