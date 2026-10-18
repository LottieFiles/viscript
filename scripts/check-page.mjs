// Load and sample every published scene against the shipped WASM runtime.
// Usage: node scripts/check-page.mjs   (after scripts/build-page.mjs, which copies the runtime in)
import assert from "node:assert/strict";
import {readdirSync, readFileSync} from "node:fs";
import {createHash} from "node:crypto";
import {createVis, evaluateScene} from "../docs/page/runtime/client.js";
import {HERO} from "../docs/page/scenes/hero.js";
import {SKILLS} from "../docs/page/scenes/skills.js";
import {COLLECTIONS} from "../docs/page/scenes/collections.js";

const bundle = new URL("../docs/page/runtime/", import.meta.url);
const manifest = JSON.parse(readFileSync(new URL("BUILD-MANIFEST.json", bundle)));
for (const {path, bytes, sha256} of manifest.artifacts) {
    const data = readFileSync(new URL(path, bundle));
    assert.equal(data.length, bytes, `${path}: manifest size`);
    assert.equal(createHash("sha256").update(data).digest("hex"), sha256, `${path}: manifest hash`);
}
const runtime = await createVis("return vis.scene {width = 16, height = 16}", "bootstrap.lua", {
    wasmBinary: readFileSync(new URL("vis-wasm.wasm", bundle)), renderEngine: "cpu",
});
for (const [name, file] of [["Pretendard", "Pretendard.ttf"], ["Source Serif 4", "SourceSerif4-Semibold.ttf"],
    ["IBM Plex Sans KR", "IBMPlexSansKR-SemiBold.ttf"], ["JetBrainsMono", "JetBrainsMono.ttf"]]) {
    runtime.font(name, readFileSync(new URL(file, bundle)), "ttf");
}
for (const name of ["CMUTypewriter-Regular", "CMUSerif-Roman"]) {
    runtime.font(name, readFileSync(new URL(`../docs/page/assets/thorvg/${name}.ttf`, import.meta.url)), "ttf");
}
runtime.asset("earth.png", readFileSync(new URL("../docs/page/assets/earth.png", import.meta.url)), "png");
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
const sample = (clips = []) => {
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
        try {
            runtime.loadLua(code, `${name}.lua`);
            const lua = sample(clips);
            if (!js) continue;
            runtime.loadScene(evaluateScene(js, `${name}.js`), `${name}.js`);
            const other = sample(clips);
            assert.ok(Math.abs(lua.duration - other.duration) < 1e-3, `JS duration ${other.duration} != Lua ${lua.duration}`);
            lua.frames.forEach((a, f) => {
                let diff = 0;
                for (let k = 0; k < a.length; k++) if (Math.abs(a[k] - other.frames[f][k]) > 2) diff++;
                assert.ok(diff <= a.length * 0.001, `JS frame ${f} differs from Lua in ${diff / 4} pixels`);
            });
            paired++;
        } catch (error) { failed++; console.error(`${name}: ${error.message}`); }
    }
} finally { runtime.destroy(); }
console.log(`${checked.length - failed}/${checked.length} scenes rendered, ${paired} JS versions match Lua; runtime manifest verified.`);
if (failed) process.exitCode = 1;
