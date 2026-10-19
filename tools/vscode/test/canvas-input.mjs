import assert from "node:assert/strict";
import fs from "node:fs";
import path from "node:path";
import {fileURLToPath} from "node:url";
import {
    INPUT_KEY_ACTION,
    canvasLogicalPoint,
    createCanvasInputRouter,
    inputKeyAction,
    inputModifiers,
    inputResult,
    isHostKeyTarget,
} from "../media/canvas-input.mjs";
import {inputKeyName} from "../runtime/client.js";

class EventTargetStub {
    constructor() {
        this.listeners = new Map();
    }

    addEventListener(type, listener) {
        const values = this.listeners.get(type) ?? [];
        values.push(listener);
        this.listeners.set(type, values);
    }

    removeEventListener(type, listener) {
        this.listeners.set(type, (this.listeners.get(type) ?? []).filter((value) => value !== listener));
    }

    emit(type, fields = {}) {
        const event = {
            type,
            target: this,
            clientX: 0,
            clientY: 0,
            button: -1,
            buttons: 0,
            pointerId: 0,
            deltaX: 0,
            deltaY: 0,
            deltaMode: 0,
            shiftKey: false,
            ctrlKey: false,
            altKey: false,
            metaKey: false,
            defaultPrevented: false,
            preventDefault() {
                this.defaultPrevented = true;
            },
            ...fields,
        };
        for (const listener of this.listeners.get(type) ?? []) listener(event);
        return event;
    }
}

class CanvasStub extends EventTargetStub {
    constructor() {
        super();
        this.dataset = {};
        this.clientWidth = 400;
        this.clientHeight = 200;
        this.captured = new Set();
    }

    getBoundingClientRect() {
        return {left: 100, top: 50, width: 400, height: 200};
    }

    setPointerCapture(pointerId) {
        this.captured.add(pointerId);
    }

    hasPointerCapture(pointerId) {
        return this.captured.has(pointerId);
    }

    releasePointerCapture(pointerId) {
        this.captured.delete(pointerId);
    }
}

class FocusTargetStub extends EventTargetStub {
    constructor() {
        super();
        this.focusCalls = [];
    }

    focus(options) {
        this.focusCalls.push(options);
    }
}

function frames() {
    let next = 1;
    const pending = new Map();
    return {
        request(callback) {
            const id = next++;
            pending.set(id, callback);
            return id;
        },
        cancel(id) {
            pending.delete(id);
        },
        flush() {
            const callbacks = [...pending.values()];
            pending.clear();
            for (const callback of callbacks) callback();
        },
        get size() {
            return pending.size;
        },
    };
}

assert.deepEqual(canvasLogicalPoint(new CanvasStub(), 800, 400, 300, 150), {x: 400, y: 200});
assert.deepEqual(canvasLogicalPoint(new CanvasStub(), 0, 400, 300, 150), {x: 0, y: 0});
assert.equal(inputModifiers({shiftKey: true, ctrlKey: true, altKey: true, metaKey: true}), 15);
assert.equal(inputKeyAction("ArrowLeft"), INPUT_KEY_ACTION.ArrowLeft);
assert.equal(inputKeyAction("="), INPUT_KEY_ACTION.ZoomIn);
assert.equal(inputKeyAction("r"), INPUT_KEY_ACTION.Reset);
assert.equal(inputKeyAction("Escape"), undefined);
assert.equal(inputKeyName("ArrowLeft"), "ArrowLeft");
assert.equal(inputKeyName("w"), "W");
assert.equal(inputKeyName("="), "Plus");
assert.equal(inputKeyName("_"), "Minus");
assert.equal(inputKeyName(" "), "Space");
assert.equal(inputKeyName("Escape"), "Escape");
assert.equal(inputKeyName("Shift", "ShiftLeft"), "Shift");
assert.equal(inputKeyName("Tab"), "Tab");
assert.equal(inputKeyName("\u3148", "KeyW"), "W");
assert.equal(inputKeyName("", "NumpadAdd"), "Plus");
assert.equal(inputKeyName("F12"), undefined);
assert.equal(isHostKeyTarget({closest: () => ({tagName: "BUTTON"})}), true);
assert.equal(isHostKeyTarget({closest: () => null}), false);
assert.equal(isHostKeyTarget(null), false);
assert.deepEqual(inputResult({handled: 1, redraw: true, release: true, sample: {}}), {
    handled: true,
    redraw: true,
    capture: false,
    release: true,
});
assert.equal(inputResult({error: "unknown"}).error, "unknown");

