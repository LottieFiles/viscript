import * as esbuild from "esbuild";
import {rm} from "node:fs/promises";

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
