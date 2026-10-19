import assert from "node:assert/strict";
import fs from "node:fs";
import path from "node:path";
import {fileURLToPath} from "node:url";
import {createVis} from "../runtime/client.js";

const extensionRoot = path.dirname(path.dirname(fileURLToPath(import.meta.url)));
const wasmBinary = fs.readFileSync(path.join(extensionRoot, "runtime", "vis-wasm.wasm"));

const runtime = await createVis(`
local root = vis.scene {width = 200, height = 100, camera = {height = 2}}
root:rectangle {center = {1, 0}, size = {1, 1}, fill = "#fff", id = "root-box"}
local child = vis.scene {width = 100, height = 100, camera = {height = 2}}
local panel = child:rectangle {center = {0, 0}, size = {1, 1}, fill = "#fff", id = "panel"}
panel:rectangle {center = {0.55, 0}, size = {0.4, 0.4}, fill = "#fff", id = "overflow"}
root:viewport(child, {x = 0.5, y = 0, width = 0.5, height = 1})
return root
`, "layout-report.lua", {wasmBinary});

const report = runtime.layoutReport(0, 2);
assert.equal(report.scenes.length, 2);
const rootBox = report.objects.find((object) => object.scenePath === "root" && object.id === "root-box");
const panel = report.objects.find((object) => object.scenePath === "root/viewport:0" && object.id === "panel");
const overflow = report.objects.find((object) => object.scenePath === "root/viewport:0" && object.id === "overflow");
assert.ok(rootBox && panel && overflow);
assert.equal(overflow.parent, panel.index);
const rootVisual = report.visuals.find((visual) => visual.object === rootBox.index);
const panelVisual = report.visuals.find((visual) => visual.object === panel.index);
assert.ok(rootVisual && panelVisual);
assert.equal(rootVisual.occluded, true);
assert.equal(rootVisual.occluder, panel.scene);
assert.equal(panelVisual.occluded, false);
assert.ok(!report.collisions.some((item) =>
    (item.first === rootBox.index && item.second === panel.index)
    || (item.first === panel.index && item.second === rootBox.index)));
const containment = report.containments.find((item) =>
    item.container === panel.index && item.content === overflow.index);
assert.ok(containment);
assert.equal(containment.contained, false);
assert.ok(containment.overflow.right > 0);

// A code block is a typeset Group: its token Texts form one block.
runtime.font("JetBrainsMono", fs.readFileSync(path.join(extensionRoot, "runtime", "JetBrainsMono.ttf")), "ttf");
runtime.load(`
local root = vis.scene {width = 200, height = 100, camera = {height = 2}}
root:group {id = "plain"}
root:code {text = "a = b", language = "plain", id = "snippet"}
return root
`, "layout-report-typeset.lua");
const typeset = runtime.layoutReport(0, 2);
const snippet = typeset.objects.find((object) => object.id === "snippet");
assert.equal(snippet?.typeset, true);
assert.equal(typeset.objects.find((object) => object.id === "plain")?.typeset, false);
const inSnippet = (object) => {
    for (let parent = object.parent; parent !== null; parent = typeset.objects[parent].parent) {
        if (parent === snippet.index) return true;
    }
    return false;
};
assert.ok(typeset.objects.some((object) => object.type === "text" && inSnippet(object)));

runtime.destroy();
console.log("whole-Scene layout report passed");
