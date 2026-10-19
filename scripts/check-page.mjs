// Load and sample every published scene against the shipped WASM runtime.
// Usage: node scripts/check-page.mjs   (after scripts/build-page.mjs, which copies the runtime in)
import assert from "node:assert/strict";
import {readdirSync, readFileSync} from "node:fs";
import {createHash} from "node:crypto";
import {createVis, createVisModule, evaluateScene} from "../docs/page/runtime/client.js";
import {ASSETS, assetsFor, FONTS, FULL_PRETENDARD, needsFullPretendard} from "../docs/page/resources.js";
import {HERO} from "../docs/page/scenes/hero.js";
import {SKILLS} from "../docs/page/scenes/skills.js";
import {COLLECTIONS} from "../docs/page/scenes/collections.js";

const page = new URL("../docs/page/", import.meta.url);
const bundle = new URL("runtime/", page);
const manifest = JSON.parse(readFileSync(new URL("BUILD-MANIFEST.json", bundle)));
for (const {path, bytes, sha256} of manifest.artifacts) {
    const data = readFileSync(new URL(path, bundle));
    assert.equal(data.length, bytes, `${path}: manifest size`);
    assert.equal(createHash("sha256").update(data).digest("hex"), sha256, `${path}: manifest hash`);
}
const wasmBinary = readFileSync(new URL("vis-wasm.wasm", bundle));
const read = (path) => readFileSync(new URL(path, page));
// The font the page registers under a name, as player.js's provider gives it.
const pageFont = (name, full) => read(name === "Pretendard" && full ? FULL_PRETENDARD : FONTS.get(name));
const boot = async ({fonts = [], full = false, assets = []} = {}, module) => {
    const runtime = await createVis("return vis.scene {width = 16, height = 16}", "bootstrap.lua", {wasmBinary, module});
    for (const name of fonts) runtime.font(name, pageFont(name, full), "ttf");
    for (const name of assets) runtime.asset(name, read(ASSETS.get(name).path), ASSETS.get(name).mime);
    return runtime;
};
// The reference registers every font, the full Pretendard among them, and every image.
const runtime = await boot({fonts: [...FONTS.keys()], full: true, assets: [...ASSETS.keys()]});
// The page registers a font when a scene first sets text in it, and its players share WASM
// instances: fonts go in once through an engine kept with the instance, and every player gets an
// engine of its own. An instance with no fonts but the engine's default one names every font a
// scene sets text in; each set of them gets an instance here, so a scene can only use the fonts
// the page would give it, as the page would give them.
const bare = await boot({assets: [...ASSETS.keys()]}, await createVisModule({wasmBinary}));
const instances = new Map();
const engineFor = async (source, load) => {
    load(bare);
    const fonts = bare.missingFonts().sort(), full = needsFullPretendard(source);
    const unknown = fonts.filter((name) => !FONTS.has(name));
    assert.deepEqual(unknown, [], "the page has no font for these names, so their text is drawn in the default font");
    const key = JSON.stringify([full, fonts]);
    if (!instances.has(key)) {
        const module = await createVisModule({wasmBinary});
        instances.set(key, {module, anchor: await boot({fonts, full}, module)});
    }
    const engine = await boot({assets: assetsFor(source)}, instances.get(key).module);
    load(engine);
    return engine;
};
const scenes = [];
const add = (name, code, js, clips = []) => { if (code) scenes.push({name, code, js, clips}); };
add("home/hero", HERO);
add("home/skills", SKILLS);
for (const c of COLLECTIONS) for (const e of c.episodes) {
    add(`examples/${c.id}/${e.id}`, e.src ? readFileSync(e.src, "utf8") : e.code, e.js, e.clips);
}
const content = new URL("../docs/page/content/", import.meta.url);
for (const file of readdirSync(content, {recursive: true}).filter((f) => f.endsWith(".mdx"))) {
    const text = readFileSync(new URL(file, content), "utf8");
    for (const [i, m] of [...text.matchAll(/```lua live[^\n]*\n([\s\S]*?)\n```(?:\n+```js live[^\n]*\n([\s\S]*?)\n```)?/g)].entries()) {
        add(`${file}/${i + 1}`, m[1], m[2]);
    }
}
// Usage filter: node scripts/check-page.mjs [name-substring]
const only = process.argv[2];
const sample = (runtime, clips = []) => {
    assert.ok(Number.isFinite(runtime.duration) && runtime.duration >= 0);
    for (const {label, range: [start, end], bounds} of clips) {
        assert.ok(start >= 0 && start < end && end <= runtime.duration + 1e-3, `${label}: playback range fits scene`);
        const [x, y, w, h] = bounds;
        assert.ok(x >= 0 && y >= 0 && w > 0 && h > 0 && x + w <= 1 && y + h <= 1, `${label}: hotspot fits canvas`);
    }
    const frames = [];
    const times = [0, runtime.duration / 3, runtime.duration * 2 / 3, runtime.duration,
        ...clips.flatMap(({range: [start, end]}) => [start, (start + end) / 2, end])];
    for (const time of times) {
        if (runtime.retainedLua) {
            runtime.beginInputFrame(runtime.runtimeTime);
            runtime.input({type: "pointermove", x: 480, y: 270, dx: 0, dy: 0, wheelX: 0, wheelY: 0,
                button: -1, buttons: 0, modifiers: 0, pointerId: 0, time: runtime.runtimeTime});
            runtime.advanceRuntime(1 / 30);
        }
        const [width, height] = runtime.size();
        const pixels = runtime.render(time, true);
        assert.equal(pixels.length, width * height * 4);
        frames.push(pixels);
    }
    return {duration: runtime.duration, frames};
};
let failed = 0, paired = 0;
const checked = scenes.filter((s) => !only || s.name.includes(only));
try {
    for (const {name, code, js, clips} of checked) {
        const engines = [];
        try {
            const same = (a, b, label) => a.frames.forEach((frame, f) =>
                assert.ok(Buffer.from(frame).equals(Buffer.from(b.frames[f])), `${label} frame ${f} changes with the page's resources or shared instance`));
            const loadLua = (engine) => engine.loadLua(code, `${name}.lua`);
            loadLua(runtime);
            const lua = sample(runtime, clips);
            const luaPage = await engineFor(code, loadLua);
            engines.push(luaPage);
            same(lua, sample(luaPage, clips), "Lua");
            if (!js) continue;
            const loadJs = (engine) => engine.loadScene(evaluateScene(js, `${name}.js`), `${name}.js`);
            loadJs(runtime);
            const other = sample(runtime, clips);
            const jsPage = await engineFor(js, loadJs);
            engines.push(jsPage);
            same(other, sample(jsPage, clips), "JS");
            assert.ok(Math.abs(lua.duration - other.duration) < 1e-3, `JS duration ${other.duration} != Lua ${lua.duration}`);
            lua.frames.forEach((a, f) => {
                let diff = 0;
                for (let k = 0; k < a.length; k++) if (Math.abs(a[k] - other.frames[f][k]) > 2) diff++;
                assert.ok(diff <= a.length * 0.001, `JS frame ${f} differs from Lua in ${diff / 4} pixels`);
            });
            paired++;
        } catch (error) { failed++; console.error(`${name}: ${error.message}`); }
        finally { for (const engine of engines) engine.destroy(); }
    }
} finally {
    runtime.destroy();
    bare.destroy();
    for (const {anchor} of instances.values()) anchor.destroy();
}
console.log(`${checked.length - failed}/${checked.length} scenes rendered, ${paired} JS versions match Lua, page resources and shared instances match; runtime manifest verified.`);
if (failed) process.exitCode = 1;
