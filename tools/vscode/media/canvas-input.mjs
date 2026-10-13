export const INPUT_KEY_ACTION = Object.freeze({
    ArrowLeft: 0,
    ArrowRight: 1,
    ArrowUp: 2,
    ArrowDown: 3,
    ZoomIn: 4,
    ZoomOut: 5,
    Reset: 6,
    View2D: 7,
    View3D: 8,
});

const EMPTY_RESULT = Object.freeze({handled: false, redraw: false, capture: false, release: false});
const HOST_KEY_TARGETS = [
    "input",
    "select",
    "textarea",
    "button",
    "a[href]",
    "summary",
    "[contenteditable]:not([contenteditable='false'])",
    "[role='button']",
].join(", ");

export function isHostKeyTarget(target) {
    return typeof target?.closest === "function" && Boolean(target.closest(HOST_KEY_TARGETS));
}

export function inputModifiers(event) {
    return (event.shiftKey ? 1 : 0)
        | (event.ctrlKey ? 2 : 0)
        | (event.altKey ? 4 : 0)
        | (event.metaKey ? 8 : 0);
}

export function inputKeyAction(key) {
    const actions = {
        ArrowLeft: INPUT_KEY_ACTION.ArrowLeft,
        ArrowRight: INPUT_KEY_ACTION.ArrowRight,
        ArrowUp: INPUT_KEY_ACTION.ArrowUp,
        ArrowDown: INPUT_KEY_ACTION.ArrowDown,
        "+": INPUT_KEY_ACTION.ZoomIn,
        "=": INPUT_KEY_ACTION.ZoomIn,
        "-": INPUT_KEY_ACTION.ZoomOut,
        r: INPUT_KEY_ACTION.Reset,
        R: INPUT_KEY_ACTION.Reset,
        "2": INPUT_KEY_ACTION.View2D,
        "3": INPUT_KEY_ACTION.View3D,
    };
    return actions[key];
}

export function inputKeyName(key, code) {
    const named = {
        ArrowLeft: "ArrowLeft",
        ArrowRight: "ArrowRight",
        ArrowUp: "ArrowUp",
        ArrowDown: "ArrowDown",
        " ": "Space",
        Space: "Space",
        Enter: "Enter",
        Escape: "Escape",
        Shift: "Shift",
        Tab: "Tab",
        "+": "Plus",
        "=": "Plus",
        Minus: "Minus",
        "-": "Minus",
        _: "Minus",
    };
    if (named[key]) return named[key];
    if (typeof key === "string" && /^[a-z]$/i.test(key)) return key.toUpperCase();
    if (typeof key === "string" && /^[0-9]$/.test(key)) return key;
    if (typeof code === "string" && /^Key[A-Z]$/.test(code)) return code.slice(3);
    if (typeof code === "string" && /^Digit[0-9]$/.test(code)) return code.slice(5);
    if (code === "Space") return "Space";
    if (code === "NumpadAdd") return "Plus";
    if (code === "NumpadSubtract") return "Minus";
    return undefined;
}

export function canvasLogicalPoint(canvas, sceneWidth, sceneHeight, clientX, clientY) {
    const bounds = canvas.getBoundingClientRect();
    if (![bounds.left, bounds.top, bounds.width, bounds.height, sceneWidth, sceneHeight, clientX, clientY]
        .every(Number.isFinite)
        || bounds.width <= 0
        || bounds.height <= 0
        || sceneWidth <= 0
        || sceneHeight <= 0) {
        return {x: 0, y: 0};
    }
    return {
        x: (clientX - bounds.left) * sceneWidth / bounds.width,
        y: (clientY - bounds.top) * sceneHeight / bounds.height,
    };
}

