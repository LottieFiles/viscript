import assert from "node:assert/strict";
import {
    normalizeSourceContexts,
    reconcileInspectorOpenState,
    resetSourceContextView,
    resolveActiveSourceIndex,
    sourceContextExpansionCount,
    sourceContextKeys,
    sourceContextRowIsFocused,
    sourceContextRows,
} from "../media/source-inspector.mjs";

const contexts = normalizeSourceContexts([
    {
        id: "dispatch-reference",
        label: "Dispatch",
        path: "src/renderer/tvggRaster.cpp",
        language: "cpp",
        startLine: 184,
        endLine: 186,
        focusStartLine: 185,
        focusEndLine: 185,
        totalLineCount: 420,
        hasMoreAbove: true,
        hasMoreBelow: true,
        expanded: true,
        lines: ["if (ready) {", "    dispatch();", "}"],
        truncated: true,
        unavailable: false,
    },
    {label: "Fallback", path: "src/fallback.cpp", startLine: 40, lines: ["fallback();"]},
]);

assert.equal(contexts.length, 2);
assert.deepEqual(contexts[0], {
    kind: "source",
    id: "dispatch-reference",
    label: "Dispatch",
    path: "src/renderer/tvggRaster.cpp",
    language: "cpp",
    startLine: 184,
    endLine: 186,
    focusStartLine: 185,
    focusEndLine: 185,
    totalLineCount: 420,
    hasMoreAbove: true,
    hasMoreBelow: true,
    baseStartLine: 185,
    baseEndLine: 185,
    expanded: true,
    lines: ["if (ready) {", "    dispatch();", "}"],
    truncated: true,
    unavailable: false,
    oldPath: "",
    diffRows: undefined,
});
assert.deepEqual(sourceContextRows(contexts[0]), [
    {lineNumber: 184, text: "if (ready) {"},
    {lineNumber: 185, text: "    dispatch();"},
    {lineNumber: 186, text: "}"},
]);
assert.equal(sourceContextRowIsFocused(contexts[0], 184), false);
assert.equal(sourceContextRowIsFocused(contexts[0], 185), true);
assert.equal(sourceContextRowIsFocused(contexts[0], 186), false);
assert.equal(sourceContextRowIsFocused({...contexts[0], expanded: false}, 185), false);

const diffContext = normalizeSourceContexts([{
    kind: "diff",
    id: "verified-diff",
    label: "Dispatch change",
    path: "src/new.cpp",
    oldPath: "src/old.cpp",
    language: "cpp",
    startLine: 20,
    endLine: 21,
    focusStartLine: 20,
    focusEndLine: 21,
    lines: ["oldCall();", "newCall();", "return value;"],
    diffRows: [
        {kind: "removed", oldLineNumber: 20, text: "oldCall();"},
        {kind: "inserted", newLineNumber: 20, text: "newCall();"},
        {kind: "context", oldLineNumber: 21, newLineNumber: 21, text: "return value;"},
    ],
    unavailable: false,
}])[0];
assert.equal(diffContext.kind, "diff");
assert.equal(diffContext.oldPath, "src/old.cpp");
assert.deepEqual(sourceContextRows(diffContext), [
    {kind: "removed", oldLineNumber: 20, newLineNumber: undefined, lineNumber: 20, text: "oldCall();"},
    {kind: "inserted", oldLineNumber: undefined, newLineNumber: 20, lineNumber: 20, text: "newCall();"},
    {kind: "context", oldLineNumber: 21, newLineNumber: 21, lineNumber: 21, text: "return value;"},
]);
assert.equal(sourceContextRowIsFocused({...diffContext, expanded: true}, 20), false);
assert.deepEqual(resetSourceContextView(diffContext), diffContext);
assert.deepEqual(resetSourceContextView(contexts[0]), {
    ...contexts[0],
    startLine: 185,
    endLine: 185,
    hasMoreAbove: true,
    hasMoreBelow: true,
    expanded: false,
    lines: ["    dispatch();"],
    truncated: false,
});
assert.equal(sourceContextExpansionCount(contexts[0], "above"), 5);
assert.equal(sourceContextExpansionCount(contexts[0], "below"), 5);
assert.equal(sourceContextExpansionCount(contexts[0], "above", 10), 10);
assert.equal(sourceContextExpansionCount(contexts[0], "below", 20), 20);
assert.equal(sourceContextExpansionCount({...contexts[0], startLine: 6}, "above"), 5);
assert.equal(sourceContextExpansionCount({...contexts[0], endLine: 418}, "below"), 2);
assert.equal(sourceContextExpansionCount(contexts[0], "sideways"), 0);
assert.equal(sourceContextExpansionCount(contexts[0], "above", 0), 0);
assert.equal(contexts[1].endLine, 40);
assert.equal(contexts[1].baseStartLine, 40);
assert.equal(contexts[1].baseEndLine, 40);
assert.equal(contexts[1].expanded, false);
assert.equal(contexts[1].truncated, false);
assert.equal(contexts[1].unavailable, false);
const unavailable = normalizeSourceContexts([{
    label: "Missing",
    startLine: 99,
    endLine: 101,
    lines: [],
    unavailable: true,
}])[0];
assert.equal(unavailable.unavailable, true);
assert.equal(resetSourceContextView(unavailable).hasMoreAbove, false);
assert.equal(resetSourceContextView(unavailable).hasMoreBelow, false);
assert.equal(resolveActiveSourceIndex(contexts, 1), 1);
assert.equal(resolveActiveSourceIndex(contexts, 9), 0);
assert.equal(resolveActiveSourceIndex([], 0), -1);

