import assert from "node:assert/strict";
import test from "node:test";
import {scenePairAtLine, scenePairs} from "../src/code-pairs.ts";

const example = [
    "<!-- api-example: scene.circle -->", "", "```viscript loop", "local scene = vis.scene {}", "return scene", "```", "",
    "```viscript-js", "const scene = vis.scene({});", "return scene;", "```",
].join("\n");

test("one API example pairs its complete Lua and JS sources from either fence", () => {
    const pair = scenePairAtLine(example, 2);
    assert.ok(pair);
    assert.equal(pair.id, "scene.circle");
    assert.deepEqual(pair, scenePairAtLine(example, 7));
    assert.equal(pair.lua.source, "local scene = vis.scene {}\nreturn scene");
    assert.equal(pair.lua.fence, "```viscript loop");
    assert.equal(pair.lua.lineStart, 2);
    assert.equal(pair.js.lineEnd, 10);
    assert.equal(pair.js.source, "const scene = vis.scene({});\nreturn scene;");
    assert.equal(scenePairAtLine(example, 11), null);
});

test("unmarked adjacent scenes and scenes separated by prose keep their own players", () => {
    assert.equal(scenePairAtLine(example.replace("<!-- api-example: scene.circle -->", "### Independent scenes"), 2), null);
    assert.equal(scenePairAtLine(example.replace("\n\n```viscript-js", "\nA different example:\n```viscript-js"), 2), null);
    assert.equal(scenePairAtLine(example.replace("```viscript-js", "```viscript"), 2), null);
});

test("grouped API examples pair when coverage comments follow their example marker", () => {
    const grouped = example.replace("<!-- api-example: scene.circle -->", [
        "<!-- api-example: group.reveal -->", "<!-- api-covers: scene.create scene.uncreate -->", "",
        "<!-- api-covers: property.progress -->",
    ].join("\n"));
    const pair = scenePairAtLine(grouped, 5);
    assert.ok(pair);
    assert.equal(pair.id, "group.reveal");
    assert.equal(pair.lua.source, "local scene = vis.scene {}\nreturn scene");
    assert.deepEqual(scenePairAtLine(grouped, 10), pair);
    assert.equal(scenePairAtLine(grouped.replace("<!-- api-example: group.reveal -->", ""), 5), null);
});

test("a stable example ID finds updated variants after source edits move its fences", () => {
    const original = scenePairs(example);
    const edited = scenePairs("Introductory prose\n\n" + example.replace("return scene;", "scene.wait(7);\nreturn scene;"));
    assert.equal(edited.length, 1);
    assert.equal(edited[0]?.id, original[0]?.id);
    assert.equal(edited[0]?.lua.lineStart, original[0]!.lua.lineStart + 2);
    assert.match(edited[0]!.js.source, /scene\.wait\(7\)/);
    assert.equal(edited[0]?.lua.source, original[0]?.lua.source);
});

test("partial or nested fences cannot hide an unrelated scene", () => {
    assert.equal(scenePairAtLine(example.slice(0, example.lastIndexOf("```")), 2), null);
    const quotedCode = "````markdown\n" + example + "\n````\n";
    assert.equal(scenePairAtLine(quotedCode, 3), null);
    const middle = example.replace("\n\n```viscript-js", "\n\n```text\nseparate\n```\n\n```viscript-js");
    assert.equal(scenePairAtLine(middle, 2), null);
});

test("API examples retain Windows newlines, tilde fences and callout contents", () => {
    const windows = example.replaceAll("\n", "\r\n").replaceAll("```", "~~~~").replace("viscript loop", "viscript-lua loop");
    assert.equal(scenePairAtLine(windows, 2)?.lua.language, "lua");
    const callout = example.split("\n").map((line) => "> " + line).join("\n");
    const pair = scenePairAtLine(callout, 7);
    assert.ok(pair);
    assert.equal(pair.lua.source, "local scene = vis.scene {}\nreturn scene");
    assert.equal(pair.js.source, "const scene = vis.scene({});\nreturn scene;");
    assert.equal(pair.lua.fence, "```viscript loop");
});
