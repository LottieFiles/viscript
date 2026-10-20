import type {SceneLanguage} from "../../player/src/protocol.ts";

/** A complete scene fence in the note, with zero-based Markdown line numbers. */
export interface SceneFence {
    readonly language: SceneLanguage;
    readonly source: string;
    readonly fence: string;
    readonly lineStart: number;
    readonly lineEnd: number;
}

export interface ScenePair {
    readonly id: string;
    readonly lua: SceneFence;
    readonly js: SceneFence;
}

const LANGUAGES: Readonly<Record<string, SceneLanguage>> = {
    viscript: "lua", "viscript-lua": "lua", "viscript-js": "js",
};
const OPEN = /^([ \t]*(?:>[ \t]*)*)(`{3,}|~{3,})[ \t]*([^\s`~]*)[^\n]*$/;
const EXAMPLE = /^<!--\s*api-example:\s*([^<>]+?)\s*-->$/;
const COVERS = /^<!--\s*api-covers:\s*[^<>]+\s*-->$/;

/**
 * The API reference keeps both language variants in ordinary Markdown. Only fences under an
 * explicit example marker are counterparts; two unrelated scenes must keep their own players.
 */
export function scenePairs(markdown: string): ScenePair[] {
    const lines = markdown.split(/\r?\n/);
    const fences: (SceneFence & {readonly prefix: string})[] = [];
    for (let at = 0; at < lines.length; at++) {
        const open = OPEN.exec(lines[at]!);
        if (!open) continue;
        const [, prefix = "", ticks = "", name = ""] = open;
        let end = at + 1;
        // Skip every fenced block, including ordinary code containing apparent scene fences.
        for (; end < lines.length; end++) {
            if (!lines[end]!.startsWith(prefix)) continue;
            const closing = lines[end]!.slice(prefix.length).trim();
            if (closing.length >= ticks.length && [...closing].every((char) => char === ticks[0])) break;
        }
        if (end === lines.length) break;
        const language = LANGUAGES[name];
        if (language) {
            fences.push({
                language, prefix, fence: lines[at]!.slice(prefix.length), lineStart: at, lineEnd: end,
                source: lines.slice(at + 1, end).map((text) => text.startsWith(prefix) ? text.slice(prefix.length) : text).join("\n"),
            });
        }
        at = end;
    }
    const pairs: ScenePair[] = [];
    for (let at = 0; at < fences.length - 1; at++) {
        const lua = fences[at]!, js = fences[at + 1]!;
        if (lua.language !== "lua" || js.language !== "js" || lua.prefix !== js.prefix) continue;
        const content = (start: number, end: number) => lines.slice(start, end)
            .map((text) => text.startsWith(lua.prefix) ? text.slice(lua.prefix.length) : text);
        if (content(lua.lineEnd + 1, js.lineStart).some((text) => text.trim())) continue;
        let marker = lua.lineStart - 1;
        while (marker >= 0) {
            const text = content(marker, marker + 1)[0]!.trim();
            if (text && !COVERS.test(text)) break;
            marker--;
        }
        const example = marker >= 0 ? EXAMPLE.exec(content(marker, marker + 1)[0]!.trim()) : null;
        if (example) pairs.push({id: example[1]!.trim(), lua, js});
    }
    return pairs;
}

export function scenePairAtLine(markdown: string, line: number): ScenePair | null {
    return scenePairs(markdown).find((pair) => line >= pair.lua.lineStart && line <= pair.js.lineEnd) ?? null;
}
