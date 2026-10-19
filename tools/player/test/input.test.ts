import assert from "node:assert/strict";
import {registerHooks} from "node:module";
import test from "node:test";
import type {InputEvent} from "../src/protocol.ts";

// The player takes key names from the runtime's client, as the bundle resolves it.
const client = new URL("../../skill/vis-skills/assets/wasm/client.js", import.meta.url).href;
registerHooks({resolve: (specifier, context, next) =>
    specifier === "vis-wasm-bundle" ? {url: client, shortCircuit: true} : next(specifier, context)});
const {bindInput} = await import("../src/player/input.ts");

const queued: InputEvent[] = [];
const canvas = new EventTarget();
bindInput(canvas as HTMLCanvasElement, {live: () => true, size: () => [200, 100], queue: (event) => queued.push(event), releaseAll() {}});
// Dispatches a key event on the canvas: the queued input it gave, and whether it took the key.
function press(type: "keydown" | "keyup", init: Partial<KeyboardEvent>) {
    const event = Object.assign(new Event(type, {cancelable: true}), {code: "", repeat: false, shiftKey: false, ctrlKey: false, altKey: false, metaKey: false}, init);
    canvas.dispatchEvent(event);
    return {input: queued.splice(0), taken: event.defaultPrevented};
}

test("= and _ are the Plus and Minus keys, as in VS Code", () => {
    assert.deepEqual(press("keydown", {key: "="}).input, [{type: "keydown", key: "Plus", buttons: 0, modifiers: 0}]);
    assert.deepEqual(press("keyup", {key: "+", shiftKey: true}).input, [{type: "keyup", key: "Plus", buttons: 0, modifiers: 1}]);
    assert.deepEqual(press("keydown", {key: "_", shiftKey: true}).input.map((event) => event.key), ["Minus"]);
    assert.deepEqual(press("keyup", {key: "-"}).input.map((event) => event.key), ["Minus"]);
});
test("a letter of another layout is the key at its place", () =>
    assert.deepEqual(press("keydown", {key: "ㅈ", code: "KeyW"}).input.map((event) => event.key), ["W"]));
test("keys the runtime does not name, and shortcuts, stay the page's", () => {
    assert.deepEqual(press("keydown", {key: "F12", code: "F12"}), {input: [], taken: false});
    assert.deepEqual(press("keydown", {key: "w", code: "KeyW", ctrlKey: true}), {input: [], taken: false});
});
test("a held key's repeats are taken but not queued", () =>
    assert.deepEqual(press("keydown", {key: "a", code: "KeyA", repeat: true}), {input: [], taken: true}));