const canvas = new CanvasStub();
const wheelTarget = new EventTargetStub();
const keyTarget = new EventTargetStub();
const focusTarget = new FocusTargetStub();
const scheduled = frames();
const inputs = [];
const camera = [];
const previewPans = [];
const previewWheels = [];
const contexts = [];
const samples = [];
const inputErrors = [];
const releases = [];
let redraws = 0;
let fit = true;
let busy = false;
let inputResponse = () => ({handled: false, redraw: false, capture: false, release: false});
const runtime = {
    cameraMode: "interactive",
    cameraView: "3d",
    input(event) {
        inputs.push(event);
        return inputResponse(event);
    },
    releaseInput(time) {
        releases.push(time);
        return {handled: false, redraw: false, capture: false, release: false};
    },
};
const router = createCanvasInputRouter({
    canvas,
    wheelTarget,
    keyTarget,
    focusTarget,
    getRuntime: () => runtime,
    getSceneSize: () => [800, 400],
    getTime: () => 1.25,
    keyName: inputKeyName,
    isBusy: () => busy,
    isFit: () => fit,
    camera: (...command) => {
        camera.push(command.slice(0, 3));
        return true;
    },
    previewPan: (...value) => previewPans.push(value.slice(0, 3)),
    previewWheel: (event) => previewWheels.push(event.deltaY),
    previewKey: (event) => Boolean((event.ctrlKey || event.metaKey) && ["+", "=", "-", "0"].includes(event.key)),
    contextMenu: (event) => contexts.push([event.clientX, event.clientY]),
    onSample: (sample) => samples.push(sample),
    inputError: (error) => inputErrors.push(error),
    redraw: () => redraws++,
    requestFrame: (callback) => scheduled.request(callback),
    cancelFrame: (id) => scheduled.cancel(id),
});

inputResponse = () => ({handled: false, redraw: true});
const passiveMove = canvas.emit("pointermove", {
    clientX: 300,
    clientY: 150,
    pointerId: 6,
});
canvas.emit("pointermove", {
    clientX: 320,
    clientY: 160,
    pointerId: 6,
});
assert.equal(passiveMove.defaultPrevented, false, "passive hover must not consume host input");
assert.deepEqual(inputs.slice(-2).map((event) => ({type: event.type, x: event.x, y: event.y, dx: event.dx, dy: event.dy})), [
    {type: "pointermove", x: 400, y: 200, dx: 0, dy: 0},
    {type: "pointermove", x: 440, y: 220, dx: 40, dy: 20},
]);
assert.equal(scheduled.size, 1, "passive hover redraws must coalesce");
canvas.emit("pointerleave", {pointerId: 6});
assert.equal(inputs.at(-1).type, "pointercancel", "leaving the Canvas must clear passive hover");
scheduled.flush();
assert.equal(redraws, 1);

inputResponse = (event) => ({
    handled: true,
    redraw: true,
    capture: event.type === "pointerdown",
    release: event.type === "pointerup",
});
const capturedInputStart = inputs.length;
const down = canvas.emit("pointerdown", {
    clientX: 300,
    clientY: 150,
    button: 0,
    buttons: 1,
    pointerId: 7,
    shiftKey: true,
});
assert.deepEqual(
    focusTarget.focusCalls,
    [{preventScroll: true}],
    "pressing the Canvas must explicitly transfer keyboard focus before pointer input is consumed",
);
const inputCountBeforeCapturedLeave = inputs.length;
canvas.emit("pointerleave", {pointerId: 7});
assert.equal(inputs.length, inputCountBeforeCapturedLeave, "pointerleave must not cancel an active capture");
const move = canvas.emit("pointermove", {
    clientX: 340,
    clientY: 170,
    button: -1,
    buttons: 1,
    pointerId: 7,
    shiftKey: true,
});
const up = canvas.emit("pointerup", {
    clientX: 340,
    clientY: 170,
    button: 0,
    buttons: 0,
    pointerId: 7,
});
assert.equal(down.defaultPrevented, true);
assert.equal(move.defaultPrevented, true);
assert.equal(canvas.captured.has(7), false);
assert.equal(up.defaultPrevented, true);
assert.deepEqual(inputs.slice(capturedInputStart, capturedInputStart + 3).map((event) => event.type), ["pointerdown", "pointermove", "pointerup"]);
assert.deepEqual(inputs[capturedInputStart], {
    type: "pointerdown",
    x: 400,
    y: 200,
    dx: 0,
    dy: 0,
    button: 0,
    buttons: 1,
    modifiers: 1,
    pointerId: 7,
    time: 1.25,
});
assert.deepEqual({dx: inputs[capturedInputStart + 1].dx, dy: inputs[capturedInputStart + 1].dy}, {dx: 80, dy: 40});
assert.equal(scheduled.size, 1, "UI redraws must coalesce into one animation frame");
scheduled.flush();
assert.equal(redraws, 2);

