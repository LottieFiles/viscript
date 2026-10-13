import assert from "node:assert/strict";
import Module from "node:module";

class Range {
    constructor(startLine, startCharacter, endLine, endCharacter) {
        this.start = {line: startLine, character: startCharacter};
        this.end = {line: endLine, character: endCharacter};
    }
}

const uri = {toString: () => "file:///workspace/source.cpp"};
const fullRange = new Range(7, 0, 14, 1);
const selectionRange = new Range(8, 5, 8, 11);
let providedSymbols = [{
    name: "Container",
    range: new Range(0, 0, 20, 1),
    selectionRange: new Range(0, 6, 0, 15),
    children: [{
        name: "target(int)",
        range: fullRange,
        selectionRange,
        children: [],
    }],
}];
const vscode = {
    Range,
    commands: {
        executeCommand: async () => providedSymbols,
    },
};

const originalLoad = Module._load;
Module._load = function load(request, parent, isMain) {
    if (request === "vscode") return vscode;
    return originalLoad.call(this, request, parent, isMain);
};

try {
    const {explicitSourceRange, findSymbolRange, findSymbolRanges} = await import("../out/symbolLocator.js");
    const lines = [
        "void unrelated();",
        "void target() {",
        "}",
    ];
    const document = {
        uri,
        lineCount: lines.length,
        lineAt(line) {
            const text = lines[line];
            return {text, firstNonWhitespaceCharacterIndex: text.search(/\S|$/)};
        },
    };

    const located = await findSymbolRanges(document, "Namespace::target");
    assert.equal(located.range, fullRange);
    assert.equal(located.selectionRange, selectionRange);
    assert.equal(await findSymbolRange(document, "target"), selectionRange);

    const explicit = await findSymbolRanges(document, "target", 2);
    assert.deepEqual(explicit.range.start, {line: 1, character: 5});
    assert.deepEqual(explicit.range.end, {line: 1, character: 11});
    assert.equal(explicit.range, explicit.selectionRange);

    const manifestRange = explicitSourceRange(document, {
        startLine: 1,
        endLine: 2,
        startColumn: 2,
        endColumn: 7,
    });
    assert.deepEqual(manifestRange.start, {line: 0, character: 1});
    assert.deepEqual(manifestRange.end, {line: 1, character: 6});
    assert.equal(explicitSourceRange(document, {startLine: 4, endLine: 4}), undefined);
    assert.equal(explicitSourceRange(document, {
        startLine: 1,
        endLine: 1,
        startColumn: 2,
        endColumn: 99,
    }), undefined);

    providedSymbols = [];
    const fallback = await findSymbolRanges(document, "target");
    assert.deepEqual(fallback.range.start, {line: 1, character: 5});
    assert.deepEqual(fallback.range.end, {line: 1, character: 11});
    assert.equal(fallback.range, fallback.selectionRange);
} finally {
    Module._load = originalLoad;
}

console.log("symbol locator ranges passed");
