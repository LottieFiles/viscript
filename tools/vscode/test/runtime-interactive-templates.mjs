import assert from "node:assert/strict";
import fs from "node:fs";
import path from "node:path";
import {fileURLToPath, pathToFileURL} from "node:url";

const extensionRoot = path.dirname(path.dirname(fileURLToPath(import.meta.url)));
const repositoryRoot = path.resolve(extensionRoot, "..", "..");
const runtimeRoot = process.env.VIS_RUNTIME_ROOT
    ? path.resolve(process.env.VIS_RUNTIME_ROOT)
    : path.join(extensionRoot, "runtime");
const templateRoot = path.join(
    repositoryRoot,
    "tools",
    "skill",
    "vis-interactive",
    "templates",
);
const {createVis} = await import(pathToFileURL(path.join(runtimeRoot, "client.js")));
const wasmBinary = fs.readFileSync(path.join(runtimeRoot, "vis-wasm.wasm"));
const fonts = [
    ["Source Serif 4", "SourceSerif4-Semibold.ttf"],
    ["Pretendard", "Pretendard.ttf"],
].map(([name, file]) => [
    name,
    fs.readFileSync(path.join(extensionRoot, "runtime", file)),
]);
fonts.push(["JetBrainsMono", fs.readFileSync(path.join(repositoryRoot, "src/examples/assets/JetBrainsMono.ttf"))]);

const center = (box) => ({
    x: box.x + box.width / 2,
    y: box.y + box.height / 2,
});

const event = (runtime, type, fields = {}) => runtime.input({type, pointerId: 1, time: 0, ...fields});

async function loadTemplate(name) {
    const source = fs.readFileSync(path.join(templateRoot, name), "utf8");
    const runtime = await createVis({wasmBinary});
    try {
        for (const [fontName, data] of fonts) runtime.font(fontName, data, "ttf");
        runtime.load(source, name);
        return runtime;
    } catch (error) {
        runtime.destroy();
        throw error;
    }
}

{
    const runtime = await loadTemplate("ui-input-routing.lua");
    try {
        const initial = center(runtime.bounds("routing:input:follower:body", 0.2));
        const down = event(runtime, "pointerdown", {
            x: 140,
            y: 285,
            button: 0,
            buttons: 1,
            pointerId: 12,
            time: 0.2,
        });
        assert.equal(down.capture, true);
        const captured = event(runtime, "pointermove", {
            x: 700,
            y: 250,
            buttons: 1,
            pointerId: 12,
            time: 0.24,
        });
        assert.equal(captured.handled, true);
        const duringCapture = center(runtime.bounds("routing:input:follower:body", 0.4));
        assert.ok(Math.abs(duringCapture.x - initial.x) < 0.1);
        assert.ok(Math.abs(duringCapture.y - initial.y) < 0.1);
        const up = event(runtime, "pointerup", {
            x: 700,
            y: 250,
            button: 0,
            pointerId: 12,
            time: 0.4,
        });
        assert.equal(up.release, true);

        const unhandled = event(runtime, "pointermove", {
            x: 700,
            y: 250,
            pointerId: 12,
            time: 0.5,
        });
        assert.equal(unhandled.handled, true);
        const afterRelease = center(runtime.bounds("routing:input:follower:body", 0.66));
        assert.ok(Math.abs(afterRelease.x - 700) < 0.2);
        assert.ok(Math.abs(afterRelease.y - 250) < 0.2);
    } finally {
        runtime.destroy();
    }
}

console.log("interactive skill templates passed");
