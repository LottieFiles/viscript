import assert from "node:assert/strict";
import test from "node:test";
import {CODE_BLOCK, SCENE, untitledName} from "../src/new-scene.ts";

test("a new scene file is Untitled, numbered past the names its folder holds", () => {
    assert.equal(untitledName([], "lua"), "Untitled.lua");
    assert.equal(untitledName(["Untitled.md", "Untitled.canvas", "scene.lua"], "lua"), "Untitled.lua");
    assert.equal(untitledName(["Untitled.lua"], "lua"), "Untitled 1.lua");
    assert.equal(untitledName(["Untitled.lua", "Untitled 2.lua"], "lua"), "Untitled 1.lua");
});

test("names taken in another case are taken, as on a case-insensitive disk", () => {
    assert.equal(untitledName(["untitled.LUA", "UNTITLED 1.lua"], "lua"), "Untitled 2.lua");
});

test("the code block holds the scene a new file starts from", () => {
    assert.equal(CODE_BLOCK, "```viscript\n" + SCENE + "```\n");
    assert.match(SCENE, /^local scene = vis\.scene \{/);
    assert.match(SCENE, /\nreturn scene\n$/);
});