export function inputResult(value) {
    if (!value || typeof value !== "object") return EMPTY_RESULT;
    const result = {
        handled: Boolean(value.handled),
        redraw: Boolean(value.redraw),
        capture: Boolean(value.capture),
        release: Boolean(value.release),
    };
    const sample = value.sample;
    const position = sample?.position;
    const normalized = sample?.value;
    const rgba = sample?.rgba;
    const object = sample?.object;
    if (position && normalized && Array.isArray(rgba) && object
        && Number.isFinite(position.x) && Number.isFinite(position.y)
        && Number.isFinite(normalized.x) && normalized.x >= 0 && normalized.x <= 1
        && Number.isFinite(normalized.y) && normalized.y >= 0 && normalized.y <= 1
        && rgba.length === 4
        && rgba.every((channel) => Number.isInteger(channel) && channel >= 0 && channel <= 255)
        && Number.isInteger(object.handle) && object.handle >= 0 && object.handle <= 0xffffffff
        && (object.id === null || typeof object.id === "string")
        && typeof object.type === "string") {
        result.sample = Object.freeze({
            position: Object.freeze({x: position.x, y: position.y}),
            value: Object.freeze({x: normalized.x, y: normalized.y}),
            rgba: Object.freeze([...rgba]),
            object: Object.freeze({handle: object.handle, id: object.id, type: object.type}),
        });
    }
    if (typeof value.error === "string" && value.error) result.error = value.error;
    return result;
}

function wheelDelta(event, axis, pageSize) {
    const value = Number(event[axis]) || 0;
    if (event.deltaMode === 1) return value * 16;
    if (event.deltaMode === 2) return value * Math.max(1, pageSize);
    return value;
}

