// Builds @viscript/player for a host: the main-thread module, the render server as one classic
// script that runs in a worker or, as a fallback, on the page, and the compiler that a sandbox
// runs JavaScript scenes with.
//
//   node build.mjs --runtime ../skill/vis-skills/assets/wasm --out dist [--no-minify]
//
// `runtime` is a runtime bundle directory (client.js, bindings.js, vis-wasm.js) as
// scripts/sync-wasm.mjs writes it, the skill's unless given. The server bundle includes the runtime's JavaScript; the
// host fetches vis-wasm.wasm itself and hands it to the server. The compiler bundle has only the
// JavaScript API (bindings.js), no runtime.
import {build} from "esbuild";
import {mkdir, writeFile} from "node:fs/promises";
import {dirname, join, resolve} from "node:path";
import {fileURLToPath} from "node:url";

const here = dirname(fileURLToPath(import.meta.url));

// Sources import the runtime as "vis-wasm-bundle" and its JavaScript API alone as
// "vis-wasm-bindings"; tsconfig.json maps both to the typings.
function runtime(runtimeDir) {
    return {
        name: "vis-wasm-bundle",
        setup(build) {
            build.onResolve({filter: /^vis-wasm-bundle$/}, () => ({path: resolve(runtimeDir, "client.js")}));
            build.onResolve({filter: /^vis-wasm-bindings$/}, () => ({path: resolve(runtimeDir, "bindings.js")}));
        },
    };
}

/** The render server's source, for `{code}` in createRenderHost or to write as player-worker.js. */
export async function bundleServer({runtimeDir, minify = true}) {
    if (!runtimeDir) throw new Error("bundleServer needs runtimeDir");
    const result = await build({
        entryPoints: [join(here, "src/worker.ts")],
        bundle: true,
        write: false,
        format: "iife",
        platform: "browser",
        target: "es2022",
        minify,
        // The emscripten loader resolves its own URL on start even when handed the compiled
        // module, and a worker started from a blob has no URL to resolve against.
        define: {"import.meta.url": JSON.stringify("https://viscript.invalid/vis-wasm.js")},
        plugins: [runtime(runtimeDir)],
        logLevel: "warning",
    });
    return result.outputFiles[0].text;
}

/** The compiler's source, for `{code}` in createSandboxCompiler or to write as player-compiler.js. */
export async function bundleCompiler({runtimeDir, minify = true}) {
    if (!runtimeDir) throw new Error("bundleCompiler needs runtimeDir");
    const result = await build({
        entryPoints: [join(here, "src/compiler.ts")],
        bundle: true,
        write: false,
        format: "iife",
        platform: "browser",
        target: "es2022",
        minify,
        plugins: [runtime(runtimeDir)],
        logLevel: "warning",
    });
    return result.outputFiles[0].text;
}

/** The main-thread module as one ES module. */
export async function bundlePlayer({minify = true} = {}) {
    const result = await build({
        entryPoints: [join(here, "src/index.ts")],
        bundle: true,
        write: false,
        format: "esm",
        platform: "browser",
        target: "es2022",
        minify,
        logLevel: "warning",
    });
    return result.outputFiles[0].text;
}

/** Writes player.js, player-worker.js and player-compiler.js to `outdir`. */
export async function buildWeb({outdir, runtimeDir, minify = true}) {
    const [player, server, compiler] = await Promise.all([
        bundlePlayer({minify}), bundleServer({runtimeDir, minify}), bundleCompiler({runtimeDir, minify}),
    ]);
    await mkdir(outdir, {recursive: true});
    await Promise.all([
        writeFile(join(outdir, "player.js"), player),
        writeFile(join(outdir, "player-worker.js"), server),
        writeFile(join(outdir, "player-compiler.js"), compiler),
    ]);
}

if (process.argv[1] && resolve(process.argv[1]) === fileURLToPath(import.meta.url)) {
    const args = process.argv.slice(2);
    const option = (name, fallback) => {
        const at = args.indexOf(name);
        return at >= 0 ? args[at + 1] : fallback;
    };
    const outdir = resolve(option("--out", join(here, "dist")));
    await buildWeb({
        outdir,
        runtimeDir: resolve(option("--runtime", join(here, "../skill/vis-skills/assets/wasm"))),
        minify: !args.includes("--no-minify"),
    });
    console.log(`built ${outdir}`);
}
