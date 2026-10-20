import * as esbuild from "esbuild";
import {mkdir, rm, writeFile} from "node:fs/promises";
import {bundlePlayer, bundleServer} from "../player/build.mjs";

// The shared player, with the extension's runtime: the main-thread module and the render server,
// which the webview runs on its main thread. Lua scenes need no compiler.
await rm("media/player", {recursive: true, force: true});
await mkdir("media/player", {recursive: true});
const [playerModule, server] = await Promise.all([
    bundlePlayer({runtimeDir: "runtime"}),
    bundleServer({runtimeDir: "runtime"}),
]);
await Promise.all([
    writeFile("media/player/player.js", playerModule),
    writeFile("media/player/player-worker.js", server),
]);

await esbuild.build({
    entryPoints: ["webview/savers.js"],
    bundle: true,
    format: "esm",
    platform: "browser",
    target: "chrome120",
    outfile: "media/savers.js",
    minify: true,
    sourcemap: false,
});

await rm("media/shiki", {recursive: true, force: true});
await esbuild.build({
    entryPoints: {"syntax-highlighter": "webview/syntax-highlighter.js"},
    bundle: true,
    format: "esm",
    platform: "browser",
    target: "chrome120",
    outdir: "media/shiki",
    entryNames: "[name]",
    chunkNames: "chunks/[name]-[hash]",
    splitting: true,
    minify: true,
    sourcemap: false,
    legalComments: "inline",
});
