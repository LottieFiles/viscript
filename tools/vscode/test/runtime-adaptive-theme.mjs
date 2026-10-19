import assert from "node:assert/strict";
import fs from "node:fs";
import path from "node:path";
import {fileURLToPath} from "node:url";
import {createVis} from "../runtime/client.js";

const extensionRoot = path.dirname(path.dirname(fileURLToPath(import.meta.url)));
const codeFont = fs.readFileSync(path.resolve(extensionRoot, "../../src/examples/assets/JetBrainsMono.ttf"));
const wasmBinary = fs.readFileSync(path.join(extensionRoot, "runtime", "vis-wasm.wasm"));
const headingFont = fs.readFileSync(path.join(extensionRoot, "runtime", "IBMPlexSansKR-SemiBold.ttf"));
const serifHeadingFont = fs.readFileSync(path.join(extensionRoot, "runtime", "SourceSerif4-Semibold.ttf"));
// Without a source the engine holds no scene until load, so fonts and the host theme go in first.
const runtime = await createVis({wasmBinary});
assert.equal(runtime.duration, 0);
assert.deepEqual(runtime.size(), [0, 0]);
assert.throws(() => runtime.render(0), /InsufficientCondition/);
runtime.font("IBM Plex Sans KR", headingFont, "ttf");
runtime.font("Source Serif 4", serifHeadingFont, "ttf");
runtime.font("JetBrainsMono", codeFont, "ttf");

runtime.hostTheme({
    dark: true,
    background: "#101820",
    foreground: "rgb(232, 237, 242)",
    muted: "rgba(168, 177, 187, 1)",
    accent: "#62b0ff",
    secondary: "#c8a7ff",
    success: "#7ee787",
    warning: "#e3b341",
    danger: "#ff7b72",
    info: "#79c0ff",
    surface: "#252526",
    line: "#39434d",
    result: "#d9b949",
    focus: "#4f9dde",
    objects: ["#e89432", "#ef6464", "#72bd78", "#b390d4"],
});
runtime.load("local s=vis.scene{width=64,height=64,theme='adaptive_vscode',camera={height=4}}; s:line{from={-1,0},to={1,0},stroke='danger',width=10}; return s", "adaptive.lua");
assert.equal(runtime.adaptiveTheme, true);
const adaptivePixels = runtime.render(0);
assert.deepEqual([...adaptivePixels.slice(0, 4)], [16, 24, 32, 255]);
const center = (32 * 64 + 32) * 4;
assert.deepEqual([...adaptivePixels.slice(center, center + 4)], [255, 123, 114, 255]);

runtime.load("return vis.scene {width = 8, height = 8, theme = 'pro_white'}", "fixed.lua");
assert.equal(runtime.adaptiveTheme, false);
assert.deepEqual([...runtime.render(0).slice(0, 4)], [255, 255, 255, 255]);

runtime.load("return vis.scene {width = 8, height = 8}", "implicit-pro-white.lua");
assert.equal(runtime.adaptiveTheme, false);
assert.deepEqual([...runtime.render(0).slice(0, 4)], [255, 255, 255, 255]);

runtime.load("return vis.scene {width = 8, height = 8, theme = 'pro_black'}", "pro-black.lua");
assert.equal(runtime.adaptiveTheme, false);
assert.deepEqual([...runtime.render(0).slice(0, 4)], [0, 0, 0, 255]);
for (const removed of ["default", "3_blue_1_eyes"]) {
    assert.throws(
        () => runtime.load(`return vis.scene {theme = '${removed}'}`, "removed-preset.lua"),
        /theme preset must be 'pro_white', 'pro_black' or 'adaptive_vscode'/,
    );
}

runtime.load("return vis.scene {width = 8, height = 8, theme = {preset = 'adaptive_vscode'}}", "adaptive-table.lua");
assert.equal(runtime.adaptiveTheme, true);
assert.deepEqual([...runtime.render(0).slice(0, 4)], [16, 24, 32, 255]);

