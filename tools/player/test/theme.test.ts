import assert from "node:assert/strict";
import test from "node:test";
import {paletteKey, resolvePalette} from "../src/theme.ts";

test("each color takes the first property that is set, else stays unset for the runtime", () => {
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
    assert.equal(palette.accent, undefined);
    assert.equal(palette.danger, undefined);
    assert.deepEqual(palette.objects, [undefined, "#777", undefined, undefined]);
    assert.ok(Object.isFrozen(palette) && Object.isFrozen(palette.objects));
});

test("palettes with the same colors have the same key", () => {
    const variables = {background: ["--background"]};
    const light = resolvePalette(false, () => undefined, variables);
    assert.equal(paletteKey(light), paletteKey(resolvePalette(false, () => "", variables)));
    assert.notEqual(paletteKey(light), paletteKey(resolvePalette(true, () => undefined, variables)));
    assert.notEqual(paletteKey(light), paletteKey(resolvePalette(false, () => "#101010", variables)));
});
