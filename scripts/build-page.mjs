// Compile docs/page/content/**/*.mdx into ES modules the page shell imports, and copy the runtime
// into docs/page/runtime/.
// Usage: node scripts/build-page.mjs   (needs `npm ci` in docs/page)
import {cpSync, mkdirSync, readFileSync, readdirSync, rmSync, writeFileSync} from "node:fs";
import {dirname, join, relative, resolve} from "node:path";
import {fileURLToPath} from "node:url";

const page = resolve(dirname(fileURLToPath(import.meta.url)), "../docs/page");
const {compile} = await import(resolve(page, "node_modules/@mdx-js/mdx/index.js"));
const {default: remarkGfm} = await import(resolve(page, "node_modules/remark-gfm/index.js"));
const {default: rehypeHighlight} = await import(resolve(page, "node_modules/rehype-highlight/index.js"));
const content = join(page, "content"), out = join(page, "build");

// ```lua live``` fences become <Scene code="..." editable /> players; a ```js live``` fence
// right after one is the same scene in JavaScript and joins that player.
// Add `preview` (```lua live preview```) for a full-width player without the editor.
const live = (node, language) => node?.type === "code" && node.lang === language && /\blive\b/.test(node.meta ?? "");
// Adjacent fences tagged `tab=Label` are grouped into <Tabs><Tab label="Label">…</Tab></Tabs>.
const tabOf = (node) => node?.type === "code" && /\btab=(\w+)/.exec(node.meta ?? "")?.[1];
const codeTabs = () => (tree) => {
    const walk = (node) => {
        if (!node.children) return;
        for (let i = 0; i < node.children.length; i++) {
            if (!tabOf(node.children[i])) { walk(node.children[i]); continue; }
            let end = i;
            while (tabOf(node.children[end + 1])) end++;
            const tabs = node.children.slice(i, end + 1).map((code) => ({
                type: "mdxJsxFlowElement", name: "Tab", children: [code],
                attributes: [{type: "mdxJsxAttribute", name: "label", value: tabOf(code)}],
            }));
            node.children.splice(i, end - i + 1, {type: "mdxJsxFlowElement", name: "Tabs", attributes: [], children: tabs});
        }
    };
    walk(tree);
};
const liveScenes = () => (tree) => {
    const walk = (node) => {
        if (!node.children) return;
        for (let i = 0; i < node.children.length; i++) {
            const child = node.children[i];
            if (!live(child, "lua")) { walk(child); continue; }
            const js = live(node.children[i + 1], "js") ? node.children.splice(i + 1, 1)[0].value : null;
            node.children[i] = {type: "mdxJsxFlowElement", name: "Scene", children: [], attributes: [
                {type: "mdxJsxAttribute", name: "code", value: child.value},
                ...(js ? [{type: "mdxJsxAttribute", name: "js", value: js}] : []),
                // ```lua live preview``` plays full width without the editor
                ...(/\bpreview\b/.test(child.meta) ? [] : [{type: "mdxJsxAttribute", name: "editable", value: null}]),
            ]};
        }
    };
    walk(tree);
};
// Minimal `key: value` frontmatter.
const frontmatter = (text) => {
    const m = text.match(/^---\n([\s\S]*?)\n---\n/);
    if (!m) return [{}, text];
    const meta = Object.fromEntries(m[1].split("\n").map((l) => l.split(/:\s*/, 2)).map(([k, ...v]) => [k.trim(), l2(v)]));
    return [meta, text.slice(m[0].length)];
};
const l2 = (v) => v.join(": ").trim();

const files = [];
const scan = (dir) => readdirSync(dir, {withFileTypes: true}).forEach((e) =>
    e.isDirectory() ? scan(join(dir, e.name)) : e.name.endsWith(".mdx") && files.push(join(dir, e.name)));
scan(content);
rmSync(out, {recursive: true, force: true});

const docs = [];
for (const file of files.sort()) {
    const id = relative(content, file).replace(/\.mdx$/, "");
    const [meta, body] = frontmatter(readFileSync(file, "utf8"));
    const target = join(out, id + ".js");
    let code = String(await compile(body, {jsxImportSource: "@page", remarkPlugins: [remarkGfm, liveScenes, codeTabs], rehypePlugins: [[rehypeHighlight, {plainText: ["text"]}]]}));
    code = code.replace('"@page/jsx-runtime"', JSON.stringify(relative(dirname(target), join(page, "jsx-runtime.js")).replace(/^(?!\.)/, "./")));
    code += `\nexport const meta = ${JSON.stringify(meta)};\n`;
    mkdirSync(dirname(target), {recursive: true});
    writeFileSync(target, code);
    if (id.startsWith("docs/")) docs.push({id: id.slice(5).replace(/^[\d.]+-/, ""), file: id, ...meta});
}
writeFileSync(join(out, "manifest.js"), `export const DOCS = ${JSON.stringify(docs, null, 1)};\n`);
console.log(`compiled ${files.length} mdx files`);

// The runtime the page ships, which the repository keeps only in the skill's bundle.
const runtime = join(page, "runtime");
rmSync(runtime, {recursive: true, force: true});
cpSync(resolve(page, "../../tools/skill/vis-skills/assets/wasm"), runtime, {recursive: true});
console.log("copied the runtime into runtime/");
