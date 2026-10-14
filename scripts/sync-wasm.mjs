// Copy a WASM build plus the JS bindings into bundle directories and refresh
// each BUILD-MANIFEST.json (artifact sizes/hashes, source commit, build date).
// Usage: node scripts/sync-wasm.mjs <build-dir> <bundle-dir>...
import {execFileSync} from "node:child_process";
import {createHash} from "node:crypto";
import {existsSync, readFileSync, writeFileSync} from "node:fs";
import {dirname, join, resolve} from "node:path";
import {fileURLToPath} from "node:url";

const root = resolve(dirname(fileURLToPath(import.meta.url)), "..");
const [build, ...bundles] = process.argv.slice(2);
if (!build || bundles.length === 0) {
    console.error("usage: node scripts/sync-wasm.mjs <build-dir> <bundle-dir>...");
    process.exit(2);
}

const sources = {
    "vis-wasm.js": join(build, "vis-wasm.js"),
    "vis-wasm.wasm": join(build, "vis-wasm.wasm"),
    "client.js": join(root, "src/bindings/js/client.js"),
    "client.d.ts": join(root, "src/bindings/js/client.d.ts"),
    "bindings.js": join(root, "src/bindings/js/bindings.js"),
    "bindings.d.ts": join(root, "src/bindings/js/bindings.d.ts"),
    "JetBrainsMono.ttf": join(root, "src/examples/assets/JetBrainsMono.ttf"),
    "JetBrainsMono-OFL.txt": join(root, "src/examples/assets/JetBrainsMono-OFL.txt"),
    // Licenses for everything compiled into vis-wasm.wasm.
    "LICENSE-viscript.txt": join(root, "LICENSE"),
    "LICENSE-ThorVG.txt": join(root, "licenses/LICENSE-ThorVG.txt"),
    "LICENSE-LodePNG.txt": join(root, "licenses/LICENSE-LodePNG.txt"),
    "LICENSE-Lua.txt": join(root, "licenses/LICENSE-Lua.txt"),
    "LICENSE-miniaudio.txt": join(root, "licenses/LICENSE-miniaudio.txt"),
    "NotoSansMath-OFL.txt": join(root, "src/core/renderer/tvg/NotoSansMath-OFL.txt"),
};
for (const [name, path] of Object.entries(sources)) {
    if (!existsSync(path)) throw new Error(`missing ${name}: ${path}`);
}

const git = (...args) => execFileSync("git", ["-C", root, ...args], {encoding: "utf8"}).trim();
const commit = git("rev-parse", "--short=12", "HEAD");
const dirty = git("status", "--porcelain").length > 0;

for (const bundle of bundles) {
    // write the bytes rather than copyFileSync: on APFS a copy clones the linker's file and keeps its
    // preallocated blocks, so a 1.3 MB wasm took 2.2 MB on disk
    for (const [name, path] of Object.entries(sources)) writeFileSync(join(bundle, name), readFileSync(path));

    const manifestPath = join(bundle, "BUILD-MANIFEST.json");
    const manifest = JSON.parse(readFileSync(manifestPath, "utf8"));
    manifest.sourceCommit = commit;
    manifest.sourceDirty = dirty;
    manifest.buildDate = new Date().toISOString().slice(0, 10);
    for (const path of Object.keys(sources)) {
        if (!manifest.artifacts.some((artifact) => artifact.path === path)) manifest.artifacts.push({path});
    }
    // drop artifacts that are no longer shipped, such as a removed loader's license
    manifest.artifacts = manifest.artifacts.filter(({path}) => existsSync(join(bundle, path))).map(({path}) => {
        const bytes = readFileSync(join(bundle, path));
        return {path, bytes: bytes.length, sha256: createHash("sha256").update(bytes).digest("hex")};
    });
    writeFileSync(manifestPath, `${JSON.stringify(manifest, null, 2)}\n`);
    console.log(`synced ${bundle}`);
}
