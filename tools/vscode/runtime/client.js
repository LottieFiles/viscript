// The runtime's JavaScript client. createVis() starts one engine in the WebAssembly module and
// returns methods that load Lua or JavaScript scenes, render frames, read bounds, intersections
// and layout reports, load fonts and assets, feed input, and drive the retained runtime and its
// audio. The player's server, the VS Code preview and the tests use it. Copies live in
// tools/vscode/runtime and tools/skill/vis-skills/assets/wasm, refreshed by scripts/sync-wasm.mjs.
import createModule from "./vis-wasm.js";
import {compileScene} from "./bindings.js";

export {compileScene, evaluateScene, SceneBuilder, vis} from "./bindings.js";

// The tables stay plain and private, and the module does nothing when imported, so a bundle that
// takes only inputKeyName leaves the rest out.
const CAMERA_ACTIONS = {pan: 0, orbit: 1, zoom: 2, reset: 3, view2d: 4, view3d: 5};
const INPUT_TYPES = {
    pointerdown: 0,
    pointermove: 1,
    pointerup: 2,
    pointercancel: 3,
    wheel: 4,
    keydown: 5,
    keyup: 6,
};
// The keys input() takes by name, as the runtime numbers them; A to Z and 0 to 9 follow.
const INPUT_KEYS = {
    ArrowLeft: 1,
    ArrowRight: 2,
    ArrowUp: 3,
    ArrowDown: 4,
    Space: 5,
    Enter: 6,
    Escape: 7,
    Plus: 44,
    Minus: 45,
    Shift: 46,
    Tab: 47,
};
// The names browsers give some of those keys, as KeyboardEvent.key and KeyboardEvent.code.
const KEY_NAMES = {" ": "Space", "+": "Plus", "=": "Plus", "-": "Minus", "_": "Minus"};
const CODE_NAMES = {Space: "Space", NumpadAdd: "Plus", NumpadSubtract: "Minus"};
// The image types asset() takes, as a file extension or a MIME type, by the runtime's name for
// each; the browser runtime decodes no others.
const IMAGE_TYPES = {png: "png", "image/png": "png", svg: "svg", "image/svg+xml": "svg"};
// The colors hostTheme gives those the host leaves unset, close to VS Code's default Light and
// Dark themes, in the order _vis_host_theme takes them.
const HOST_THEMES = {
    light: {
        background: "#ffffff",
        foreground: "#202124",
        muted: "#555b64",
        accent: "#005fb8",
        secondary: "#6750a4",
        success: "#2e7d32",
        warning: "#8a5d00",
        danger: "#b3261e",
        info: "#00639b",
        surface: "#f5f6f8",
        line: "#d8dadd",
        result: "#8a6500",
        focus: "#315f8c",
        objects: ["#b45f06", "#b63a3c", "#2e7f7a", "#815681"],
    },
    dark: {
        background: "#1e1e1e",
        foreground: "#cccccc",
        muted: "#9d9d9d",
        accent: "#4daafc",
        secondary: "#c8a7ff",
        success: "#7ee787",
        warning: "#e3b341",
        danger: "#ff7b72",
        info: "#79c0ff",
        surface: "#252526",
        line: "#454545",
        result: "#edc948",
        focus: "#6ea8fe",
        objects: ["#f28e2b", "#ff6b6b", "#7bc96f", "#c29bc0"],
    },
};

// The name input() takes for a key a browser reports as KeyboardEvent key and code, or undefined
// for a key the runtime does not take. The code tells where the key is when the layout does not
// give it a Latin letter or digit.
export function inputKeyName(key, code) {
    if (typeof key === "string") {
        const name = Object.hasOwn(KEY_NAMES, key) ? KEY_NAMES[key] : key;
        if (Object.hasOwn(INPUT_KEYS, name)) return name;
        if (/^[a-z0-9]$/i.test(key)) return key.toUpperCase();
    }
    if (typeof code !== "string") return undefined;
    if (/^(?:Key[A-Z]|Digit[0-9])$/.test(code)) return code.at(-1);
    return Object.hasOwn(CODE_NAMES, code) ? CODE_NAMES[code] : undefined;
}