runtime.load(`
local scene = vis.scene {
    width = 320,
    height = 180,
    theme = {
        preset = "adaptive_vscode",
        text = {
            h1 = {font = "IBM Plex Sans KR"},
            h2 = {font = "IBM Plex Sans KR"},
        },
    },
    camera = {height = 4},
}
scene:text {text = "\uc804\ubb38\uc801\uc778 Heading", point = {0, 0.6}, align = {0.5, 0.5}, role = "h1", id = "h1"}
scene:text {text = "\ud55c\uae00\uacfc English", point = {0, -0.6}, align = {0.5, 0.5}, role = "h2", id = "h2"}
return scene
`, "adaptive-heading-font.lua");
assert.ok(runtime.bounds("h1").width > 0);
assert.ok(runtime.bounds("h2").width > 0);

runtime.load(`
local scene = vis.scene {
    width = 320,
    height = 180,
    theme = {
        preset = "adaptive_vscode",
        text = {
            h1 = {font = "Source Serif 4"},
            h2 = {font = "Source Serif 4"},
            h3 = {font = "Source Serif 4"},
        },
    },
    camera = {height = 4},
}
scene:text {text = "English Title", point = {0, 0.8}, align = {0.5, 0.5}, role = "h1", id = "serif-h1"}
scene:text {text = "Mechanism", point = {0, 0}, align = {0.5, 0.5}, role = "h2", id = "serif-h2"}
scene:text {text = "Settled result", point = {0, -0.8}, align = {0.5, 0.5}, role = "h3", id = "serif-h3"}
return scene
`, "adaptive-serif-heading-font.lua");
assert.ok(runtime.bounds("serif-h1").width > 0);
assert.ok(runtime.bounds("serif-h2").width > 0);
assert.ok(runtime.bounds("serif-h3").width > 0);

runtime.load(`
local scene = vis.scene {
    width = 480,
    height = 240,
    theme = "adaptive_vscode",
    camera = {height = 4},
}
scene:text {text = "English explanation", point = {0, 0.9}, align = {0.5, 0.5}, role = "text", id = "automatic-serif-text"}
scene:text {text = "const value = render()", point = {0, 0}, align = {0.5, 0.5}, role = "code", id = "automatic-serif-code"}
scene:text {text = "\ud55c\uae00 \uc124\uba85", point = {0, -0.9}, align = {0.5, 0.5}, role = "h3", id = "automatic-korean"}
return scene
`, "adaptive-automatic-language-font.lua");
assert.ok(runtime.bounds("automatic-serif-text").width > 0);
assert.ok(runtime.bounds("automatic-serif-code").width > 0);
assert.ok(runtime.bounds("automatic-korean").width > 0);
assert.ok(runtime.render(0).some((value, index) => index % 4 !== 3 && value !== 255));

// A color the host leaves undefined or empty is the runtime's light or dark one, and object colors
// keep the number given. Two lines take the first two object colors; the third is the danger color.
const hostColors = (palette) => {
    runtime.hostTheme(palette);
    runtime.load(`local s = vis.scene {width = 64, height = 64, theme = "adaptive_vscode", camera = {height = 4}}
s:line {from = {-2, 1}, to = {2, 1}, width = 8}
s:line {from = {-2, 0}, to = {2, 0}, width = 8}
s:line {from = {-2, -1}, to = {2, -1}, stroke = "danger", width = 8}
return s`, "host-colors.lua");
    const pixels = runtime.render(0);
    return [4, 16, 32, 48].map((row) => [...pixels.slice((row * 64 + 32) * 4, (row * 64 + 32) * 4 + 4)]);
};
assert.deepEqual(hostColors({dark: true}), [[30, 30, 30, 255], [242, 142, 43, 255], [255, 107, 107, 255], [255, 123, 114, 255]]);
assert.deepEqual(
    hostColors({dark: false, danger: "", objects: [undefined, "#00ff00"]}),
    [[255, 255, 255, 255], [180, 95, 6, 255], [0, 255, 0, 255], [179, 38, 30, 255]],
);
assert.deepEqual(
    hostColors({dark: true, background: "#101820", danger: "#123456", objects: ["#0000ff"]}),
    [[16, 24, 32, 255], [0, 0, 255, 255], [0, 0, 255, 255], [18, 52, 86, 255]],
);
assert.throws(() => runtime.hostTheme({dark: true, objects: Array(5)}), /Host theme object color 5 must be a CSS color string/);
assert.throws(() => runtime.hostTheme({background: "#101820"}), /boolean dark value/);

runtime.destroy();
console.log("Adaptive VS Code WASM theme passed");
