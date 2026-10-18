// Pointer and key events on a canvas, as the runtime's input events in logical scene pixels.
import type {InputEvent} from "../protocol.ts";

export interface InputSink {
    /** Whether the scene takes input now. */
    live(): boolean;
    /** The scene's logical size. */
    size(): readonly [number, number];
    queue(event: InputEvent): void;
    /** Focus left the canvas: queued input is dropped and every held key and button let go. */
    releaseAll(): void;
}

const KEYS = new Set(["ArrowLeft", "ArrowRight", "ArrowUp", "ArrowDown", " ", "Enter", "Escape", "Shift", "Tab", "+", "-"]);

/** The runtime's name for a key, or null for one it does not take. */
export function keyName(event: KeyboardEvent): string | null {
    if (KEYS.has(event.key)) return event.key;
    return /^[a-z0-9]$/i.test(event.key) ? event.key.toUpperCase() : null;
}

const modifiers = (event: MouseEvent | KeyboardEvent): number =>
    (event.shiftKey ? 1 : 0) | (event.ctrlKey ? 2 : 0) | (event.altKey ? 4 : 0) | (event.metaKey ? 8 : 0);

/** Starts forwarding `canvas` events to `sink`; the returned function stops it. */
export function bindInput(canvas: HTMLCanvasElement, sink: InputSink): () => void {
    const controller = new AbortController();
    const options = {signal: controller.signal};
    // Key events carry the buttons held, as the pointer events last reported them.
    let buttons = 0;
    // Canvas CSS pixels to logical scene pixels, from the top-left.
    const at = (event: MouseEvent) => {
        const box = canvas.getBoundingClientRect();
        const [width, height] = sink.size();
        return {x: (event.clientX - box.left) * width / box.width, y: (event.clientY - box.top) * height / box.height};
    };
    const pointer = (type: InputEvent["type"], event: PointerEvent, button = -1): InputEvent => {
        buttons = event.buttons;
        return {type, ...at(event), button, buttons, modifiers: modifiers(event), pointerId: 0};
    };

    canvas.addEventListener("pointerdown", (event) => {
        if (!sink.live()) return;
        canvas.focus({preventScroll: true});
        canvas.setPointerCapture(event.pointerId);
        sink.queue(pointer("pointerdown", event, event.button));
    }, options);
    canvas.addEventListener("pointermove", (event) => {
        if (sink.live()) sink.queue(pointer("pointermove", event));
    }, options);
    canvas.addEventListener("pointerup", (event) => {
        if (sink.live()) sink.queue(pointer("pointerup", event, event.button));
    }, options);
    // Leaving without a button held moves the pointer off the scene, so hover states end.
    canvas.addEventListener("pointerleave", (event) => {
        if (sink.live() && !event.buttons) sink.queue({type: "pointermove", x: -1, y: -1});
    }, options);
    canvas.addEventListener("pointercancel", () => {
        buttons = 0;
        if (sink.live()) sink.queue({type: "pointercancel", buttons});
    }, options);
    for (const type of ["keydown", "keyup"] as const) {
        canvas.addEventListener(type, (event) => {
            const key = keyName(event);
            if (!sink.live() || !key || event.metaKey || event.ctrlKey || event.altKey) return;
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
    return () => controller.abort();
}
