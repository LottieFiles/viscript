// Quartz compiles the API vault to HTML fragments consumed by the existing page shell.
import {cpSync, mkdirSync, rmSync} from "node:fs";
import {execFileSync} from "node:child_process";
import {dirname, join, resolve} from "node:path";
import {fileURLToPath} from "node:url";

const root = resolve(dirname(fileURLToPath(import.meta.url)), "..");
export function buildApi(outdir = join(root, "docs/page/build/api")) {
    execFileSync("npm", ["run", "quartz:build"], {cwd: join(root, "docs/api"), stdio: "inherit"});
    rmSync(outdir, {recursive: true, force: true});
    cpSync(join(root, "docs/api/public"), outdir, {recursive: true});
    const assets = join(outdir, "static/viscript");
    mkdirSync(assets, {recursive: true});
    cpSync(join(root, "docs/api/player"), assets, {recursive: true});
    console.log("compiled the API Markdown with Quartz into build/api/");
}

if (process.argv[1] && resolve(process.argv[1]) === fileURLToPath(import.meta.url)) buildApi();
