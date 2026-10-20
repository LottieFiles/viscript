import assert from "node:assert/strict";
import {registerHooks} from "node:module";
import test from "node:test";
import type {CameraMove, InputEvent} from "../src/protocol.ts";

// The player takes key names from the runtime's client, as the bundle resolves it.
const client = new URL("../../skill/vis-skills/assets/wasm/client.js", import.meta.url).href;
registerHooks({resolve: (specifier, context, next) =>
    specifier === "vis-wasm-bundle" ? {url: client, shortCircuit: true} : next(specifier, context)});
const {bindInput} = await import("../src/player/input.ts");

const queued: InputEvent[] = [];
const moves: CameraMove[] = [];
// A canvas 200 by 100 CSS pixels at the page's top-left.
const canvas = Object.assign(new EventTarget(), {
    clientHeight: 100,
    getBoundingClientRect: () => ({left: 0, top: 0, width: 200, height: 100}),
    focus() {},
    setPointerCapture() {},
});
let live = true, camera = false, view: "2d" | "3d" = "3d", clicks = 0;
bindInput(canvas as unknown as HTMLCanvasElement, {
    live: () => live, camera: () => camera, view: () => view, size: () => [200, 100],
    queue: (event) => queued.push(event), moveCamera: (action, x, y) => moves.push({action, x, y}),
    releaseAll() {}, click: () => clicks++,
});
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
test("a click reaches the player only when the scene takes no input", () => {
    canvas.dispatchEvent(new Event("click"));
    assert.equal(clicks, 0);
    live = false;
    canvas.dispatchEvent(new Event("click"));
    live = true;
    assert.equal(clicks, 1);
});

// Dispatches a pointer event at (x, y) on the canvas, with the left button.
function point(type: string, x: number, y: number, init: Record<string, unknown> = {}) {
    const event = Object.assign(new Event(type, {cancelable: true}), {pointerId: 1, button: 0, buttons: 1, clientX: x, clientY: y,
        shiftKey: false}, init);
    canvas.dispatchEvent(event);
    return event;
}
function drag(points: readonly (readonly [number, number])[], init: Record<string, unknown> = {}) {
    const [first, ...rest] = points;
    point("pointerdown", ...first!, init);
    for (const at of rest) point("pointermove", ...at, init);
    point("pointerup", ...rest.at(-1) ?? first!, init);
    canvas.dispatchEvent(new Event("click"));
    return moves.splice(0);
}

test("a drag on a scene the camera may move orbits a 3D camera and pans a 2D one, and is not a click", () => {
    live = false;
    camera = true;
    clicks = 0;
    assert.deepEqual(drag([[100, 50], [102, 50], [120, 60]]), [{action: "orbit", x: 0.1, y: 0.1}]);
    assert.equal(clicks, 0);
    assert.deepEqual(drag([[100, 50], [80, 50]], {shiftKey: true}), [{action: "pan", x: -0.1, y: 0}]);
    view = "2d";
    assert.deepEqual(drag([[100, 50], [100, 30]]), [{action: "pan", x: 0, y: -0.2}]);
    // A press that barely moves is a click.
    assert.deepEqual(drag([[100, 50], [102, 51]]), []);
    assert.equal(clicks, 1);
    view = "3d";
    live = true;
    camera = false;
});

test("the wheel zooms and keys move the camera; with the camera off they stay the page's", () => {
    live = false;
    camera = true;
    const wheel = Object.assign(new Event("wheel", {cancelable: true}), {deltaY: 100, deltaMode: 0});
    canvas.dispatchEvent(wheel);
    assert.ok(wheel.defaultPrevented);
    assert.deepEqual(press("keydown", {key: "+"}), {input: [], taken: true});
    assert.deepEqual(press("keydown", {key: "ArrowLeft"}).taken, true);
    assert.deepEqual(press("keydown", {key: "3"}).taken, true);
    assert.deepEqual(press("keydown", {key: "x"}).taken, false);
    assert.deepEqual(moves.splice(0), [
        {action: "zoom", x: 0, y: 0.12}, {action: "zoom", x: 0, y: -0.12}, {action: "pan", x: 0.04, y: 0}, {action: "view3d", x: 0, y: 0},
    ]);
    camera = false;
    const turned = Object.assign(new Event("wheel", {cancelable: true}), {deltaY: 100, deltaMode: 0});
    canvas.dispatchEvent(turned);
    assert.equal(turned.defaultPrevented, false);
    assert.equal(press("keydown", {key: "+"}).taken, false);
    assert.deepEqual(drag([[100, 50], [140, 50]]), []);
    assert.deepEqual(moves, []);
    live = true;
});
