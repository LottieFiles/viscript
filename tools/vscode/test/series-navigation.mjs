import assert from "node:assert/strict";
import fs from "node:fs";
import path from "node:path";
import {fileURLToPath} from "node:url";

const extensionRoot = path.dirname(path.dirname(fileURLToPath(import.meta.url)));
const source = fs.readFileSync(path.join(extensionRoot, "src", "previewPanel.ts"), "utf8");
const start = source.indexOf("private async openSeries(");
const end = source.indexOf("private showDiagnostic(", start);
assert.ok(start >= 0 && end > start, "openSeries implementation must be present");
const openSeries = source.slice(start, end);
const followStart = source.indexOf("private async followDocument(");
const followEnd = source.indexOf("private async followActiveTab(", followStart);
assert.ok(followStart >= 0 && followEnd > followStart, "followDocument implementation must be present");
const followDocument = source.slice(followStart, followEnd);

assert.match(source, /private seriesOpenGeneration = 0;/);
assert.match(source, /private sourceRevealUri: string \| undefined;/);
assert.match(followDocument, /if \(this\.sourceRevealUri === document\.uri\.toString\(\)\) return;/);
assert.match(openSeries, /const generation = \+\+this\.seriesOpenGeneration;/);
assert.match(
    openSeries,
    /document = await vscode\.workspace\.openTextDocument\(item\.uri\);[\s\S]*if \(generation !== this\.seriesOpenGeneration \|\| documentGeneration !== this\.setDocumentGeneration\) return;/,
);
assert.match(openSeries, /await this\.setDocument\(document, context\);/);
assert.doesNotMatch(openSeries, /openSource|sourceTargetColumn/);

console.log("latest series navigation wins");
