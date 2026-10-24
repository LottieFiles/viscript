// Keep the API reference exhaustive against the Lua registrations and public JS declarations.
// Usage: node scripts/check-page-api.mjs
import assert from "node:assert/strict";
import {readFileSync} from "node:fs";
import path from "node:path";
import {loadApiPages, loadApiExamples, apiExampleKey} from "./lib/api-markdown.mjs";

const root = new URL("../", import.meta.url);
const read = (path) => readFileSync(new URL(path, root), "utf8");
const pages = loadApiPages();
const examples = loadApiExamples(pages);
const rows = pages.flatMap((page) => page.rows);
// Only language columns count: mentioning a function in the introduction or a description
// must not hide an omitted entry. Escaped pipes may appear inside a signature.
const columns = rows.map((row) => row.split(/(?<!\\)\|/).slice(1, 3));
const documented = (column) => columns.map((cells) => cells[column] ?? "").join("\n");
const luaDocs = documented(0), jsDocs = documented(1);
const missing = [], checked = {Lua: new Set(), JavaScript: new Set()};
function requireCall(language, receiver, name, alternatives = []) {
    const candidates = [receiver, ...alternatives].map((prefix) => `${prefix}${name}`);
    const docs = language === "Lua" ? luaDocs : jsDocs;
    checked[language].add(candidates[0]);
    if (!candidates.some((call) => docs.includes(`${call}(`) || docs.includes(`${call} {`))) {
        missing.push(`${language}: ${candidates.join(" or ")}`);
    }
}
function registrations(source, table) {
    const body = new RegExp(`luaL_Reg ${table}\\[\\]\\s*=\\s*\\{([\\s\\S]*?)\\};`).exec(source)?.[1];
    assert.ok(body, `Cannot find Lua registration table ${table}`);
    const names = [...body.matchAll(/\{"(\w+)"\s*,/g)].map((match) => match[1]);
    assert.ok(names.length, `Empty Lua registration table ${table}`);
    return names;
}
const lua = read("src/core/lua/visLua.cpp");
for (const [table, receiver] of Object.entries({FACTORY_METHODS: "scene:", SCENE_METHODS: "scene:",
    OBJECT_METHODS: "object:", GROUP_METHODS: "group:", SPACE_METHODS: "space:", VIEW_METHODS: "view:"})) {
    for (const name of registrations(lua, table)) {
        // Shared factories need one entry; layout is exposed on Object but accepts Groups.
        const alternatives = receiver === "object:" ? ["scene:", "group:"] : [];
        requireCall("Lua", receiver, name, alternatives);
    }
}
const prelude = read("src/core/lua/visLuaTracker.h");
for (const [, name] of prelude.matchAll(/^function methods\.(\w+)\(/gm)) requireCall("Lua", "scene:", name);
for (const [, name] of prelude.matchAll(/tracker\.(\w+) = function/g)) requireCall("Lua", "tracker.", name);
const core = read("src/core/lua/visLuaApi.cpp");
const open = core.slice(core.indexOf("int detail::luaOpenApi("));
for (const [, name] of open.matchAll(/lua_pushcfunction\(lua, [^)]+\);\s*lua_setfield\(lua, -2, "(\w+)"\)/g)) {
    requireCall("Lua", "vis.", name);
}
for (const [module, receiver] of [["ui", "panel:"], ["input", "input:"], ["audio", "cue:"],
    ["states", "states:"], ["lua_runtime", "runtime:"]]) {
    const file = {ui: "visUiLua", input: "visInputLua", audio: "visAudioLua",
        states: "visStatesLua", lua_runtime: "visLuaRuntime"}[module];
    const source = read(`src/core/${module}/${file}.cpp`);
    for (const name of registrations(source, "methods")) requireCall("Lua", receiver, name);
    const section = source.slice(source.indexOf("static int _open("));
    const fields = [...section.matchAll(/lua_setfield\(lua, -2, "(\w+)"\)/g)].map((m) => m[1]);
    const namespace = ["ui", "input", "audio"].includes(module) ? fields.at(-1) : null;
    for (const name of fields.filter((name) => name !== namespace)) {
        requireCall("Lua", namespace ? `vis.${namespace}.` : "vis.", name);
    }
}
const diagram = read("src/core/diagram/visDiagramLua.cpp");
requireCall("Lua", "vis.", "diagram");
for (const name of registrations(diagram, "diagramMethods")) requireCall("Lua", "diagram:", name);
for (const name of registrations(diagram, "builtMethods")) requireCall("Lua", "built:", name);
const tex = read("src/core/tex/visTexLua.cpp");
for (const [, type, name] of tex.matchAll(/luaSetMethod\(lua, \w+::LuaHandle::(\w+), "(\w+)"\)/g)) {
    const receiver = {Scene: "scene:", Object: "object:", Group: "group:"}[type];
    assert.ok(receiver, `Unknown TeX receiver ${type}`);
    requireCall("Lua", receiver, name, type === "Object" ? ["scene:"] : []);
}

// Read each declaration block with balanced braces: nested parameter objects must not
// truncate the list of methods. Overloads count once, and inherited factories count once.
function block(source, declaration) {
    const match = declaration.exec(source);
    assert.ok(match, `Cannot find JS declaration ${declaration}`);
    const start = source.indexOf("{", match.index);
    let depth = 1, end = start + 1;
    for (; end < source.length && depth; end++) {
        if (source[end] === "{") depth++;
        else if (source[end] === "}") depth--;
    }
    assert.equal(depth, 0, "Unbalanced JS declaration");
    return source.slice(start + 1, end - 1);
}
const bindings = read("src/bindings/js/bindings.d.ts").replace(/\/\*[\s\S]*?\*\//g, "");
const client = read("src/bindings/js/client.d.ts").replace(/\/\*[\s\S]*?\*\//g, "");
const types = {ObjectFactory: "scene.", ObjectHandle: "object.", GroupHandle: "group.",
    TexHandle: "group.", SpaceHandle: "space.", AudioCueHandle: "cue.", ViewHandle: "view.",
    Tracker: "tracker.", SceneBuilder: "scene.", AnimCurveFactory: "vis.animCurve.",
    VisScene: "host.", VisAudio: "host.audio."};
for (const [type, receiver] of Object.entries(types)) {
    const source = type.startsWith("Vis") ? client : bindings;
    const declaration = new RegExp(`export interface ${type}(?: extends [^{]+)?\\s*\\{`);
    const body = block(source, declaration);
    for (const [, name] of body.matchAll(/^    (\w+)(?:<[^\n;]*>)?\??\(/gm)) {
        requireCall("JavaScript", receiver, name);
    }
}
for (const [, name] of `${bindings}\n${client}`.matchAll(/export function (\w+)\(/g)) {
    requireCall("JavaScript", "", name);
}
const vis = block(bindings, /export const vis: Readonly</);
for (const [, name] of vis.matchAll(/^    (\w+)\(/gm)) requireCall("JavaScript", "vis.", name);
// The callable generator interface and sandbox replacements are not handle methods.
requireCall("JavaScript", "", "rng");
for (const [receiver, name] of [["", "tostring"], ["string.", "format"],
    ["math.", "randomseed"], ["math.", "random"]]) requireCall("Lua", receiver, name);
assert.deepEqual(missing, [], `Undocumented APIs:\n${missing.join("\n")}`);
let exampleRows = 0;
const byId = new Map(pages.map((page) => [page.id, page]));
const byPath = new Map(pages.map((page) => [page.absolutePath, page]));
assert.equal(byId.size, pages.length, "Duplicate API page ids");
for (const page of pages) {
    const covered = new Map();
    const ids = new Set();
    for (const example of page.examples) {
        assert.ok(!ids.has(example.id), `${page.id}: duplicate example ${example.id} on this page`);
        ids.add(example.id);
        for (const key of example.covers ?? [example.id]) covered.set(key, example);
    }
    for (const row of page.rows) {
        const signatures = row.split(/(?<!\\)\|/).slice(1, 3).map((cell) => /`([^`]+)`/.exec(cell)?.[1] ?? "");
        const key = apiExampleKey(...signatures);
        if (!key) continue;
        const example = covered.get(key);
        assert.ok(example, `${page.id}: ${key} needs an example on this page`);
        assert.ok(example.code || example.hostCode, `${key}: example has no executable source`);
        if (signatures[1] && !example.hostCode) assert.ok(example.js, `${key}: missing JavaScript example`);
        exampleRows++;
    }
    const seen = new Set([page.id]);
    for (let parent = page.parent; parent; parent = byId.get(parent).parent) {
        assert.ok(byId.has(parent), `${page.id}: missing parent ${parent}`);
        assert.ok(!seen.has(parent), `${page.id}: cycle in API page tree`);
        seen.add(parent);
    }
    for (const link of page.links) {
        if (/^(?:[a-z][\w+.-]*:|\/\/|#)/i.test(link)) continue;
        let target;
        assert.doesNotThrow(() => { target = decodeURIComponent(link.split(/[?#]/)[0]); },
            `${page.id}: invalid escaped Markdown link ${link}`);
        if (!target.endsWith(".md")) continue;
        assert.ok(byPath.has(path.resolve(path.dirname(page.absolutePath), target)),
            `${page.id}: broken Markdown page link ${link}`);
    }
}
console.log(`API reference covers ${checked.Lua.size} Lua and ${checked.JavaScript.size} JavaScript entries from bindings and declarations.`);
console.log(`${pages.length} API pages: ${exampleRows} API rows have examples in their own categories; page tree and links verified.`);
console.log(`${examples.length} unique examples read from Markdown: ${examples.filter((example) => example.code).length} scenes and ${examples.filter((example) => example.hostCode).length} host snippets.`);