const sample = {position: {x: 60, y: 50}, value: {x: 0.3, y: 0.5}, rgba: [205, 0, 50, 255]};
inputResponse = (event) => ({
    handled: true,
    capture: event.type === "pointerdown",
    release: event.type === "pointerup",
    sample: event.type === "pointerup" ? sample : undefined,
});
canvas.emit("pointerdown", {clientX: 300, clientY: 150, button: 0, buttons: 1, pointerId: 76});
canvas.emit("pointerup", {clientX: 340, clientY: 170, button: 0, buttons: 0, pointerId: 76});
assert.equal(samples.length, 1, "a completed SampleArea result must reach the host callback");
assert.equal(samples[0], sample);

inputResponse = (event) => ({
    handled: true,
    capture: event.type === "pointerdown",
    release: event.type === "pointerup",
    error: event.type === "pointerup" ? "unknown" : undefined,
});
canvas.emit("pointerdown", {clientX: 300, clientY: 150, button: 0, buttons: 1, pointerId: 77});
canvas.emit("pointerup", {clientX: 340, clientY: 170, button: 0, buttons: 0, pointerId: 77});
assert.equal(inputErrors.at(-1)?.message, "unknown");
assert.equal(canvas.captured.has(77), false, "a sampling failure must still release capture");

inputResponse = () => ({handled: true, release: true});
canvas.emit("pointerdown", {clientX: 300, clientY: 150, button: 0, buttons: 1, pointerId: 70});
assert.equal(canvas.dataset.dragging, "false", "an immediate UI release must not leave a drag session active");
const releasedInputCount = inputs.length;
canvas.emit("pointermove", {clientX: 320, clientY: 160, button: -1, buttons: 1, pointerId: 70});
assert.equal(inputs.length, releasedInputCount + 1);
assert.equal(inputs.at(-1).type, "pointermove", "an immediate release must return to passive motion");

inputResponse = (event) => ({handled: true, capture: event.type === "pointerdown", release: event.type === "pointercancel"});
canvas.emit("pointerdown", {clientX: 300, clientY: 150, button: 0, buttons: 1, pointerId: 71});
assert.equal(canvas.captured.has(71), true);
router.cancel();
assert.equal(inputs.at(-1).type, "pointercancel", "scene replacement must cancel the old UI capture");
assert.equal(canvas.captured.has(71), false);
assert.equal(canvas.dataset.dragging, "false");
const cancelledInputCount = inputs.length;
canvas.emit("pointermove", {clientX: 320, clientY: 160, button: -1, buttons: 1, pointerId: 71});
assert.equal(inputs.length, cancelledInputCount + 1);
assert.equal(inputs.at(-1).type, "pointermove", "cancelled capture must not suppress later passive motion");

inputResponse = (event) => ({handled: true, capture: event.type === "pointerdown", release: event.type === "pointerup"});
canvas.emit("pointerdown", {clientX: 300, clientY: 150, button: 0, buttons: 1, pointerId: 72});
busy = true;
canvas.emit("pointerup", {clientX: 300, clientY: 150, button: 0, buttons: 0, pointerId: 72});
busy = false;
assert.equal(inputs.at(-1).type, "pointerup", "a busy host must still release the native UI capture");
assert.equal(canvas.captured.has(72), false);

inputResponse = (event) => ({handled: true, capture: event.type === "pointerdown", release: event.type === "pointerup"});
canvas.emit("pointerdown", {clientX: 300, clientY: 150, button: 0, buttons: 1, pointerId: 73});
const inputCountBeforeSecondPointer = inputs.length;
const cameraCountBeforeSecondPointer = camera.length;
const secondPointer = canvas.emit("pointerdown", {
    clientX: 200,
    clientY: 100,
    button: 0,
    buttons: 1,
    pointerId: 74,
});
assert.equal(secondPointer.defaultPrevented, true);
assert.equal(inputs.length, inputCountBeforeSecondPointer);
assert.equal(camera.length, cameraCountBeforeSecondPointer);
canvas.emit("pointerup", {clientX: 300, clientY: 150, button: 0, buttons: 0, pointerId: 73});
assert.equal(inputs.at(-1).type, "pointerup");
canvas.emit("pointerdown", {clientX: 300, clientY: 150, button: 0, buttons: 1, pointerId: 74});
canvas.emit("pointerup", {clientX: 300, clientY: 150, button: 0, buttons: 0, pointerId: 74});
assert.equal(inputs.at(-1).pointerId, 74, "the original capture must release after a second touch is ignored");

