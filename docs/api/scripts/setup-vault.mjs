// Install the local plugin and prepare the API content folder as an Obsidian vault.
import {execFileSync} from "node:child_process";
import {existsSync, mkdirSync, readFileSync, writeFileSync} from "node:fs";
import {dirname, join, resolve} from "node:path";
import {fileURLToPath} from "node:url";

const api = resolve(dirname(fileURLToPath(import.meta.url)), "..");
const root = resolve(api, "../..");
const vault = join(api, "content");
const config = join(vault, ".obsidian");
const write = (path, value) => writeFileSync(path, JSON.stringify(value, null, 2) + "\n");
const read = (path, fallback) => existsSync(path) ? JSON.parse(readFileSync(path, "utf8")) : fallback;
const seed = (path, value) => { if (!existsSync(path)) write(path, value); };

execFileSync(process.execPath, [join(root, "tools/obsidian/esbuild.config.mjs"), "--vault", vault],
    {cwd: root, stdio: "inherit"});
mkdirSync(config, {recursive: true});
const community = join(config, "community-plugins.json");
write(community, [...new Set([...read(community, []), "viscript"])]);
seed(join(config, "app.json"), {defaultViewMode: "preview"});
seed(join(config, "core-plugins.json"), {
    "file-explorer": true, "global-search": true, switcher: true, graph: true,
    backlink: true, "outgoing-link": true, outline: true, "command-palette": true,
});
const pluginSettings = join(config, "plugins/viscript/data.json");
write(pluginSettings, {controls: "always", showSource: true, ...read(pluginSettings, {})});
seed(join(config, "graph.json"), {close: true});

const note = (id, file) => ({id, type: "leaf", state: {type: "markdown",
    state: {file, mode: "preview", source: false}, title: file.split("/").at(-1).replace(/\.md$/, "")}});
seed(join(config, "workspace.json"), {
    main: {id: "api-main", type: "split", direction: "vertical", children: [
        {id: "api-notes", type: "tabs", currentTab: 1, children: [note("api-index", "index.md"), note("api-shapes", "Object/shapes.md")]},
    ]},
    left: {id: "api-left", type: "split", direction: "horizontal", width: 250, collapsed: false, children: [
        {id: "api-files", type: "tabs", children: [{id: "api-explorer", type: "leaf", state: {type: "file-explorer", state: {sortOrder: "alphabetical"}}}]},
    ]},
    right: {id: "api-right", type: "split", direction: "horizontal", width: 300, collapsed: false, children: [
        {id: "api-connections", type: "tabs", children: [{id: "api-graph", type: "leaf", state: {type: "graph", state: {}}}]},
    ]},
    active: "api-shapes",
    lastOpenFiles: ["Object/shapes.md", "index.md"],
});

console.log(`Obsidian vault ready: ${vault}`);
console.log("Open this folder as a vault. On first open in a new profile, trust the bundled plugin when Obsidian asks.");
