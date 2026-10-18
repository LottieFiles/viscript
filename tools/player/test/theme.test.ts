import assert from "node:assert/strict";
import test from "node:test";
import {DARK_PALETTE, LIGHT_PALETTE, paletteKey, resolvePalette} from "../src/theme.ts";

test("each color takes the first property that is set, else the fallback for the mode", () => {
    const values: Record<string, string> = {"--background-primary": " #101010 ", "--text-normal": "", "--text-muted": "#777"};
    const palette = resolvePalette(true, (name) => values[name], {
        background: ["--background-primary"],
        foreground: ["--text-normal", "--text-muted"],
        accent: ["--missing"],
        objects: [["--missing"], ["--text-muted"]],
    });
    assert.equal(palette.dark, true);
    assert.equal(palette.background, "#101010");
    assert.equal(palette.foreground, "#777");
    assert.equal(palette.accent, DARK_PALETTE.accent);
    assert.deepEqual(palette.objects, [DARK_PALETTE.objects[0], "#777", DARK_PALETTE.objects[2], DARK_PALETTE.objects[3]]);
    assert.ok(Object.isFrozen(palette) && Object.isFrozen(palette.objects));
});

test("palettes with the same colors have the same key", () => {
    const light = resolvePalette(false, () => undefined, {});
    assert.deepEqual(light, LIGHT_PALETTE);
    assert.equal(paletteKey(light), paletteKey(LIGHT_PALETTE));
    assert.notEqual(paletteKey(light), paletteKey(DARK_PALETTE));
});