inputResponse = () => ({handled: true, capture: false, release: false});
canvas.emit("pointerdown", {clientX: 300, clientY: 150, button: 0, buttons: 1, pointerId: 75});
assert.equal(canvas.dataset.dragging, "false", "handled-only input must not create a pointer session");
const handledOnlyInputCount = inputs.length;
canvas.emit("pointermove", {clientX: 320, clientY: 160, button: -1, buttons: 1, pointerId: 75});
canvas.emit("pointerup", {clientX: 320, clientY: 160, button: 0, buttons: 0, pointerId: 75});
assert.equal(inputs.length, handledOnlyInputCount + 1);
assert.equal(inputs.at(-1).type, "pointermove", "handled-only controls may observe passive position changes");

inputResponse = () => ({handled: false});
canvas.emit("pointerdown", {clientX: 200, clientY: 100, button: 0, buttons: 1, pointerId: 8});
canvas.emit("pointermove", {clientX: 240, clientY: 120, button: -1, buttons: 1, pointerId: 8});
canvas.emit("pointerup", {clientX: 240, clientY: 120, button: 0, buttons: 0, pointerId: 8});
assert.deepEqual(camera.at(-1), ["orbit", 0.1, 0.1]);

const optionalInput = runtime.input;
delete runtime.input;
canvas.emit("pointerdown", {clientX: 200, clientY: 100, button: 0, buttons: 1, pointerId: 80});
canvas.emit("pointermove", {clientX: 220, clientY: 110, button: -1, buttons: 1, pointerId: 80});
canvas.emit("pointerup", {clientX: 220, clientY: 110, button: 0, buttons: 0, pointerId: 80});
assert.deepEqual(camera.at(-1), ["orbit", 0.05, 0.05], "camera fallback must work without optional UI support");
runtime.input = optionalInput;

canvas.emit("pointerdown", {clientX: 200, clientY: 100, button: 1, buttons: 4, pointerId: 9});
canvas.emit("pointermove", {clientX: 225, clientY: 110, button: -1, buttons: 4, pointerId: 9});
canvas.emit("pointerup", {clientX: 225, clientY: 110, button: 1, buttons: 0, pointerId: 9});
assert.deepEqual(previewPans, [["start", 0, 0], ["move", 25, 10], ["end", 0, 0]]);
fit = false;
canvas.emit("pointerdown", {clientX: 200, clientY: 100, button: 1, buttons: 4, pointerId: 10});
canvas.emit("pointermove", {clientX: 225, clientY: 110, button: -1, buttons: 4, pointerId: 10});
canvas.emit("pointerup", {clientX: 225, clientY: 110, button: 1, buttons: 0, pointerId: 10});
fit = true;
assert.equal(previewPans.length, 3, "the middle button pans the preview only while it fits the window");
runtime.cameraMode = "fixed";
const cameraCountBeforeFixed = camera.length;
canvas.emit("pointerdown", {clientX: 200, clientY: 100, button: 0, buttons: 1, pointerId: 11});
canvas.emit("pointermove", {clientX: 240, clientY: 120, button: -1, buttons: 1, pointerId: 11});
canvas.emit("pointerup", {clientX: 240, clientY: 120, button: 0, buttons: 0, pointerId: 11});
runtime.cameraMode = "interactive";
assert.equal(camera.length, cameraCountBeforeFixed, "a fixed camera ignores drags");

const context = canvas.emit("contextmenu", {clientX: 120, clientY: 70, button: 2});
assert.equal(context.defaultPrevented, true);
assert.deepEqual(contexts, [[120, 70]]);

inputResponse = () => ({handled: true, redraw: true});
const handledWheel = wheelTarget.emit("wheel", {clientX: 300, clientY: 150, deltaX: 2, deltaY: 3, deltaMode: 1});
assert.equal(handledWheel.defaultPrevented, true);
assert.equal(previewWheels.length, 0);
assert.deepEqual({wheelX: inputs.at(-1).wheelX, wheelY: inputs.at(-1).wheelY}, {wheelX: 32, wheelY: 48});

inputResponse = () => ({handled: false});
wheelTarget.emit("wheel", {clientX: 300, clientY: 150, deltaY: 12});
assert.deepEqual(previewWheels, [12]);
for (const modifier of [{ctrlKey: true}, {metaKey: true}, {altKey: true}]) {
    const modifiedWheel = wheelTarget.emit("wheel", {
        clientX: 300,
        clientY: 150,
        deltaY: 13,
        ...modifier,
    });
    assert.equal(modifiedWheel.defaultPrevented, false);
}
assert.deepEqual(previewWheels, [12], "modified wheel gestures belong to the VS Code host");
fit = false;
const outsideFit = wheelTarget.emit("wheel", {clientX: 300, clientY: 150, deltaY: 14});
assert.equal(outsideFit.defaultPrevented, false);

