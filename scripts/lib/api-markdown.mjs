// Read the API vault directly: Markdown is the source of both the reference and its examples.
import assert from "node:assert/strict";
import {readFileSync, readdirSync} from "node:fs";
import path from "node:path";
import {fileURLToPath} from "node:url";

const defaultDirectory = new URL("../../docs/api/content/", import.meta.url);
const sceneLanguages = new Set(["viscript", "viscript-lua", "viscript-js"]);

function markdownParts(source, context) {
    const lines = source.split("\n"), fences = [], outside = [];
    for (let start = 0; start < lines.length; start++) {
        const open = /^(`{3,}|~{3,})(.*)$/.exec(lines[start]);
        if (!open) {
            outside.push({index: start, text: lines[start]});
            continue;
        }
        const close = new RegExp(`^${open[1][0]}{${open[1].length},}\\s*$`);
        let end = start + 1;
        for (; end < lines.length && !close.test(lines[end]); end++);
        assert.ok(end < lines.length, `${context}:${start + 1}: unclosed code fence`);
        const [language, ...options] = open[2].trim().split(/\s+/);
        fences.push({start, end, language, options, code: lines.slice(start + 1, end).join("\n")});
        start = end;
    }
    return {lines, fences, outside};
}

function metadata(source, context) {
    const header = /^---\n([\s\S]*?)\n---(?:\n|$)/.exec(source);
    assert.ok(header, `${context}: missing YAML frontmatter`);
    const values = {};
    for (const line of header[1].split("\n")) {
        if (!line.trim() || line.trimStart().startsWith("#")) continue;
        const match = /^(\w+):\s*(.*?)\s*$/.exec(line);
        assert.ok(match, `${context}: unsupported frontmatter line ${line}`);
        let value = match[2];
        if (value.startsWith('"')) value = JSON.parse(value);
        else if (value.startsWith("'")) {
            assert.ok(value.endsWith("'"), `${context}: invalid quoted frontmatter`);
            value = value.slice(1, -1).replaceAll("''", "'");
        }
        values[match[1]] = value;
    }
    assert.ok(values.id && values.title, `${context}: id and title are required`);
    assert.ok(Number.isFinite(Number(values.order)), `${context}: numeric order is required`);
    return {id: values.id, title: values.title, parent: values.parent, order: Number(values.order)};
}

function pageExamples(parts, context) {
    const markers = parts.outside.filter(({text}) => /^<!-- api-example:/.test(text));
    const used = new Set(), examples = [];
    for (let i = 0; i < markers.length; i++) {
        const marker = markers[i];
        const id = /^<!-- api-example:\s*(\S+)\s*-->$/.exec(marker.text)?.[1];
        assert.ok(id, `${context}:${marker.index + 1}: invalid API example marker`);
        const next = markers[i + 1]?.index ?? parts.lines.length;
        const fence = parts.fences.find((candidate) => candidate.start > marker.index && candidate.start < next);
        assert.ok(fence, `${context}: ${id} has no code fence`);
        const prefix = parts.lines.slice(marker.index + 1, fence.start).join("\n");
        const covers = /<!-- api-covers:\s*([^>]+?)\s*-->/.exec(prefix)?.[1].split(/\s+/);
        assert.ok(!prefix.replace(/<!-- api-covers:[^>]+-->/g, "").trim(),
            `${context}: ${id} marker must immediately precede its code fence`);
        const label = parts.outside.filter(({index, text}) => index < marker.index && /^### /.test(text)).at(-1)?.text.slice(4);
        const example = {id, covers, label, interactive: fence.options.includes("interactive")};
        assert.ok(fence.code.trim(), `${context}: ${id} has empty source`);
        if (fence.language === "javascript" || fence.language === "js") example.hostCode = fence.code;
        else if (fence.language === "viscript-js") example.js = fence.code;
        else {
            assert.ok(sceneLanguages.has(fence.language), `${context}: ${id} uses unsupported fence ${fence.language}`);
            example.code = fence.code;
        }
        used.add(fence);
        let after = fence.end + 1;
        while (after < parts.lines.length && !parts.lines[after].trim()) after++;
        const pair = parts.fences.find((candidate) => candidate.start === after && candidate.start < next);
        if (example.code && pair?.language === "viscript-js") {
            assert.ok(pair.code.trim(), `${context}: ${id} has empty JavaScript source`);
            example.js = pair.code;
            used.add(pair);
        }
        examples.push(example);
    }
    for (const fence of parts.fences.filter((candidate) => sceneLanguages.has(candidate.language))) {
        assert.ok(used.has(fence), `${context}:${fence.start + 1}: scene fence needs an api-example marker or an adjacent Lua pair`);
    }
    return examples;
}

export function loadApiPages(directory = defaultDirectory) {
    const base = directory instanceof URL ? fileURLToPath(directory) : path.resolve(directory);
    function walk(folder) {
        return readdirSync(folder, {withFileTypes: true}).flatMap((entry) => {
            const filename = path.join(folder, entry.name);
            return entry.isDirectory() ? walk(filename) : entry.name.endsWith(".md") ? [filename] : [];
        });
    }
    return walk(base).sort().map((absolutePath) => {
        const filename = path.relative(base, absolutePath).split(path.sep).join("/");
        const source = readFileSync(absolutePath, "utf8").replaceAll("\r\n", "\n");
        const parts = markdownParts(source, filename);
        return {...metadata(source, filename), path: filename, absolutePath, source,
            rows: parts.outside.filter(({text}) => text.startsWith("|")).map(({text}) => text),
            links: [...parts.outside.map(({text}) => text).join("\n").matchAll(/\]\(([^)\s]+)(?:\s+"[^"]*")?\)/g)]
                .map((match) => match[1]),
            examples: pageExamples(parts, filename)};
    });
}

export function loadApiExamples(pagesOrDirectory = defaultDirectory) {
    const pages = Array.isArray(pagesOrDirectory) ? pagesOrDirectory : loadApiPages(pagesOrDirectory);
    const examples = new Map();
    for (const page of pages) for (const example of page.examples) {
        const existing = examples.get(example.id);
        if (existing) {
            for (const key of ["code", "js", "hostCode", "interactive", "covers"]) {
                assert.deepEqual(example[key], existing[key], `${page.path}: ${example.id} differs between API pages (${key})`);
            }
        } else examples.set(example.id, example);
    }
    return [...examples.values()];
}

// The Lua spelling is the canonical key for a paired example.
export function apiExampleKey(lua, js) {
    const name = (signature) => /^(?:new\s+)?([\w.:]+)\s*\(/.exec(signature)?.[1]?.replaceAll(":", ".");
    const key = name(lua) ?? name(js);
    if (key === "math.random") {
        if (lua.startsWith("math.random(0)")) return "math.random.0";
        if (lua.startsWith("math.random(upper)")) return "math.random.upper";
        if (lua.startsWith("math.random(lower,")) return "math.random.range";
    }
    if (key === "createVis" && js.startsWith("createVis(source,")) return "createVis.source";
    if (key) return key;
    if (/^host\.\w+$/.test(js)) return js;
    if (/^\w+$/.test(lua) && /^\w+$/.test(js)) return `property.${lua}`;
    return null;
}
