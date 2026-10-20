// Builds main.js, the one script Obsidian runs for the plugin. Obsidian installs a plugin as
// main.js, manifest.json and styles.css alone, so the render server, the compiler JavaScript
// scenes run in, the runtime binary and the bundled fonts go inside main.js, gzipped and
// base64-encoded; src/embedded.ts unpacks them.
//
//   node esbuild.config.mjs [--watch] [--no-minify] [--vault <vault folder>] [--skill]
//
// --vault copies the plugin into <vault>/.obsidian/plugins/viscript after each build.
// --skill bundles the installable plugin with the vis-obsidian skill.
import {build, context} from "esbuild";
import {copyFile, mkdir, readdir, readFile, stat} from "node:fs/promises";
import {builtinModules} from "node:module";
import {dirname, join, resolve} from "node:path";
import {fileURLToPath} from "node:url";
import {gzipSync} from "node:zlib";
import {bundleCompiler, bundleServer, runtime} from "../player/build.mjs";

const here = dirname(fileURLToPath(import.meta.url));
const root = resolve(here, "../..");
const runtimeDir = join(root, "tools/skill/vis-skills/assets/wasm");
const playerSources = join(root, "tools/player/src");

// The files src/embedded.ts reads as "viscript:embedded", besides the render server and the compiler:
// the runtime binary, and the fonts the engine's themes set text in, the files VS Code ships, so a
// scene looks the same in a note on any device as in the editor.
const PAYLOAD = {
    wasm: join(runtimeDir, "vis-wasm.wasm"),
    textFont: join(runtimeDir, "Pretendard.ttf"),
    codeFont: join(runtimeDir, "JetBrainsMono.ttf"),
};

// Everything in main.js whose license asks for its notice to travel with it.
const NOTICES = [
    ["viscript", join(runtimeDir, "LICENSE-viscript.txt")],
    ["ThorVG", join(runtimeDir, "LICENSE-ThorVG.txt")],
    ["Lua", join(runtimeDir, "LICENSE-Lua.txt")],
    ["LodePNG", join(runtimeDir, "LICENSE-LodePNG.txt")],
    ["miniaudio", join(runtimeDir, "LICENSE-miniaudio.txt")],
    ["CodeMirror legacy modes", join(here, "node_modules/@codemirror/legacy-modes/LICENSE")],
    ["Pretendard, the bundled text font, and Pretendard Std, built into the runtime as its default font",
        join(runtimeDir, "Pretendard-OFL.txt")],
    ["JetBrains Mono, the bundled code font", join(runtimeDir, "JetBrainsMono-OFL.txt")],
    ["Noto Sans Math, built into the runtime", join(runtimeDir, "NotoSansMath-OFL.txt")],
];

// Obsidian provides these to plugins; the rest is bundled.
const EXTERNAL = [
    "obsidian",
    "electron",
    "@codemirror/autocomplete",
    "@codemirror/collab",
    "@codemirror/commands",
    "@codemirror/language",
    "@codemirror/lint",
    "@codemirror/search",
    "@codemirror/state",
    "@codemirror/view",
    "@lezer/common",
    "@lezer/highlight",
    "@lezer/lr",
    ...builtinModules,
    ...builtinModules.map((name) => `node:${name}`),
];

async function banner() {
    const texts = await Promise.all(NOTICES.map(async ([name, path]) => {
        const text = (await readFile(path, "utf8")).trim().replace(/[\t ]+$/gm, "").replaceAll("*/", "* /");
        return `${name}\n\n${text}`;
    }));
    return `/*\nviscript for Obsidian. This file includes the following, under these licenses.\n\n${
        texts.join(`\n\n${"-".repeat(72)}\n\n`)}\n*/`;
}

async function sourcesIn(dir) {
    const entries = await readdir(dir, {recursive: true, withFileTypes: true});
    return entries.filter((entry) => entry.isFile()).map((entry) => join(entry.parentPath, entry.name));
}

// Packed once per change, since watch rebuilds load the virtual module every time.
const packedFiles = new Map();

async function packFile(path) {
    const {mtimeMs} = await stat(path);
    const cached = packedFiles.get(path);
    if (cached?.mtimeMs === mtimeMs) return cached.value;
    const value = pack(await readFile(path));
    packedFiles.set(path, {mtimeMs, value});
    return value;
}

function pack(bytes) {
    return gzipSync(bytes, {level: 9}).toString("base64");
}

function embedded({minify}) {
    return {
        name: "viscript-embedded",
        setup(build) {
            build.onResolve({filter: /^viscript:embedded$/}, (args) => ({path: args.path, namespace: "viscript-embedded"}));
            build.onLoad({filter: /.*/, namespace: "viscript-embedded"}, async () => {
                const [server, compiler, ...files] = await Promise.all([
                    bundleServer({runtimeDir, minify}),
                    bundleCompiler({runtimeDir, minify}),
                    ...Object.values(PAYLOAD).map(packFile),
                ]);
                const values = {
                    server: pack(Buffer.from(server)),
                    compiler: pack(Buffer.from(compiler)),
                    ...Object.fromEntries(Object.keys(PAYLOAD).map((key, at) => [key, files[at]])),
                };
                return {
                    contents: Object.entries(values).map(([name, value]) => `export const ${name} = ${JSON.stringify(value)};`).join("\n"),
                    loader: "js",
                    watchFiles: [
                        ...Object.values(PAYLOAD),
                        ...await sourcesIn(playerSources),
                        ...["client.js", "bindings.js", "vis-wasm.js"].map((name) => join(runtimeDir, name)),
                    ],
                };
            });
        },
    };
}

function copyPlugin(dir) {
    return {
        name: "copy-plugin",
        setup(build) {
            build.onEnd(async (result) => {
                if (result.errors.length) return;
                await mkdir(dir, {recursive: true});
                await Promise.all(["main.js", "manifest.json", "styles.css"].map((name) => copyFile(join(here, name), join(dir, name))));
                console.log(`copied to ${dir}`);
            });
        },
    };
}

const args = process.argv.slice(2);
const watch = args.includes("--watch");
const minify = !args.includes("--no-minify") && !watch;
const vaultAt = args.indexOf("--vault");
const vault = vaultAt >= 0 ? resolve(args[vaultAt + 1] ?? "") : null;
const {id} = JSON.parse(await readFile(join(here, "manifest.json"), "utf8"));
const destinations = [
    ...vault ? [join(vault, ".obsidian/plugins", id)] : [],
    ...args.includes("--skill") ? [join(root, "tools/skill/vis-obsidian/assets/plugin")] : [],
];

const options = {
    entryPoints: [join(here, "src/main.ts")],
    outfile: join(here, "main.js"),
    bundle: true,
    format: "cjs",
    platform: "browser",
    target: "es2022",
    // Node's modules load through require on the desktop app; a dynamic import would look for a URL.
    supported: {"dynamic-import": false},
    minify,
    sourcemap: watch ? "inline" : false,
    banner: {js: await banner()},
    external: EXTERNAL,
    plugins: [embedded({minify}), runtime(runtimeDir), ...destinations.map(copyPlugin)],
    logLevel: "info",
};

if (watch) {
    await (await context(options)).watch();
} else {
    await build(options);
}
