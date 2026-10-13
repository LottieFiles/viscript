import assert from "node:assert/strict";
import fs from "node:fs";
import path from "node:path";
import {fileURLToPath} from "node:url";
import {createVis} from "../runtime/client.js";

const extensionRoot = path.dirname(path.dirname(fileURLToPath(import.meta.url)));
const codeFont = fs.readFileSync(path.resolve(extensionRoot, "../../src/examples/assets/JetBrainsMono.ttf"));
const repositoryRoot = path.resolve(extensionRoot, "..", "..");
const examplesRoot = path.join(repositoryRoot, "src", "examples", "lua");
const runtimeRoot = path.join(extensionRoot, "runtime");
const wasmBinary = fs.readFileSync(path.join(runtimeRoot, "vis-wasm.wasm"));
const fontBytes = fs.readFileSync(path.join(runtimeRoot, "Pretendard.ttf"));
const assetPattern = /\b(?:asset|texture)\s*=\s*(["'])(.*?)\1/g;

const files = fs.readdirSync(examplesRoot)
    .filter((name) => name.endsWith(".lua"))
    .sort();

async function seekAllScenes(reuseRuntime) {
    let sharedRuntime;
    if (reuseRuntime) {
        sharedRuntime = await createRuntime();
        sharedRuntime.font("Pretendard", fontBytes, "ttf");
        sharedRuntime.font("JetBrainsMono", codeFont, "ttf");
    }
    for (const fileName of files) {
        const filePath = path.join(examplesRoot, fileName);
        const source = fs.readFileSync(filePath, "utf8");
        const runtime = sharedRuntime ?? await createRuntime();
        try {
            if (!sharedRuntime) runtime.font("Pretendard", fontBytes, "ttf");
            if (!sharedRuntime) runtime.font("JetBrainsMono", codeFont, "ttf");
            for (const match of source.matchAll(assetPattern)) {
                const name = match[2];
                const bytes = fs.readFileSync(path.resolve(examplesRoot, name));
                runtime.asset(name, bytes, path.extname(name).slice(1));
            }
            runtime.loadLua(source, fileName);
            const duration = runtime.duration;
            const times = [
                0,
                duration,
                duration * 0.5,
                duration * 0.999999,
                duration * 0.125,
                duration * 0.875,
                duration * 0.25,
                duration * 0.75,
            ];
            for (let index = 0; index < 24; index += 1) {
                times.push(duration * (((index * 17) % 29) / 29));
            }
            for (const time of times) {
                assert.doesNotThrow(() => runtime.render(time), `${fileName} failed at ${time}/${duration}`);
            }
        } finally {
            if (!sharedRuntime) runtime.destroy();
        }
    }
    sharedRuntime?.destroy();
}

function createRuntime() {
    return createVis(
        "return vis.scene { width = 16, height = 16 }",
        "bootstrap.lua",
        {wasmBinary},
    );
}

await seekAllScenes(false);
await seekAllScenes(true);
console.log(`seeked ${files.length} Lua scenes in non-monotonic order with fresh and reused runtimes`);