function inputKey(value) {
    if (value === undefined || value === null || value === "") return 0;
    if (typeof value !== "string") throw new TypeError("Input key must be a string");
    const name = inputKeyName(value);
    if (name === undefined) throw new TypeError(`Unsupported input key '${value}'`);
    if (Object.hasOwn(INPUT_KEYS, name)) return INPUT_KEYS[name];
    return /^[A-Z]$/.test(name) ? 8 + name.charCodeAt(0) - 65 : 34 + name.charCodeAt(0) - 48;
}

function colorChannel(value, label) {
    const channel = Number(value);
    if (!Number.isFinite(channel) || channel < 0 || channel > 255) {
        throw new TypeError(`${label} contains an invalid color channel`);
    }
    return Math.round(channel);
}

function rgba(value, label) {
    if (typeof value !== "string") throw new TypeError(`${label} must be a CSS color string`);
    const input = value.trim().toLowerCase();
    const hex = input.match(/^#([0-9a-f]{3,4}|[0-9a-f]{6}|[0-9a-f]{8})$/i)?.[1];
    let channels;
    if (hex) {
        const expanded = hex.length <= 4 ? [...hex].map((digit) => digit + digit).join("") : hex;
        channels = [
            Number.parseInt(expanded.slice(0, 2), 16),
            Number.parseInt(expanded.slice(2, 4), 16),
            Number.parseInt(expanded.slice(4, 6), 16),
            expanded.length === 8 ? Number.parseInt(expanded.slice(6, 8), 16) : 255,
        ];
    } else {
        const functional = input.match(/^rgba?\(\s*([\d.]+)\s*[, ]\s*([\d.]+)\s*[, ]\s*([\d.]+)(?:\s*[,/]\s*([\d.]+%?))?\s*\)$/);
        if (!functional) throw new TypeError(`${label} must use hex, rgb(), or rgba() syntax`);
        const alpha = functional[4]?.endsWith("%")
            ? Number.parseFloat(functional[4]) * 2.55
            : Number.parseFloat(functional[4] ?? "1") * 255;
        channels = [functional[1], functional[2], functional[3], alpha];
    }
    const [red, green, blue, alpha] = channels.map((channel) => colorChannel(channel, label));
    return ((red << 24) | (green << 16) | (blue << 8) | alpha) >>> 0;
}

function nonNegative(value, label) {
    if (!Number.isFinite(value) || value < 0) throw new RangeError(`${label} must be a non-negative number`);
}

// Whether value is a plain object: made by an object literal, or without a prototype.
function plain(value) {
    const prototype = value ? Object.getPrototypeOf(value) : undefined;
    return prototype === Object.prototype || prototype === null;
}

// Throws unless value is a plain object whose keys are all in names.
function plainFields(value, names, label, field) {
    if (!plain(value)) throw new TypeError(`${label} must be a plain object`);
    const unknown = Object.keys(value).find((key) => !names.includes(key));
    if (unknown !== undefined) throw new TypeError(`Unknown ${field}: ${unknown}`);
}

// One WASM instance can host many engines. Fonts loaded through any of them are shared by all
// while one of them is alive; pass the result to createVis as `module` to add an engine without
// another instance.
export function createVisModule(moduleOptions = {}) {
    return createModule(moduleOptions);
}

// Starts an engine in moduleOptions.module, or in a new instance, and loads source into it.
// Without a source, as createVis(moduleOptions), the engine holds no scene until load, so fonts,
// assets and a host theme can go in before the first scene uses them.
export async function createVis(...args) {
    const [source, name, moduleOptions = {}] = plain(args[0]) ? [undefined, undefined, args[0]] : args;
    if (!moduleOptions || typeof moduleOptions !== "object" || Array.isArray(moduleOptions)) {
        throw new TypeError("viscript module options must be an object");
    }
    const {module: shared, ...options} = moduleOptions;
    const module = shared ?? await createModule(options);
    if (typeof module._vis_create !== "function") {
        throw new Error("The module is not a viscript runtime");
    }
    let engine = module._vis_create();
    if (!engine) {
        throw new Error("Unable to initialize the viscript renderer");
    }

    const assertAlive = () => {
        if (!engine) throw new Error("viscript instance has been destroyed");
    };
    const encoder = new TextEncoder();
    const lastError = () => module.UTF8ToString(module._vis_last_error(engine));
    const check = (result, operation) => {
        if (result !== 0) throw new Error(lastError() || `${operation} failed (${result})`);
    };
    // Calls use with pointers to WASM copies of blocks, then frees them. A byte array is copied with
    // a terminating zero, so encoded text passes as a C string; a number reserves that many bytes.
    const withMemory = (blocks, use) => {
        const pointers = [];
        try {
            for (const block of blocks) {
                const pointer = module._malloc(typeof block === "number" ? block : block.length + 1);
                if (!pointer) throw new Error("Unable to allocate WASM memory");
                pointers.push(pointer);
                if (typeof block === "number") continue;
                module.HEAPU8.set(block, pointer);
                module.HEAPU8[pointer + block.length] = 0;
            }
            return use(...pointers);
        } finally {
            for (const pointer of pointers) module._free(pointer);
        }
    };
    const load = (input, sourceName = typeof input === "string" ? "scene.lua" : "scene.js") => {
        assertAlive();
        const source = encoder.encode(typeof input === "string" ? input : compileScene(input));
        const name = encoder.encode(sourceName);
        if (!source.length || source.length > 0xffffffff) throw new Error("Lua source must be 1 to 4294967295 bytes");
        if (!name.length || name.includes(0)) throw new Error("Scene name must be non-empty and cannot contain zero bytes");
        check(withMemory([source, name], (sourcePtr, namePtr) =>
            module._vis_load_lua(engine, sourcePtr, source.length, namePtr)), "Lua load");
        return module._vis_duration(engine);
    };

    if (source !== undefined) {
        try {
            load(source, name);
        } catch (error) {
            module._vis_destroy(engine);
            engine = 0;
            throw error;
        }
    }

    // The bundled WASM predates vis_render's pixel ratio and takes it in vis_render_scaled; drop
    // the fallback when the bundles are rebuilt.
    const renderPixels = module._vis_render_scaled ?? module._vis_render;
    // Renders a frame and returns a view of its pixels, which the next render, a resize or WASM
    // memory growth leaves stale: render hands out a copy, draw puts the view on a canvas.
    const renderFrame = (time, pixelRatio) => {
        assertAlive();
        check(renderPixels(engine, time, pixelRatio), "Render");
        return new Uint8ClampedArray(module.HEAPU8.buffer, module._vis_pixels(engine), module._vis_pixels_size(engine));
    };
    const render = (time) => renderFrame(time, 1).slice();

    const encodedObject = (value, label) => {
        if (typeof value !== "string") throw new TypeError(`${label} must be an object ID`);
        const encoded = encoder.encode(value);
        if (!encoded.length || encoded.length > 4096 || encoded.includes(0)) {
            throw new TypeError(`${label} must be a non-empty object ID without zero bytes`);
        }
        return encoded;
    };
    const bounds = (object, time = 0) => {
        assertAlive();
        nonNegative(time, "Bounds time");
        const target = encodedObject(object, "Bounds target");
        const [x, y, width, height] = withMemory([target, 16], (objectPtr, outputPtr) => {
            check(module._vis_bounds(engine, objectPtr, time, outputPtr), "Bounds query");
            return new Float32Array(module.HEAPU8.buffer, outputPtr, 4).slice();
        });
        return Object.freeze({x, y, width, height});
    };
    const intersects = (first, second, time = 0, padding = 0) => {
        assertAlive();
        nonNegative(time, "Intersection time");
        nonNegative(padding, "Intersection padding");
        const firstTarget = encodedObject(first, "First intersection target");
        const secondTarget = encodedObject(second, "Second intersection target");
        return withMemory([firstTarget, secondTarget, 1], (firstPtr, secondPtr, outputPtr) => {
            check(module._vis_intersects(engine, firstPtr, secondPtr, time, padding, outputPtr), "Intersection query");
            return module.HEAPU8[outputPtr] !== 0;
        });
    };
    const layoutReport = (time = 0, padding = 0) => {
        assertAlive();
        nonNegative(time, "Layout report time");
        nonNegative(padding, "Layout report padding");
        check(module._vis_layout_report(engine, time, padding), "Layout report");
        const pointer = module._vis_layout_report_data(engine);
        const size = module._vis_layout_report_size(engine);
        if (!pointer || !size) throw new Error("Layout report returned no data");
        return JSON.parse(new TextDecoder().decode(module.HEAPU8.subarray(pointer, pointer + size)));
    };

    const resource = (loader, label, name, data, mime) => {
        assertAlive();
        const bytes = data instanceof Uint8Array ? data : new Uint8Array(data);
        const encodedName = encoder.encode(name);
        const encodedMime = encoder.encode(mime);
        if (!bytes.length || bytes.length > 32 * 1024 * 1024) throw new Error(`${label} must be 1 byte to 32 MiB`);
        if ([encodedName, encodedMime].some((text) => !text.length || text.length > 256 || text.includes(0))) {
            throw new Error(`${label} name and MIME must be non-empty, at most 256 bytes, and contain no zero bytes`);
        }
        check(withMemory([bytes, encodedName, encodedMime], (dataPtr, namePtr, mimePtr) =>
            loader(engine, namePtr, dataPtr, bytes.length, mimePtr)), `${label} load`);
    };
    const font = (name, data, mime = "ttf") => resource(module._vis_load_font, "Font", name, data, mime);
    const missingFonts = () => {
        assertAlive();
        return module.UTF8ToString(module._vis_missing_fonts(engine)).split("\n").filter(Boolean);
    };
    const asset = (name, data, mime) => {
        const type = String(mime).toLowerCase();
        if (!Object.hasOwn(IMAGE_TYPES, type)) {
            throw new TypeError(`Unsupported image type '${mime}' for asset '${name}'; the runtime decodes PNG and SVG`);
        }
        resource(module._vis_load_asset, "Asset", name, data, IMAGE_TYPES[type]);
    };
    const audioExportNames = [
        "_vis_audio_load",
        "_vis_audio_start",
        "_vis_audio_play",
        "_vis_audio_stop",
        "_vis_audio_active",
        "_vis_audio_master_gain",
        "_vis_audio_bus_gain",
        "_vis_audio_transport",
    ];
    const hasAudio = audioExportNames.every((exportName) => typeof module[exportName] === "function");
    const audio = hasAudio ? (() => {
        const buses = Object.freeze({music: 0, effect: 1, ui: 2});
        const nameBytes = (name) => {
            if (typeof name !== "string" || !name.length || name.includes("\0")) {
                throw new TypeError("Audio asset name must be non-empty text without zero bytes");
            }
            const encoded = encoder.encode(name);
            if (encoded.length > 255) throw new RangeError("Audio asset name must contain at most 255 UTF-8 bytes");
            return encoded;
        };
        const gain = (value, label) => {
            if (!Number.isFinite(value) || value < 0 || value > 4) {
                throw new RangeError(`${label} must be finite and between 0 and 4`);
            }
            return Math.fround(value);
        };
        const rate = (value) => {
            if (!Number.isFinite(value) || value < 0.125 || value > 8) {
                throw new RangeError("Audio rate must be finite and between 0.125 and 8");
            }
            return Math.fround(value);
        };
        const bus = (value) => {
            if (!Object.hasOwn(buses, value)) throw new TypeError("Audio bus must be music, effect, or ui");
            return buses[value];
        };
        const voice = (value) => {
            if (!Number.isInteger(value) || value <= 0 || value > 0xffffffff) {
                throw new TypeError("Audio voice must be a non-zero uint32 handle");
            }
            return value >>> 0;
        };
        return Object.freeze({
            load(name, data) {
                assertAlive();
                const bytes = data instanceof Uint8Array ? data : new Uint8Array(data);
                if (!bytes.length || bytes.length > 32 * 1024 * 1024) {
                    throw new RangeError("Audio data must be 1 byte to 32 MiB");
                }
                check(withMemory([bytes, nameBytes(name)], (dataPtr, namePtr) =>
                    module._vis_audio_load(engine, namePtr, dataPtr, bytes.length)), "Audio load");
            },
            start() {
                assertAlive();
                check(module._vis_audio_start(engine), "Audio start");
            },
            play(name, options = {}) {
                assertAlive();
                plainFields(options, ["bus", "gain", "rate", "loop"], "Audio playback options",
                    "audio playback option");
                const busCode = bus(options.bus ?? "effect");
                const gainValue = gain(options.gain ?? 1, "Audio playback gain");
                const rateValue = rate(options.rate ?? 1);
                const loop = options.loop ?? false;
                if (typeof loop !== "boolean") throw new TypeError("Audio playback loop must be boolean");
                return withMemory([nameBytes(name), 4], (namePtr, output) => {
                    check(module._vis_audio_play(engine, namePtr, busCode, gainValue, rateValue, loop ? 1 : 0, output),
                        "Audio play");
                    const handle = new Uint32Array(module.HEAPU8.buffer, output, 1)[0];
                    if (!handle) throw new Error("Audio play returned no voice handle");
                    return handle;
                });
            },
            stop(handle) {
                assertAlive();
                check(module._vis_audio_stop(engine, voice(handle)), "Audio stop");
            },
            active(handle) {
                assertAlive();
                return module._vis_audio_active(engine, voice(handle)) !== 0;
            },
            masterGain(value) {
                assertAlive();
                check(module._vis_audio_master_gain(engine, gain(value, "Audio master gain")), "Audio master gain");
            },
            busGain(name, value) {
                assertAlive();
                check(module._vis_audio_bus_gain(engine, bus(name), gain(value, "Audio bus gain")), "Audio bus gain");
            },
            transport(value) {
                assertAlive();
                plainFields(value, ["time", "rate", "playing", "seek"], "Audio transport", "audio transport field");
                const time = Math.fround(value.time);
                if (!Number.isFinite(value.time) || !Number.isFinite(time) || value.time < 0) {
                    throw new RangeError("Audio transport time must be finite, non-negative, and representable as float32");
                }
                const playing = value.playing ?? false;
                const seek = value.seek ?? false;
                if (typeof playing !== "boolean" || typeof seek !== "boolean") {
                    throw new TypeError("Audio transport playing and seek must be boolean");
                }
                check(module._vis_audio_transport(engine, time, rate(value.rate ?? 1), playing ? 1 : 0, seek ? 1 : 0),
                    "Audio transport sync");
            },
        });
    })() : undefined;

    const cameraInput = (action, x = 0, y = 0) => {
        assertAlive();
        const code = CAMERA_ACTIONS[action];
        if (code === undefined) throw new TypeError(`Unknown camera action '${action}'`);
        if (!Number.isFinite(x) || !Number.isFinite(y)) throw new TypeError("Camera deltas must be finite numbers");
        if (typeof module._vis_camera !== "function") return false;
        const result = module._vis_camera(engine, code, x, y);
        if (result === 2 || result === 3) return false;
        check(result, "Camera input");
        return true;
    };
    const input = typeof module._vis_input === "function" ? (event) => {
        assertAlive();
        if (!event || typeof event !== "object") throw new TypeError("Input must be an event object");
        const type = INPUT_TYPES[event.type];
        if (type === undefined) throw new TypeError(`Unknown input type '${event.type}'`);
        const {
            x = 0, y = 0, dx = 0, dy = 0, wheelX = 0, wheelY = 0,
            button = -1, buttons = 0, modifiers = 0, pointerId = 0, repeat, time,
        } = event;
        if (![x, y, dx, dy, wheelX, wheelY, time].every(Number.isFinite) || time < 0) {
            throw new TypeError("Input coordinates, deltas, wheel values, and time must be finite and time must be non-negative");
        }
        if (!Number.isInteger(button) || button < -1 || button > 0x7fffffff
            || !Number.isInteger(buttons) || buttons < 0 || buttons > 0xffffffff
            || !Number.isInteger(modifiers) || modifiers < 0 || modifiers > 0xffffffff
            || !Number.isInteger(pointerId) || pointerId < 0 || pointerId > 0xffffffff) {
            throw new TypeError("Input button, buttons, modifiers, and pointer ID must be valid integers");
        }
        const key = inputKey(event.key);
        if (type >= INPUT_TYPES.keydown && key === 0) {
            throw new TypeError("Input key is required for keydown and keyup events");
        }
        const flags = module._vis_input(engine, type, x, y, dx, dy, wheelX, wheelY, button, buttons, modifiers,
            pointerId, key, repeat ? 1 : 0, time) >>> 0;
        let sample;
        if (flags & 16) {
            const getters = [
                "_vis_ui_sample_handle",
                "_vis_ui_sample_tag",
                "_vis_ui_sample_type",
                "_vis_ui_sample_x",
                "_vis_ui_sample_y",
                "_vis_ui_sample_u",
                "_vis_ui_sample_v",
                "_vis_ui_sample_rgba",
            ];
            if (!getters.every((getter) => typeof module[getter] === "function")) {
                throw new Error("UI sample result is missing its metadata exports");
            }
            const tag = module._vis_ui_sample_tag(engine);
            const typeName = module._vis_ui_sample_type(engine);
            const packed = module._vis_ui_sample_rgba(engine) >>> 0;
            sample = Object.freeze({
                position: Object.freeze({
                    x: module._vis_ui_sample_x(engine),
                    y: module._vis_ui_sample_y(engine),
                }),
                value: Object.freeze({
                    x: module._vis_ui_sample_u(engine),
                    y: module._vis_ui_sample_v(engine),
                }),
                rgba: Object.freeze([
                    (packed >>> 24) & 0xff,
                    (packed >>> 16) & 0xff,
                    (packed >>> 8) & 0xff,
                    packed & 0xff,
                ]),
                object: Object.freeze({
                    handle: module._vis_ui_sample_handle(engine) >>> 0,
                    id: tag ? module.UTF8ToString(tag) : null,
                    type: typeName ? module.UTF8ToString(typeName) : "",
                }),
            });
        }
        const result = {
            handled: Boolean(flags & 1),
            redraw: Boolean(flags & 2),
            capture: Boolean(flags & 4),
            release: Boolean(flags & 8),
        };
        if (sample) result.sample = sample;
        if (flags & 32) result.error = lastError() || "Input handling failed";
        return Object.freeze(result);
    } : undefined;
    const hasInputFrames = typeof module._vis_input_begin === "function";
    const beginInputFrame = (time) => {
        assertAlive();
        if (!Number.isFinite(time) || time < 0) throw new TypeError("Input frame time must be finite and non-negative");
        check(module._vis_input_begin(engine, time), "Input frame begin");
    };
    const releaseInput = (time) => {
        assertAlive();
        if (!Number.isFinite(time) || time < 0) throw new TypeError("Input release time must be finite and non-negative");
        const flags = module._vis_input_release(engine, time) >>> 0;
        const result = {handled: Boolean(flags & 1), redraw: Boolean(flags & 2), capture: false, release: false};
        if (flags & 32) result.error = lastError() || "Input release failed";
        return Object.freeze(result);
    };
    const retainedRuntimeExports = [
        "_vis_runtime_active",
        "_vis_runtime_advance",
        "_vis_runtime_time",
        "_vis_runtime_dropped",
        "_vis_runtime_tick",
        "_vis_runtime_steps",
        "_vis_runtime_interpolation",
        "_vis_runtime_sound_count",
        "_vis_runtime_sound",
    ];
    const hasRetainedRuntime = retainedRuntimeExports.every(
        (exportName) => typeof module[exportName] === "function",
    );
    const runtimeSounds = () => {
        const count = module._vis_runtime_sound_count(engine) >>> 0;
        if (!count) return Object.freeze([]);
        if (count > 64) throw new Error("Retained runtime returned too many sound events");
        const buses = ["music", "effect", "ui"];
        // Each event fills the 256-byte asset name, then its bus, gain, rate and loop.
        return Object.freeze(withMemory([272], (asset) => Array.from({length: count}, (_, index) => {
            check(module._vis_runtime_sound(engine, index, asset, 256,
                asset + 256, asset + 260, asset + 264, asset + 268), "Retained runtime sound query");
            const fields = new DataView(module.HEAPU8.buffer, asset + 256, 16);
            return Object.freeze({
                asset: module.UTF8ToString(asset),
                bus: buses[fields.getUint32(0, true)] ?? "effect",
                gain: fields.getFloat32(4, true),
                rate: fields.getFloat32(8, true),
                loop: fields.getUint8(12) !== 0,
            });
        })));
    };
    const retainedLua = () => {
        assertAlive();
        return hasRetainedRuntime && module._vis_runtime_active(engine) !== 0;
    };
    const advanceRuntime = hasRetainedRuntime ? (elapsed) => {
        assertAlive();
        const nativeElapsed = Math.fround(elapsed);
        if (!Number.isFinite(elapsed) || elapsed < 0 || !Number.isFinite(nativeElapsed)) {
            throw new RangeError("Retained runtime elapsed time must be finite, non-negative, and representable as float32");
        }
        if (!retainedLua()) return null;
        check(module._vis_runtime_advance(engine, nativeElapsed), "Retained runtime advance");
        return Object.freeze({
            time: module._vis_runtime_time(engine),
            dropped: module._vis_runtime_dropped(engine),
            tick: module._vis_runtime_tick(engine),
            steps: module._vis_runtime_steps(engine) >>> 0,
            interpolation: module._vis_runtime_interpolation(engine),
            sounds: runtimeSounds(),
        });
    } : undefined;
    const hostTheme = (palette) => {
        assertAlive();
        if (!palette || typeof palette !== "object" || typeof palette.dark !== "boolean") {
            throw new TypeError("Host theme must include a boolean dark value");
        }
        const {objects: fallbackObjects, ...fallback} = HOST_THEMES[palette.dark ? "dark" : "light"];
        // A color the host leaves undefined or empty is the fallback's.
        const color = (value, otherwise, label) => rgba(value === undefined || value === "" ? otherwise : value, label);
        const given = palette.objects ?? fallbackObjects;
        if (!Array.isArray(given) || !given.length || given.length > 10) {
            throw new TypeError("Host theme objects must contain 1 to 10 colors");
        }
        const objects = Uint32Array.from(given, (value, index) =>
            color(value, fallbackObjects[index], `Host theme object color ${index + 1}`));
        const colors = Object.entries(fallback).map(([name, otherwise]) =>
            color(palette[name], otherwise, `Host theme ${name} color`));
        check(withMemory([new Uint8Array(objects.buffer)], (objectsPtr) => module._vis_host_theme(
            engine, palette.dark ? 1 : 0, ...colors, objectsPtr, objects.length)), "Host theme");
    };

    return {
        get duration() {
            assertAlive();
            return module._vis_duration(engine);
        },
        get loop() {
            assertAlive();
            return module._vis_loop(engine) !== 0;
        },
        get cameraMode() {
            assertAlive();
            return module._vis_camera_mode(engine) === 1 ? "interactive" : "fixed";
        },
        get cameraView() {
            assertAlive();
            return module._vis_camera_view(engine) === 1 ? "3d" : "2d";
        },
        get adaptiveTheme() {
            assertAlive();
            return module._vis_adaptive_theme(engine) !== 0;
        },
        get retainedLua() {
            return retainedLua();
        },
        get runtimeTime() {
            assertAlive();
            return retainedLua() ? module._vis_runtime_time(engine) : 0;
        },
        load,
        render,
        bounds,
        intersects,
        layoutReport,
        font,
        missingFonts,
        asset,
        ...(audio ? {audio} : {}),
        cameraInput,
        ...(input ? {input} : {}),
        ...(hasInputFrames ? {beginInputFrame, releaseInput} : {}),
        ...(advanceRuntime ? {advanceRuntime} : {}),
        hostTheme,
        draw(canvas, time, pixelRatio = 1) {
            assertAlive();
            const logicalWidth = module._vis_width(engine);
            const logicalHeight = module._vis_height(engine);
            if (!Number.isInteger(pixelRatio) || pixelRatio < 1 || pixelRatio > 0xffffffff
                || logicalWidth > Math.floor(0xffffffff / pixelRatio)
                || logicalHeight > Math.floor(0xffffffff / pixelRatio)) {
                throw new Error("Pixel ratio must be a positive integer within the render size limit");
            }
            const width = logicalWidth * pixelRatio;
            const height = logicalHeight * pixelRatio;
            if (height > Math.floor(67108864 / width)) {
                throw new Error("Rendered canvas must contain at most 67108864 pixels");
            }
            if (canvas.width !== width) canvas.width = width;
            if (canvas.height !== height) canvas.height = height;
            canvas.getContext("2d").putImageData(
                new ImageData(renderFrame(time, pixelRatio), width, height),
                0,
                0,
            );
        },
        size() {
            assertAlive();
            return [module._vis_width(engine), module._vis_height(engine)];
        },
        resize(width, height) {
            assertAlive();
            if (!Number.isInteger(width) || !Number.isInteger(height) || width <= 0 || height <= 0 ||
                width > 0xffffffff || height > Math.floor(67108864 / width)) {
                throw new Error("Size must be positive integers with at most 67108864 pixels");
            }
            check(module._vis_resize(engine, width, height), "Resize");
        },
        destroy() {
            if (!engine) return;
            module._vis_destroy(engine);
            engine = 0;
        },
    };
}
