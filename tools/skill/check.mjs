// Static skill checks: every relative Markdown link resolves, every
// `scene:name {`/`obj:name(` call in skill docs and templates is a bound Lua name, and
// the host review renders with the extension's fallback palettes.
// Usage: node tools/skill/check.mjs [repo-root]
import {readFileSync, readdirSync, existsSync, statSync} from "node:fs";
import {dirname, join, normalize, resolve} from "node:path";
import {fileURLToPath, pathToFileURL} from "node:url";

const root = resolve(process.argv[2] ?? join(dirname(fileURLToPath(import.meta.url)), "..", ".."));
const skills = join(root, "tools/skill");

const walk = (dir) => readdirSync(dir).flatMap((name) => {
    const path = join(dir, name);
    if (name === "assets") return [];
    return statSync(path).isDirectory() ? walk(path) : [path];
});
const files = walk(skills);
const errors = [];

for (const file of files.filter((f) => f.endsWith(".md"))) {
    const text = readFileSync(file, "utf8");
    for (const [, target] of text.matchAll(/\]\(([^)#\s]+)(?:#[^)]*)?\)/g)) {
        if (/^[a-z]+:/.test(target)) continue;
        if (!existsSync(normalize(join(dirname(file), target)))) errors.push(`${file}: broken link ${target}`);
    }
}

const bound = new Set();
for (const file of walk(join(root, "src/core")).filter((f) => /\.(cpp|h)$/.test(f))) {
    const text = readFileSync(file, "utf8");
    for (const [, name] of text.matchAll(/\{"([a-z_0-9]+)",\s*_/g)) bound.add(name);
    for (const [, name] of text.matchAll(/function methods\.([a-z_0-9]+)\(/g)) bound.add(name);
    // A module adds its methods to the handles one by one.
    for (const [, name] of text.matchAll(/luaSetMethod\(lua, [^,]+, "([a-z_0-9]+)"\)/g)) bound.add(name);
}
for (const file of files.filter((f) => /\.(md|lua)$/.test(f))) {
    const text = readFileSync(file, "utf8");
    for (const [, name] of text.matchAll(/\b[a-z_]+:([a-z_][a-z_0-9]*)\s*[{(]/g)) {
        if (!bound.has(name)) errors.push(`${file}: unknown Lua method ${name}`);
    }
}

// host-review.mjs keeps its own copy of the palettes the Viewer falls back to.
const {PALETTES} = await import(pathToFileURL(join(skills, "vis-vscode/scripts/host-review.mjs")).href);
const {resolveAdaptiveVscodeTheme} = await import(pathToFileURL(join(root, "tools/vscode/media/adaptive-theme.mjs")).href);
for (const mode of ["light", "dark"]) {
    const {dark, ...fallback} = resolveAdaptiveVscodeTheme({dark: mode === "dark", read: () => ""});
    const keys = new Set([...Object.keys(PALETTES[mode]), ...Object.keys(fallback)]);
    const differing = [...keys].filter((key) => JSON.stringify(PALETTES[mode][key]) !== JSON.stringify(fallback[key]));
    if (differing.length) errors.push(`host-review.mjs: the ${mode} palette differs from adaptive-theme.mjs in ${differing.join(", ")}`);
}

for (const error of errors) console.error(error);
console.log(`${files.length} files checked, ${errors.length} problems`);
process.exit(errors.length ? 1 : 0);
