import assert from "node:assert/strict";
import {addedFileRowsForRange, normalizedGitSource, unifiedDiffRowsForRange} from "../out/unifiedDiff.js";

const replacement = unifiedDiffRowsForRange(
    [
        "diff --git a/example.cpp b/example.cpp",
        "--- a/example.cpp",
        "+++ b/example.cpp",
        "@@ -1,4 +1,4 @@",
        " line one",
        "-old value",
        "+new value",
        " line three",
        " line four",
    ].join("\n"),
    "line one\nnew value\nline three\nline four",
    2,
    3,
);
assert.deepEqual(replacement, [
    {kind: "removed", oldLineNumber: 2, newLineNumber: undefined, text: "old value"},
    {kind: "inserted", oldLineNumber: undefined, newLineNumber: 2, text: "new value"},
    {kind: "context", oldLineNumber: 3, newLineNumber: 3, text: "line three"},
]);

assert.deepEqual(unifiedDiffRowsForRange(
    "@@ -1,2 +1,2 @@\n-old\n+new\n stable",
    "new\nstable",
    2,
    2,
), []);

const deletion = unifiedDiffRowsForRange(
    "@@ -1,3 +1,2 @@\n alpha\n-beta\n gamma",
    "alpha\ngamma",
    2,
    2,
);
assert.deepEqual(deletion, [
    {kind: "removed", oldLineNumber: 2, newLineNumber: undefined, text: "beta"},
    {kind: "context", oldLineNumber: 3, newLineNumber: 2, text: "gamma"},
]);

const endDeletion = unifiedDiffRowsForRange(
    "@@ -1,2 +1 @@\n alpha\n-beta",
    "alpha",
    1,
    1,
);
assert.deepEqual(endDeletion, [
    {kind: "context", oldLineNumber: 1, newLineNumber: 1, text: "alpha"},
    {kind: "removed", oldLineNumber: 2, newLineNumber: undefined, text: "beta"},
]);

const inserted = unifiedDiffRowsForRange(
    "@@ -1 +1,2 @@\n alpha\n+beta",
    "alpha\nbeta",
    1,
    2,
);
assert.deepEqual(inserted, [
    {kind: "context", oldLineNumber: 1, newLineNumber: 1, text: "alpha"},
    {kind: "inserted", oldLineNumber: undefined, newLineNumber: 2, text: "beta"},
]);

assert.equal(normalizedGitSource("a\r\nb\rc"), "a\nb\nc");
assert.deepEqual(unifiedDiffRowsForRange("", "alpha", 1, 1), []);
assert.deepEqual(unifiedDiffRowsForRange("@@ -1 +1 @@\n-a\n+b", "b", 2, 2), []);

assert.deepEqual(addedFileRowsForRange(
    "alpha\nbeta\ngamma\ndelta",
    2,
    3,
), [
    {kind: "inserted", oldLineNumber: undefined, newLineNumber: 2, text: "beta"},
    {kind: "inserted", oldLineNumber: undefined, newLineNumber: 3, text: "gamma"},
]);
assert.deepEqual(addedFileRowsForRange("alpha", 0, 1), []);
assert.deepEqual(addedFileRowsForRange("alpha", 1, 2), []);

console.log("unified diff range passed");
