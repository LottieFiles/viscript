// Pointer and key events on a canvas, as the runtime's input events in logical scene pixels; the
// drags, wheel turns and keys that move a scene's interactive camera; and clicks on a scene that
// takes no input.
import {inputKeyName} from "vis-wasm-bundle";
import type {CameraAction, InputEvent} from "../protocol.ts";

export interface InputSink {
    /** Whether the scene takes input now. */
    live(): boolean;
    /** Whether drags, the wheel and keys move the scene's camera now. */
    camera(): boolean;
    /** The camera's view: a drag orbits a 3D camera and pans a 2D one. */
    view(): "2d" | "3d";
    /** The scene's logical size. */
    size(): readonly [number, number];
    queue(event: InputEvent): void;
    moveCamera(action: CameraAction, x: number, y: number): void;
    /** Focus left the canvas: queued input is dropped and every held key and button let go. */
    releaseAll(): void;
    /** A click on a scene that does not take input. */
    click(): void;
}

// Pixels a press moves before it is a drag rather than a click.
const DRAG = 4;
// The camera's keys, as the VS Code preview had them: a pan by a fraction of the canvas, a zoom
// step, a reset or a view.
const CAMERA_KEYS: Readonly<Record<string, readonly [CameraAction, number, number]>> = {
    ArrowLeft: ["pan", 0.04, 0], ArrowRight: ["pan", -0.04, 0], ArrowUp: ["pan", 0, -0.04], ArrowDown: ["pan", 0, 0.04],
    "+": ["zoom", 0, -0.12], "=": ["zoom", 0, -0.12], "-": ["zoom", 0, 0.12],
    r: ["reset", 0, 0], R: ["reset", 0, 0], "2": ["view2d", 0, 0], "3": ["view3d", 0, 0],
};
// The zoom a wheel notch of 100 pixels gives, as one key press does, and the most one event gives.
const WHEEL_ZOOM = 0.12, WHEEL_LIMIT = 0.36;

const modifiers = (event: MouseEvent | KeyboardEvent): number =>
    (event.shiftKey ? 1 : 0) | (event.ctrlKey ? 2 : 0) | (event.altKey ? 4 : 0) | (event.metaKey ? 8 : 0);

interface Drag {
    readonly id: number;
    readonly startX: number;
    readonly startY: number;
    x: number;
    y: number;
    /** Shift was held as it began, so it pans a 3D camera too. */
    readonly pan: boolean;
    moved: boolean;
}

/** Starts forwarding `canvas` events to `sink`; the returned function stops it. */
export function bindInput(canvas: HTMLCanvasElement, sink: InputSink): () => void {
    const controller = new AbortController();
    const options = {signal: controller.signal};
    // Key events carry the buttons held, as the pointer events last reported them.
    let buttons = 0;
    let drag: Drag | null = null;
    // The last press was a drag, so the click that ends it is not a click.
    let dragged = false;
    // Canvas CSS pixels to logical scene pixels, from the top-left.
    const at = (event: MouseEvent) => {
        const box = canvas.getBoundingClientRect();
        const [width, height] = sink.size();
        return {x: (event.clientX - box.left) * width / box.width, y: (event.clientY - box.top) * height / box.height};
    };
    const pointer = (type: InputEvent["type"], event: PointerEvent, button = -1): InputEvent => {
        buttons = event.buttons;
        return {type, ...at(event), button, buttons, modifiers: modifiers(event)};
    };
    const endDrag = (event: PointerEvent) => {
        if (drag?.id !== event.pointerId) return;
        dragged = drag.moved;
        drag = null;
    };

    canvas.addEventListener("pointerdown", (event) => {
        dragged = false;
        if (sink.live()) {
            canvas.focus({preventScroll: true});
            canvas.setPointerCapture(event.pointerId);
            sink.queue(pointer("pointerdown", event, event.button));
            return;
        }
        if (event.button !== 0 || drag || !sink.camera()) return;
        canvas.focus({preventScroll: true});
        canvas.setPointerCapture(event.pointerId);
        drag = {id: event.pointerId, startX: event.clientX, startY: event.clientY, x: event.clientX, y: event.clientY,
            pan: event.shiftKey, moved: false};
    }, options);
    canvas.addEventListener("pointermove", (event) => {
        if (sink.live()) {
            sink.queue(pointer("pointermove", event));
            return;
        }
        if (drag?.id !== event.pointerId) return;
        if (!drag.moved && Math.hypot(event.clientX - drag.startX, event.clientY - drag.startY) < DRAG) return;
        drag.moved = true;
        const box = canvas.getBoundingClientRect();
        const dx = (event.clientX - drag.x) / Math.max(1, box.width), dy = (event.clientY - drag.y) / Math.max(1, box.height);
        drag.x = event.clientX;
        drag.y = event.clientY;
        if (sink.camera()) sink.moveCamera(sink.view() === "3d" && !drag.pan ? "orbit" : "pan", dx, dy);
    }, options);
    canvas.addEventListener("pointerup", (event) => {
        endDrag(event);
        if (sink.live()) sink.queue(pointer("pointerup", event, event.button));
    }, options);
    // Leaving without a button held moves the pointer off the scene, so hover states end.
    canvas.addEventListener("pointerleave", (event) => {
        if (sink.live() && !event.buttons) sink.queue({type: "pointermove", x: -1, y: -1});
    }, options);
    canvas.addEventListener("pointercancel", (event) => {
        buttons = 0;
        endDrag(event);
        if (sink.live()) sink.queue({type: "pointercancel", buttons});
    }, options);
    canvas.addEventListener("lostpointercapture", endDrag, options);
    canvas.addEventListener("wheel", (event) => {
        if (sink.live() || !sink.camera()) return;
        event.preventDefault();
        const pixels = event.deltaY * (event.deltaMode === 1 ? 16 : event.deltaMode === 2 ? canvas.clientHeight : 1);
        const zoom = Math.max(-WHEEL_LIMIT, Math.min(WHEEL_LIMIT, pixels / 100 * WHEEL_ZOOM));
        if (zoom) sink.moveCamera("zoom", 0, zoom);
    }, {...options, passive: false});
    for (const type of ["keydown", "keyup"] as const) {
        canvas.addEventListener(type, (event) => {
            if (event.metaKey || event.ctrlKey || event.altKey) return;
            if (!sink.live()) {
                const move = type === "keydown" && sink.camera() ? CAMERA_KEYS[event.key] : undefined;
                if (!move) return;
                event.preventDefault();
                sink.moveCamera(...move);
                return;
            }
            const key = inputKeyName(event.key, event.code);
            if (!key) return;
            event.preventDefault();
            // Retained input state ignores repeats; a held key is already down.
            if (type === "keydown" && event.repeat) return;
            sink.queue({type, key, buttons, modifiers: modifiers(event)});
        }, options);
    }
    canvas.addEventListener("blur", () => {
        buttons = 0;
        if (sink.live()) sink.releaseAll();
    }, options);
    // A scene that takes input has its clicks as pointer events above.
    canvas.addEventListener("click", () => {
        if (dragged) dragged = false;
        else if (!sink.live()) sink.click();
    }, options);
    return () => controller.abort();
}