export function createCanvasInputRouter(options) {
    const {
        canvas,
        wheelTarget = canvas,
        focusTarget = canvas,
        keyTarget = globalThis,
        getRuntime,
        getSceneSize,
        getTime,
        isBusy,
        isFit,
        camera,
        previewPan,
        previewWheel,
        previewKey,
        contextMenu,
        redraw,
        onSample,
        ignoreKey,
        inputError,
        requestFrame = globalThis.requestAnimationFrame?.bind(globalThis),
        cancelFrame = globalThis.cancelAnimationFrame?.bind(globalThis),
    } = options;
    if (!canvas || typeof canvas.addEventListener !== "function") {
        throw new TypeError("Canvas input routing requires a Canvas event target");
    }

    let activePointer;
    let passivePointer;
    let redrawFrame = 0;
    const heldKeys = new Map();
    const listeners = [];
    const reportInputError = (error) => {
        try {
            inputError?.(error);
        } catch {
            // Host diagnostics must never break pointer capture or release.
        }
    };
    const listen = (target, type, handler, listenerOptions) => {
        target?.addEventListener(type, handler, listenerOptions);
        listeners.push([target, type, handler, listenerOptions]);
    };
    const focusCanvas = () => {
        if (typeof focusTarget?.focus !== "function") return;
        try {
            focusTarget.focus({preventScroll: true});
        } catch {
            focusTarget.focus();
        }
    };
    const scheduleRedraw = () => {
        if (redrawFrame || typeof redraw !== "function") return;
        if (typeof requestFrame !== "function") {
            redraw();
            return;
        }
        redrawFrame = requestFrame(() => {
            redrawFrame = 0;
            redraw();
        });
    };
    const runtimeInput = (event, allowBusy = false) => {
        const runtime = getRuntime?.();
        if (!runtime || (!allowBusy && isBusy?.()) || typeof runtime.input !== "function") {
            return EMPTY_RESULT;
        }
        let result;
        try {
            result = inputResult(runtime.input(event));
        } catch (error) {
            reportInputError(error);
            return EMPTY_RESULT;
        }
        if (result.redraw) scheduleRedraw();
        if (result.error) reportInputError(new Error(result.error));
        if (result.sample && typeof onSample === "function") {
            try {
                onSample(result.sample);
            } catch (error) {
                reportInputError(error);
            }
        }
        return result;
    };
    const scenePoint = (event) => {
        const [width, height] = getSceneSize?.() ?? [0, 0];
        return canvasLogicalPoint(canvas, width, height, event.clientX, event.clientY);
    };
    const eventTime = () => {
        const value = Number(getTime?.());
        return Number.isFinite(value) && value >= 0 ? value : 0;
    };
    const dispatchKey = (type, key, button, modifiers, repeat, allowBusy = false) => runtimeInput({
        type,
        x: 0,
        y: 0,
        dx: 0,
        dy: 0,
        wheelX: 0,
        wheelY: 0,
        button,
        buttons: 0,
        modifiers,
        pointerId: 0,
        key,
        repeat,
        time: eventTime(),
    }, allowBusy);
    const releaseHeldKeys = () => {
        const entries = [...heldKeys];
        heldKeys.clear();
        for (const [key, button] of entries) {
            dispatchKey("keyup", key, button, 0, false, true);
        }
        const runtime = getRuntime?.();
        if (!runtime || typeof runtime.releaseInput !== "function") return;
        try {
            const result = inputResult(runtime.releaseInput(eventTime()));
            if (result.redraw) scheduleRedraw();
            if (result.error) reportInputError(new Error(result.error));
        } catch (error) {
            reportInputError(error);
        }
    };
    const dispatchPointer = (type, event, point, delta, allowBusy = false) => runtimeInput({
        type,
        x: point.x,
        y: point.y,
        dx: delta.x,
        dy: delta.y,
        wheelX: 0,
        wheelY: 0,
        button: Number.isInteger(event.button) ? event.button : -1,
        buttons: Number.isInteger(event.buttons) && event.buttons >= 0 ? event.buttons : 0,
        modifiers: inputModifiers(event),
        pointerId: Number.isInteger(event.pointerId) && event.pointerId >= 0 ? event.pointerId : 0,
        time: eventTime(),
    }, allowBusy);
    const capturePointer = (pointerId) => {
        if (typeof canvas.setPointerCapture !== "function") return;
        canvas.setPointerCapture(pointerId);
    };
    const releasePointer = (pointerId) => {
        if (typeof canvas.releasePointerCapture !== "function") return;
        if (typeof canvas.hasPointerCapture === "function" && !canvas.hasPointerCapture(pointerId)) return;
        canvas.releasePointerCapture(pointerId);
    };
    const cancelPassivePointer = () => {
        if (!passivePointer) return;
        const pointer = passivePointer;
        passivePointer = undefined;
        dispatchPointer("pointercancel", {
            button: -1,
            buttons: 0,
            pointerId: pointer.id,
            shiftKey: false,
            ctrlKey: false,
            altKey: false,
            metaKey: false,
        }, {x: pointer.sceneX, y: pointer.sceneY}, {x: 0, y: 0}, true);
    };
    const cancelActivePointer = () => {
        if (!activePointer) return;
        const pointer = activePointer;
        activePointer = undefined;
        passivePointer = undefined;
        if (pointer.kind === "ui") {
            dispatchPointer("pointercancel", {
                button: -1,
                buttons: 0,
                pointerId: pointer.id,
                shiftKey: false,
                ctrlKey: false,
                altKey: false,
                metaKey: false,
            }, {x: pointer.sceneX, y: pointer.sceneY}, {x: 0, y: 0}, true);
        } else if (pointer.kind === "preview-pan") {
            previewPan?.("end", 0, 0);
        }
        releasePointer(pointer.id);
        canvas.dataset.dragging = "false";
    };
    const finishPointer = (type, event) => {
        if (!activePointer || activePointer.id !== event.pointerId) return;
        const pointer = activePointer;
        activePointer = undefined;
        if (pointer.kind === "ui") {
            const point = scenePoint(event);
            const delta = {x: point.x - pointer.sceneX, y: point.y - pointer.sceneY};
            const result = dispatchPointer(type, event, point, delta, true);
            if (result.capture) capturePointer(pointer.id);
            if (result.release) releasePointer(pointer.id);
            if (result.handled) event.preventDefault();
            if (type === "pointerup") {
                passivePointer = {id: pointer.id, sceneX: point.x, sceneY: point.y};
            }
        } else if (pointer.kind === "preview-pan") {
            previewPan?.("end", 0, 0, event);
        }
        canvas.dataset.dragging = "false";
    };

    listen(canvas, "contextmenu", (event) => {
        event.preventDefault();
        contextMenu?.(event);
    });
    listen(canvas, "auxclick", (event) => {
        if (event.button === 1) event.preventDefault();
    });
    listen(canvas, "pointerdown", (event) => {
        focusCanvas();
        if (activePointer) {
            event.preventDefault();
            return;
        }
        const runtime = getRuntime?.();
        if (isFit?.() && runtime && !isBusy?.() && event.button === 1) {
            cancelPassivePointer();
            activePointer = {
                kind: "preview-pan",
                id: event.pointerId,
                clientX: event.clientX,
                clientY: event.clientY,
            };
            capturePointer(event.pointerId);
            canvas.dataset.dragging = "true";
            previewPan?.("start", 0, 0, event);
            event.preventDefault();
            return;
        }
        if (event.button !== 0 || !runtime || isBusy?.()) return;

        const point = scenePoint(event);
        const result = dispatchPointer("pointerdown", event, point, {x: 0, y: 0});
        if (result.capture) {
            passivePointer = undefined;
            activePointer = {
                kind: "ui",
                id: event.pointerId,
                sceneX: point.x,
                sceneY: point.y,
            };
            if (result.capture) capturePointer(event.pointerId);
        }
        if (result.release) {
            activePointer = undefined;
            releasePointer(event.pointerId);
        }
        if (result.handled || result.capture || result.release) {
            if (!activePointer) {
                passivePointer = {id: event.pointerId, sceneX: point.x, sceneY: point.y};
            }
            canvas.dataset.dragging = String(Boolean(activePointer));
            event.preventDefault();
            return;
        }
        if (runtime.cameraMode !== "interactive") return;
        cancelPassivePointer();
        activePointer = {
            kind: "camera",
            id: event.pointerId,
            clientX: event.clientX,
            clientY: event.clientY,
            pan: event.shiftKey,
        };
        capturePointer(event.pointerId);
        canvas.dataset.dragging = "true";
        event.preventDefault();
    });
    listen(canvas, "pointermove", (event) => {
        if (!activePointer) {
            const point = scenePoint(event);
            const previous = passivePointer?.id === event.pointerId ? passivePointer : undefined;
            const delta = previous
                ? {x: point.x - previous.sceneX, y: point.y - previous.sceneY}
                : {x: 0, y: 0};
            passivePointer = {id: event.pointerId, sceneX: point.x, sceneY: point.y};
            dispatchPointer("pointermove", event, point, delta);
            return;
        }
        if (activePointer.id !== event.pointerId) return;
        if (activePointer.kind === "preview-pan") {
            if ((event.buttons & 4) === 0) {
                finishPointer("pointercancel", event);
                return;
            }
            const dx = event.clientX - activePointer.clientX;
            const dy = event.clientY - activePointer.clientY;
            activePointer.clientX = event.clientX;
            activePointer.clientY = event.clientY;
            previewPan?.("move", dx, dy, event);
            event.preventDefault();
            return;
        }
        if (activePointer.kind === "ui") {
            const point = scenePoint(event);
            const delta = {x: point.x - activePointer.sceneX, y: point.y - activePointer.sceneY};
            activePointer.sceneX = point.x;
            activePointer.sceneY = point.y;
            const result = dispatchPointer("pointermove", event, point, delta);
            if (result.capture) capturePointer(event.pointerId);
            if (result.release) {
                activePointer = undefined;
                releasePointer(event.pointerId);
                canvas.dataset.dragging = "false";
            }
            if (result.handled) event.preventDefault();
            return;
        }

        const bounds = canvas.getBoundingClientRect();
        const dx = (event.clientX - activePointer.clientX) / Math.max(bounds.width, 1);
        const dy = (event.clientY - activePointer.clientY) / Math.max(bounds.height, 1);
        activePointer.clientX = event.clientX;
        activePointer.clientY = event.clientY;
        const runtime = getRuntime?.();
        const action = runtime?.cameraView === "3d" && !activePointer.pan ? "orbit" : "pan";
        if (camera?.(action, dx, dy, event)) scheduleRedraw();
        event.preventDefault();
    });
    listen(canvas, "pointerup", (event) => finishPointer("pointerup", event));
    listen(canvas, "pointercancel", (event) => {
        if (activePointer) finishPointer("pointercancel", event);
        else cancelPassivePointer();
    });
    listen(canvas, "pointerleave", () => {
        if (!activePointer) cancelPassivePointer();
    });
    listen(canvas, "lostpointercapture", (event) => finishPointer("pointercancel", event));
    listen(wheelTarget, "wheel", (event) => {
        const runtime = getRuntime?.();
        if (!runtime || isBusy?.()) return;
        const point = scenePoint(event);
        const result = runtimeInput({
            type: "wheel",
            x: point.x,
            y: point.y,
            dx: 0,
            dy: 0,
            wheelX: wheelDelta(event, "deltaX", canvas.clientWidth),
            wheelY: wheelDelta(event, "deltaY", canvas.clientHeight),
            button: -1,
            buttons: 0,
            modifiers: inputModifiers(event),
            pointerId: 0,
            time: eventTime(),
        });
        if (result.handled) {
            event.preventDefault();
            return;
        }
        if (!isFit?.() || inputModifiers(event) !== 0) return;
        event.preventDefault();
        previewWheel?.(event);
    }, {passive: false});
    listen(keyTarget, "keydown", (event) => {
        if (ignoreKey?.(event)) return;
        if (event.altKey) return;
        const keyAction = inputKeyAction(event.key);
        const key = inputKeyName(event.key, event.code);
        if (key !== undefined) {
            const button = keyAction ?? -1;
            const result = dispatchKey(
                "keydown",
                key,
                button,
                inputModifiers(event),
                Boolean(event.repeat),
            );
            if (result.handled) {
                heldKeys.set(key, button);
                event.preventDefault();
                return;
            }
        }
        if (previewKey?.(event)) {
            event.preventDefault();
            return;
        }
        if (event.ctrlKey || event.metaKey) return;
        const runtime = getRuntime?.();
        if (!runtime || runtime.cameraMode !== "interactive" || keyAction === undefined) return;
        const actions = {
            [INPUT_KEY_ACTION.ArrowLeft]: ["pan", 0.04, 0],
            [INPUT_KEY_ACTION.ArrowRight]: ["pan", -0.04, 0],
            [INPUT_KEY_ACTION.ArrowUp]: ["pan", 0, -0.04],
            [INPUT_KEY_ACTION.ArrowDown]: ["pan", 0, 0.04],
            [INPUT_KEY_ACTION.ZoomIn]: ["zoom", 0, -0.12],
            [INPUT_KEY_ACTION.ZoomOut]: ["zoom", 0, 0.12],
            [INPUT_KEY_ACTION.Reset]: ["reset", 0, 0],
            [INPUT_KEY_ACTION.View2D]: ["view2d", 0, 0],
            [INPUT_KEY_ACTION.View3D]: ["view3d", 0, 0],
        };
        if (camera?.(...actions[keyAction], event)) scheduleRedraw();
        event.preventDefault();
    });
    listen(keyTarget, "keyup", (event) => {
        const key = inputKeyName(event.key, event.code);
        if (key === undefined) return;
        if (ignoreKey?.(event) && !heldKeys.has(key)) return;
        const result = dispatchKey(
            "keyup",
            key,
            heldKeys.get(key) ?? inputKeyAction(event.key) ?? -1,
            inputModifiers(event),
            false,
            true,
        );
        heldKeys.delete(key);
        if (result.handled) event.preventDefault();
    });
    listen(keyTarget, "blur", releaseHeldKeys);

    return {
        cancel() {
            releaseHeldKeys();
            cancelActivePointer();
            cancelPassivePointer();
        },
        dispose() {
            releaseHeldKeys();
            cancelActivePointer();
            cancelPassivePointer();
            for (const [target, type, handler, listenerOptions] of listeners) {
                target?.removeEventListener(type, handler, listenerOptions);
            }
            if (redrawFrame && typeof cancelFrame === "function") cancelFrame(redrawFrame);
            redrawFrame = 0;
        },
    };
}