inputResponse = () => ({handled: true, redraw: true});
const handledKey = keyTarget.emit("keydown", {key: "ArrowLeft"});
assert.equal(handledKey.defaultPrevented, true);
assert.equal(inputs.at(-1).button, INPUT_KEY_ACTION.ArrowLeft);
assert.equal(inputs.at(-1).key, "ArrowLeft");
const cameraCount = camera.length;

const handledLetter = keyTarget.emit("keydown", {key: "w"});
assert.equal(handledLetter.defaultPrevented, true);
assert.equal(inputs.at(-1).key, "W");
assert.equal(inputs.at(-1).button, -1);
const handledTab = keyTarget.emit("keydown", {key: "Tab"});
assert.equal(handledTab.defaultPrevented, true);
assert.equal(inputs.at(-1).key, "Tab");
keyTarget.emit("keyup", {key: "Tab"});
const handledKeyUp = keyTarget.emit("keyup", {key: "w"});
assert.equal(handledKeyUp.defaultPrevented, true);
assert.equal(inputs.at(-1).type, "keyup");
assert.equal(inputs.at(-1).key, "W");
// "=" is the Plus key and "_" the Minus key, so a held "=" lets go when the key reads "+".
keyTarget.emit("keydown", {key: "="});
assert.equal(inputs.at(-1).key, "Plus");
keyTarget.emit("keyup", {key: "+"});
assert.deepEqual([inputs.at(-1).type, inputs.at(-1).key], ["keyup", "Plus"]);
keyTarget.emit("keydown", {key: "_"});
keyTarget.emit("keyup", {key: "-"});
assert.deepEqual(inputs.slice(-2).map((event) => [event.type, event.key]), [["keydown", "Minus"], ["keyup", "Minus"]]);

keyTarget.emit("keydown", {key: "d"});
busy = true;
keyTarget.emit("keyup", {key: "d"});
busy = false;
assert.equal(inputs.at(-1).type, "keyup", "keyup must clear retained state while the host is busy");
assert.equal(inputs.at(-1).key, "D");

keyTarget.emit("keydown", {key: "a"});
keyTarget.emit("blur");
assert.equal(inputs.at(-1).type, "keyup", "blur must release every retained Scene key");
assert.equal(inputs.at(-1).key, "A");
assert.equal(releases.at(-1), 1.25, "blur must release unbound raw Input state too");

inputResponse = () => ({handled: false});
keyTarget.emit("keydown", {key: "ArrowRight"});
assert.deepEqual(camera.at(-1), ["pan", -0.04, 0]);
const previewKey = keyTarget.emit("keydown", {key: "=", ctrlKey: true});
assert.equal(previewKey.defaultPrevented, true);
assert.equal(camera.length, cameraCount + 1, "host preview keys must not fall through to Scene camera");
const inputCountBeforeAlt = inputs.length;
const cameraCountBeforeAlt = camera.length;
const altNavigation = keyTarget.emit("keydown", {key: "ArrowLeft", altKey: true});
assert.equal(altNavigation.defaultPrevented, false);
assert.equal(inputs.length, inputCountBeforeAlt, "Alt navigation belongs to VS Code, not the Canvas");
assert.equal(camera.length, cameraCountBeforeAlt);

router.dispose();
assert.equal(canvas.dataset.dragging, "false");
assert.equal(releases.at(-1), 1.25, "dispose must release raw Input state");
const afterDispose = inputs.length;
canvas.emit("pointerdown", {button: 0, buttons: 1, pointerId: 10});
assert.equal(inputs.length, afterDispose);

const extensionRoot = path.dirname(path.dirname(fileURLToPath(import.meta.url)));
const repositoryRoot = path.resolve(extensionRoot, "..", "..");
assert.equal(
    fs.readFileSync(path.join(repositoryRoot, "src", "bindings", "js", "client.js"), "utf8"),
    fs.readFileSync(path.join(extensionRoot, "runtime", "client.js"), "utf8"),
    "VS Code must bundle the matching optional input runtime",
);
assert.equal(
    fs.readFileSync(path.join(repositoryRoot, "src", "bindings", "js", "client.d.ts"), "utf8"),
    fs.readFileSync(path.join(extensionRoot, "runtime", "client.d.ts"), "utf8"),
    "VS Code must bundle the matching optional input declarations",
);

console.log("canvas input routing passed");
