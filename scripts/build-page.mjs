// Compile docs/page/content/**/*.mdx into ES modules the page shell imports, copy the runtime into
// docs/page/runtime/, and build the shared player (tools/player) into docs/page/build/player/.
// Usage: node scripts/build-page.mjs [--with-api] (API content is opt-in for local use)
import {cpSync, mkdirSync, readFileSync, readdirSync, rmSync, writeFileSync} from "node:fs";
import {dirname, join, relative, resolve} from "node:path";
import {fileURLToPath} from "node:url";
import {buildApi} from "./build-api.mjs";

const page = resolve(dirname(fileURLToPath(import.meta.url)), "../docs/page");
const {compile} = await import(resolve(page, "node_modules/@mdx-js/mdx/index.js"));
const {default: remarkGfm} = await import(resolve(page, "node_modules/remark-gfm/index.js"));
const {default: rehypeHighlight} = await import(resolve(page, "node_modules/rehype-highlight/index.js"));
const content = join(page, "content"), out = join(page, "build");
const withApi = process.argv.includes("--with-api");

// ```lua live``` fences become <Scene code="..." editable /> players; a ```js live``` fence
// right after one is the same scene in JavaScript and joins that player. Other adjacent fences
// tagged `tab=Label` are grouped into <Tabs><Tab label="Label">…</Tab></Tabs>.
const live = (node, language) => node?.type === "code" && node.lang === language && /\blive\b/.test(node.meta ?? "");
const tabOf = (node) => node?.type === "code" && !live(node, "lua") && /\btab=(\w+)/.exec(node.meta ?? "")?.[1];
const element = (name, attributes, children = []) => ({type: "mdxJsxFlowElement", name, children,
    attributes: Object.entries(attributes).map(([key, value]) => ({type: "mdxJsxAttribute", name: key, value}))});
const fences = () => (tree) => {
    const walk = (node) => {
        if (!node.children) return;
        for (let i = 0; i < node.children.length; i++) {
            const child = node.children[i];
            if (live(child, "lua")) {
                const js = live(node.children[i + 1], "js") ? node.children.splice(i + 1, 1)[0].value : null;
                node.children[i] = element("Scene", {code: child.value, ...(js ? {js} : {}), editable: null});
            } else if (tabOf(child)) {
                let end = i;
                while (tabOf(node.children[end + 1])) end++;
                const tabs = node.children.slice(i, end + 1).map((tab) => element("Tab", {label: tabOf(tab)}, [tab]));
                node.children.splice(i, end - i + 1, element("Tabs", {}, tabs));
            } else {
                walk(child);
            }
        }
    };
    walk(tree);
};
// Minimal `key: value` frontmatter: the key ends at the first colon, and the value keeps any others.
const frontmatter = (text) => {
    const m = text.match(/^---\n([\s\S]*?)\n---\n/);
    if (!m) return [{}, text];
    const meta = Object.fromEntries(m[1].split("\n").map((l) => l.split(":")).map(([k, ...v]) => [k.trim(), v.join(":").trim()]));
    return [meta, text.slice(m[0].length)];
};

const files = [];
const scan = (dir) => readdirSync(dir, {withFileTypes: true}).forEach((e) =>
    e.isDirectory() ? scan(join(dir, e.name)) : e.name.endsWith(".mdx") && files.push(join(dir, e.name)));
scan(content);
rmSync(out, {recursive: true, force: true});

let api = [];
if (withApi) {
    buildApi(join(out, "api"));
    api = JSON.parse(readFileSync(join(out, "api/manifest.json"), "utf8"));
}
const docs = [];
for (const file of files.sort((a, b) => a.localeCompare(b, "en", {numeric: true}))) {
    const id = relative(content, file).replace(/\.mdx$/, "");
    const [meta, body] = frontmatter(readFileSync(file, "utf8"));
    const target = join(out, id + ".js");
    let code = String(await compile(body, {jsxImportSource: "@page", remarkPlugins: [remarkGfm, fences], rehypePlugins: [[rehypeHighlight, {plainText: ["text"]}]]}));
    code = code.replace('"@page/jsx-runtime"', JSON.stringify(relative(dirname(target), join(page, "jsx-runtime.js")).replace(/^(?!\.)/, "./")));
    mkdirSync(dirname(target), {recursive: true});
    writeFileSync(target, code);
    if (id.startsWith("docs/")) docs.push({id: id.slice(5).replace(/^[\d.]+-/, ""), file: id, ...meta});
}
writeFileSync(join(out, "manifest.js"), `export const DOCS = ${JSON.stringify(docs, null, 1)};\nexport const API = ${JSON.stringify(api, null, 1)};\n`);
console.log(`compiled ${files.length} mdx files`);

// The runtime the page ships, which the repository keeps only in the skill's bundle, and Pretendard
// Std, the page's "Pretendard" until a scene needs the full font.
const runtime = join(page, "runtime");
rmSync(runtime, {recursive: true, force: true});
cpSync(resolve(page, "../../tools/skill/vis-skills/assets/wasm"), runtime, {recursive: true});
cpSync(resolve(page, "../../src/examples/assets/PretendardStd-Regular.ttf"), join(runtime, "PretendardStd-Regular.ttf"));
console.log("copied the runtime into runtime/");

// The player and its render worker, with the runtime the page ships in runtime/.
const {buildWeb} = await import("../tools/player/build.mjs");
await buildWeb({outdir: join(out, "player"), runtimeDir: runtime});
console.log("built the player into build/player");