const contextKeys = sourceContextKeys(contexts);
assert.equal(new Set(contextKeys).size, contexts.length);
assert.equal(new Set(sourceContextKeys([contexts[0], contexts[0]])).size, 2);
const samePresentationDifferentReferences = [
    {...contexts[0], id: "dispatch-range-a"},
    {...contexts[0], id: "dispatch-range-b"},
];
const referenceKeys = sourceContextKeys(samePresentationDifferentReferences);
assert.deepEqual(
    sourceContextKeys([...samePresentationDifferentReferences].reverse()),
    [...referenceKeys].reverse(),
);
assert.deepEqual(reconcileInspectorOpenState(contexts, undefined, 1, false), {
    notesOpen: false,
    sourceKeys: [contextKeys[1]],
});
assert.deepEqual(reconcileInspectorOpenState([], undefined, -1, false), {
    notesOpen: true,
    sourceKeys: [],
});

const multiOpen = {notesOpen: true, sourceKeys: contextKeys};
assert.deepEqual(reconcileInspectorOpenState(contexts, multiOpen, 0, true), multiOpen);
assert.deepEqual(
    reconcileInspectorOpenState([contexts[1], contexts[0]], multiOpen, 0, true),
    {notesOpen: true, sourceKeys: [contextKeys[1], contextKeys[0]]},
);
assert.deepEqual(
    reconcileInspectorOpenState([contexts[1]], multiOpen, 0, true),
    {notesOpen: true, sourceKeys: [contextKeys[1]]},
);
const shiftedContext = {...contexts[0], startLine: 190, endLine: 192};
assert.deepEqual(
    reconcileInspectorOpenState([shiftedContext], multiOpen, 0, true),
    {notesOpen: true, sourceKeys: [contextKeys[0]]},
);
assert.deepEqual(
    reconcileInspectorOpenState(contexts, {notesOpen: false, sourceKeys: []}, 0, true),
    {notesOpen: false, sourceKeys: []},
);
assert.deepEqual(normalizeSourceContexts(null), []);
assert.deepEqual(normalizeSourceContexts([...contexts, {label: "invalid", lines: "not an array"}]), []);
assert.deepEqual(normalizeSourceContexts([{
    kind: "diff",
    lines: ["bad"],
    diffRows: [{kind: "inserted", text: "bad"}],
}]), []);

console.log("source inspector passed");
