import assert from "node:assert/strict";
import fs from "node:fs";
import path from "node:path";
import {fileURLToPath} from "node:url";
import {createVis} from "../runtime/client.js";

const extensionRoot = path.dirname(path.dirname(fileURLToPath(import.meta.url)));
const runtimeDir = path.join(extensionRoot, "runtime");

// With the shipped runtime and fonts: the first load names every font the timeline sets text in,
// including text added later, a later scene of a transition, and Hangul in a role of the host's
// list "Source Serif 4, Pretendard", and the scene loaded again with only those fonts, as the
// player loads it, draws as it does with all three fonts.
const wasmBinary = fs.readFileSync(path.join(runtimeDir, "vis-wasm.wasm"));
const FILES = {
    "Pretendard": "Pretendard.ttf",
    "JetBrainsMono": "JetBrainsMono.ttf",
    "Source Serif 4": "SourceSerif4-Semibold.ttf",
};
const HOST_THEME = {
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
    objects: ["#f28e2b", "#ff6b6b", "#7bc96f", "#c29bc0"],
    font: "Source Serif 4, Pretendard",
};
const BOOTSTRAP = "return vis.scene {width = 16, height = 16, loop = false}";
const CAMERA = `camera = {mode = "fixed", view = "2d", height = 4}`;
const SCENES = [
    {
        name: "adaptive.lua",
        // What the first second shows, and what the whole scene sets text in.
        early: ["Source Serif 4"],
        needs: ["Source Serif 4", "Pretendard", "JetBrainsMono"],
        times: (duration) => [0, 0.5, 1.5, 2.25, duration],
        source: `
local scene = vis.scene {width = 480, height = 270, theme = "adaptive", ${CAMERA}}
local label = scene:text {text = "Fonts on demand", point = {0, 1.3}, align = {0.5, 0.5}, role = "h2"}
scene:wait(1)
scene:text {text = "한글 설명", point = {0, 0.4}, align = {0.5, 0.5}, role = "text"}
scene:wait(1)
scene:text {text = "local fonts = {}", point = {0, -0.5}, align = {0.5, 0.5}, role = "code"}
scene:tween({target = label, text = "글꼴"}, {duration = 0.5})
scene:wait(0.5)
return scene`,
    },
    {
        name: "transition.lua",
        early: ["Pretendard"],
        needs: ["Pretendard", "JetBrainsMono"],
        times: (duration) => [0, duration / 4, duration / 2, (duration * 3) / 4, duration],
        source: `
local function stage(title, code)
    local scene = vis.scene {width = 480, height = 270, theme = "pro_white", ${CAMERA}}
    scene:text {text = title, point = {0, 0.8}, align = {0.5, 0.5}, role = "h2"}
    if code then scene:text {text = code, point = {0, -0.4}, align = {0.5, 0.5}, role = "code"} end
    return scene
end
local scene = vis.scene {width = 480, height = 270, theme = "pro_white", ${CAMERA}}
scene:scene_transition({stage("First"), stage("Then", "return scene")}, {live = false, duration = 1, hold = 0.5})
return scene`,
    },
];

async function engine(fonts = []) {
    const runtime = await createVis(BOOTSTRAP, "bootstrap.lua", {wasmBinary});
    runtime.hostTheme(HOST_THEME);
    for (const name of fonts) runtime.font(name, fs.readFileSync(path.join(runtimeDir, FILES[name])), "ttf");
    return runtime;
}
const frames = (runtime, times) => times.map((time) => Buffer.from(runtime.render(time, true)));

for (const scene of SCENES) {
    const eager = await engine(Object.keys(FILES));
    eager.load(scene.source, scene.name);
    assert.deepEqual(eager.missingFonts(), []);
    const times = scene.times(eager.duration);
    const expected = frames(eager, times);
    eager.destroy();

    // The fonts the opening frame shows draw it alone; the rest come later in the timeline.
    const early = await engine(scene.early);
    early.load(scene.source, scene.name);
    const [opening, end] = frames(early, [times[0], times.at(-1)]);
    assert.ok(opening.equals(expected[0]), `${scene.name}: the opening frame needs only ${scene.early}`);
    assert.ok(!end.equals(expected.at(-1)), `${scene.name}: the last frame needs more fonts`);
    const later = scene.needs.filter((name) => !scene.early.includes(name)).sort();
    assert.deepEqual(early.missingFonts().sort(), later, `${scene.name}: fonts the timeline needs later`);
    early.destroy();

    // As the player loads a scene: once, then again with the fonts that load named.
    const lazy = await engine();
    lazy.load(scene.source, scene.name);
    const named = lazy.missingFonts();
    assert.deepEqual([...named].sort(), [...scene.needs].sort(), `${scene.name}: fonts named on the first load`);
    for (const name of named) lazy.font(name, fs.readFileSync(path.join(runtimeDir, FILES[name])), "ttf");
    lazy.load(scene.source, scene.name);
    assert.deepEqual(lazy.missingFonts(), []);
    const actual = frames(lazy, times);
    times.forEach((time, index) => {
        assert.ok(actual[index].equals(expected[index]), `${scene.name} at ${time} s draws as with every font`);
    });
    lazy.destroy();
}

console.log("runtime fonts load on demand passed");
