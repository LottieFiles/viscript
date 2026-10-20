import assert from "node:assert/strict";
import fs from "node:fs";
import path from "node:path";
import {fileURLToPath} from "node:url";
import {createVis} from "../runtime/client.js";
import {createFontCache, loadWithFonts} from "../media/scene-fonts.mjs";

const extensionRoot = path.dirname(path.dirname(fileURLToPath(import.meta.url)));
const runtimeDir = path.join(extensionRoot, "runtime");

// The cache fetches a font once, however many loads ask for it at once, and again after a failure.
{
    const asked = [];
    let status = 404;
    const fonts = createFontCache({A: "a.ttf"}, async (url) => {
        asked.push(url);
        return new Response(new Uint8Array([1, 2, 3]), {status});
    });
    assert.equal(fonts.has("A"), true);
    assert.equal(fonts.has("B"), false);
    assert.equal(fonts.has("toString"), false);
    await assert.rejects(fonts.bytes("A"), /Bundled font A failed to load \(404\)/);
    status = 200;
    const [first, second] = await Promise.all([fonts.bytes("A"), fonts.bytes("A")]);
    assert.equal(first, second);
    assert.deepEqual([...first], [1, 2, 3]);
    assert.deepEqual(asked, ["a.ttf", "a.ttf"]);
}

// A runtime whose scene sets text in `needs`; it reports those it was not given as missing.
function fakeRuntime(needs) {
    const runtime = {
        registered: [],
        loads: 0,
        font(name, data, mime) {
            assert.ok(data instanceof Uint8Array);
            assert.equal(mime, "ttf");
            runtime.registered.push(name);
        },
        missingFonts: () => needs.filter((name) => !runtime.registered.includes(name)),
    };
    return runtime;
}
const fakeFonts = createFontCache({A: "a", B: "b", C: "c"}, async () => new Response(new Uint8Array(4)));
const load = (runtime) => { runtime.loads += 1; };
{
    // Nothing expected: the scene loads, then again with the fonts it lacks that the cache has.
    const runtime = fakeRuntime(["A", "B", "Custom"]);
    assert.deepEqual(await loadWithFonts(runtime, load, fakeFonts), ["A", "B"]);
    assert.deepEqual(runtime.registered, ["A", "B"]);
    assert.equal(runtime.loads, 2);
}
{
    // The fonts the document needed last time go in first, and one load is enough.
    const runtime = fakeRuntime(["A", "B"]);
    assert.deepEqual(await loadWithFonts(runtime, load, fakeFonts, {expected: ["B", "A", "Gone"]}), ["B", "A"]);
    assert.equal(runtime.loads, 1);
}
{
    // A font the scene now needs as well is added, and the scene loads again.
    const runtime = fakeRuntime(["A", "C"]);
    assert.deepEqual(await loadWithFonts(runtime, load, fakeFonts, {expected: ["A"]}), ["A", "C"]);
    assert.deepEqual(runtime.registered, ["A", "C"]);
    assert.equal(runtime.loads, 2);
}
{
    // A load that a newer one replaced while fonts were on their way stops there.
    let current = true;
    const pending = createFontCache({A: "a"}, async () => {
        current = false;
        return new Response(new Uint8Array(4));
    });
    const runtime = fakeRuntime(["A"]);
    await loadWithFonts(runtime, load, pending, {expected: ["A"], current: () => current});
    assert.deepEqual(runtime.registered, []);
    assert.equal(runtime.loads, 0);
}

// With the shipped runtime and fonts: the first load names every font the timeline sets text in,
// including text added later, a later scene of a transition, and Hangul in a role of the host's
// list "Source Serif 4, Pretendard", and the scene the preview loads with only those fonts draws
// as it does with all three fonts.
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
local scene = vis.scene {width = 480, height = 270, theme = "adaptive_vscode", ${CAMERA}}
local label = scene:text {text = "Fonts on demand", point = {0, 1.3}, align = {0.5, 0.5}, role = "h2"}
scene:wait(1)
scene:text {text = "한글 설명", point = {0, 0.4}, align = {0.5, 0.5}, role = "text"}
scene:wait(1)
scene:text {text = "local fonts = {}", point = {0, -0.5}, align = {0.5, 0.5}, role = "code"}
scene:play({target = label, text = "글꼴"}, 0.5)
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

    const bare = await engine();
    bare.load(scene.source, scene.name);
    assert.deepEqual(bare.missingFonts().sort(), [...scene.needs].sort(), `${scene.name}: fonts named on the first load`);
    bare.destroy();

    const fetched = [];
    const fonts = createFontCache(FILES, async (file) => {
        fetched.push(file);
        return new Response(fs.readFileSync(path.join(runtimeDir, file)));
    });
    let remembered = [];
    for (const loads of [2, 1]) {
        const lazy = await engine();
        let count = 0;
        remembered = await loadWithFonts(lazy, (runtime) => {
            count += 1;
            runtime.load(scene.source, scene.name);
        }, fonts, {expected: remembered});
        assert.deepEqual([...remembered].sort(), [...scene.needs].sort(), `${scene.name}: fonts given`);
        assert.equal(count, loads, `${scene.name}: loads`);
        assert.deepEqual(lazy.missingFonts(), []);
        const actual = frames(lazy, times);
        times.forEach((time, index) => {
            assert.ok(actual[index].equals(expected[index]), `${scene.name} at ${time} s draws as with every font`);
        });
        lazy.destroy();
    }
    assert.deepEqual(fetched.sort(), scene.needs.map((name) => FILES[name]).sort(), `${scene.name}: each font fetched once`);
}

console.log("scene fonts load on demand passed");
