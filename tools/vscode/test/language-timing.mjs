import assert from "node:assert/strict";
import Module from "node:module";
import path from "node:path";
import {fileURLToPath} from "node:url";

const root = path.dirname(path.dirname(fileURLToPath(import.meta.url)));
const providers = {};
class Position {constructor(line, character) {Object.assign(this, {line, character});}}
class Range {constructor(start, end) {Object.assign(this, {start, end});}}
class MarkdownString {
    constructor(value = "") {this.value = value;}
    appendCodeblock(value) {this.value += value; return this;}
    appendMarkdown(value) {this.value += value; return this;}
}
class Item {constructor(label) {this.label = label;}}
const vscode = {
    Position, Range, MarkdownString, CompletionItem: Item, SnippetString: Item,
    SignatureHelp: class {}, SignatureInformation: Item, ParameterInformation: Item,
    CompletionItemKind: {Field: 5},
    Uri: {joinPath: (uri, ...parts) => ({fsPath: path.join(uri.fsPath, ...parts)})},
    languages: Object.fromEntries(["CompletionItem", "Hover", "SignatureHelp"].map((name) =>
        [`register${name}Provider`, (_, provider) => {providers[name] = provider; return {dispose() {}};}])),
};
const load = Module._load;
Module._load = function(request, ...args) {return request === "vscode" ? vscode : load.call(this, request, ...args);};
try {
    const {registerLanguageFeatures} = await import("../out/languageProvider.js");
    registerLanguageFeatures({extensionUri: {fsPath: root}}, () => true);
    const fields = (call) => {
        const source = `local scene = vis.scene{}\nlocal a = scene:circle{}\nlocal b = scene:circle{}\n${call}`;
        const document = {getText: () => source, lineAt: () => ({text: call})};
        return providers.CompletionItem.provideCompletionItems(document, new Position(3, call.length))
            ?.map((item) => item.label).sort();
    };
    assert.deepEqual(fields("scene:shift(a, {1, 0}, {"), ["curve", "duration"]);
    assert.deepEqual(fields("scene:create({a, b}, {"), ["curve", "direction", "duration", "lag"]);
    assert.deepEqual(fields("scene:morph(a, b, {"), ["copy", "curve", "duration", "lag", "pairs"]);
    assert.deepEqual(fields("scene:arrange({a, b}, {"), ["center", "curve", "direction", "duration", "gap"]);
    assert.deepEqual(fields("scene:shift(a, {1, 0}, {duration = 1, "), ["curve"]);
    assert.ok(fields("scene:rectangle {").includes("size"));
    const signature = (call) => providers.SignatureHelp.provideSignatureHelp({
        getText: () => `local scene = vis.scene{}\n${call}`,
    }, new Position(1, call.length));
    const named = signature("scene:create({a, b}, {duration = ");
    assert.equal(named.activeSignature, 1);
    assert.equal(named.activeParameter, 1);
    assert.match(named.signatures[1].label, /options: RevealOptions/);
    assert.equal(signature("scene:create(a, 0.4, ").activeSignature, 0);
} finally {
    Module._load = load;
}
console.log("Lua named timing completion passed");
