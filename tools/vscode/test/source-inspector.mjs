import assert from "node:assert/strict";
import {
    reconcileInspectorOpenState,
    resetSourceContextView,
    resolveActiveSourceIndex,
    sourceContextExpansionCount,
    sourceContextRowIsFocused,
    sourceContextRows,
} from "../media/source-inspector.mjs";

// Source contexts as the extension sends them.
const dispatch = {
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
};
const fallback = {
    kind: "source",
    id: "fallback-reference",
    label: "Fallback",
    path: "src/fallback.cpp",
    language: "cpp",
    startLine: 40,
    endLine: 40,
    focusStartLine: 40,
    focusEndLine: 40,
    totalLineCount: 80,
    hasMoreAbove: true,
    hasMoreBelow: true,
    baseStartLine: 40,
    baseEndLine: 40,
    expanded: false,
    lines: ["fallback();"],
    truncated: false,
    unavailable: false,
};
const contexts = [dispatch, fallback];

assert.deepEqual(sourceContextRows(dispatch), [
    {lineNumber: 184, text: "if (ready) {"},
    {lineNumber: 185, text: "    dispatch();"},
    {lineNumber: 186, text: "}"},
]);
assert.equal(sourceContextRowIsFocused(dispatch, 184), false);
assert.equal(sourceContextRowIsFocused(dispatch, 185), true);
assert.equal(sourceContextRowIsFocused(dispatch, 186), false);
assert.equal(sourceContextRowIsFocused({...dispatch, expanded: false}, 185), false);

const diffContext = {
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
    totalLineCount: 21,
    hasMoreAbove: true,
    hasMoreBelow: false,
    baseStartLine: 20,
    baseEndLine: 21,
    expanded: false,
    lines: ["oldCall();", "newCall();", "return value;"],
    truncated: false,
    unavailable: false,
    diffRows: [
        {kind: "removed", oldLineNumber: 20, text: "oldCall();"},
        {kind: "inserted", newLineNumber: 20, text: "newCall();"},
        {kind: "context", oldLineNumber: 21, newLineNumber: 21, text: "return value;"},
    ],
};
assert.deepEqual(sourceContextRows(diffContext), [
    {kind: "removed", oldLineNumber: 20, lineNumber: 20, text: "oldCall();"},
    {kind: "inserted", newLineNumber: 20, lineNumber: 20, text: "newCall();"},
    {kind: "context", oldLineNumber: 21, newLineNumber: 21, lineNumber: 21, text: "return value;"},
]);
assert.equal(sourceContextRowIsFocused({...diffContext, expanded: true}, 20), false);
assert.deepEqual(resetSourceContextView(diffContext), diffContext);
assert.deepEqual(resetSourceContextView(dispatch), {
    ...dispatch,
    startLine: 185,
    endLine: 185,
    hasMoreAbove: true,
    hasMoreBelow: true,
    expanded: false,
    lines: ["    dispatch();"],
    truncated: false,
});
assert.equal(sourceContextExpansionCount(dispatch, "above"), 5);
assert.equal(sourceContextExpansionCount(dispatch, "below"), 5);
assert.equal(sourceContextExpansionCount(dispatch, "above", 10), 10);
assert.equal(sourceContextExpansionCount(dispatch, "below", 20), 20);
assert.equal(sourceContextExpansionCount({...dispatch, startLine: 6}, "above"), 5);
assert.equal(sourceContextExpansionCount({...dispatch, endLine: 418}, "below"), 2);
assert.equal(sourceContextExpansionCount(dispatch, "sideways"), 0);
assert.equal(sourceContextExpansionCount(dispatch, "above", 0), 0);
const unavailable = {...dispatch, id: "missing", lines: [], unavailable: true};
assert.equal(resetSourceContextView(unavailable).hasMoreAbove, false);
assert.equal(resetSourceContextView(unavailable).hasMoreBelow, false);
assert.equal(resolveActiveSourceIndex(contexts, 1), 1);
assert.equal(resolveActiveSourceIndex(contexts, 9), 0);
assert.equal(resolveActiveSourceIndex([], 0), -1);

assert.deepEqual(reconcileInspectorOpenState(contexts, undefined, 1, false), {
    notesOpen: true,
    sourceKeys: [fallback.id],
});
assert.deepEqual(reconcileInspectorOpenState(contexts, {notesOpen: false, sourceKeys: []}, 0, false), {
    notesOpen: true,
    sourceKeys: [dispatch.id],
});
assert.deepEqual(reconcileInspectorOpenState([], undefined, -1, false), {
    notesOpen: true,
    sourceKeys: [],
});

const multiOpen = {notesOpen: true, sourceKeys: [dispatch.id, fallback.id]};
assert.deepEqual(reconcileInspectorOpenState(contexts, multiOpen, 0, true), multiOpen);
assert.deepEqual(
    reconcileInspectorOpenState([fallback, dispatch], multiOpen, 0, true),
    {notesOpen: true, sourceKeys: [fallback.id, dispatch.id]},
);
assert.deepEqual(
    reconcileInspectorOpenState([fallback], multiOpen, 0, true),
    {notesOpen: true, sourceKeys: [fallback.id]},
);
const shiftedContext = {...dispatch, startLine: 190, endLine: 192};
assert.deepEqual(
    reconcileInspectorOpenState([shiftedContext], multiOpen, 0, true),
    {notesOpen: true, sourceKeys: [dispatch.id]},
);
assert.deepEqual(
    reconcileInspectorOpenState([{...dispatch, id: "range-a"}, {...dispatch, id: "range-b"}],
        {notesOpen: true, sourceKeys: ["range-b"]}, 0, true),
    {notesOpen: true, sourceKeys: ["range-b"]},
    "open state follows the reference, not the presentation",
);
assert.deepEqual(
    reconcileInspectorOpenState(contexts, {notesOpen: false, sourceKeys: []}, 0, true),
    {notesOpen: false, sourceKeys: []},
);

console.log("source inspector passed");
