const OBJECTS = [
    "point",
    "line",
    "arrow",
    "vector",
    "circle",
    "arc",
    "angle",
    "rectangle",
    "polygon",
    "plot",
    "route",
    "path",
    "curve",
    "surface",
    "text",
    "number",
    "picture",
    "cell",
];

// Region factories combine closed shapes into one retained path.
const REGIONS = ["union", "intersection", "difference", "exclusion"];
const FACTORIES = ["group", "space", ...OBJECTS, ...REGIONS];
const PLAY_PROPERTIES = [
    "shift",
    "transform",
    "arc",
    "opacity",
    "stroke",
    "fill",
    "dash_offset",
    "tail",
    "tip",
    "progress",
    "width",
    "radius",
    "size",
    "corner",
    "trimDistance",
    "gradient",
    "blur",
    "shadow",
    "value",
    "text",
    "retype",
    "layer",
    "points",
    "from",
    "to",
    "origin",
    "control1",
    "control2",
    "fn",
    "x_range",
    "y_range",
    "discontinuities",
    "center",
    "vertex",
    "start",
    "angle",
    "level",
    "values",
];
// Descriptor fields that redraw plots and contours.
const PLOT_STATE_KEYS = ["fn", "x_range", "y_range", "discontinuities", "level", "values"];
// Geometry fields, their supported factories, and validation labels.
const GEOMETRY_KEYS = new Map([
    ["center", [["arc"], "an arc"]],
    ["vertex", [["angle"], "an angle"]],
    ["start", [["arc"], "an arc"]],
    ["angle", [["arc"], "an arc"]],
    ["points", [["polygon", "plot", "route", "path"], "a polygon, plot, route or path of one contour"]],
    ["from", [["line", "arrow", "curve", "angle"], "a line, arrow, curve or angle; move other objects with shift"]],
    ["to", [["line", "arrow", "curve", "angle"], "a line, arrow, curve or angle; move other objects with shift"]],
    ["origin", [["vector"], "a vector"]],
    ["control1", [["curve"], "a curve"]],
    ["control2", [["curve"], "a curve"]],
]);
const PIVOT_ANCHORS = new Set([
    "center",
    "left",
    "right",
    "top",
    "bottom",
    "top_left",
    "top_right",
    "bottom_left",
    "bottom_right",
]);
// Bound-state properties and the per-track segment budget.
const TRACKER_KEYS = [
    "transform",
    "opacity",
    "stroke",
    "fill",
    "progress",
    "dash_offset",
    "tail",
    "tip",
    "width",
    "radius",
    "size",
    "corner",
    "trimDistance",
    "gradient",
    "blur",
    "shadow",
    "value",
    "points",
    "from",
    "to",
    "origin",
    "control1",
    "control2",
    "fn",
    "x_range",
    "y_range",
    "discontinuities",
    "center",
    "vertex",
    "start",
    "angle",
    "level",
    "values",
];
const TRACKER_CLIP_LIMIT = 65536;
// Treat sub-tolerance intervals as one instant when checking track overlap.
const TRACKER_EPSILON = 1e-4;
// Flush pending track samples before this budget is exceeded.
const TRACKER_PENDING_LIMIT = 32768;
const ROTATE_OPTIONS = ["about", "axis", "shift", "arc", "duration", "easing", "curve", "lag"];
const ORBIT_OPTIONS = ["axis", "duration", "easing", "curve"];
const FOCUS_OPTIONS = ["padding", "duration", "easing", "curve"];
const VIEW_OPTIONS = ["camera", "optical", "background", "opacity", "layer", "hide", "follow"];
// Cap repeated rendering of the same scene through inset views.
const VIEW_LIMIT = 8;
const VIEW_HIDDEN_LIMIT = 256;
const SCALE_OPTIONS = ["about", "shift", "arc", "duration", "easing", "curve", "lag"];
const MOVE_ALONG_OPTIONS = ["from", "to", "orient", "duration", "easing", "curve", "lag"];
const CREATE_DIRECTIONS = new Set(["forward", "reverse", "clockwise", "counterclockwise"]);
const GROWTH_EDGES = new Set(["left", "right", "bottom", "top"]);
const AUDIO_BUSES = new Set(["music", "effect", "ui"]);
const MASK_MODES = new Set(["alpha", "inverse_alpha", "luma", "inverse_luma"]);
const TRANSITION_STYLES = new Set(["match", "fade", "slide", "zoom_in", "zoom_out"]);
const TRANSITION_SIDES = new Set(["left", "right", "bottom", "top"]);
// Options that shape one transition style, at the top of the options or in an entry of styles.
const TRANSITION_STYLE_OPTIONS = ["style", "from", "area", "area_margin", "match"];
const ANIM_CURVE_PRESETS = new Set([
    "linear",
    "smooth",
    "ease_in",
    "ease_out",
    "ease_in_out",
    "gentle",
    "snappy",
    "back",
    "bounce",
    "elastic",
]);
const FADE_OPTIONS = Object.freeze({
    shift: [0, 0, 0],
    scale: 1,
    duration: 1,
    easing: "smooth",
});
const MATCH_OPTIONS = Object.freeze(["arc", "lag", "pairs", "strings", "diff", "shapes", "fade", "copy", "path"]);
const SEARCH_OPTIONS = Object.freeze(["match", "lines", "whole"]);
const EDIT_OPTIONS = Object.freeze(["id", "arc", "lag", "fade"]);
const HIGHLIGHT_OPTIONS = Object.freeze(["dim", "duration", "easing", "curve"]);
const WARP_OPTIONS = Object.freeze(["x_range", "y_range", "step"]);
// Cap mapping samples to keep emitted Lua near one megabyte.
const WARP_SAMPLE_LIMIT = 65536;
// Match native plot sample and bisection budgets for band/undefined boundaries.
const PLOT_SAMPLE_LIMIT = 1000000;
const PLOT_HALVINGS = 40;
// The largest finite single; a value past it is undefined to a plot, as to the scene.
const SINGLE_MAX = 3.4028234663852886e38;
// A point Lua plot breaks a polyline at, as its coordinates are not numbers.
const PLOT_BREAK = "{0/0,0/0}";
// Where a value of a plot lies: undefined, or below, inside or above its y_range.
const PLOT_UNDEFINED = 0;
const PLOT_BELOW = 1;
const PLOT_INSIDE = 2;
const PLOT_ABOVE = 3;
const ARRANGE_OPTIONS = Object.freeze(["direction", "gap", "center"]);
// The options of the labels of a space: lists of [value, label] for each axis, and their style.
const LABELS_OPTIONS = Object.freeze(["x", "y", "z", "gap", "size", "color", "side", "placement"]);
const INDICATE_OPTIONS = Object.freeze({
    color: "focus",
    scale: 1.2,
    duration: 1,
    easing: "smooth",
});
const WIGGLE_OPTIONS = Object.freeze({
    scale: 1.1,
    angle: 0.0628318531,
    swings: 6,
    duration: 2,
    easing: "smooth",
});
const CIRCUMSCRIBE_OPTIONS = Object.freeze({
    shape: "rectangle",
    color: "focus",
    width: 3,
    padding: 0.1,
    window: 0.3,
    duration: 1,
    easing: "smooth",
});
const PASSING_FLASH_OPTIONS = Object.freeze({
    color: "focus",
    width: 6,
    window: 0.3,
    duration: 1,
    easing: "smooth",
});
const FLASH_OPTIONS = Object.freeze({
    color: "focus",
    width: 3,
    padding: 0.1,
    size: 0.2,
    lines: 12,
    duration: 1,
    easing: "smooth",
});
const RIPPLE_OPTIONS = Object.freeze({
    color: "focus",
    width: 3,
    padding: 0,
    size: 0.4,
    duration: 1,
    easing: "snappy",
});

const SCENE_OWNERS = new WeakMap();
const SCENE_SNAPSHOTS = new WeakMap();
const SCENE_STATES = new WeakMap();
const HANDLE_TYPES = new WeakMap();
// Retain geometry point counts from creation or the first descriptor. Plots may change counts.
const POINT_COUNTS = new WeakMap();
// Retained plot ranges, bands, and sorted discontinuities; null means absent.
const PLOT_CUTS = new WeakMap();
const SPACE_RANGES = new WeakMap();
const TRACKERS = new WeakMap();
const SCENE_TRACKERS = new WeakMap();
// The text, language, id and parent of each code block, which search and edit read.
const CODE_BLOCKS = new WeakMap();

function _string(value) {
    if (value.includes("\0")) throw new TypeError("Strings cannot contain zero bytes");
    const escaped = value.replace(/[\\"\x01-\x1f\x7f]/g, (character) => {
        if (character === "\\") return "\\\\";
        if (character === '"') return '\\"';
        if (character === "\n") return "\\n";
        if (character === "\r") return "\\r";
        if (character === "\t") return "\\t";
        return `\\${character.charCodeAt(0).toString().padStart(3, "0")}`;
    });
    return `"${escaped}"`;
}

const GRADIENT_KEYS = Object.freeze({
    linear: ["type", "stops", "from", "to"],
    radial: ["type", "stops", "center", "radius", "focal", "focalRadius"],
    conic: ["type", "stops", "center", "angle"],
});

// Maps the camelCase JS gradient description onto the Lua `gradient` table.
// Booleans and end-stop colors keep the legacy two-stop linear form.
function _gradient(value) {
    if (value === undefined || typeof value === "boolean" || typeof value === "string") return value;
    if (!value || (Object.getPrototypeOf(value) !== Object.prototype && Object.getPrototypeOf(value) !== null)) {
        throw new TypeError("gradient must be a boolean, color, or gradient options object");
    }
    const keys = GRADIENT_KEYS[value.type];
    if (!keys) throw new TypeError('gradient.type must be "linear", "radial", or "conic"');
    const output = {};
    for (const key of Object.keys(value)) {
        if (!keys.includes(key)) throw new TypeError(`Unknown ${value.type} gradient option: ${key}`);
        if (value[key] === undefined) continue;
        output[key === "focalRadius" ? "focal_radius" : key] = value[key];
    }
    if (output.stops !== undefined) {
        if (!Array.isArray(output.stops) || output.stops.length < 2 || output.stops.length > 8) {
            throw new TypeError("gradient.stops must contain 2 to 8 stops");
        }
    }
    return output;
}

function _styleOptions(options) {
    if (!options || typeof options !== "object" || !Object.hasOwn(options, "gradient")) return options;
    return {...options, gradient: _gradient(options.gradient)};
}

function _maskMode(value) {
    if (!MASK_MODES.has(value)) throw new TypeError("Mask mode must be alpha, inverse_alpha, luma, or inverse_luma");
    return value;
}

// The style of a scene transition: a style name, or a plain object naming it as style beside
// the options of that style.
function _transitionStyle(value) {
    if (typeof value === "string") value = { style: value };
    if (!value || (Object.getPrototypeOf(value) !== Object.prototype && Object.getPrototypeOf(value) !== null)) {
        throw new TypeError("A scene transition style must be a style name or a plain object");
    }
    const unknown = Object.keys(value).find((key) => !TRANSITION_STYLE_OPTIONS.includes(key));
    if (unknown) throw new TypeError(`Unknown scene transition style option: ${unknown}`);
    if (Object.hasOwn(value, "style") && !TRANSITION_STYLES.has(value.style)) {
        throw new TypeError("Scene transition style must be match, fade, slide, zoom_in or zoom_out");
    }
    if (Object.hasOwn(value, "from")) {
        if (value.style !== "slide") throw new TypeError("Scene transition from applies only to the slide style");
        if (!TRANSITION_SIDES.has(value.from)) {
            throw new TypeError("Scene transition from must be left, right, top or bottom");
        }
    }
    const style = { ...value };
    const zoom = value.style === "zoom_in" || value.style === "zoom_out";
    if (Object.hasOwn(value, "match")) {
        if (typeof value.match !== "boolean") throw new TypeError("Scene transition match must be a boolean");
        if (value.match && !zoom && value.style !== "slide") {
            throw new TypeError("Scene transition match applies only to the slide and zoom styles");
        }
    }
    if (!Object.hasOwn(value, "area")) {
        if (zoom) throw new TypeError("A scene transition zoom needs an area: an object id or { x, y, width, height }");
    } else if (!zoom) {
        throw new TypeError("Scene transition area applies only to the zoom styles");
    } else if (typeof value.area !== "string") {
        // A part of the viewport, as fractions of it from its top left corner.
        const area = value.area;
        const keys = ["x", "y", "width", "height"];
        if (
            !area ||
            Object.getPrototypeOf(area) !== Object.prototype ||
            Object.keys(area).some((key) => !keys.includes(key)) ||
            keys.some((key) => !Number.isFinite(area[key]))
        ) {
            throw new TypeError("Scene transition area must be an object id or { x, y, width, height }");
        }
        style.area = { x: area.x, y: area.y, width: area.width, height: area.height };
    }
    if (Object.hasOwn(value, "area_margin")) {
        if (!zoom || typeof value.area !== "string") {
            throw new TypeError("Scene transition area_margin applies only to an object zoom area");
        }
        if (!Number.isFinite(value.area_margin) || value.area_margin < 0 || value.area_margin > SINGLE_MAX) {
            throw new TypeError("Scene transition area_margin must be a finite non-negative number");
        }
    }
    return style;
}

function _lua(value) {
    if (typeof value === "number") {
        if (!Number.isFinite(value)) throw new TypeError("Scene numbers must be finite");
        return Object.is(value, -0) ? "0" : String(value);
    }
    if (typeof value === "string") return _string(value);
    if (typeof value === "boolean") return value ? "true" : "false";
    if (Array.isArray(value)) return `{${value.map(_lua).join(",")}}`;
    if (value && (Object.getPrototypeOf(value) === Object.prototype || Object.getPrototypeOf(value) === null)) {
        return `{${Object.keys(value)
            .sort()
            .map((key) => `[${_string(key)}]=${_lua(value[key])}`)
            .join(",")}}`;
    }
    throw new TypeError("Scene values must be finite numbers, strings, booleans, arrays, or plain objects");
}

function _spaceRange(value, name) {
    if (value === undefined) return Object.freeze([-5, 5, 1]);
    let range;
    if (Array.isArray(value) && value.length === 3) range = value;
    else if (
        value &&
        (Object.getPrototypeOf(value) === Object.prototype || Object.getPrototypeOf(value) === null) &&
        Object.hasOwn(value, "min") &&
        Object.hasOwn(value, "max") &&
        Object.hasOwn(value, "step")
    ) {
        range = [value.min, value.max, value.step];
    } else {
        throw new TypeError(`${name} must contain min, max, and step`);
    }
    const normalized = range.map(Math.fround);
    if (!normalized.every(Number.isFinite) || normalized[0] > normalized[1] || normalized[2] <= 0) {
        throw new RangeError(`${name} must be finite with min <= max and step > 0`);
    }
    return Object.freeze(normalized);
}

// The steps of step that cover from minimum to maximum, the last one shortened to end there.
function _rangeSteps([minimum, maximum, step], name, limit) {
    const steps = Math.ceil((maximum - minimum) / step - 1e-4);
    if (!(steps >= 1 && steps <= limit)) throw new RangeError(`${name} must cover from 1 to ${limit} steps`);
    return steps;
}

// The x at which step i of steps starts, or the end of the range for the last.
function _stepAt([minimum, maximum, step], i, steps) {
    return i >= steps ? maximum : Math.fround(minimum + step * i);
}

// Inclusive range samples at fixed steps.
function _rangeSamples([minimum, maximum, step], name, limit) {
    const intervals = Math.floor((maximum - minimum) / step + 1e-4);
    if (!(intervals >= 0 && intervals < limit)) throw new RangeError(`${name} must hold from 1 to ${limit} samples`);
    return Array.from({ length: intervals + 1 }, (_, i) => Math.fround(minimum + step * i));
}

// Warp-axis samples and their normalized retained range.
function _warpSamples(value, name) {
    const [minimum, maximum, step] = _spaceRange(value, name);
    const steps = Math.ceil((maximum - minimum) / step - 1e-4);
    if (!(steps >= 1 && steps < WARP_SAMPLE_LIMIT)) {
        throw new RangeError(`${name} must run from min to a greater max in at most ${WARP_SAMPLE_LIMIT} samples`);
    }
    const samples = Array.from({ length: steps + 1 }, (_, i) => Math.fround(minimum + step * i));
    return { samples, range: [minimum, samples[steps], step].map(_single) };
}

// Emit the shortest decimal that round-trips the native float.
function _single(value) {
    const single = Math.fround(value);
    for (let digits = 1; digits < 9; digits++) {
        const shorter = Number(single.toPrecision(digits));
        if (Math.fround(shorter) === single) return shorter;
    }
    return Number(single.toPrecision(9));
}

// Calls a function of x and y the builder samples, which must return [x, y] of finite numbers.
function _sampleVector(fn, x, y, name) {
    const value = fn(x, y);
    if (!Array.isArray(value) || value.length !== 2 || !value.every((n) => Number.isFinite(Math.fround(n)))) {
        throw new RangeError(`${name} must return [x, y] of finite numbers; at (${x}, ${y}) it did not`);
    }
    return value;
}

// Calls a function of x the builder samples, which must return a finite number.
function _sampleNumber(fn, x, name) {
    const y = fn(x);
    if (!Number.isFinite(y)) throw new RangeError(`${name} must return a finite number`);
    return y;
}

// The single just above a single, as C's nextafterf toward infinity.
function _singleAbove(value) {
    const single = new Float32Array([value]);
    const bits = new Int32Array(single.buffer);
    if (value === 0) bits[0] = 1;
    else bits[0] += value > 0 ? 1 : -1;
    return single[0];
}

// y_range of a plot: [lo, hi] in single precision, as the scene reads the numbers the builder
// writes, with lo below hi.
function _plotBand(value) {
    if (!Array.isArray(value) || value.length !== 2 || !value.every((n) => typeof n === "number")) {
        throw new TypeError("Plot y_range must be [lo, hi]");
    }
    const band = value.map(_single);
    if (!band.every(Number.isFinite)) throw new RangeError("Plot y_range must hold two finite numbers");
    if (!(band[0] < band[1])) throw new RangeError("Plot y_range must have lo below hi");
    return band;
}

// The samples a plot of fn takes over x_range, where Lua plot takes them: from min every step up
// to max, which counts when a step lands on it.
function _plotGrid([minimum, maximum, step]) {
    const intervals = Math.floor((maximum - minimum) / step);
    if (!(intervals >= 1 && intervals < PLOT_SAMPLE_LIMIT)) {
        throw new RangeError(`Plot x_range must hold from 2 to ${PLOT_SAMPLE_LIMIT} samples`);
    }
    const upper = _singleAbove(maximum);
    const at = (i) => Math.fround(minimum + step * i);
    let count = intervals + 1;
    while (count > 1 && !(at(count - 1) <= upper)) count--;
    while (count < PLOT_SAMPLE_LIMIT && at(count) <= upper) count++;
    return Array.from({ length: count }, (_, i) => {
        const x = at(i);
        return x > maximum && x <= upper ? maximum : x;
    });
}

// Match native graph clipping at undefined values, bands, and sorted discontinuities.
// Discontinuity margins are 0.001 steps; output pieces use float coordinates.
function _plotPieces(fn, grid, step, band, breaks, stored = null) {
    // The points of the open piece, and the pieces before it.
    const points = [];
    const pieces = [];
    let open = false;
    const sample = (x) => {
        const y = fn(x);
        if (y === undefined || y === null) return { x, y: 0, side: PLOT_UNDEFINED };
        if (typeof y !== "number") {
            throw new TypeError(
                `Plot fn must return a number, or undefined where it is undefined; at ${x} it returned ${typeof y}`,
            );
        }
        if (!(Math.abs(y) <= SINGLE_MAX)) return { x, y: 0, side: PLOT_UNDEFINED };
        const side = band && y < band[0] ? PLOT_BELOW : band && y > band[1] ? PLOT_ABOVE : PLOT_INSIDE;
        return { x, y, side };
    };
    const sampleAt = (x) => {
        const value = sample(x);
        if (stored) stored.push([x, value.side === PLOT_UNDEFINED ? NaN : value.y]);
        return value;
    };
    const add = (x, y) => points.push([Math.fround(x), Math.fround(y)]);
    // A point a cut makes, unless the open piece already ends there.
    const put = (x, y) => {
        const last = points[points.length - 1];
        if (last && last[0] === Math.fround(x) && last[1] === Math.fround(y)) return;
        add(x, y);
    };
    // Ends the open piece, which is dropped if it has fewer than two points.
    const end = () => {
        if (!open) return;
        open = false;
        if (points.length >= 2) pieces.push(points.slice());
        points.length = 0;
    };
    const begin = () => {
        end();
        open = true;
    };
    // Begins a run of samples with its first, a piece if it is inside.
    const first = (s) => {
        end();
        if (s.side !== PLOT_INSIDE) return;
        begin();
        add(s.x, s.y);
    };
    // Where the graph between a sample inside and one that is not leaves the band: on its edge
    // where it runs up to it, or else at the last value inside.
    const border = (inside, outside) => {
        for (let i = 0; i < PLOT_HALVINGS; i++) {
            const x = 0.5 * (inside.x + outside.x);
            if (x === inside.x || x === outside.x) break;
            const middle = sample(x);
            if (middle.side === PLOT_INSIDE) inside = middle;
            else outside = middle;
        }
        let y = inside.y;
        if (band) {
            const tolerance = (band[1] - band[0]) * 1e-3;
            if (outside.side === PLOT_ABOVE && band[1] - y <= tolerance) y = band[1];
            if (outside.side === PLOT_BELOW && y - band[0] <= tolerance) y = band[0];
        }
        return { x: inside.x, y };
    };
    // Between samples on opposite sides of the band, the graph crosses it or jumps over it.
    const across = (left, right) => {
        for (let i = 0; i < PLOT_HALVINGS; i++) {
            const x = 0.5 * (left.x + right.x);
            if (x === left.x || x === right.x) return false;
            const middle = sample(x);
            if (middle.side === PLOT_INSIDE) {
                const entry = border(middle, left);
                const exit = border(middle, right);
                begin();
                put(entry.x, entry.y);
                put(middle.x, middle.y);
                put(exit.x, exit.y);
                end();
                return true;
            }
            if (middle.side === left.side) left = middle;
            else if (middle.side === right.side) right = middle;
            else return false;
        }
        return false;
    };
    // The graph from sample p to the next sample q, where p is drawn if it is inside.
    const stepTo = (p, q) => {
        if (p.side === PLOT_INSIDE) {
            if (q.side === PLOT_INSIDE) {
                add(q.x, q.y);
                return;
            }
            const exit = border(p, q);
            put(exit.x, exit.y);
            end();
            return;
        }
        if (q.side === PLOT_INSIDE) {
            const entry = border(q, p);
            begin();
            put(entry.x, entry.y);
            put(q.x, q.y);
            return;
        }
        if (p.side !== PLOT_UNDEFINED && q.side !== PLOT_UNDEFINED && p.side !== q.side && !across(p, q) && stored) {
            // Keep the rejected crossing in the retained column, before its last sample.
            const last = stored.pop();
            stored.push([NaN, NaN], last);
        }
    };
    const firstX = grid[0];
    const lastX = grid[grid.length - 1];
    const margin = step * 1e-3;
    let next = 0;
    while (next < breaks.length && breaks[next] < firstX) next++;
    const within = (index) => index < breaks.length && breaks[index] <= lastX;
    let resume = -Infinity;
    let running = false;
    let previous = null;
    for (const x of grid) {
        while (within(next) && breaks[next] - margin < x) {
            const at = breaks[next++];
            if (running && previous.x < at - margin) stepTo(previous, sampleAt(at - margin));
            end();
            if (stored) stored.push([NaN, NaN]);
            running = false;
            resume = at + margin;
            // A discontinuity within two margins of the next one shares its gap.
            if (within(next) && breaks[next] - margin <= resume) continue;
            if (resume < lastX) {
                previous = sampleAt(resume);
                first(previous);
                running = true;
            }
        }
        if (x <= resume) continue;
        const s = sampleAt(x);
        if (running) stepTo(previous, s);
        else first(s);
        previous = s;
        running = true;
    }
    end();
    return pieces;
}

function _plotStored(pieces, samples) {
    if (samples.length > PLOT_SAMPLE_LIMIT) throw new RangeError(`Plot samples must hold at most ${PLOT_SAMPLE_LIMIT} points`);
    const number = (value) => Number.isFinite(value) ? _lua(_single(value)) : "0/0";
    const column = `{${samples.map(([x, y]) => `{${number(x)},${number(y)}}`).join(",")}}`;
    return `{points=${_plotLine(pieces)},samples=${column}}`;
}

// A point of a plot as Lua reads it; one with a coordinate that is not finite breaks the line.
// Whether a point of a plot has a coordinate that is not finite, where Lua plot breaks the line.
function _plotUndefined(point) {
    const values = Array.isArray(point) ? point : point && typeof point === "object" ? [point.x, point.y, point.z] : [];
    return values.some((n) => typeof n === "number" && !Number.isFinite(n));
}

function _plotPoint(point) {
    return _plotUndefined(point) ? PLOT_BREAK : _lua(point);
}

// What a plot made with options, which plot() has checked, cuts the graphs of its states with
// (see PLOT_CUTS).
function _plotCut(options) {
    return {
        range: options.fn === undefined ? null : _spaceRange(options.x_range, "Plot x_range"),
        band: options.y_range === undefined ? null : _plotBand(options.y_range),
        breaks: options.discontinuities === undefined ? null : [...options.discontinuities].sort((a, b) => a - b),
    };
}

// The pieces of a graph as the points of a Lua plot, with a break between each two.
function _plotLine(pieces) {
    const piece = (vertices) => vertices.map(([x, y]) => `{${_lua(_single(x))},${_lua(_single(y))}}`).join(",");
    return `{${pieces.map(piece).join(`,${PLOT_BREAK},`)}}`;
}

function _spaceRanges(options) {
    return Object.freeze([
        _spaceRange(options.x, "Space x range"),
        _spaceRange(options.y, "Space y range"),
        _spaceRange(options.z, "Space z range"),
    ]);
}

function _sampleAxis(range, name) {
    const [minimum, maximum, step] = range;
    const exactStart = Math.ceil(minimum / step) * step;
    const count = Math.floor((maximum + step * 0.001 - exactStart) / step) + 1;
    if (!Number.isFinite(exactStart) || count < 1 || count > 4096) {
        throw new RangeError(`${name} range has no samples or exceeds 4096 samples per axis`);
    }
    return Object.freeze({start: Math.fround(exactStart), step, count});
}

// The x, y and z of a surface point as the scene reads them, in single precision.
function _surfacePoint(point) {
    const values = Array.isArray(point)
        ? point
        : point && typeof point === "object" ? [point.x, point.y, ...(point.z === undefined ? [] : [point.z])] : [];
    if ((values.length !== 2 && values.length !== 3) || !values.every((n) => Number.isFinite(Math.fround(n)))) {
        throw new TypeError("Surface points must contain 2 or 3 finite numbers");
    }
    return [Math.fround(values[0]), Math.fround(values[1]), Math.fround(values[2] ?? 0)];
}

// A surface colored by a function takes the colors the function gives instead, one for each
// point in order: the builder calls it where Lua surface would, with the x, y and z of the point.
function _surfaceOptions(scene, options) {
    if (!options || typeof options.colors !== "function") return options;
    if (!Array.isArray(options.points)) throw new TypeError("Surface requires points");
    const state = SCENE_STATES.get(scene);
    const colors = [];
    state.sampleActive = true;
    try {
        for (const point of options.points) {
            const [x, y, z] = _surfacePoint(point);
            const color = options.colors(x, y, z);
            if (typeof color !== "string") {
                throw new TypeError(
                    `Surface colors must return a hex color or Theme color role; at (${x}, ${y}, ${z}) it did not`,
                );
            }
            colors.push(color);
        }
    } finally {
        state.sampleActive = false;
    }
    return { ...options, colors };
}

function _sampleColor(value, name) {
    if (typeof value !== "string" || !/^#?(?:[0-9a-f]{3,4}|[0-9a-f]{6}|[0-9a-f]{8})$/i.test(value)) {
        throw new TypeError(`${name} callback must return a #rgb, #rgba, #rrggbb, or #rrggbbaa color`);
    }
    return value;
}

function _viewportBounds(bounds) {
    if (!bounds || (Object.getPrototypeOf(bounds) !== Object.prototype && Object.getPrototypeOf(bounds) !== null)) {
        throw new TypeError("Viewport bounds must be a normalized rectangle");
    }
    const { x, y, width, height } = bounds;
    if (
        ![x, y, width, height].every(Number.isFinite) ||
        x < 0 ||
        y < 0 ||
        width <= 0 ||
        height <= 0 ||
        x + width > 1 ||
        y + height > 1
    ) {
        throw new RangeError("Viewport bounds must be finite, positive, and normalized within 0..1");
    }
    return { x, y, width, height };
}

function _direction(value) {
    if (!CREATE_DIRECTIONS.has(value)) {
        throw new TypeError("Create direction must be forward, reverse, clockwise, or counterclockwise");
    }
    return value;
}

function _growthEdge(value) {
    if (!GROWTH_EDGES.has(value)) {
        throw new TypeError("Growth edge must be left, right, bottom, or top");
    }
    return value;
}

function _springResponse(seconds, options) {
    if (!Number.isFinite(seconds) || seconds < 0) {
        throw new RangeError("seconds must be a finite non-negative number");
    }
    if (!options || (Object.getPrototypeOf(options) !== Object.prototype && Object.getPrototypeOf(options) !== null)) {
        throw new TypeError("spring response requires an options object");
    }
    const fields = ["mass", "stiffness", "damping", "displacement", "velocity"];
    for (const key of Object.keys(options)) {
        if (!fields.includes(key)) throw new TypeError(`unknown option '${key}'`);
    }
    const values = fields.map((key) => key === "velocity" && options[key] === undefined ? 0 : options[key]);
    for (let i = 0; i < values.length; i++) {
        if (!Number.isFinite(values[i])) throw new RangeError(`${fields[i]} must be a finite number`);
    }
    const [mass, stiffness, damping, displacement, velocity] = values;
    if (mass <= 0 || stiffness <= 0 || damping < 0) {
        throw new RangeError("mass and stiffness must be positive and damping non-negative");
    }
    const squared = stiffness / mass;
    const drag = damping / mass;
    const rangeError = () => { throw new RangeError("spring response is outside the finite numeric range"); };
    if (!Number.isFinite(squared) || squared <= 0 || !Number.isFinite(drag)) rangeError();
    if (seconds === 0) return displacement;

    const omega = Math.sqrt(squared);
    const alpha = 0.5 * drag;
    let cosine;
    let sine;
    let decay;
    if (alpha < omega) {
        const rate = Math.sqrt(omega - alpha) * Math.sqrt(omega + alpha);
        const phase = rate * seconds;
        if (!Number.isFinite(phase)) rangeError();
        decay = alpha * seconds;
        const sinc = Math.abs(phase) < 1e-4
            ? 1 - phase * phase / 6 + phase * phase * phase * phase / 120
            : Math.sin(phase) / phase;
        cosine = Math.cos(phase);
        sine = seconds * sinc;
    } else if (alpha === omega) {
        decay = alpha * seconds;
        cosine = 1;
        sine = seconds;
    } else {
        const rate = Math.sqrt(alpha - omega) * Math.sqrt(alpha + omega);
        const slow = -(omega / (alpha + rate)) * omega;
        decay = -slow * seconds;
        cosine = 0.5 * (1 + Math.exp(-2 * rate * seconds));
        sine = -Math.expm1(-2 * rate * seconds) / (2 * rate);
    }
    if (!Number.isFinite(decay)) return 0;
    const stateScale = Math.max(Math.abs(displacement), Math.abs(velocity));
    if (stateScale === 0) return 0;
    const response = cosine + alpha * sine;
    const responseScale = Math.max(Math.abs(response), Math.abs(sine));
    const normalized = (displacement / stateScale) * (response / responseScale)
        + (velocity / stateScale) * (sine / responseScale);
    if (normalized === 0) return 0;
    const magnitude = Math.exp(Math.log(stateScale) + Math.log(responseScale)
        + Math.log(Math.abs(normalized)) - decay);
    if (!Number.isFinite(magnitude)) rangeError();
    return normalized < 0 ? -magnitude : magnitude;
}

function _curveStrength(value) {
    if (!Number.isFinite(value) || value < 0 || value > 2) {
        throw new RangeError("Animation curve strength must be a finite number between 0 and 2");
    }
    return value;
}

function _curvePreset(preset, strength = 1) {
    if (!ANIM_CURVE_PRESETS.has(preset)) {
        throw new TypeError(`Unknown animation curve preset: ${preset}`);
    }
    return Object.freeze({ preset, strength: _curveStrength(strength) });
}

function _cubicBezier(x1, y1, x2, y2, strength = 1) {
    if (
        ![x1, y1, x2, y2].every(Number.isFinite) ||
        x1 < 0 ||
        x1 > 1 ||
        x2 < 0 ||
        x2 > 1 ||
        y1 < -2 ||
        y1 > 2 ||
        y2 < -2 ||
        y2 > 2
    ) {
        throw new RangeError("Cubic Bezier controls require x in 0..1 and y in -2..2");
    }
    return Object.freeze({
        bezier: Object.freeze([x1, y1, x2, y2]),
        strength: _curveStrength(strength),
    });
}

// A preset string or curve object, checked and normalized; reverse stays only when true.
function _curveValue(value) {
    let curve;
    if (typeof value === "string") {
        curve = _curvePreset(value);
    } else if (value && (Object.getPrototypeOf(value) === Object.prototype || Object.getPrototypeOf(value) === null)) {
        const unknown = Object.keys(value).filter(
            (key) => key !== "preset" && key !== "bezier" && key !== "strength" && key !== "reverse",
        );
        if (unknown.length) throw new TypeError(`Unknown animation curve option: ${unknown[0]}`);
        const hasPreset = Object.hasOwn(value, "preset");
        const hasBezier = Object.hasOwn(value, "bezier");
        if (hasPreset === hasBezier) throw new TypeError("Animation curve requires exactly one of preset or bezier");
        if (Object.hasOwn(value, "reverse") && typeof value.reverse !== "boolean") {
            throw new TypeError("Animation curve reverse must be a boolean");
        }
        if (hasPreset) curve = _curvePreset(value.preset, value.strength ?? 1);
        else if (Array.isArray(value.bezier) && value.bezier.length === 4) {
            curve = _cubicBezier(...value.bezier, value.strength ?? 1);
        } else {
            throw new TypeError("Animation curve bezier must contain four numbers");
        }
        if (value.reverse) curve = Object.freeze({...curve, reverse: true});
    } else {
        throw new TypeError("Animation curve must be a preset string or curve object");
    }
    return curve;
}

function _reverseCurve(value) {
    const curve = _curveValue(value);
    if (!curve.reverse) return Object.freeze({...curve, reverse: true});
    const {reverse, ...forward} = curve;
    return Object.freeze(forward);
}

// Match the native float pi used by elastic easing.
const FLOAT_PI = Math.fround(Math.PI);

function _cubic(t, first, second) {
    const inverse = 1 - t;
    return 3 * inverse * inverse * t * first + 3 * inverse * t * t * second + t * t * t;
}

function _bounce(t) {
    const n = 7.5625;
    const d = 2.75;
    if (t < 1 / d) return n * t * t;
    if (t < 2 / d) {
        t -= 1.5 / d;
        return n * t * t + 0.75;
    }
    if (t < 2.5 / d) {
        t -= 2.25 / d;
        return n * t * t + 0.9375;
    }
    t -= 2.625 / d;
    return n * t * t + 0.984375;
}

// Preserve native evaluation order for curve presets.
const CURVE_SHAPES = Object.freeze({
    linear: (s) => s,
    smooth: (s) => s * s * (3 - 2 * s),
    ease_in: (s) => s * s,
    ease_out: (s) => 1 - (1 - s) * (1 - s),
    ease_in_out: (s) => (s < 0.5 ? 2 * s * s : 1 - 2 * (1 - s) * (1 - s)),
    gentle: (s) => s * s * s * (s * (s * 6 - 15) + 10),
    snappy: (s) => 1 - (1 - s) * (1 - s) * (1 - s) * (1 - s),
    back: (s) => 1 + (1.70158 + 1) * (s - 1) * (s - 1) * (s - 1) + 1.70158 * (s - 1) * (s - 1),
    bounce: _bounce,
    elastic: (s) => Math.pow(2, -10 * s) * Math.sin((10 * s - 0.75) * (2 * FLOAT_PI / 3)) + 1,
});

// The function from progress to the eased share of a curve (smooth when none is given), as the
// scene computes it: in double precision from the single-precision values it keeps, rounded to
// single precision. Linear callbacks keep the supplied progress in double precision.
function _curveEase(value) {
    const curve = value === undefined || value === null ? _curvePreset("smooth") : _curveValue(value);
    const strength = Math.fround(curve.strength);
    let shape = CURVE_SHAPES[curve.preset];
    if (curve.bezier) {
        const [x1, y1, x2, y2] = curve.bezier.map(Math.fround);
        shape = (progress) => {
            let low = 0;
            let high = 1;
            for (let i = 0; i < 18; i++) {
                const parameter = (low + high) * 0.5;
                if (_cubic(parameter, x1, x2) < progress) low = parameter;
                else high = parameter;
            }
            return _cubic((low + high) * 0.5, y1, y2);
        };
    }
    return (progress) => {
        if (curve.preset === "linear" || strength === 0) return Math.max(0, Math.min(1, progress));
        const single = Math.fround(progress);
        const t = single < 0 ? 0 : single > 1 ? 1 : single;
        if (t === 0 || t === 1) return t;
        const sample = curve.reverse ? 1 - t : t;
        const eased = sample + (shape(sample) - sample) * strength;
        return Math.fround(curve.reverse ? 1 - eased : eased);
    };
}

function _timingValues(options, fallback = 1, zero = false, name = "Timing") {
    if (Object.hasOwn(options, "easing") && Object.hasOwn(options, "curve")) {
        throw new TypeError("Timing options must use either easing or curve, not both");
    }
    const duration = options.duration === undefined ? fallback : options.duration;
    const curve = options.curve ?? options.easing ?? "smooth";
    const lag = options.lag === undefined ? 0 : options.lag;
    if (!Number.isFinite(duration) || (zero ? duration < 0 : duration <= 0)) {
        throw new RangeError(`${name} duration must be a finite ${zero ? "non-negative" : "positive"} number of seconds`);
    }
    if (!Number.isFinite(lag) || lag < 0) throw new RangeError("Lag must be finite and non-negative");
    _curveValue(curve);
    return {duration, curve, lag};
}

function _timing(options, fallback = 1, stagger = false, zero = false, fields = [], name = "Timing") {
    if (!options || (Object.getPrototypeOf(options) !== Object.prototype && Object.getPrototypeOf(options) !== null)) {
        throw new TypeError(`${name} options must be a plain object`);
    }
    const unknown = Object.keys(options).find((key) =>
        !["duration", "curve", "easing"].includes(key) && !(stagger && key === "lag") && !fields.includes(key));
    if (unknown) throw new TypeError(`Unknown ${name} option: ${unknown}`);
    return {..._timingValues(options, fallback, zero, name),
        options: Object.fromEntries(fields.filter((key) => Object.hasOwn(options, key)).map((key) => [key, options[key]]))};
}

function _effectOptions(value, defaults, name, minimumScale) {
    if (!value || (Object.getPrototypeOf(value) !== Object.prototype && Object.getPrototypeOf(value) !== null)) {
        throw new TypeError(`${name} options must be a plain object`);
    }
    const unknown = Object.keys(value).filter((key) => !Object.hasOwn(defaults, key) && key !== "curve");
    if (unknown.length) throw new TypeError(`Unknown ${name} option: ${unknown[0]}`);
    _timingValues(value, defaults.duration, false, name);
    const options = { ...defaults, ...value };
    if (Object.hasOwn(value, "curve")) delete options.easing;
    if (Object.hasOwn(defaults, "scale") && (!Number.isFinite(options.scale) || options.scale <= minimumScale)) {
        const comparison = minimumScale === 1 ? "greater than 1" : "positive";
        throw new RangeError(`${name} scale must be a finite ${comparison} number`);
    }
    return options;
}

function _emphasisValues(values, name) {
    if (!Number.isFinite(values.width) || values.width <= 0) {
        throw new RangeError(`${name} width must be a finite positive number of pixels`);
    }
    if (Object.hasOwn(values, "padding") && (!Number.isFinite(values.padding) || values.padding < 0)) {
        throw new RangeError(`${name} padding must be a finite number of zero or more`);
    }
    if (Object.hasOwn(values, "window") && (!Number.isFinite(values.window) || values.window <= 0 || values.window > 1)) {
        throw new RangeError(`${name} window must be greater than 0 and at most 1`);
    }
    if (Object.hasOwn(values, "size") && (!Number.isFinite(values.size) || values.size <= 0)) {
        throw new RangeError(`${name} size must be a finite positive number of world units`);
    }
}

function _snapshot(scene) {
    const existing = SCENE_SNAPSHOTS.get(scene);
    if (existing) return existing;
    _flushTracks(scene);
    const state = SCENE_STATES.get(scene);
    const snapshot = {
        config: state.config, lines: [], handles: state.handles, audio: state.audio, timelines: state.timelines,
    };
    SCENE_SNAPSHOTS.set(scene, snapshot);
    for (const line of state.lines) {
        if (typeof line === "function") snapshot.lines.push(line);
        else snapshot.lines.push(line);
    }
    Object.freeze(snapshot.lines);
    return Object.freeze(snapshot);
}

// Store handles, cues, and marks in tables to avoid Lua's 200-local function limit.
function _compile(source, scene, depth) {
    const indent = "    ".repeat(depth);
    const output = [`${indent}local ${scene} = vis.scene ${source.config}`];
    if (source.handles) output.push(`${indent}local objects = {}`);
    if (source.audio) output.push(`${indent}local cues = {}`);
    if (source.timelines) output.push(`${indent}local times = {}`);
    for (const line of source.lines) {
        if (typeof line === "function") {
            output.push(`${indent}${line(scene)}`);
            continue;
        }
        output.push(`${indent}do`);
        if (line.kind === "viewport") {
            const child = `viewportScene${depth + 1}`;
            output.push(..._compile(line.snapshot, child, depth + 1));
            output.push(`${indent}    ${scene}:viewport(${child},${line.rect})`);
        } else {
            const stages = [];
            for (let index = 0; index < line.snapshots.length; index++) {
                const child = `transitionScene${depth + 1}_${index + 1}`;
                const snapshot = line.snapshots[index];
                stages.push(child);
                output.push(..._compile(snapshot, child, depth + 1));
            }
            output.push(`${indent}    ${scene}:scene_transition({${stages.join(",")}},${line.options})`);
        }
        output.push(`${indent}end`);
    }
    return output;
}

class Handle {
    constructor(owner, name, type = "object") {
        this.owner = owner;
        this.name = name;
        HANDLE_TYPES.set(this, type);
        Object.freeze(this);
    }

    _object(method, options) {
        return this.owner._object(method, options, this);
    }

    connector(from, to, options = {}) {
        return this.owner._connector(from, to, options, this);
    }

    moveTo(point) {
        this.owner._assertQuiet();
        const value = _lua(point);
        SCENE_STATES.get(this.owner).lines.push(() => `${this.name}:move_to(${value})`);
        return this;
    }

    nextTo(target, direction, gap = 0.25) {
        this.owner._assertQuiet();
        const targetName = this.owner._handle(target);
        const directionValue = _lua(direction);
        const gapValue = _lua(gap);
        SCENE_STATES.get(this.owner).lines.push(
            () => `${this.name}:next_to(${targetName},${directionValue},${gapValue})`,
        );
        return this;
    }

    alignTo(target, direction) {
        this.owner._assertQuiet();
        const targetName = this.owner._handle(target);
        const directionValue = _lua(direction);
        SCENE_STATES.get(this.owner).lines.push(() => `${this.name}:align_to(${targetName},${directionValue})`);
        return this;
    }

    mask(target, mode = "alpha") {
        this.owner._assertQuiet();
        const targetName = target === null ? "nil" : this.owner._handle(target);
        const modeValue = _lua(_maskMode(mode));
        SCENE_STATES.get(this.owner).lines.push(() => `${this.name}:mask(${targetName},${modeValue})`);
        return this;
    }

    // The handles created below this one, in order.
    children() {
        return [...(SCENE_STATES.get(this.owner).children.get(this) ?? [])];
    }
}

export class AudioCueHandle {
    constructor(owner, name) {
        this.owner = owner;
        this.name = name;
        Object.freeze(this);
    }

    gain(value, begin, duration = 1, curve = "smooth") {
        if (typeof duration === "object") {
            if (arguments.length > 3) throw new TypeError("Timing options cannot be combined with positional arguments");
            ({duration, curve} = _timing(duration));
        }
        this.owner._assertQuiet();
        if (!Number.isFinite(value) || value < 0 || value > 4) {
            throw new RangeError("Audio gain must be finite and between 0 and 4");
        }
        if (!Number.isFinite(begin) || begin < 0) {
            throw new RangeError("Audio gain begin must be finite and non-negative");
        }
        if (!Number.isFinite(duration) || duration <= 0) {
            throw new RangeError("Audio gain duration must be finite and positive");
        }
        const values = [value, begin, duration, curve].map(_lua).join(",");
        // A cue exists only where the scene has the audio module; see sound().
        SCENE_STATES.get(this.owner).lines.push(() => `if ${this.name} then ${this.name}:gain(${values}) end`);
        return this;
    }
}

// A view from scene.view: its camera moves and the whole view fades with the calls the scene's
// camera takes, which check their arguments as the scene's do.
export class ViewHandle {
    constructor(owner, name) {
        this.owner = owner;
        this.name = name;
        Object.freeze(this);
    }

    look(camera, duration = 1, easing = "smooth") {
        if (typeof duration === "object" && arguments.length > 2) throw new TypeError("Timing options cannot be combined with positional arguments");
        this.owner._look(this.name, camera, duration, easing);
        return this;
    }

    orbit(radians, options = {}) {
        this.owner._orbit(this.name, radians, options);
        return this;
    }

    // Frames the targets in the view's rectangle, or returns to the camera the view opened with.
    focus(target = null, options = {}) {
        this.owner._focus(this.name, target, options);
        return this;
    }

    // Fades the whole view; the fades of a view follow one another.
    fade(opacity, duration = 1, easing = "smooth") {
        if (typeof duration === "object") {
            if (arguments.length > 2) throw new TypeError("Timing options cannot be combined with positional arguments");
            ({duration, curve: easing} = _timing(duration));
        }
        this.owner._assertMutable();
        if (!Number.isFinite(opacity) || opacity < 0 || opacity > 1) {
            throw new RangeError("View opacity must be between 0 and 1");
        }
        if (!Number.isFinite(duration) || duration <= 0) {
            throw new RangeError("View fade duration must be a positive number");
        }
        _curveValue(easing);
        const values = [opacity, duration, easing].map(_lua).join(",");
        SCENE_STATES.get(this.owner).lines.push(() => `${this.name}:fade(${values})`);
        return this;
    }
}

class GroupHandle extends Handle {
    arrange(direction = [1, 0, 0], gap = 0.25) {
        this.owner._assertQuiet();
        const directionValue = _lua(direction);
        const gapValue = _lua(gap);
        SCENE_STATES.get(this.owner).lines.push(() => `${this.name}:arrange(${directionValue},${gapValue})`);
        return this;
    }

    arrangeGrid(columns, columnGap = 0.25, rowGap = 0.25) {
        this.owner._assertQuiet();
        const columnsValue = _lua(columns);
        const columnGapValue = _lua(columnGap);
        const rowGapValue = _lua(rowGap);
        SCENE_STATES.get(this.owner).lines.push(
            () => `${this.name}:arrange_grid(${columnsValue},${columnGapValue},${rowGapValue})`,
        );
        return this;
    }

    // The rows of a group are the groups directly below it, in order, such as the lines of a
    // code block; whatever else it holds lies between them uncounted.
    _rows() {
        const rows = this.children().filter((child) => child instanceof GroupHandle);
        if (!rows.length) throw new RangeError("The group has no rows; its rows are the groups directly below it");
        return rows;
    }

    // The rowth group directly below this one, counting from 1.
    row(number) {
        _ordinal(number, "Row");
        const rows = this._rows();
        if (number > rows.length) throw new RangeError(`Row ${number} is past the last of the group's ${rows.length} rows`);
        return rows[number - 1];
    }

    // The columnth handle directly below row number of this group.
    entry(number, column) {
        _ordinal(number, "Entry row");
        _ordinal(column, "Entry column");
        const row = this.row(number);
        const entries = row.children();
        if (column > entries.length) {
            throw new RangeError(`Entry ${column} is past the last of the ${entries.length} objects in row ${number}`);
        }
        return entries[column - 1];
    }

    // The columnth handle directly below each row of this group that has one, in order.
    column(number) {
        _ordinal(number, "Column");
        const entries = this._rows().map((row) => row.children()[number - 1]).filter((entry) => entry !== undefined);
        if (!entries.length) throw new RangeError(`No row of the group has an object ${number}`);
        return entries;
    }

    // The tokens of this code block that the matches of find, a string or a RegExp, cover any
    // part of, in order. A match that covers part of a token takes the whole token, the
    // smallest piece a block draws.
    search(find, options = {}) {
        const code = CODE_BLOCKS.get(this);
        if (!code) throw new TypeError("Search needs a block that code made");
        if (!(typeof find === "string" && find.length) && !(find instanceof RegExp)) {
            throw new TypeError("Search needs a non-empty string or a RegExp to find");
        }
        if (!options || (Object.getPrototypeOf(options) !== Object.prototype && Object.getPrototypeOf(options) !== null)) {
            throw new TypeError("Search options must be a plain object");
        }
        const unknown = Object.keys(options).find((key) => !SEARCH_OPTIONS.includes(key));
        if (unknown) throw new TypeError(`Unknown Search option: ${unknown}`);
        const query = _codeQuery(options, "");
        const spans = _codeSpans(code.text, code.language);
        const covered = new Set();
        for (const [begin, end] of _codeFind(code.text, find, query, query.whole ? spans : null, "")) {
            for (let i = _codeSpanAfter(spans, begin); i < spans.length && spans[i].begin < end; i++) covered.add(i);
        }
        if (!covered.size) {
            throw new RangeError(`${typeof find === "string" ? `'${find}'` : String(find)} matches only blanks, which no token holds`);
        }
        const rows = this._rows();
        return [...covered].map((i) => rows[spans[i].row].children()[spans[i].index]);
    }
}

function _ordinal(value, name) {
    if (!Number.isInteger(value) || value < 1) throw new RangeError(`${name} takes a whole number from 1`);
}

// A formula the tex module sets, or one of its parts or pieces.
class TexHandle extends GroupHandle {
    // The nth part whose TeX reads tex, blanks aside, or else the nth glyph or shape whose TeX
    // does, such as "c" or "\\frac" for a bar; on a part, the nth of its own glyphs and shapes.
    part(tex, nth = 1) {
        this.owner._assertQuiet();
        if (typeof tex !== "string") throw new TypeError("Tex part requires the TeX of what it finds as a string");
        _ordinal(nth, "Tex part");
        const state = SCENE_STATES.get(this.owner);
        const name = `objects[${++state.handles}]`;
        const value = _lua(tex);
        state.lines.push(() => `${name} = ${this.name}:part(${value},${nth})`);
        return new TexHandle(this.owner, name, "tex");
    }
}

function _sampleField(handle, callback, options, spatial) {
    handle.owner._assertQuiet();
    const name = spatial ? "voxel" : "cell";
    if (typeof callback !== "function") throw new TypeError(`${name} callback must be a function`);
    if (!options || (Object.getPrototypeOf(options) !== Object.prototype && Object.getPrototypeOf(options) !== null)) {
        throw new TypeError(`${name} options must be a plain object`);
    }
    const unknown = Object.keys(options).filter(
        (key) => key !== "mode" && key !== "padding" && key !== "duration" && key !== "fps",
    );
    if (unknown.length) throw new TypeError(`Unknown ${name} option: ${unknown[0]}`);
    const mode = options.mode ?? "padd";
    const padding = options.padding ?? 0.05;
    const durationValue = options.duration ?? 0;
    const fps = options.fps ?? 30;
    if (mode !== "full" && mode !== "padd") {
        throw new TypeError(`${name} mode must be full or padd`);
    }
    if (!Number.isFinite(padding) || padding < 0 || padding >= 0.5) {
        throw new RangeError(`${name} padding must be finite and between 0 and 0.5`);
    }
    if (!Number.isFinite(durationValue) || durationValue < 0) {
        throw new RangeError(`${name} duration must be a finite non-negative number`);
    }
    const duration = Math.fround(durationValue);
    if (!Number.isFinite(duration)) {
        throw new RangeError(`${name} duration exceeds the native number range`);
    }
    if (!Number.isInteger(fps) || fps <= 0 || fps > 0xffffffff) {
        throw new RangeError(`${name} fps must be a positive 32-bit integer`);
    }
    const dimensions = spatial ? 3 : 2;
    const axes = SPACE_RANGES.get(handle).slice(0, dimensions).map((range) => _sampleAxis(range, name));
    const slice = axes[0].count * axes[1].count;
    const depth = spatial ? axes[2].count : 1;
    if (slice > 16384 || slice * depth > 65536) {
        throw new RangeError(`${name} exceeds the 16384-cell slice or 65536-voxel limit`);
    }
    const intervals = duration === 0 ? 0 : Math.ceil(duration * fps);
    if (intervals > 4095) {
        throw new RangeError(`${name} exceeds the 4096-frame limit`);
    }
    const frames = intervals + 1;
    if (slice * depth * frames > 262144) {
        throw new RangeError(`${name} exceeds the 262144-color sample limit`);
    }
    const state = SCENE_STATES.get(handle.owner);
    const colors = [];
    state.sampleActive = true;
    try {
        for (let frame = 0; frame < frames; frame++) {
            const time = frames > 1 ? Math.fround(duration * frame / (frames - 1)) : 0;
            for (let iz = 0; iz < depth; iz++) {
                const z = spatial ? Math.fround(axes[2].start + Math.fround(iz * axes[2].step)) : 0;
                for (let iy = 0; iy < axes[1].count; iy++) {
                    const y = Math.fround(axes[1].start + Math.fround(iy * axes[1].step));
                    for (let ix = 0; ix < axes[0].count; ix++) {
                        const x = Math.fround(axes[0].start + Math.fround(ix * axes[0].step));
                        colors.push(_sampleColor(
                            spatial ? callback(x, y, z, time) : callback(x, y, time),
                            name,
                        ));
                    }
                }
            }
        }
    } finally {
        state.sampleActive = false;
    }
    const object = `objects[${++state.handles}]`;
    const settings = `{mode=${_string(mode)},padding=${_lua(padding)},duration=${_lua(duration)},fps=${fps}}`;
    const values = _lua(colors);
    const parameters = spatial ? "x,y,z,time" : "x,y,time";
    // The replay's colors and counter stay inside a block, where the callback still sees them.
    state.lines.push(() =>
        `do local colors=${values};local index=0;${object}=${handle.name}:${name}(function(${parameters}) index=index+1;return colors[index] end,${settings}) end`,
    );
    return handle.owner._track(new GroupHandle(handle.owner, object, "group"), handle);
}

class SpaceHandle extends Handle {
    constructor(owner, name, type, ranges) {
        super(owner, name, type);
        SPACE_RANGES.set(this, ranges);
    }

    cell(value = {}, options = {}) {
        if (typeof value === "function") return _sampleField(this, value, options, false);
        return this._object("cell", value);
    }

    voxel(callback, options = {}) {
        return _sampleField(this, callback, options, true);
    }

    // Labels beside the ticks of this space, as Lua space:labels places them: x, y and z list
    // [value, label] pairs, each label a string or a handle of the scene. Returns their handles.
    // It makes objects and lays out handles, so the tracks that wait keep waiting, as in Lua.
    labels(options) {
        this.owner._assertQuiet();
        return this.owner._placeLabels(this, this.owner._labelsTable(options));
    }
}

for (const method of FACTORIES) {
    Handle.prototype[method] = function (options = {}) {
        return this._object(method, options);
    };
}

Handle.prototype.label = function (options = {}) {
    return this._object("label", options);
};

Handle.prototype.spans = function (parts, options = {}) {
    return this.owner._spans(parts, options, this);
};

Handle.prototype.tex = function (options) {
    return this.owner._tex(options, this);
};

Handle.prototype.brace = function (options) {
    return this.owner._brace(options, this);
};

Handle.prototype.code = function (options) {
    return this.owner._code(options, this);
};

Handle.prototype.area = function (options) {
    return this.owner._area(options, this);
};

Handle.prototype.riemann = function (options) {
    return this.owner._riemann(options, this);
};

Handle.prototype.vectorField = function (options) {
    return this.owner._vectorField(options, this);
};
Handle.prototype.contour = function (options) {
    return this.owner._contour(options, this);
};

Handle.prototype.matrix = function (options) {
    return this.owner._matrix(options, this);
};

Handle.prototype.table = function (options) {
    return this.owner._table(options, this);
};

// The children spans makes of its parts, in order: one per part object, a text or a formula,
// and the texts each string splits into. Each entry holds the id of its part, if any, and the
// TeX of a formula part.
function _spanTexts(parts, split) {
    const texts = [];
    for (const part of parts) {
        if (typeof part !== "string") {
            texts.push({ id: part.id, tex: part.tex });
            continue;
        }
        let inside = false;
        for (const character of part) {
            if (character === "\n" || (split !== "none" && (character === " " || character === "\t"))) {
                inside = false;
            } else if (split === "letters" || !inside) {
                texts.push({});
                inside = true;
            }
        }
    }
    return texts;
}

// Checks the TeX of a formula: a string, or a non-empty array of strings and part objects that
// hold their TeX as text.
function _texSource(tex, what) {
    const plain = (value) => value && (Object.getPrototypeOf(value) === Object.prototype
        || Object.getPrototypeOf(value) === null);
    if (typeof tex !== "string" && !(Array.isArray(tex) && tex.length)) {
        throw new TypeError(`${what} requires tex: a string, or a non-empty array of strings and part objects`);
    }
    if (!Array.isArray(tex)) return;
    for (const part of tex) {
        if (typeof part !== "string" && (!plain(part) || typeof part.text !== "string")) {
            throw new TypeError("A tex part must be a string or a plain object with a text string");
        }
    }
}

const CODE_LANGUAGES = new Set(["plain", "lua", "python", "javascript", "typescript", "c", "cpp"]);
const MATRIX_BRACKETS = new Set(["square", "round", "bar", "none"]);
const TABLE_RULES = new Set(["header", "rows", "columns", "grid", "none"]);

// Checks the rows of a matrix or table: rows of numbers and strings, all as long, at most 1024
// entries.
function _gridRows(rows, name) {
    if (!Array.isArray(rows) || !rows.length) {
        throw new TypeError(`${name} rows must be an array of rows, each an array of entries`);
    }
    rows.forEach((row, r) => {
        if (!Array.isArray(row) || !row.length) {
            throw new TypeError(`${name} row ${r + 1} must be an array of at least one entry`);
        }
        if (row.length !== rows[0].length) {
            throw new TypeError(`${name} rows must all hold as many entries as the first, ${rows[0].length}; row ${r + 1} holds ${row.length}`);
        }
        for (const entry of row) {
            if (typeof entry === "string") continue;
            if (typeof entry !== "number") throw new TypeError(`A ${name.toLowerCase()} entry must be a number or a string`);
            if (!Number.isFinite(Math.fround(entry))) throw new RangeError(`${name} entries must be finite numbers`);
        }
    });
    if (rows.length * rows[0].length > 1024) throw new RangeError(`A ${name.toLowerCase()} holds at most 1024 entries`);
    return rows;
}

// The comments, strings and names of a language, as the native tokenizer reads them.
function _codeGrammar(language) {
    const curly = language !== "plain" && language !== "lua" && language !== "python";
    return {
        lineComment: language === "lua" ? "--" : language === "python" ? "#" : curly ? "//" : null,
        blockOpen: curly ? "/*" : null,
        blockClose: curly ? "*/" : null,
        quotes: language !== "plain",
        longBrackets: language === "lua",
        tripleQuotes: language === "python",
        backticks: language === "javascript" || language === "typescript",
        directives: language === "c" || language === "cpp",
        dollar: language === "javascript" || language === "typescript",
    };
}

const _codeBlank = (c) => c === " " || c === "\t" || c === "\r";
const _codeDigit = (c) => c >= "0" && c <= "9";
const _codeAlpha = (c) => (c >= "a" && c <= "z") || (c >= "A" && c <= "Z");
const _codeLetter = (c) => _codeAlpha(c) || c === "_" || c.charCodeAt(0) >= 0x80;
const _codeSeparator = (c) => "()[]{},;".includes(c);

// How many tokens one line splits into; state carries block comments and long strings to the
// next line, and ranges, when given, gets the [begin, end] of each token. Every rule mirrors
// codeTokens in src/core/lua/visCode.cpp.
function _codeLineTokens(grammar, line, state, ranges = null) {
    const size = line.length;
    let count = 0;
    const starts = (at, prefix) => prefix !== null && line.startsWith(prefix, at);
    const emit = (begin, end) => {
        while (end > begin && _codeBlank(line[end - 1])) end--;
        if (end <= begin) return;
        count++;
        ranges?.push([begin, end]);
    };
    const carry = (begin, body, close) => {
        const end = line.indexOf(close, body);
        if (end < 0) {
            emit(begin, size);
            state.close = close;
            return size;
        }
        emit(begin, end + close.length);
        state.close = null;
        return end + close.length;
    };
    const longBracket = (at) => {
        if (line[at] !== "[") return -1;
        let i = at + 1;
        while (line[i] === "=") i++;
        const level = i - at - 1;
        return line[i] === "[" && level <= 6 ? level : -1;
    };
    let i = 0;
    if (state.close !== null) {
        while (i < size && _codeBlank(line[i])) i++;
        i = carry(i, i, state.close);
    }
    let opening = i === 0;
    while (i < size) {
        const c = line[i];
        if (_codeBlank(c)) {
            i++;
            continue;
        }
        const first = opening;
        opening = false;
        if (grammar.longBrackets && starts(i, "--")) {
            const level = longBracket(i + 2);
            if (level >= 0) {
                i = carry(i, i + 4 + level, `]${"=".repeat(level)}]`);
                continue;
            }
            emit(i, size);
            break;
        }
        if (starts(i, grammar.lineComment)) {
            emit(i, size);
            break;
        }
        if (starts(i, grammar.blockOpen)) {
            i = carry(i, i + 2, grammar.blockClose);
            continue;
        }
        if (grammar.longBrackets && c === "[") {
            const level = longBracket(i);
            if (level >= 0) {
                i = carry(i, i + 2 + level, `]${"=".repeat(level)}]`);
                continue;
            }
        }
        if (grammar.directives && first && c === "#") {
            let j = i + 1;
            while (j < size && _codeBlank(line[j])) j++;
            const word = j;
            while (j < size && _codeLetter(line[j])) j++;
            emit(i, j);
            const include = line.slice(word, j) === "include";
            i = j;
            while (i < size && _codeBlank(line[i])) i++;
            if (include && line[i] === "<") {
                const end = line.indexOf(">", i + 1);
                const stop = end < 0 ? size : end + 1;
                emit(i, stop);
                i = stop;
            }
            continue;
        }
        let quote = i;
        if (grammar.tripleQuotes && _codeAlpha(c)) {
            let j = i;
            while (j < size && j - i < 2 && "rRbBfFuU".includes(line[j])) j++;
            if (j > i && (line[j] === '"' || line[j] === "'")) quote = j;
        }
        const q = line[quote];
        if ((grammar.quotes && (q === '"' || q === "'")) || (grammar.backticks && q === "`")) {
            if (grammar.tripleQuotes && line[quote + 1] === q && line[quote + 2] === q) {
                i = carry(i, quote + 3, q.repeat(3));
                continue;
            }
            let j = quote + 1;
            let closed = false;
            while (j < size) {
                if (line[j] === "\\") {
                    j += 2;
                    continue;
                }
                if (line[j++] === q) {
                    closed = true;
                    break;
                }
            }
            if (j > size) j = size;
            if (!closed && q === "`") {
                i = carry(i, size, "`");
                continue;
            }
            emit(i, j);
            i = j;
            continue;
        }
        if (_codeDigit(c) || (c === "." && i + 1 < size && _codeDigit(line[i + 1]))) {
            const hex = c === "0" && (line[i + 1] === "x" || line[i + 1] === "X");
            let j = i + 1;
            while (j < size) {
                const d = line[j];
                if (d === "." && line[j + 1] === ".") break;
                if (_codeDigit(d) || _codeAlpha(d) || d === "_" || d === ".") {
                    j++;
                    continue;
                }
                const e = line[j - 1];
                if ((d === "+" || d === "-") && (hex ? e === "p" || e === "P" : e === "e" || e === "E")) {
                    j++;
                    continue;
                }
                break;
            }
            emit(i, j);
            i = j;
            continue;
        }
        const name = (d) => _codeLetter(d) || (grammar.dollar && d === "$");
        if (name(c)) {
            let j = i + 1;
            while (j < size && (name(line[j]) || _codeDigit(line[j]))) j++;
            emit(i, j);
            i = j;
            continue;
        }
        if (_codeSeparator(c)) {
            emit(i, i + 1);
            i++;
            continue;
        }
        let j = i + 1;
        while (j < size && !_codeBlank(line[j]) && !name(line[j]) && !_codeDigit(line[j])
               && !_codeSeparator(line[j]) && line[j] !== '"' && line[j] !== "'" && line[j] !== "`"
               && !starts(j, grammar.lineComment) && !starts(j, grammar.blockOpen)
               && !(grammar.longBrackets && starts(j, "--"))) {
            j++;
        }
        emit(i, j);
        i = j;
    }
    return count;
}

// How many tokens each line of a code block holds, blank lines too, as the native code factory
// splits them, so that the builder names every line and token before the scene runs.
function _codeTokenCounts(text, language) {
    const grammar = _codeGrammar(language);
    const source = text.endsWith("\n") ? text.slice(0, -1) : text;
    const state = {close: null};
    return source.split("\n").map((line) => _codeLineTokens(grammar, line, state));
}

// The tokens of a code block's text as the native code factory splits them, in order: where
// each begins and ends in the text, and its line and its place in that line, counting from 0.
function _codeSpans(text, language) {
    const grammar = _codeGrammar(language);
    const source = text.endsWith("\n") ? text.slice(0, -1) : text;
    const state = {close: null};
    const spans = [];
    let offset = 0;
    source.split("\n").forEach((line, row) => {
        const ranges = [];
        _codeLineTokens(grammar, line, state, ranges);
        ranges.forEach(([begin, end], index) => spans.push({begin: offset + begin, end: offset + end, row, index}));
        offset += line.length + 1;
    });
    return spans;
}

// The first of the spans that ends after at, or their count.
function _codeSpanAfter(spans, at) {
    let low = 0;
    let high = spans.length;
    while (low < high) {
        const middle = (low + high) >> 1;
        if (spans[middle].end <= at) low = middle + 1;
        else high = middle;
    }
    return low;
}

// The lines of text as code counts them: a line feed at the very end opens none.
function _codeLineCount(text) {
    return (text.endsWith("\n") ? text.slice(0, -1) : text).split("\n").length;
}

// Where line number of text starts, counting from 1; past the last line, the end of the text.
function _codeLineStart(text, number) {
    let at = 0;
    for (let line = 1; line < number; line++) {
        const end = text.indexOf("\n", at);
        if (end < 0) return text.length;
        at = end + 1;
    }
    return at;
}

// A line number or [first, last], counting from 1, as [first, last]; message explains a wrong
// one.
function _codeLines(value, message) {
    const range = typeof value === "number" ? [value, value] : value;
    if (!Array.isArray(range) || range.length !== 2 || !range.every((line) => Number.isInteger(line) && line >= 1)
        || range[0] > range[1]) {
        throw new RangeError(message);
    }
    return range;
}

// Which matches a search or an edit takes and where it looks, as Lua reads them: pick is the
// match to take from 1, 0 for all and Infinity for the last; first and last bound the lines to
// look in, 0 for all; whole passes over matches that cut a token.
function _codeQuery(options, prefix) {
    const query = {pick: 1, first: 0, last: 0, whole: false};
    const {match} = options;
    if (match === "last") query.pick = Infinity;
    else if (match === "all") query.pick = 0;
    else if (Number.isInteger(match) && match >= 1) query.pick = match;
    else if (match !== undefined && match !== "first") {
        throw new TypeError(`${prefix}Match must be "first", "last", "all" or a match number from 1`);
    }
    if (options.lines !== undefined) {
        [query.first, query.last] = _codeLines(options.lines,
            `${prefix}Lines takes a line number or [first, last], counting from 1`);
    }
    if (options.whole !== undefined && typeof options.whole !== "boolean") {
        throw new TypeError(`${prefix}Whole must be a boolean`);
    }
    query.whole = options.whole === true;
    return query;
}

// The matches of find, a string or a RegExp, in text that query takes, as [begin, end] pairs
// in order. Matches do not overlap and hold a character at least; with spans given, each is
// one that cuts none of them. A RegExp reads only the lines query looks in, so ^ and $ with
// the m flag match at their ends.
function _codeFind(text, find, query, spans, prefix) {
    let begin = 0;
    let end = text.length;
    if (query.first) {
        const lines = _codeLineCount(text);
        if (query.last > lines) {
            throw new RangeError(`${prefix}Line ${query.last} is past the last of the code's ${lines} lines`);
        }
        begin = _codeLineStart(text, query.first);
        end = _codeLineStart(text, query.last + 1);
    }
    const region = text.slice(begin, end);
    const pattern = typeof find === "string" ? null
        : new RegExp(find, find.flags.includes("g") ? find.flags : `${find.flags}g`);
    const edge = (at) => {
        const next = _codeSpanAfter(spans, at);
        return next === spans.length || spans[next].begin >= at;
    };
    const matches = [];
    for (let at = 0; at <= region.length;) {
        let found;
        let stop;
        if (pattern) {
            pattern.lastIndex = at;
            const result = pattern.exec(region);
            if (!result) break;
            found = result.index;
            stop = found + result[0].length;
        } else {
            found = region.indexOf(find, at);
            if (found < 0) break;
            stop = found + find.length;
        }
        if (stop === found || (spans && (!edge(begin + found) || !edge(begin + stop)))) {
            at = found + 1;
            continue;
        }
        matches.push([begin + found, begin + stop]);
        at = stop;
        if (matches.length === query.pick) break;
    }
    const name = typeof find === "string" ? `'${find}'` : String(find);
    const whole = spans ? " made of whole tokens" : "";
    if (!matches.length) {
        if (!query.first) throw new RangeError(`${prefix}The code has no ${name}${whole}`);
        if (query.first === query.last) {
            throw new RangeError(`${prefix}Line ${query.first} of the code has no ${name}${whole}`);
        }
        throw new RangeError(`${prefix}Lines ${query.first} to ${query.last} of the code have no ${name}${whole}`);
    }
    if (query.pick === 0) return matches;
    if (matches.length < query.pick && query.pick !== Infinity) {
        throw new RangeError(`${prefix}Match ${query.pick} is past the last of the ${matches.length} matches of ${name}`);
    }
    return matches.slice(-1);
}

// Puts value in text in front of line number, counting from 1, or after the last line when
// number is one past it, as whole lines: a line feed at the end of value opens none.
function _codeInsertLines(text, number, value) {
    const lines = value.endsWith("\n") ? value.slice(0, -1) : value;
    if (number <= _codeLineCount(text)) {
        const at = _codeLineStart(text, number);
        return `${text.slice(0, at)}${lines}\n${text.slice(at)}`;
    }
    return text.endsWith("\n") ? `${text}${lines}\n` : `${text}\n${lines}`;
}

// Replaces lines first to last of text, counting from 1, by value as whole lines, or removes
// them with their line feeds when value is null.
function _codeReplaceLines(text, first, last, value) {
    const lines = _codeLineCount(text);
    const body = text.endsWith("\n") ? text.length - 1 : text.length;
    let at;
    let resume;
    if (value !== null) {
        if (value.endsWith("\n")) value = value.slice(0, -1);
        at = _codeLineStart(text, first);
        resume = last < lines ? _codeLineStart(text, last + 1) - 1 : body;
    } else if (last < lines) {
        at = _codeLineStart(text, first);
        resume = _codeLineStart(text, last + 1);
    } else {
        // The last line goes with the line feed before it, which ends the line above.
        at = first > 1 ? _codeLineStart(text, first) - 1 : 0;
        resume = first > 1 ? body : text.length;
    }
    return `${text.slice(0, at)}${value ?? ""}${text.slice(resume)}`;
}

// The kinds of edit, each named by a key of its edit object, and the keys each kind takes.
const CODE_EDITS = Object.freeze({
    replace: Object.freeze(["replace", "with", "match", "lines", "whole"]),
    insert: Object.freeze(["insert", "before", "after", "match", "lines", "whole"]),
    remove: Object.freeze(["remove", "match", "lines", "whole"]),
    append: Object.freeze(["append"]),
    prepend: Object.freeze(["prepend"]),
});

// The text that one edit makes of text, as Lua edit makes it, with a RegExp allowed wherever
// it finds a text; prefix starts its messages.
function _codeEdit(text, edit, language, prefix) {
    if (!edit || (Object.getPrototypeOf(edit) !== Object.prototype && Object.getPrototypeOf(edit) !== null)) {
        throw new TypeError(`${prefix}An edit must be a plain object`);
    }
    const kinds = Object.keys(CODE_EDITS).filter((kind) => Object.hasOwn(edit, kind));
    if (kinds.length !== 1) {
        throw new TypeError(`${prefix}An edit takes one of replace, insert, remove, append and prepend`);
    }
    const [kind] = kinds;
    const unknown = Object.keys(edit).find((key) => !CODE_EDITS[kind].includes(key));
    if (unknown) throw new TypeError(`${prefix}Unknown ${kind} option: ${unknown}`);
    const name = `${kind[0].toUpperCase()}${kind.slice(1)}`;
    // The text the edit puts in: with for replace, the value of insert, append and prepend.
    const value = kind === "replace" ? edit.with : edit[kind];
    if (kind !== "remove" && typeof value !== "string") {
        throw new TypeError(kind === "replace" ? `${prefix}Replace needs with, the text to put in its place`
            : `${prefix}${name} takes a string`);
    }
    const lines = _codeLineCount(text);
    if (kind === "append") return _codeInsertLines(text, lines + 1, value);
    if (kind === "prepend") return _codeInsertLines(text, 1, value);

    // The target: a text to find, or for insert a line number and for replace and remove lines.
    const after = kind === "insert" && Object.hasOwn(edit, "after");
    if (kind === "insert" && after === Object.hasOwn(edit, "before")) {
        throw new TypeError(`${prefix}Insert needs one of before and after`);
    }
    const find = kind === "insert" ? edit[after ? "after" : "before"] : edit[kind];
    if (typeof find !== "string" && !(find instanceof RegExp)) {
        if (["match", "lines", "whole"].some((key) => Object.hasOwn(edit, key))) {
            throw new TypeError(`${prefix}Match, lines and whole need a text to find, not line numbers`);
        }
        const message = kind === "insert" ? `${prefix}Before and after take a text to find or a line number`
            : `${prefix}${name} takes a text to find, a line number or [first, last]`;
        if (kind === "insert" && typeof find !== "number") throw new TypeError(message);
        const [first, last] = _codeLines(find, message);
        if (last > lines) throw new RangeError(`${prefix}Line ${last} is past the last of the code's ${lines} lines`);
        if (kind === "insert") return _codeInsertLines(text, after ? first + 1 : first, value);
        return _codeReplaceLines(text, first, last, kind === "remove" ? null : value);
    }
    if (find === "") throw new RangeError(`${prefix}The text to find cannot be empty`);
    const query = _codeQuery(edit, prefix);
    const matches = _codeFind(text, find, query, query.whole ? _codeSpans(text, language) : null, prefix);
    let output = "";
    let cursor = 0;
    for (const [found, end] of matches) {
        // insert after keeps the match before the text and insert before after it.
        output += text.slice(cursor, after ? end : found);
        if (kind !== "remove") output += value;
        if (kind === "insert" && !after) output += text.slice(found, end);
        cursor = end;
    }
    return output + text.slice(cursor);
}

function _makeHandle(owner, name, method, options) {
    if (method === "group") return new GroupHandle(owner, name, method);
    if (method === "space") return new SpaceHandle(owner, name, method, _spaceRanges(options));
    return new Handle(owner, name, method);
}

// A number that bound objects follow: scene.tracker makes one and scene.track moves it.
class Tracker {
    get() {
        const entry = TRACKERS.get(this);
        if (!entry) throw new TypeError("Expected a tracker from scene.tracker");
        return entry.value;
    }
}

// The state a binding's function gives for the values its trackers have reached, or for values.
function _boundState(binding, values = binding.trackers.map((tracker) => TRACKERS.get(tracker).value)) {
    const state = binding.fn(...values);
    if (!state || (Object.getPrototypeOf(state) !== Object.prototype && Object.getPrototypeOf(state) !== null)) {
        throw new TypeError("A bound function must return a state object");
    }
    const key = Object.keys(state).find(
        (name) => state[name] !== undefined && name !== "shift" && !TRACKER_KEYS.includes(name),
    );
    if (key !== undefined) throw new TypeError(`A bound state cannot set '${key}'`);
    if (state.shift !== undefined && state.transform !== undefined) {
        throw new TypeError("A bound state cannot contain both shift and transform");
    }
    return state;
}

// A bound state as a play descriptor. Its shift is the offset from where the object was when it
// was bound, so the descriptor moves by the difference from the shift the binding gave last.
function _boundDescriptor(binding, state) {
    const descriptor = { target: binding.target };
    for (const key of TRACKER_KEYS) {
        if (state[key] !== undefined) descriptor[key] = state[key];
    }
    if (state.shift !== undefined) {
        if (!Array.isArray(state.shift)) throw new TypeError("A bound shift must be an array of numbers");
        const shift = [state.shift[0] ?? 0, state.shift[1] ?? 0, state.shift[2] ?? 0];
        descriptor.shift = shift.map((value, axis) => value - binding.shift[axis]);
        binding.shift = shift;
    }
    return descriptor;
}

const DECIMAL_POWERS = [1e0, 1e1, 1e2, 1e3, 1e4, 1e5, 1e6, 1e7, 1e8, 1e9, 1e10, 1e11, 1e12, 1e13, 1e14];

// Rounds to the nearest integer and a half to the even one, as C's nearbyint() does.
function _nearest(value) {
    const floor = Math.floor(value);
    const rest = value - floor;
    return rest > 0.5 || (rest === 0.5 && floor % 2 !== 0) ? floor + 1 : floor;
}

// The shortest decimal that the single nearest value reads as, worked out as the scene works it
// out for the times it reckons its cursor and clips from: 0.3 for Math.fround(0.3), which lies
// above 0.3. Singles beyond the range of times keep their own value.
function _decimal(value) {
    const single = Math.fround(value);
    const magnitude = Math.abs(single);
    if (!(magnitude >= 1e-6 && magnitude < 1e7)) return single;
    const exponent = Math.floor(Math.log10(magnitude));
    for (let digits = 1; digits <= 9; digits++) {
        const places = digits - 1 - exponent;
        const decimal = places >= 0
            ? _nearest(magnitude * DECIMAL_POWERS[places]) / DECIMAL_POWERS[places]
            : _nearest(magnitude / DECIMAL_POWERS[-places]) * DECIMAL_POWERS[-places];
        if (Math.fround(decimal) === magnitude) return single < 0 ? -decimal : decimal;
    }
    return single;
}

// The cursor after a wait of duration seconds from time, in the single precision the scene
// keeps it in, added up from decimals as the scene adds them.
function _after(time, duration) {
    return Math.fround(_decimal(time) + _decimal(duration));
}

// Moves the clock of a scene as a wait of duration seconds moves its cursor.
function _advance(scene, duration) {
    const timing = SCENE_TRACKERS.get(scene);
    if (typeof duration !== "number" || !(duration >= 0)) {
        timing.clock = { anchor: {}, time: 0 };
    } else if (Math.fround(duration) > 0) {
        timing.clock = { anchor: timing.clock.anchor, time: _after(timing.clock.time, duration) };
    }
}

// Whether the span from start to finish shares more than a moment with window.
function _overlaps(start, finish, window) {
    return start < window.finish - TRACKER_EPSILON && window.start < finish - TRACKER_EPSILON;
}

// The step of a track whose time is within a moment of time, or null.
function _trackStepAt(track, time) {
    let low = 0;
    let high = track.steps;
    while (low < high) {
        const middle = (low + high) >> 1;
        if (track.bounds[middle] < time - TRACKER_EPSILON) low = middle + 1;
        else high = middle;
    }
    return Math.abs(track.bounds[low] - time) <= TRACKER_EPSILON ? low : null;
}

// The value a tracker has at time among the tracks of a merged binding: the value of the track
// that moves it then, at its own samples exactly; else the end of the last track that moved it
// before; else the start of the first that moves it after; else the value it had when the
// first of those tracks was authored.
function _valueAt(group, tracker, time) {
    let before = null;
    let after = null;
    for (const track of group.tracks) {
        for (const move of track.moves) {
            if (move.tracker !== tracker) continue;
            if (time >= track.start - TRACKER_EPSILON && time <= track.finish + TRACKER_EPSILON) {
                const step = _trackStepAt(track, time);
                if (step !== null) return move.values[step];
                const progress = Math.max(0, Math.min(1, (time - track.start) / (track.finish - track.start)));
                return move.from + (move.to - move.from) * track.ease(progress);
            }
            if (track.finish < time) {
                if (!before || track.finish > before.finish) before = { finish: track.finish, value: move.to };
            } else if (!after || track.start < after.start) {
                after = { start: track.start, value: move.from };
            }
        }
    }
    if (before) return before.value;
    if (after) return after.value;
    return group.tracks[0].snapshot.get(tracker);
}

// Plays one binding through the tracks that overlap on it, into lines: a segment between each
// pair of neighboring sample times of all of them, at the values every tracker has then. Each
// segment starts and ends at the absolute sample times the scene recorded.
function _playGroup(scene, group, lines) {
    const { binding, tracks } = group;
    tracks.sort((first, second) => first.order - second.order);
    const marks = [];
    for (const track of tracks) {
        for (let step = 0; step <= track.steps; step++) marks.push({ time: track.bounds[step], track, step });
    }
    marks.sort((first, second) => first.time - second.time || first.track.order - second.track.order);
    const kept = [marks[0]];
    for (const mark of marks) {
        if (mark.time > kept[kept.length - 1].time + TRACKER_EPSILON) kept.push(mark);
    }
    for (let i = 1; i < kept.length; i++) {
        const from = kept[i - 1];
        const to = kept[i];
        const state = _boundState(binding, binding.trackers.map((tracker) => _valueAt(group, tracker, to.time)));
        const [descriptor] = scene._descriptors([_boundDescriptor(binding, state)], "play");
        lines.push((name) => {
            const begin = `times[${from.track.first + from.step}]`;
            const duration = `times[${to.track.first + to.step}]-${begin}`;
            return `${name}:seek(${begin});${name}:play({${descriptor}},${duration},"linear",0)`;
        });
    }
}

// Plays the tracks of a scene that wait, in the order they were authored, as a Lua scene does at
// a call that adds or reads clips. Each track records its absolute frame times where it was
// authored, and every segment plays here between those times before the cursor returns.
function _flushTracks(scene) {
    const timing = SCENE_TRACKERS.get(scene);
    const list = timing.pending;
    if (!list.length) return;
    const shifts = new Map();
    for (const track of list) {
        for (const binding of track.affected) if (!shifts.has(binding)) shifts.set(binding, binding.shift);
    }
    const lines = [];
    try {
        const played = new Set();
        for (const track of list) {
            for (const binding of track.affected) {
                const group = track.groups.get(binding);
                if (group && !played.has(group)) {
                    played.add(group);
                    _playGroup(scene, group, lines);
                }
            }
            const steps = track.states.map((row) => {
                const batch = [];
                track.affected.forEach((binding, index) => {
                    if (row[index] && !track.groups.has(binding)) batch.push(_boundDescriptor(binding, row[index]));
                });
                return batch.length ? scene._descriptors(batch, "play") : null;
            });
            for (let step = 0; step < steps.length; step++) {
                const batch = steps[step];
                if (batch) {
                    lines.push((name) => {
                        const begin = `times[${track.first + step}]`;
                        const end = `times[${track.first + step + 1}]`;
                        return `${name}:seek(${begin});${name}:play({${batch.join(",")}},${end}-${begin},"linear",0)`;
                    });
                }
            }
        }
    } catch (error) {
        for (const [binding, shift] of shifts) binding.shift = shift;
        throw error;
    }
    timing.pending = [];
    timing.waiting = 0;
    for (const track of list) {
        for (const binding of track.affected) {
            binding.windows.push({ anchor: track.anchor, start: track.start, finish: track.finish });
        }
    }
    // The times the frames of each track start and end go into the scene's times table, from
    // times[track.first] on, beside the time the cursor returns to.
    const state = SCENE_STATES.get(scene);
    for (const track of list) {
        track.first = state.timelines + 1;
        state.timelines += track.steps + 1;
    }
    const resume = `times[${++state.timelines}]`;
    state.lines.push((name) => `${resume}=${name}:time()`, ...lines, (name) => `${name}:seek(${resume})`);
}

// Reserves one frame of a track from its start, without accumulating per-frame cursor error.
function _trackStep(name, track, step) {
    const open = step > 1 ? "" : `times[${track.first}]=${name}:time();`;
    const offset = _lua(track.duration * (step / track.steps));
    return `${open}${name}:seek(times[${track.first}]+${offset});times[${track.first + step}]=${name}:time()`;
}

export class SceneBuilder {
    constructor(config = {}) {
        SCENE_STATES.set(this, {
            config: _lua(config), lines: [], handles: 0, audio: 0, timelines: 0, views: 0,
            transition: false, sampleActive: false, ids: new Map(), children: new Map(), parents: new Map(),
        });
        // Tracks sample the trackers once a frame, at the rate the scene renders. The clock is
        // where the cursor is, as seconds after an anchor: the start of the scene, or a time only
        // the scene knows, after a call whose length it computes. Tracks merge only when their
        // clocks share an anchor.
        const origin = {};
        SCENE_TRACKERS.set(this, {
            fps: config.fps ?? 30, bindings: [], bound: new Set(), origin, clock: { anchor: origin, time: 0 },
            markers: new Map(), pending: [], waiting: 0, authored: 0,
        });
        Object.freeze(this);
    }

    // Checks that the scene can still be authored, for a call that only moves or reads the
    // authoring time, makes objects, binds or tracks, or lays out a handle.
    _assertQuiet() {
        if (SCENE_OWNERS.has(this)) throw new TypeError("Viewport child scene is sealed");
        if (SCENE_STATES.get(this).sampleActive) {
            throw new TypeError("Scene authoring is unavailable inside a sampling callback");
        }
    }

    // Checks that the scene can still be authored, for any other call: it adds or reads clips,
    // so it first plays the tracks that wait, as in a Lua scene, and unless the call is instant
    // and takes no time, leaves the cursor at a time only the scene knows.
    _assertMutable(timing) {
        this._assertQuiet();
        _flushTracks(this);
        if (timing !== "instant") SCENE_TRACKERS.get(this).clock = { anchor: {}, time: 0 };
    }

    _object(method, options, parent = null) {
        this._assertQuiet();
        if (["text", "label", "number"].includes(method) && options?.reserve !== undefined
            && typeof options.reserve !== "string") {
            throw new TypeError("Text reserve must be a string");
        }
        if (["text", "label", "number"].includes(method) && options?.tabular !== undefined
            && typeof options.tabular !== "boolean") {
            throw new TypeError("Text tabular must be a boolean");
        }
        if (method === "space" && options?.axis_width !== undefined
            && (typeof options.axis_width !== "number" || !Number.isFinite(options.axis_width) || options.axis_width < 0)) {
            throw new TypeError("Space axis_width must be zero or more pixels");
        }
        if (method === "surface") options = _surfaceOptions(this, options);
        // A space places the labels it is given once it is made, as space:labels does.
        let labels = null;
        if (method === "space" && options && typeof options === "object" && Object.hasOwn(options, "labels")) {
            const { labels: value, ...rest } = options;
            labels = this._labelsTable(value);
            options = rest;
        }
        const state = SCENE_STATES.get(this);
        const name = `objects[${++state.handles}]`;
        const value = method === "plot" ? this._plotOptions(options) : this._options(options);
        state.lines.push((scene) => `${name} = ${parent ? parent.name : scene}:${method} ${value}`);
        const handle = _makeHandle(this, name, method, options);
        if (["polygon", "route"].includes(method) && Array.isArray(options?.points)) {
            POINT_COUNTS.set(handle, options.points.length);
        }
        if (method === "plot") PLOT_CUTS.set(handle, _plotCut(options));
        this._track(handle, parent, options?.id);
        if (method === "angle" && options?.label !== undefined) {
            const label = options.label;
            const tex = typeof label === "object" && label?.tex !== undefined;
            const name = `${handle.name}:children()[1]`;
            const child = this._track(tex ? new TexHandle(this, name, "tex") : new Handle(this, name, "text"),
                handle, typeof label === "object" ? label.id : undefined);
            if (tex && Array.isArray(label.tex)) this._texParts(child, label.tex, label.id);
        }
        if (labels) this._placeLabels(handle, labels);
        return handle;
    }

    // The labels of a space as a Lua table, and the labels in the order the scene places them:
    // x, y and z, each in the order given. A label is a string, which becomes a text, or a handle,
    // which the script names.
    _labelsTable(options) {
        if (!options || (Object.getPrototypeOf(options) !== Object.prototype
            && Object.getPrototypeOf(options) !== null)) {
            throw new TypeError("Space labels must be a plain object of x, y and z lists of [value, label]");
        }
        const unknown = Object.keys(options).find((key) => !LABELS_OPTIONS.includes(key));
        if (unknown) throw new TypeError(`Unknown space labels option: ${unknown}`);
        if (options.placement !== undefined && options.placement !== "world" && options.placement !== "screen") {
            throw new TypeError("Space labels placement must be 'world' or 'screen'");
        }
        if (options.placement === "screen" && options.side !== undefined) {
            throw new TypeError("Screen labels do not take side");
        }
        const labels = [];
        const fields = Object.keys(options).sort().map((key) => {
            const value = options[key];
            if (key !== "x" && key !== "y" && key !== "z") return `[${_string(key)}]=${_lua(value)}`;
            if (!Array.isArray(value)) throw new TypeError(`Space labels ${key} must be an array of [value, label]`);
            const entries = value.map((entry, index) => {
                const label = Array.isArray(entry) ? entry[1] : undefined;
                if (!Array.isArray(entry) || entry.length !== 2 || !Number.isFinite(entry[0])
                    || (typeof label !== "string" && !(label instanceof Handle))) {
                    throw new TypeError(
                        `Space labels ${key} entry ${index + 1} must be [value, label], a finite number and a string or handle`,
                    );
                }
                labels.push({ axis: key, label });
                return `{${_lua(entry[0])},${typeof label === "string" ? _lua(label) : this._handle(label)}}`;
            });
            return `[${_string(key)}]={${entries.join(",")}}`;
        });
        return { table: `{${fields.join(",")}}`, labels };
    }

    // Places the labels of a table beside the ticks of a space and returns their handles: a new
    // text, below the space, for each string, and each handle given as it is.
    _placeLabels(space, { table, labels }) {
        const state = SCENE_STATES.get(this);
        const list = `objects[${++state.handles}]`;
        state.lines.push(() => `${list} = ${space.name}:labels ${table}`);
        const id = [...state.ids].find(([, handle]) => handle === space)?.[0];
        const numbers = { x: 0, y: 0, z: 0 };
        return labels.map(({ axis, label }, index) => {
            if (label instanceof Handle) return label;
            let named;
            if (id !== undefined) {
                do {
                    named = `${id}/label/${axis}/${++numbers[axis]}`;
                } while (state.ids.has(named));
            }
            return this._track(new Handle(this, `${list}[${index + 1}]`, "text"), space, named);
        });
    }

    // Materialize function plots as endpoint and uncut-column samples. Native playback blends
    // columns and recuts bands; non-finite explicit points split polylines.
    _plotOptions(options) {
        if (!options || (Object.getPrototypeOf(options) !== Object.prototype
            && Object.getPrototypeOf(options) !== null)) {
            throw new TypeError("Plot options must be a plain object");
        }
        const { fn, points, ...rest } = options;
        const fields = _styleOptions(rest);
        if (fields.endpoint_radius !== undefined
            && (typeof fields.endpoint_radius !== "number" || !Number.isFinite(fields.endpoint_radius)
                || fields.endpoint_radius < 0 || !Number.isFinite(Math.fround(fields.endpoint_radius)))) {
            throw new RangeError("Plot endpoint_radius must be a finite number of zero or more");
        }
        if (fields.y_range !== undefined) fields.y_range = _plotBand(fields.y_range);
        if (fields.discontinuities !== undefined
            && (!Array.isArray(fields.discontinuities) || !fields.discontinuities.every(Number.isFinite))) {
            throw new TypeError("Plot discontinuities must be an array of finite numbers");
        }
        let line;
        let stored = null;
        if (fn !== undefined) {
            if (typeof fn !== "function") throw new TypeError("Plot fn must be a function of x");
            if (points !== undefined) throw new TypeError("Plot accepts either points or fn, not both");
            if (fields.x_range === undefined) throw new TypeError("Plot fn requires x_range");
            const range = _spaceRange(fields.x_range, "Plot x_range");
            const grid = _plotGrid(range);
            const breaks = [...(fields.discontinuities ?? [])].sort((a, b) => a - b);
            const state = SCENE_STATES.get(this);
            let pieces;
            const samples = [];
            state.sampleActive = true;
            try {
                pieces = _plotPieces(fn, grid, range[2], fields.y_range ?? null, breaks, samples);
            } finally {
                state.sampleActive = false;
            }
            if (!pieces.length) {
                throw new RangeError(fields.y_range === undefined
                    ? "Plot has nothing to draw: fn needs two defined values in a row"
                    : "Plot has nothing to draw inside y_range");
            }
            fields.x_range = range;
            line = _plotLine(pieces);
            stored = _plotStored(pieces, samples);
        } else if (points !== undefined) {
            line = Array.isArray(points) ? `{${points.map(_plotPoint).join(",")}}` : _lua(points);
        } else {
            throw new TypeError("Plot requires points, or fn with x_range");
        }
        return `{${[...Object.keys(fields), stored ? "fn" : "points"]
            .sort()
            .map((key) => (key === "fn" ? `["fn"]=${stored}` : key === "points" ? `["points"]=${line}` : this._field(key, fields[key])))
            .join(",")}}`;
    }

    // Records a new handle under its parent and its id, for children(), find() and clone().
    _track(handle, parent, id) {
        const state = SCENE_STATES.get(this);
        const key = parent ?? this;
        if (!state.children.has(key)) state.children.set(key, []);
        state.children.get(key).push(handle);
        state.parents.set(handle, parent ?? null);
        if (typeof id === "string") state.ids.set(id, handle);
        return handle;
    }

    // A row of texts set side by side on shared baselines; its children are the texts, and the
    // formulas the tex module sets inline for parts that hold tex.
    _spans(parts, options, parent = null) {
        this._assertQuiet();
        if (!Array.isArray(parts) || !parts.length) {
            throw new TypeError("Spans require a non-empty array of strings and part objects");
        }
        const plain = (value) => value && (Object.getPrototypeOf(value) === Object.prototype
            || Object.getPrototypeOf(value) === null);
        if (!plain(options)) throw new TypeError("Spans options must be a plain object");
        const items = parts.map((part) => {
            if (typeof part === "string") return _lua(part);
            if (!plain(part)) throw new TypeError("A spans part must be a string or a plain object");
            if (!Object.hasOwn(part, "tex")) return this._options(part);
            if (Object.hasOwn(part, "text")) throw new TypeError("A spans part sets text or tex, not both");
            _texSource(part.tex, "A spans tex part");
            return this._texTable(part);
        });
        const styled = _styleOptions(options);
        const fields = Object.keys(styled).sort().map((key) => this._field(key, styled[key]));
        const value = `{${[...items, ...fields].join(",")}}`;
        const state = SCENE_STATES.get(this);
        const name = `objects[${++state.handles}]`;
        const texts = `objects[${++state.handles}]`;
        state.lines.push((scene) => `${name} = ${parent ? parent.name : scene}:spans ${value}`);
        state.lines.push(() => `${texts} = ${name}:children()`);
        const row = this._track(new GroupHandle(this, name, "group"), parent, options.id);
        // A child without an id of its own is named after the row and its place in it.
        _spanTexts(parts, options.split ?? "none").forEach(({ id, tex }, index) => {
            const named = id ?? (typeof options.id === "string" ? `${options.id}/${index + 1}` : undefined);
            const child = `${texts}[${index + 1}]`;
            if (tex === undefined) {
                this._track(new Handle(this, child, "text"), row, named);
                return;
            }
            const formula = this._track(new TexHandle(this, child, "tex"), row, named);
            if (Array.isArray(tex)) this._texParts(formula, tex, named);
        });
        return row;
    }

    spans(parts, options = {}) {
        return this._spans(parts, options);
    }

    // TeX math the tex module sets as a typeset group. With tex as an array of parts, the
    // group's children are the groups of the parts, named after a part's own id or the
    // formula's, as the module names them; part() finds glyphs and shapes by their TeX.
    _tex(options, parent = null) {
        this._assertQuiet();
        const plain = (value) => value && (Object.getPrototypeOf(value) === Object.prototype
            || Object.getPrototypeOf(value) === null);
        if (!plain(options)) throw new TypeError("Tex options must be a plain object");
        _texSource(options.tex, "Tex");
        const value = this._texTable(options);
        const state = SCENE_STATES.get(this);
        const name = `objects[${++state.handles}]`;
        state.lines.push((scene) => `${name} = ${parent ? parent.name : scene}:tex ${value}`);
        const formula = this._track(new TexHandle(this, name, "tex"), parent, options.id);
        if (Array.isArray(options.tex)) this._texParts(formula, options.tex, options.id);
        return formula;
    }

    // The options of a formula as a Lua table: its TeX as a string, or as a list of parts with
    // options of their own, beside its other options.
    _texTable(options) {
        const tex = options.tex;
        const list = Array.isArray(tex);
        const parts = list && tex.map((part) => (typeof part === "string" ? _lua(part) : this._options(part)));
        const fields = Object.keys(options).sort().map((key) => {
            if (key !== "tex") return this._field(key, options[key]);
            return `["tex"]=${list ? `{${parts.join(",")}}` : _lua(tex)}`;
        });
        return `{${fields.join(",")}}`;
    }

    // Names the groups of a formula set from parts as the tex module names them: after a part's
    // own id, or after the formula's id and the part's place.
    _texParts(formula, tex, id) {
        const state = SCENE_STATES.get(this);
        const groups = `objects[${++state.handles}]`;
        state.lines.push(() => `${groups} = ${formula.name}:children()`);
        tex.forEach((part, index) => {
            const own = typeof part === "string" ? undefined : part.id;
            const named = own ?? (typeof id === "string" ? `${id}/${index + 1}` : undefined);
            this._track(new TexHandle(this, `${groups}[${index + 1}]`, "tex"), formula, named);
        });
    }

    tex(options) {
        return this._tex(options);
    }

    // A curly brace beside the handles of around, or along the segment from from to to, which
    // the script draws as a filled path, and at its tip the label: a string the script sets as
    // a text named <id>/label, or a handle it moves there. Returns the brace and the label, as
    // the script returns them; the label is undefined when there is none.
    _brace(options, parent = null) {
        this._assertQuiet();
        if (!options || (Object.getPrototypeOf(options) !== Object.prototype && Object.getPrototypeOf(options) !== null)) {
            throw new TypeError("Brace options must be a plain object");
        }
        const label = options.label;
        if (label !== undefined && typeof label !== "string" && !(label instanceof Handle)) {
            throw new TypeError("Brace label must be a string or a handle");
        }
        const styled = _styleOptions(options);
        const fields = Object.keys(styled).sort().map((key) =>
            key === "label" && label instanceof Handle ? `["label"]=${this._handle(label)}` : this._field(key, styled[key]));
        const value = `{${fields.join(",")}}`;
        const state = SCENE_STATES.get(this);
        const name = `objects[${++state.handles}]`;
        const text = typeof label === "string" ? `objects[${++state.handles}]` : null;
        const targets = text ? `${name},${text}` : name;
        state.lines.push((scene) => `${targets} = ${parent ? parent.name : scene}:brace ${value}`);
        const brace = this._track(new Handle(this, name, "brace"), parent, options.id);
        if (!text) return [brace, label];
        const id = typeof options.id === "string" ? `${options.id}/label` : undefined;
        return [brace, this._track(new Handle(this, text, "text"), parent, id)];
    }

    brace(options) {
        return this._brace(options);
    }

    // Source code set line by line in colored tokens; its children are the lines, and theirs
    // the tokens.
    _code(options, parent = null) {
        this._assertQuiet();
        if (!options || (Object.getPrototypeOf(options) !== Object.prototype
            && Object.getPrototypeOf(options) !== null)) {
            throw new TypeError("Code options must be a plain object");
        }
        if (typeof options.text !== "string") throw new TypeError("Code requires a text string");
        const language = options.language ?? "plain";
        if (!CODE_LANGUAGES.has(language)) {
            throw new TypeError("Code language must be plain, lua, python, javascript, typescript, c, or cpp");
        }
        if (options.baseline !== undefined && typeof options.baseline !== "boolean") {
            throw new TypeError("Code baseline must be a boolean");
        }
        if (options.pitch !== undefined) {
            if (!Number.isFinite(options.pitch) || options.pitch <= 0) {
                throw new TypeError("Code pitch must be a positive distance in world units");
            }
            if (options.leading !== undefined) throw new TypeError("Code takes pitch or leading, not both");
        }
        const value = this._options(options);
        return this._codeBlock((scene) => `${parent ? parent.name : scene}:code ${value}`, parent, options.id,
            options.text, language);
    }

    // Names the block that the Lua expression make(scene) adds below parent, its lines and
    // their tokens, as the native factory splits text, and keeps the text for search and edit.
    _codeBlock(make, parent, blockId, text, language) {
        const counts = _codeTokenCounts(text, language);
        const state = SCENE_STATES.get(this);
        const name = `objects[${++state.handles}]`;
        const lines = `objects[${++state.handles}]`;
        const tokens = `objects[${++state.handles}]`;
        state.lines.push((scene) => `${name} = ${make(scene)}`);
        state.lines.push(() => `${lines} = ${name}:children()`);
        state.lines.push(() => `${tokens} = {} for i, line in ipairs(${lines}) do ${tokens}[i] = line:children() end`);
        const block = this._track(new GroupHandle(this, name, "group"), parent, blockId);
        // The block names its lines <id>/<line> and their tokens <id>/<line>/<token>.
        const id = typeof blockId === "string" ? blockId : null;
        CODE_BLOCKS.set(block, {text, language, id, parent});
        counts.forEach((count, row) => {
            const lineId = id === null ? undefined : `${id}/${row + 1}`;
            const line = this._track(new GroupHandle(this, `${lines}[${row + 1}]`, "group"), block, lineId);
            for (let index = 0; index < count; index++) {
                const tokenId = id === null ? undefined : `${lineId}/${index + 1}`;
                this._track(new Handle(this, `${tokens}[${row + 1}][${index + 1}]`, "text"), line, tokenId);
            }
        });
        return block;
    }

    code(options) {
        return this._code(options);
    }

    // The region between the graph of fn and base over x_range, as a polygon: the builder
    // calls the functions, as Lua area does at each step.
    _area(options, parent = null) {
        this._assertQuiet();
        if (!options || (Object.getPrototypeOf(options) !== Object.prototype
            && Object.getPrototypeOf(options) !== null)) {
            throw new TypeError("Area options must be a plain object");
        }
        const { fn, x_range: xRange, base, ...style } = options;
        if (typeof fn !== "function") throw new TypeError("Area requires fn, a function");
        if (xRange === undefined) throw new TypeError("Area requires x_range");
        if (base !== undefined && typeof base !== "function" && !Number.isFinite(base)) {
            throw new TypeError("Area base must be a number or a function");
        }
        const range = _spaceRange(xRange, "Area x_range");
        const steps = _rangeSteps(range, "Area x_range", 499999);
        const points = [];
        for (let i = 0; i <= steps; i++) {
            const x = _stepAt(range, i, steps);
            points.push([x, _sampleNumber(fn, x, "Area fn")]);
        }
        if (typeof base === "function") {
            for (let i = steps; i >= 0; i--) {
                const x = _stepAt(range, i, steps);
                points.push([x, _sampleNumber(base, x, "Area base")]);
            }
        } else {
            points.push([range[1], base ?? 0], [range[0], base ?? 0]);
        }
        return this._object("polygon", { ...style, points }, parent);
    }

    area(options) {
        return this._area(options);
    }

    // The rectangles of a Riemann sum, one per step of x_range, as a group whose children are
    // the rectangles: the builder calls fn where Lua riemann would and emits the heights.
    _riemann(options, parent = null) {
        this._assertQuiet();
        if (!options || (Object.getPrototypeOf(options) !== Object.prototype
            && Object.getPrototypeOf(options) !== null)) {
            throw new TypeError("Riemann options must be a plain object");
        }
        const { fn, values, sample, x_range: xRange, ...style } = options;
        if (xRange === undefined) throw new TypeError("Riemann requires x_range");
        const range = _spaceRange(xRange, "Riemann x_range");
        const steps = _rangeSteps(range, "Riemann x_range", 1024);
        if ((fn === undefined) === (values === undefined)) throw new TypeError("Riemann requires either fn or values");
        let heights = values;
        if (fn !== undefined) {
            if (typeof fn !== "function") throw new TypeError("Riemann fn must be a function");
            const share = { left: 0, right: 1, midpoint: 0.5 }[sample ?? "left"];
            if (share === undefined) throw new TypeError("Riemann sample must be left, right, or midpoint");
            heights = Array.from({ length: steps }, (_, i) => {
                const left = _stepAt(range, i, steps);
                const right = _stepAt(range, i + 1, steps);
                return _sampleNumber(fn, Math.fround(left + (right - left) * share), "Riemann fn");
            });
        } else {
            if (sample !== undefined) throw new TypeError("Riemann sample needs fn, since values are the heights themselves");
            if (!Array.isArray(values) || values.length !== steps || !values.every(Number.isFinite)) {
                throw new TypeError(`Riemann values must hold one finite height for each of the ${steps} steps of x_range`);
            }
        }
        const value = this._options({ ...style, x_range: range, values: heights });
        const state = SCENE_STATES.get(this);
        const name = `objects[${++state.handles}]`;
        const bars = `objects[${++state.handles}]`;
        state.lines.push((scene) => `${name} = ${parent ? parent.name : scene}:riemann ${value}`);
        state.lines.push(() => `${bars} = ${name}:children()`);
        const sum = this._track(new GroupHandle(this, name, "group"), parent, style.id);
        // The sum names its rectangles <id>/<n>, counting from the left.
        for (let i = 0; i < steps; i++) {
            const id = typeof style.id === "string" ? `${style.id}/${i + 1}` : undefined;
            this._track(new Handle(this, `${bars}[${i + 1}]`, "rectangle"), sum, id);
        }
        return sum;
    }

    riemann(options) {
        return this._riemann(options);
    }

    _contourSamples(fn, values, xRange, yRange, levelCount = 1) {
        const finite = (value) => Number.isFinite(value) && Math.abs(value) <= SINGLE_MAX;
        if (xRange === undefined) throw new TypeError("Contour requires x_range");
        if (yRange === undefined) throw new TypeError("Contour requires y_range");
        const gridX = _spaceRange(xRange, "Contour x_range"), gridY = _spaceRange(yRange, "Contour y_range");
        const xs = _rangeSamples(gridX, "Contour x_range", 65536);
        const ys = _rangeSamples(gridY, "Contour y_range", 65536);
        if (xs.length < 2 || ys.length < 2) throw new RangeError("Contour needs at least two samples along each range");
        if ([xs, ys].some((axis) => axis.some((v, i) => i && v <= axis[i - 1]))) {
            throw new RangeError("Contour grid samples must have distinct coordinates");
        }
        const count = xs.length * ys.length;
        if (count > 65536) throw new RangeError("A contour grid holds at most 65536 samples; widen the steps of x_range and y_range");
        if (count * levelCount > 1000000) throw new RangeError("Contour samples times levels must be at most 1000000");
        if ((fn === undefined) === (values === undefined)) throw new TypeError("Contour requires either fn or values");
        if (fn !== undefined && typeof fn !== "function") throw new TypeError("Contour fn must be a function");
        if (values !== undefined && (!Array.isArray(values) || values.length !== count)) {
            throw new TypeError(`Contour values must hold one finite number for each of the ${count} samples of x_range and y_range`);
        }
        const state = SCENE_STATES.get(this);
        const samples = [];
        state.sampleActive = true;
        try {
            for (const y of ys) for (const x of xs) {
                const value = fn === undefined ? values[samples.length] : fn(x, y);
                if (!finite(value)) throw new RangeError(`Contour ${fn === undefined ? "values" : "fn"} must give a finite number; at (${x}, ${y}) it did not`);
                samples.push(value);
            }
        } finally {
            state.sampleActive = false;
        }
        return { samples, xRange: gridX, yRange: gridY };
    }

    // Scalar samples cross into Lua as values; the core traces the bilinear field they define.
    _contour(options, parent = null) {
        this._assertQuiet();
        if (!options || (Object.getPrototypeOf(options) !== Object.prototype
            && Object.getPrototypeOf(options) !== null)) {
            throw new TypeError("Contour options must be a plain object");
        }
        const { fn, values, levels, x_range: xRange, y_range: yRange, ...style } = options;
        const allowed = ["colors", "id", "color", "stroke", "width", "dash", "dash_offset", "opacity",
            "progress", "layer", "isolate", "mask", "blend", "blur", "shadow", "labels"];
        for (const key of Object.keys(style)) {
            if (!allowed.includes(key)) throw new TypeError(`Unknown Contour option: ${key}`);
        }
        const labels = style.labels;
        if (labels !== undefined && typeof labels !== "boolean") {
            if (!labels || (Object.getPrototypeOf(labels) !== Object.prototype
                && Object.getPrototypeOf(labels) !== null)) throw new TypeError("Contour labels must be a boolean or a plain object");
            for (const key of Object.keys(labels)) {
                if (!["format", "size", "spacing", "padding", "upright"].includes(key)) throw new TypeError(`Unknown Contour labels option: ${key}`);
            }
            if (labels.format !== undefined && (typeof labels.format !== "string" || !/^%\.[0-6]f$/.test(labels.format))) {
                throw new TypeError("Contour label format must be %.0f through %.6f");
            }
            for (const key of ["size", "spacing", "padding"]) {
                if (labels[key] === undefined) continue;
                if (typeof labels[key] !== "number") throw new TypeError(`Contour label ${key} must be a number`);
                if (!Number.isFinite(labels[key]) || Math.abs(labels[key]) > SINGLE_MAX
                    || (key === "size" ? labels[key] <= 0 : labels[key] < 0)) {
                    throw new RangeError(`Contour label ${key} must be ${key === "size" ? "positive" : "zero or more"}`);
                }
            }
            if (labels.upright !== undefined && typeof labels.upright !== "boolean") throw new TypeError("Contour label upright must be a boolean");
        }
        const finite = (value) => Number.isFinite(value) && Math.abs(value) <= SINGLE_MAX;
        if (!Array.isArray(levels) || !levels.length || levels.length > 1024 || !levels.every(finite)) {
            throw new RangeError("Contour levels must hold from 1 to 1024 finite numbers");
        }
        if (style.colors !== undefined && !(typeof style.colors === "string"
            ? ["viridis", "plasma", "inferno", "magma", "cividis"].includes(style.colors)
            : Array.isArray(style.colors) && style.colors.length === levels.length)) {
            throw new TypeError("Contour colors must hold one color per level or name a colormap");
        }
        const { samples, xRange: gridX, yRange: gridY } = this._contourSamples(fn, values, xRange, yRange, levels.length);
        const state = SCENE_STATES.get(this);
        const value = this._options({ ...style, levels, x_range: xRange, y_range: yRange, values: samples });
        const name = `objects[${++state.handles}]`;
        const children = `objects[${++state.handles}]`;
        state.lines.push((scene) => `${name} = ${parent ? parent.name : scene}:contour ${value}`);
        state.lines.push(() => `${children} = ${name}:children()`);
        const group = this._track(new GroupHandle(this, name, "group"), parent, style.id);
        levels.forEach((_, i) => {
            const id = typeof style.id === "string" ? `${style.id}/${i + 1}` : undefined;
            const child = this._track(new Handle(this, `${children}[${i + 1}]`, "plot"), group, id);
            PLOT_CUTS.set(child, { contour: true, xRange: gridX, yRange: gridY, level: levels[i] });
        });
        return group;
    }

    contour(options) {
        return this._contour(options);
    }

    // An arrow for fn at every sample of the grid, as a group whose children are the arrows row
    // by row from the bottom left: the builder calls fn where Lua vector_field would and emits
    // the vectors as values.
    _vectorField(options, parent = null) {
        this._assertQuiet();
        if (!options || (Object.getPrototypeOf(options) !== Object.prototype
            && Object.getPrototypeOf(options) !== null)) {
            throw new TypeError("Vector field options must be a plain object");
        }
        const { fn, values, x_range: xRange, y_range: yRange, ...style } = options;
        if (xRange === undefined) throw new TypeError("Vector field requires x_range");
        if (yRange === undefined) throw new TypeError("Vector field requires y_range");
        const columns = _spaceRange(xRange, "Vector field x_range");
        const rows = _spaceRange(yRange, "Vector field y_range");
        const xs = _rangeSamples(columns, "Vector field x_range", 1024);
        const ys = _rangeSamples(rows, "Vector field y_range", 1024);
        const count = xs.length * ys.length;
        if (count > 1024) throw new RangeError("A vector field holds at most 1024 arrows; widen the steps of x_range and y_range");
        if (style.length !== undefined && style.scale !== undefined) {
            throw new TypeError("Vector field length and scale cannot be used together");
        }
        const length = Math.fround(style.length ?? Math.fround(0.8) * Math.min(columns[2], rows[2]));
        if (!(length > 0)) throw new RangeError("Vector field length must be a positive number of world units");
        const scale = Math.fround(style.scale ?? 1);
        if (!(scale > 0)) throw new RangeError("Vector field scale must be a positive number");
        if ((fn === undefined) === (values === undefined)) throw new TypeError("Vector field requires either fn or values");
        let vectors = values;
        if (fn !== undefined) {
            if (typeof fn !== "function") throw new TypeError("Vector field fn must be a function");
            vectors = ys.flatMap((y) => xs.map((x) => _sampleVector(fn, x, y, "Vector field fn")));
        } else if (!Array.isArray(values) || values.length !== count
            || !values.every((v) => Array.isArray(v) && v.length === 2 && v.every((n) => Number.isFinite(Math.fround(n))))) {
            throw new TypeError(`Vector field values must hold one [x, y] for each of the ${count} samples of x_range and y_range`);
        }
        const value = this._options({ ...style, x_range: xRange, y_range: yRange, values: vectors });
        const state = SCENE_STATES.get(this);
        const name = `objects[${++state.handles}]`;
        const arrows = `objects[${++state.handles}]`;
        state.lines.push((scene) => `${name} = ${parent ? parent.name : scene}:vector_field ${value}`);
        state.lines.push(() => `${arrows} = ${name}:children()`);
        const field = this._track(new GroupHandle(this, name, "group"), parent, style.id);
        // The field names its arrows <id>/<column>/<row>; it leaves out the arrows the scene
        // does, where the field vanishes or an arrow would be too short to see.
        const magnitudes = vectors.map(([x, y]) => Math.hypot(Math.fround(x), Math.fround(y)));
        const scaled = style.scale !== undefined;
        const shortest = 1e-3 * (scaled ? scale * Math.max(...magnitudes) : length);
        let child = 0;
        vectors.forEach((_, k) => {
            const magnitude = magnitudes[k];
            const drawn = scaled ? magnitude * scale : length * Math.tanh(magnitude / length);
            if (!(magnitude > 0) || drawn < shortest) return;
            const column = (k % xs.length) + 1;
            const row = Math.floor(k / xs.length) + 1;
            const id = typeof style.id === "string" ? `${style.id}/${column}/${row}` : undefined;
            this._track(new Handle(this, `${arrows}[${++child}]`, "vector"), field, id);
        });
        return field;
    }

    vectorField(options) {
        return this._vectorField(options);
    }

    // Entries in rows and columns between brackets, as a group whose children are its rows,
    // each a group of the texts of its entries, and then its two brackets.
    _matrix(options, parent = null) {
        this._assertQuiet();
        if (!options || (Object.getPrototypeOf(options) !== Object.prototype
            && Object.getPrototypeOf(options) !== null)) {
            throw new TypeError("Matrix options must be a plain object");
        }
        const rows = _gridRows(options.rows, "Matrix");
        if (options.reserve !== undefined && typeof options.reserve !== "string"
            && (!Array.isArray(options.reserve) || options.reserve.length !== rows[0].length
                || options.reserve.some((sample) => typeof sample !== "string"))) {
            throw new TypeError("Matrix reserve must be a string or one string per column");
        }
        if (options.tabular !== undefined && typeof options.tabular !== "boolean") {
            throw new TypeError("Matrix tabular must be a boolean");
        }
        const brackets = options.brackets ?? "square";
        if (!MATRIX_BRACKETS.has(brackets)) throw new TypeError("Matrix brackets must be square, round, bar, or none");
        const value = this._options(options);
        const state = SCENE_STATES.get(this);
        const name = `objects[${++state.handles}]`;
        const children = `objects[${++state.handles}]`;
        const entries = `objects[${++state.handles}]`;
        state.lines.push((scene) => `${name} = ${parent ? parent.name : scene}:matrix ${value}`);
        state.lines.push(() => `${children} = ${name}:children()`);
        state.lines.push(() => `${entries} = {} for i, row in ipairs(${children}) do ${entries}[i] = row:children() end`);
        const matrix = this._track(new GroupHandle(this, name, "group"), parent, options.id);
        // The matrix names its rows <id>/<r>, its entries <id>/<r>/<c> and its brackets
        // <id>/left and <id>/right.
        const id = typeof options.id === "string" ? options.id : null;
        rows.forEach((row, r) => {
            const rowId = id === null ? undefined : `${id}/${r + 1}`;
            const group = this._track(new GroupHandle(this, `${children}[${r + 1}]`, "group"), matrix, rowId);
            row.forEach((entry, c) => {
                const entryId = id === null ? undefined : `${rowId}/${c + 1}`;
                const type = typeof entry === "number" ? "number" : "text";
                this._track(new Handle(this, `${entries}[${r + 1}][${c + 1}]`, type), group, entryId);
            });
        });
        if (brackets !== "none") {
            ["left", "right"].forEach((end, index) => {
                const bracketId = id === null ? undefined : `${id}/${end}`;
                this._track(new Handle(this, `${children}[${rows.length + index + 1}]`, "path"), matrix, bracketId);
            });
        }
        return matrix;
    }

    matrix(options) {
        return this._matrix(options);
    }

    // Entries in cells that tile a grid, the header's first, as a group whose children are its
    // rows, each a group of the texts of its entries, then its stripes and then its rules.
    _table(options, parent = null) {
        this._assertQuiet();
        if (!options || (Object.getPrototypeOf(options) !== Object.prototype
            && Object.getPrototypeOf(options) !== null)) {
            throw new TypeError("Table options must be a plain object");
        }
        const body = _gridRows(options.rows, "Table");
        const columns = body[0].length;
        const { header, stripes } = options;
        if (header !== undefined && (!Array.isArray(header) || header.length !== columns)) {
            throw new TypeError(`Table header must be an array of one entry for each of its ${columns} columns`);
        }
        const rows = header === undefined ? body : _gridRows([header, ...body], "Table");
        const rules = options.rules ?? (header === undefined ? "none" : "header");
        if (!TABLE_RULES.has(rules)) throw new TypeError("Table rules must be header, rows, columns, grid, or none");
        if (stripes !== undefined && typeof stripes !== "boolean" && typeof stripes !== "string") {
            throw new TypeError("Table stripes must be true or a color");
        }
        const value = this._options(options);
        const state = SCENE_STATES.get(this);
        const name = `objects[${++state.handles}]`;
        const children = `objects[${++state.handles}]`;
        const entries = `objects[${++state.handles}]`;
        state.lines.push((scene) => `${name} = ${parent ? parent.name : scene}:table ${value}`);
        state.lines.push(() => `${children} = ${name}:children()`);
        state.lines.push(() => `${entries} = {} for i = 1, ${rows.length} do ${entries}[i] = ${children}[i]:children() end`);
        const table = this._track(new GroupHandle(this, name, "group"), parent, options.id);
        // The table names its rows <id>/<r> and its entries <id>/<r>/<c>, the header's first.
        const id = typeof options.id === "string" ? options.id : null;
        rows.forEach((row, r) => {
            const rowId = id === null ? undefined : `${id}/${r + 1}`;
            const group = this._track(new GroupHandle(this, `${children}[${r + 1}]`, "group"), table, rowId);
            row.forEach((entry, c) => {
                const entryId = id === null ? undefined : `${rowId}/${c + 1}`;
                const type = typeof entry === "number" ? "number" : "text";
                this._track(new Handle(this, `${entries}[${r + 1}][${c + 1}]`, type), group, entryId);
            });
        });
        // A stripe behind every second row below the header, then the rule under the header,
        // those between rows, those between columns and the four of the frame.
        let child = rows.length;
        const track = (count, type) => {
            for (let i = 0; i < count; i++) this._track(new Handle(this, `${children}[${++child}]`, type), table);
        };
        if (stripes !== undefined && stripes !== false) track(Math.floor(body.length / 2), "rectangle");
        if (rules !== "none") {
            const between = rules === "grid" || rules === "rows" ? body.length - 1 : 0;
            const across = rules === "grid" || rules === "columns" ? columns - 1 : 0;
            track((header === undefined ? 0 : 1) + between + across + (rules === "grid" ? 4 : 0), "line");
        }
        return table;
    }

    table(options) {
        return this._table(options);
    }

    // The handle created with this id, or null.
    find(id) {
        if (typeof id !== "string") throw new TypeError("Scene find requires an id string");
        return SCENE_STATES.get(this).ids.get(id) ?? null;
    }

    // A copy of source and what is below it in the state they show at this time, attached
    // beside source or below options.parent. The copy's handles mirror those created below
    // source; with options.id, find() knows the copy and the ids below it.
    clone(source, options = {}) {
        // A copy takes no time, but it takes the state its source shows, which the tracks set.
        this._assertMutable("instant");
        const sourceName = this._handle(source);
        if (!options || (Object.getPrototypeOf(options) !== Object.prototype
            && Object.getPrototypeOf(options) !== null)) {
            throw new TypeError("Clone options must be a plain object");
        }
        const state = SCENE_STATES.get(this);
        // The handles below source, each after its parent, taken before the copy joins any.
        const tree = [];
        const collect = (handle, parent) => {
            const index = tree.push({ handle, parent }) - 1;
            for (const child of state.children.get(handle) ?? []) collect(child, index);
        };
        collect(source, -1);
        for (const { handle } of tree) {
            const type = HANDLE_TYPES.get(handle);
            if (type === "space" || type === "cell") throw new TypeError(`A ${type} cannot be cloned yet`);
        }
        if (options.shift !== undefined) this._assertTransformable(source);
        const styled = _styleOptions(options);
        const fields = Object.keys(styled).sort().map((key) =>
            key === "parent" ? `["parent"]=${this._handle(styled.parent)}` : this._field(key, styled[key]));
        const tags = new Map([...state.ids].map(([id, handle]) => [handle, id]));
        const origin = tags.get(source);
        const name = `objects[${++state.handles}]`;
        const copies = `objects[${++state.handles}]`;
        const value = fields.length ? `,{${fields.join(",")}}` : "";
        state.lines.push((scene) => `${name},${copies} = ${scene}:clone(${sourceName}${value})`);
        const made = [];
        tree.forEach(({ handle, parent }, index) => {
            // Below a copy with a given id, an id that begins with the source's takes the copy's
            // in its place, and any other id takes the copy's and a slash before it.
            const tag = tags.get(handle);
            let id = index ? undefined : options.id;
            if (index && typeof options.id === "string" && tag !== undefined) {
                id = origin !== undefined && tag.startsWith(`${origin}/`)
                    ? options.id + tag.slice(origin.length) : `${options.id}/${tag}`;
            }
            const Kind = handle instanceof GroupHandle ? GroupHandle : Handle;
            const copy = new Kind(this, index ? `${copies}[${handle.name}]` : name, HANDLE_TYPES.get(handle));
            if (POINT_COUNTS.has(handle)) POINT_COUNTS.set(copy, POINT_COUNTS.get(handle));
            if (PLOT_CUTS.has(handle)) PLOT_CUTS.set(copy, PLOT_CUTS.get(handle));
            const above = index ? made[parent] : options.parent ?? state.parents.get(source) ?? null;
            made.push(this._track(copy, above, id));
        });
        return made[0];
    }

    connector(from, to, options = {}) {
        return this._connector(from, to, options);
    }

    sound(options) {
        this._assertMutable();
        if (!options || (Object.getPrototypeOf(options) !== Object.prototype
            && Object.getPrototypeOf(options) !== null)) {
            throw new TypeError("Sound options must be a plain object");
        }
        const fields = new Set(["asset", "bus", "begin", "end", "gain", "loop"]);
        const unknown = Object.keys(options).find((key) => !fields.has(key));
        if (unknown) throw new TypeError(`Unknown Sound option: ${unknown}`);
        if (typeof options.asset !== "string" || !options.asset.length
            || options.asset.includes("\0")
            || new TextEncoder().encode(options.asset).length > 255) {
            throw new TypeError("Sound asset must be 1 to 255 UTF-8 bytes without zero bytes");
        }
        const bus = options.bus ?? "effect";
        const begin = options.begin ?? 0;
        const end = options.end ?? 0;
        const gain = options.gain ?? 1;
        const loop = options.loop ?? false;
        if (!AUDIO_BUSES.has(bus)) throw new TypeError("Sound bus must be music, effect, or ui");
        if (!Number.isFinite(begin) || begin < 0) {
            throw new RangeError("Sound begin must be finite and non-negative");
        }
        if (!Number.isFinite(end) || (end !== 0 && end <= begin)) {
            throw new RangeError("Sound end must be zero or greater than begin");
        }
        if (!Number.isFinite(gain) || gain < 0 || gain > 4) {
            throw new RangeError("Sound gain must be finite and between 0 and 4");
        }
        if (typeof loop !== "boolean") throw new TypeError("Sound loop must be boolean");
        const state = SCENE_STATES.get(this);
        const name = `cues[${++state.audio}]`;
        const value = _lua({asset: options.asset, bus, begin, end, gain, loop});
        // vis.audio exists only where the host has the audio module, so the cue is guarded by
        // it, as a Lua scene guards its cues: without the module the scene plays without sound.
        state.lines.push((scene) => `if vis.audio then ${name}=vis.audio.cue(${scene},${value}) end`);
        return new AudioCueHandle(this, name);
    }

    // The scene attaches to sampled closed outlines, falling back to bounds.
    _connector(from, to, options, parent = null) {
        this._assertQuiet();
        const fromName = this._handle(from);
        const toName = this._handle(to);
        if (
            !options ||
            (Object.getPrototypeOf(options) !== Object.prototype && Object.getPrototypeOf(options) !== null)
        ) {
            throw new TypeError("Connector options must be a plain object");
        }
        if (Object.hasOwn(options, "from") || Object.hasOwn(options, "to")) {
            throw new TypeError("Connector endpoints must be passed as handles");
        }
        const styled = _styleOptions(options);
        const fields = Object.keys(styled)
            .sort()
            .map((key) => this._field(key, styled[key]));
        const state = SCENE_STATES.get(this);
        const name = `objects[${++state.handles}]`;
        const value = `{from=${fromName},to=${toName}${fields.length ? `,${fields.join(",")}` : ""}}`;
        state.lines.push((scene) => `${name} = ${parent ? parent.name : scene}:connector ${value}`);
        return this._track(new Handle(this, name, "connector"), parent, options.id);
    }

    viewport(child, bounds) {
        if (!(child instanceof SceneBuilder)) throw new TypeError("Viewport child must be a viscript JS scene");
        if (child === this) throw new TypeError("A scene cannot be its own viewport child");
        for (let parent = this; parent; parent = SCENE_OWNERS.get(parent)) {
            if (parent === child) throw new TypeError("Viewport scenes cannot form a cycle");
        }
        this._assertMutable("instant");
        if (SCENE_OWNERS.has(child)) throw new TypeError("Viewport child scene is already owned");
        const rect = _lua(_viewportBounds(bounds));
        const snapshot = _snapshot(child);
        SCENE_OWNERS.set(child, this);
        SCENE_STATES.get(this).lines.push(Object.freeze({ kind: "viewport", snapshot, rect }));
        return this;
    }

    // Draws this scene's own objects a second time in a rectangle of the frame, given as a
    // viewport's, through a camera of its own: a magnifier, an inset or a picture in picture.
    // The camera is a look over the scene's camera; the background defaults to the scene's.
    view(bounds, options = {}) {
        // A view takes no time, but its camera may aim at an object where the tracks leave it.
        this._assertMutable("instant");
        const rect = _lua(_viewportBounds(bounds));
        if (!options || (Object.getPrototypeOf(options) !== Object.prototype && Object.getPrototypeOf(options) !== null)) {
            throw new TypeError("View options must be a plain object");
        }
        const unknown = Object.keys(options).find((key) => !VIEW_OPTIONS.includes(key));
        if (unknown) throw new TypeError(`Unknown View option: ${unknown}`);
        const { camera, optical, background, opacity, layer, hide } = options;
        if (camera !== undefined && (!camera || typeof camera !== "object" || Array.isArray(camera))) {
            throw new TypeError("View camera must be a look camera object");
        }
        if (optical !== undefined && typeof optical !== "boolean") throw new TypeError("View optical must be a boolean");
        if (background !== undefined && typeof background !== "string") {
            throw new TypeError("View background must be a color");
        }
        if (opacity !== undefined && !(Number.isFinite(opacity) && opacity >= 0 && opacity <= 1)) {
            throw new RangeError("View opacity must be between 0 and 1");
        }
        if (layer !== undefined && !(Number.isInteger(layer) && layer >= -(2 ** 31) && layer < 2 ** 31)) {
            throw new RangeError("View layer must be an integer that fits in 32 bits");
        }
        const hidden = hide === undefined ? [] : Array.isArray(hide) ? hide : [hide];
        if (hidden.length > VIEW_HIDDEN_LIMIT) throw new RangeError(`A view hides at most ${VIEW_HIDDEN_LIMIT} objects`);
        const entries = [];
        for (const key of Object.keys(options).sort()) {
            const value = options[key];
            if (value === undefined) continue;
            if (key === "camera") entries.push(`["camera"]=${this._camera(value)}`);
            else if (key === "follow") entries.push(`["follow"]=${this._handle(value)}`);
            else if (key === "hide") entries.push(`["hide"]={${hidden.map((item) => this._handle(item)).join(",")}}`);
            else entries.push(`[${_string(key)}]=${_lua(value)}`);
        }
        const state = SCENE_STATES.get(this);
        if (state.views >= VIEW_LIMIT) {
            throw new RangeError(`A scene holds at most ${VIEW_LIMIT} views, as each draws the scene again`);
        }
        const name = `view${++state.views}`;
        const value = `{${entries.join(",")}}`;
        state.lines.push((scene) => `local ${name}=${scene}:view(${rect},${value})`);
        return new ViewHandle(this, name);
    }

    sceneTransition(stages, options = {}) {
        this._assertMutable();
        if (!Array.isArray(stages) || stages.length < 2) {
            throw new TypeError("Scene transition requires at least two stage scenes");
        }
        if (stages.length > 16) throw new RangeError("Scene transition supports at most 16 stages");
        const state = SCENE_STATES.get(this);
        if (state.transition) throw new TypeError("A scene can own only one scene transition");
        if (
            !options ||
            (Object.getPrototypeOf(options) !== Object.prototype && Object.getPrototypeOf(options) !== null)
        ) {
            throw new TypeError("Scene transition options must be a plain object");
        }
        const allowed = new Set(["duration", "hold", "live", "easing", "curve", "viewport", "styles", ...TRANSITION_STYLE_OPTIONS]);
        const unknown = Object.keys(options).find((key) => !allowed.has(key));
        if (unknown) throw new TypeError(`Unknown scene transition option: ${unknown}`);
        if (Object.hasOwn(options, "easing") && Object.hasOwn(options, "curve")) {
            throw new TypeError("Scene transition options must use either easing or curve, not both");
        }
        const normalized = { ...options };
        if (Object.hasOwn(options, "styles")) {
            const single = TRANSITION_STYLE_OPTIONS.find((key) => Object.hasOwn(options, key));
            if (single) {
                throw new TypeError(`Scene transition options must use either ${single} or styles, not both`);
            }
            if (!Array.isArray(options.styles) || options.styles.length !== stages.length - 1) {
                throw new TypeError(
                    `Scene transition styles must list one style for each of the ${stages.length - 1} transitions`,
                );
            }
            normalized.styles = options.styles.map(_transitionStyle);
        } else {
            const style = {};
            for (const key of TRANSITION_STYLE_OPTIONS) {
                if (Object.hasOwn(options, key)) style[key] = options[key];
            }
            Object.assign(normalized, _transitionStyle(style));
        }
        if (!Object.hasOwn(normalized, "duration")) normalized.duration = 1;
        if (!Object.hasOwn(normalized, "hold")) normalized.hold = 0;
        if (!Object.hasOwn(normalized, "easing") && !Object.hasOwn(normalized, "curve")) {
            normalized.easing = "smooth";
        }
        if (!Number.isFinite(normalized.duration) || normalized.duration <= 0) {
            throw new RangeError("Scene transition duration must be a finite positive number");
        }
        const holds = Array.isArray(normalized.hold) ? normalized.hold : [normalized.hold];
        if (Array.isArray(normalized.hold) && holds.length !== stages.length) {
            throw new RangeError(`Scene transition hold must list one duration for each of the ${stages.length} stages`);
        }
        for (let i = 0; i < holds.length; i++) {
            if (!Number.isFinite(holds[i]) || holds[i] < 0) {
                throw new RangeError("Scene transition hold must be a finite non-negative number");
            }
        }
        if (Object.hasOwn(normalized, "live") && typeof normalized.live !== "boolean") {
            throw new TypeError("Scene transition live must be a boolean");
        }
        if (Object.hasOwn(normalized, "viewport")) {
            normalized.viewport = _viewportBounds(normalized.viewport);
        }

        const unique = new Set();
        for (const child of stages) {
            if (!(child instanceof SceneBuilder)) {
                throw new TypeError("Scene transition stages must be viscript JS scenes");
            }
            if (child === this) throw new TypeError("A scene cannot transition to itself");
            if (unique.has(child)) throw new TypeError("Scene transition stages must be unique");
            unique.add(child);
            for (let parent = this; parent; parent = SCENE_OWNERS.get(parent)) {
                if (parent === child) throw new TypeError("Scene transitions cannot form a cycle");
            }
            if (SCENE_OWNERS.has(child)) throw new TypeError("Scene transition stage is already owned");
        }
        const compiledOptions = _lua(normalized);
        const snapshots = Object.freeze(stages.map(_snapshot));
        for (const child of stages) SCENE_OWNERS.set(child, this);
        state.lines.push(
            Object.freeze({
                kind: "transition",
                snapshots,
                options: compiledOptions,
            }),
        );
        state.transition = true;
        return this;
    }

    _target(value) {
        if (value instanceof Handle && value.owner === this) return value.name;
        if (Array.isArray(value) && value.length) return `{${value.map((item) => this._target(item)).join(",")}}`;
        throw new TypeError("Animation targets must be handles from the same scene");
    }

    _handle(value) {
        if (value instanceof Handle && value.owner === this) return value.name;
        throw new TypeError("Object references must be handles from the same scene");
    }

    // A look's camera as a Lua table; a target of handles names them as the script does.
    _camera(camera) {
        const target = camera && typeof camera === "object" ? camera.target : undefined;
        const objects = Array.isArray(target) ? target.some((item) => item instanceof Handle) : target instanceof Handle;
        const prototype = objects ? Object.getPrototypeOf(camera) : undefined;
        if (!objects || (prototype !== Object.prototype && prototype !== null)) return _lua(camera);
        return `{${Object.keys(camera)
            .sort()
            .map((key) => {
                if (key !== "target") return `[${_string(key)}]=${_lua(camera[key])}`;
                if (target instanceof Handle) return `["target"]=${this._handle(target)}`;
                return `["target"]={${target.map((item) => this._handle(item)).join(",")}}`;
            })
            .join(",")}}`;
    }

    // Object options as a Lua table; a mask names its target handle.
    _options(options) {
        const styled = _styleOptions(options);
        if (!styled || typeof styled !== "object"
            || (!Object.hasOwn(styled, "mask") && !Object.hasOwn(styled, "around") && !Object.hasOwn(styled, "of")
                && !Object.hasOwn(styled, "trimDistance"))) {
            return _lua(styled);
        }
        return `{${Object.keys(styled)
            .sort()
            .map((key) => this._field(key, styled[key]))
            .join(",")}}`;
    }

    _field(key, value) {
        if (key === "around") {
            // A rectangle sized around objects names them as the script does.
            if (value instanceof Handle) return `["around"]=${this._handle(value)}`;
            if (!Array.isArray(value) || !value.length || !value.every((item) => item instanceof Handle)) {
                throw new TypeError("Around must be a handle or a non-empty array of handles");
            }
            return `["around"]={${value.map((item) => this._handle(item)).join(",")}}`;
        }
        if (key === "of") {
            // A region names the shapes it combines as the script does.
            if (!Array.isArray(value) || value.length < 2 || value.length > 64
                || !value.every((item) => item instanceof Handle)) {
                throw new TypeError("Of must be an array of 2 to 64 shape handles");
            }
            return `["of"]={${value.map((item) => this._handle(item)).join(",")}}`;
        }
        if (key !== "mask") return `[${_string(key === "trimDistance" ? "trim_distance" : key)}]=${_lua(value)}`;
        if (value instanceof Handle) return `["mask"]=${this._handle(value)}`;
        if (!value || (Object.getPrototypeOf(value) !== Object.prototype && Object.getPrototypeOf(value) !== null)) {
            throw new TypeError("Mask must be a handle or {target, mode}");
        }
        const unknown = Object.keys(value).find((name) => name !== "target" && name !== "mode");
        if (unknown) throw new TypeError(`Unknown mask option: ${unknown}`);
        if (!Object.hasOwn(value, "target")) throw new TypeError("Mask requires a target");
        const mode = value.mode === undefined ? "" : `["mode"]=${_lua(_maskMode(value.mode))},`;
        return `["mask"]={${mode}["target"]=${this._handle(value.target)}}`;
    }

    _assertTransformable(value) {
        if (value instanceof Handle && value.owner === this) {
            if (HANDLE_TYPES.get(value) === "connector") {
                throw new TypeError("Connector geometry follows its endpoints and cannot be shifted or transformed");
            }
            return;
        }
        if (Array.isArray(value) && value.length) {
            for (const item of value) this._assertTransformable(item);
        }
    }

    _animate(method, target, values) {
        this._assertMutable();
        const targetName = this._target(target);
        const argumentsList = values.map(_lua).join(",");
        SCENE_STATES.get(this).lines.push((scene) => `${scene}:${method}(${targetName},${argumentsList})`);
        return this;
    }

    create(target, duration = 1, easing = "smooth", lag = 0, direction) {
        if (typeof duration === "object") {
            if (arguments.length > 2) throw new TypeError("Timing options cannot be combined with positional arguments");
            ({duration, curve: easing, lag, options: {direction}} = _timing(duration, 1, true, false, ["direction"]));
        }
        if (direction === undefined) return this._animate("create", target, [duration, easing, lag]);
        this._assertMutable();
        _direction(direction);
        return this._animate("create", target, [duration, easing, lag, direction]);
    }

    uncreate(target, duration = 1, easing = "smooth", lag = 0, direction = "forward") {
        if (typeof duration === "object") {
            if (arguments.length > 2) throw new TypeError("Timing options cannot be combined with positional arguments");
            ({duration, curve: easing, lag, options: {direction = "forward"}} = _timing(duration, 1, true, false, ["direction"]));
        }
        this._assertMutable();
        _direction(direction);
        return this._animate("uncreate", target, [duration, easing, lag, direction]);
    }

    fillReveal(target, duration = 1, easing = "smooth", lag = 0) {
        if (typeof duration === "object") {
            if (arguments.length > 2) throw new TypeError("Timing options cannot be combined with positional arguments");
            ({duration, curve: easing, lag} = _timing(duration, 1, true));
        }
        return this._animate("fill_reveal", target, [duration, easing, lag]);
    }

    drawBorderThenFill(target, duration = 1, easing = "smooth", lag = 0, direction) {
        if (typeof duration === "object") {
            if (arguments.length > 2) throw new TypeError("Timing options cannot be combined with positional arguments");
            ({duration, curve: easing, lag, options: {direction}} = _timing(duration, 1, true, false, ["direction"]));
        }
        if (direction === undefined) {
            return this._animate("draw_border_then_fill", target, [duration, easing, lag]);
        }
        this._assertMutable();
        _direction(direction);
        return this._animate("draw_border_then_fill", target, [duration, easing, lag, direction]);
    }

    write(target, duration = 1, easing = "smooth", lag = 0, direction) {
        if (typeof duration === "object") {
            if (arguments.length > 2) throw new TypeError("Timing options cannot be combined with positional arguments");
            ({duration, curve: easing, lag, options: {direction}} = _timing(duration, 1, true, false, ["direction"]));
        }
        if (direction === undefined) return this._animate("write", target, [duration, easing, lag]);
        this._assertMutable();
        _direction(direction);
        return this._animate("write", target, [duration, easing, lag, direction]);
    }

    _effect(method, target, options, defaults, name, minimumScale) {
        this._assertMutable();
        const targetName = this._handle(target);
        const value = _lua(_effectOptions(options, defaults, name, minimumScale));
        SCENE_STATES.get(this).lines.push((scene) => `${scene}:${method}(${targetName},${value})`);
        return this;
    }

    fadeIn(target, options = {}) {
        return this._effect("fade_in", target, options, FADE_OPTIONS, "FadeIn", 0);
    }

    fadeOut(target, options = {}) {
        return this._effect("fade_out", target, options, FADE_OPTIONS, "FadeOut", 0);
    }

    growFromCenter(target, duration = 1, easing = "smooth") {
        if (typeof duration === "object") {
            if (arguments.length > 2) throw new TypeError("Timing options cannot be combined with positional arguments");
            ({duration, curve: easing} = _timing(duration));
        }
        return this._animate("grow_from_center", target, [duration, easing]);
    }

    growFromEdge(target, edge, duration = 1, easing = "smooth") {
        if (typeof duration === "object") {
            if (arguments.length > 3) throw new TypeError("Timing options cannot be combined with positional arguments");
            ({duration, curve: easing} = _timing(duration));
        }
        this._assertMutable();
        _growthEdge(edge);
        return this._animate("grow_from_edge", target, [edge, duration, easing]);
    }

    shrinkToCenter(target, duration = 1, easing = "smooth") {
        if (typeof duration === "object") {
            if (arguments.length > 2) throw new TypeError("Timing options cannot be combined with positional arguments");
            ({duration, curve: easing} = _timing(duration));
        }
        return this._animate("shrink_to_center", target, [duration, easing]);
    }

    // Flashes a handle, or each of an array of handles at once, such as the tokens a search
    // finds.
    indicate(target, options = {}) {
        this._assertMutable();
        const targetName = this._target(target);
        const value = _lua(_effectOptions(options, INDICATE_OPTIONS, "Indicate", 1));
        SCENE_STATES.get(this).lines.push((scene) => `${scene}:indicate(${targetName},${value})`);
        return this;
    }

    wiggle(target, options = {}) {
        this._assertMutable();
        const values = _effectOptions(options, WIGGLE_OPTIONS, "Wiggle", 0);
        if (!Number.isFinite(values.angle)) throw new RangeError("Wiggle angle must be a finite number of radians");
        if (!Number.isFinite(values.swings) || values.swings <= 0) {
            throw new RangeError("Wiggle swings must be a finite positive number");
        }
        return this._effect("wiggle", target, options, WIGGLE_OPTIONS, "Wiggle", 0);
    }

    circumscribe(target, options = {}) {
        this._assertMutable();
        const values = _effectOptions(options, CIRCUMSCRIBE_OPTIONS, "Circumscribe", 0);
        if (values.shape !== "rectangle" && values.shape !== "ellipse") {
            throw new TypeError('Circumscribe shape must be "rectangle" or "ellipse"');
        }
        _emphasisValues(values, "Circumscribe");
        return this._effect("circumscribe", target, options, CIRCUMSCRIBE_OPTIONS, "Circumscribe", 0);
    }

    passingFlash(target, options = {}) {
        this._assertMutable();
        _emphasisValues(_effectOptions(options, PASSING_FLASH_OPTIONS, "PassingFlash", 0), "PassingFlash");
        return this._effect("passing_flash", target, options, PASSING_FLASH_OPTIONS, "PassingFlash", 0);
    }

    flash(target, options = {}) {
        this._assertMutable();
        const values = _effectOptions(options, FLASH_OPTIONS, "Flash", 0);
        _emphasisValues(values, "Flash");
        if (!Number.isInteger(values.lines) || values.lines < 1 || values.lines > 64) {
            throw new RangeError("Flash lines must be a whole number from 1 to 64");
        }
        if (target instanceof Handle) return this._effect("flash", target, options, FLASH_OPTIONS, "Flash", 0);
        // A flash may also burst out of a point in world coordinates.
        if (!Array.isArray(target) || (target.length !== 2 && target.length !== 3) || !target.every(Number.isFinite)) {
            throw new TypeError("Flash takes a handle or a finite point [x, y] or [x, y, z]");
        }
        const point = _lua(target);
        const value = _lua(values);
        SCENE_STATES.get(this).lines.push((scene) => `${scene}:flash(${point},${value})`);
        return this;
    }

    ripple(target, options = {}) {
        this._assertMutable();
        _emphasisValues(_effectOptions(options, RIPPLE_OPTIONS, "Ripple", 0), "Ripple");
        return this._effect("ripple", target, options, RIPPLE_OPTIONS, "Ripple", 0);
    }

    morph(source, target, duration = 1, easing = "smooth", lag) {
        if (typeof duration === "object") {
            if (arguments.length > 3) throw new TypeError("Timing options cannot be combined with positional arguments");
            ({duration, curve: easing, options: lag} = _timing(duration, 1, true, false, ["lag", "copy", "pairs"]));
        }
        this._assertMutable();
        const sourceArray = Array.isArray(source);
        const targetArray = Array.isArray(target);
        if (sourceArray !== targetArray) {
            throw new TypeError("Morph source and target must both be handles or arrays");
        }
        if (sourceArray && (!source.length || source.length !== target.length)) {
            throw new TypeError("Morph arrays must be non-empty and equally sized");
        }
        // The lag, or options: with copy, a copy of each source morphs in its place.
        let lagValue = "";
        let copy = false;
        if (lag === null || typeof lag !== "object") {
            if (lag !== undefined) lagValue = `,${_lua(lag)}`;
        } else {
            if (Object.getPrototypeOf(lag) !== Object.prototype && Object.getPrototypeOf(lag) !== null) {
                throw new TypeError("Morph options must be a number or a plain object");
            }
            const unknown = Object.keys(lag).find((key) => key !== "lag" && key !== "copy" && key !== "pairs");
            if (unknown) throw new TypeError(`Unknown morph option: ${unknown}`);
            if (lag.lag !== undefined && !(Number.isFinite(lag.lag) && lag.lag >= 0)) {
                throw new RangeError("Morph lag must be a finite number of zero or more");
            }
            if (lag.copy !== undefined && typeof lag.copy !== "boolean") {
                throw new TypeError("Morph copy must be a boolean");
            }
            if (lag.pairs !== undefined) {
                if (!Array.isArray(lag.pairs) || lag.pairs.some((pair) => !Array.isArray(pair) || pair.length !== 2
                    || [pair[0], pair[1]].some((index) => !Number.isInteger(index) || index < 1 || index > 0xffffffff))) {
                    throw new TypeError("Morph pairs must be an array of positive integer index pairs");
                }
                const from = new Set();
                const to = new Set();
                for (const [a, b] of lag.pairs) {
                    if (from.has(a) || to.has(b)) throw new TypeError("Morph pairs must use each piece at most once");
                    from.add(a);
                    to.add(b);
                }
            }
            copy = lag.copy === true;
            const fields = Object.keys(lag).filter((key) => lag[key] !== undefined).sort();
            lagValue = `,{${fields.map((key) => `[${_string(key)}]=${_lua(lag[key])}`).join(",")}}`;
        }
        const sources = sourceArray ? source : [source];
        const targets = targetArray ? target : [target];
        const sourceNames = new Set();
        const targetNames = new Set();
        for (let index = 0; index < sources.length; ++index) {
            const sourceName = this._handle(sources[index]);
            const targetName = this._handle(targets[index]);
            if (sourceName === targetName) {
                throw new TypeError("Morph source and target must be distinct handles");
            }
            // With copy, one source may give a copy to several targets.
            if ((!copy && sourceNames.has(sourceName)) || targetNames.has(targetName)
                || sourceNames.has(targetName) || targetNames.has(sourceName)) {
                throw new TypeError("Morph handles must be distinct across all pairs");
            }
            sourceNames.add(sourceName);
            targetNames.add(targetName);
        }
        const sourceName = this._target(source);
        const targetName = this._target(target);
        const durationValue = _lua(duration);
        const easingValue = _lua(easing);
        SCENE_STATES.get(this).lines.push(
            (scene) => `${scene}:morph(${sourceName},${targetName},${durationValue},${easingValue}${lagValue})`,
        );
        return this;
    }

    fadeTransform(source, target, duration = 1, easing = "smooth") {
        if (typeof duration === "object") {
            if (arguments.length > 3) throw new TypeError("Timing options cannot be combined with positional arguments");
            ({duration, curve: easing} = _timing(duration));
        }
        this._assertMutable();
        const sourceName = this._handle(source);
        const targetName = this._handle(target);
        if (sourceName === targetName) {
            throw new TypeError("FadeTransform source and target must be distinct handles");
        }
        SCENE_STATES.get(this).lines.push(
            (scene) => `${scene}:fade_transform(${sourceName},${targetName},${_lua(duration)},${_lua(easing)})`,
        );
        return this;
    }

    shift(target, by, duration = 1, easing = "smooth") {
        if (typeof duration === "object") {
            if (arguments.length > 3) throw new TypeError("Timing options cannot be combined with positional arguments");
            ({duration, curve: easing} = _timing(duration));
        }
        this._assertTransformable(target);
        return this._animate("shift", target, [by, duration, easing]);
    }

    // Serializes rotate, scale, and moveAlong options; an `about` handle names its local.
    matchTransform(source, target, duration = 1, easing = "smooth", options = {}) {
        if (typeof duration === "object") {
            if (arguments.length > 3) throw new TypeError("Timing options cannot be combined with positional arguments");
            ({duration, curve: easing, options} = _timing(duration, 1, true, false, MATCH_OPTIONS));
        }
        this._assertMutable();
        const sourceName = this._handle(source);
        const targetName = this._handle(target);
        if (sourceName === targetName) {
            throw new TypeError("MatchTransform source and target must be distinct handles");
        }
        if (!options || (Object.getPrototypeOf(options) !== Object.prototype && Object.getPrototypeOf(options) !== null)) {
            throw new TypeError("MatchTransform options must be a plain object");
        }
        const unknown = Object.keys(options).filter((key) => !MATCH_OPTIONS.includes(key));
        if (unknown.length) throw new TypeError(`Unknown MatchTransform option: ${unknown[0]}`);
        if (options.arc !== undefined && !(Number.isFinite(options.arc) && Math.abs(options.arc) < 2 * Math.PI)) {
            throw new RangeError("MatchTransform arc must be a finite angle between -2*pi and 2*pi radians");
        }
        if (options.lag !== undefined && !(Number.isFinite(options.lag) && options.lag >= 0)) {
            throw new RangeError("MatchTransform lag must be a finite number of zero or more");
        }
        if (options.strings !== undefined && typeof options.strings !== "boolean") {
            throw new TypeError("MatchTransform strings must be a boolean");
        }
        if (options.diff !== undefined && typeof options.diff !== "boolean") {
            throw new TypeError("MatchTransform diff must be a boolean");
        }
        if (options.shapes !== undefined && typeof options.shapes !== "boolean") {
            throw new TypeError("MatchTransform shapes must be a boolean");
        }
        if (options.fade !== undefined && !(Number.isFinite(options.fade) && options.fade > 0 && options.fade <= 1)) {
            throw new RangeError("MatchTransform fade must be a share of the duration greater than 0 and at most 1");
        }
        if (options.copy !== undefined && typeof options.copy !== "boolean") {
            throw new TypeError("MatchTransform copy must be a boolean");
        }
        if (options.path !== undefined && options.arc !== undefined && options.arc !== 0) {
            throw new TypeError("MatchTransform path and arc cannot be used together");
        }
        const entries = [];
        if (options.path !== undefined) entries.push(`["path"]=${this._handle(options.path)}`);
        if (options.arc !== undefined) entries.push(`["arc"]=${_lua(options.arc)}`);
        if (options.lag !== undefined) entries.push(`["lag"]=${_lua(options.lag)}`);
        if (options.pairs !== undefined) {
            const shape = "MatchTransform pairs must be an array of [sourcePiece, targetPiece]";
            if (!Array.isArray(options.pairs)) throw new TypeError(shape);
            const pairs = options.pairs.map((pair) => {
                if (!Array.isArray(pair) || pair.length !== 2) throw new TypeError(shape);
                return `{${this._handle(pair[0])},${this._handle(pair[1])}}`;
            });
            entries.push(`["pairs"]={${pairs.join(",")}}`);
        }
        if (options.strings !== undefined) entries.push(`["strings"]=${_lua(options.strings)}`);
        if (options.diff !== undefined) entries.push(`["diff"]=${_lua(options.diff)}`);
        if (options.shapes !== undefined) entries.push(`["shapes"]=${_lua(options.shapes)}`);
        if (options.fade !== undefined) entries.push(`["fade"]=${_lua(options.fade)}`);
        if (options.copy !== undefined) entries.push(`["copy"]=${_lua(options.copy)}`);
        const value = entries.length ? `,{${entries.join(",")}}` : "";
        const durationValue = _lua(duration);
        const easingValue = _lua(easing);
        SCENE_STATES.get(this).lines.push(
            (scene) => `${scene}:match_transform(${sourceName},${targetName},${durationValue},${easingValue}${value})`,
        );
        return this;
    }

    // Makes the code block that the text of block turns into after edits, one edit or an array
    // applied in order, and carries block into it as Lua edit does, returning the new block.
    // The builder applies the edits, so the script receives the edited text whole; the new
    // block keeps the look, place and parent of block and takes options.id or the id of block
    // with the first free .2, .3 and so on.
    edit(block, edits, duration = 1, easing = "smooth", options = {}) {
        if (typeof duration === "object") {
            if (arguments.length > 3) throw new TypeError("Timing options cannot be combined with positional arguments");
            ({duration, curve: easing, options} = _timing(duration, 1, true, false, EDIT_OPTIONS));
        }
        this._assertMutable();
        const blockName = this._handle(block);
        const code = CODE_BLOCKS.get(block);
        if (!code) throw new TypeError("Edit needs a block that code made");
        const list = Array.isArray(edits) ? edits : [edits];
        if (!list.length || list.length > 1024) throw new TypeError("Edit takes an edit or an array of 1 to 1024 edits");
        if (!options || (Object.getPrototypeOf(options) !== Object.prototype && Object.getPrototypeOf(options) !== null)) {
            throw new TypeError("Edit options must be a plain object");
        }
        const unknown = Object.keys(options).find((key) => !EDIT_OPTIONS.includes(key));
        if (unknown) throw new TypeError(`Unknown Edit option: ${unknown}`);
        if (options.id !== undefined && typeof options.id !== "string") throw new TypeError("Edit id must be a string");
        if (options.arc !== undefined && !(Number.isFinite(options.arc) && Math.abs(options.arc) < 2 * Math.PI)) {
            throw new RangeError("Edit arc must be a finite angle between -2*pi and 2*pi radians");
        }
        if (options.lag !== undefined && !(Number.isFinite(options.lag) && options.lag >= 0)) {
            throw new RangeError("Edit lag must be a finite number of zero or more");
        }
        if (options.fade !== undefined && !(Number.isFinite(options.fade) && options.fade > 0 && options.fade <= 1)) {
            throw new RangeError("Edit fade must be a share of the duration greater than 0 and at most 1");
        }
        let text = code.text;
        list.forEach((edit, index) => {
            text = _codeEdit(text, edit, code.language, list.length > 1 ? `Edit ${index + 1}: ` : "");
        });
        const state = SCENE_STATES.get(this);
        let id = options.id ?? null;
        if (id === null && code.id !== null) {
            const base = code.id.replace(/\.\d+$/, "");
            for (let number = 2; id === null; number++) {
                if (!state.ids.has(`${base}.${number}`)) id = `${base}.${number}`;
            }
        }
        const entries = id === null ? [] : [`["id"]=${_string(id)}`];
        for (const key of ["arc", "lag", "fade"]) {
            if (options[key] !== undefined) entries.push(`[${_string(key)}]=${_lua(options[key])}`);
        }
        // The script replaces every line, which keeps a line feed that ends the old text and drops
        // one that ends the value, so the value gains one when the old text has none; the block
        // records the text the script will hold, which splits into the same lines and tokens.
        const ended = code.text.endsWith("\n");
        const value = ended ? text : `${text}\n`;
        const held = ended && !text.endsWith("\n") ? `${text}\n` : text;
        const values = [
            blockName, `{{["replace"]={1,${_codeLineCount(code.text)}},["with"]=${_string(value)}}}`, _lua(duration),
            _lua(easing),
        ];
        if (entries.length) values.push(`{${entries.join(",")}}`);
        return this._codeBlock((scene) => `${scene}:edit(${values.join(",")})`, code.parent, id, held, code.language);
    }

    // Lays items, a group's children or an array of sibling handles in order, one after
    // another over duration, as Lua arrange does; an item added at the cursor grows in.
    arrange(items, duration = 1, easing = "smooth", options = {}) {
        if (typeof duration === "object") {
            if (arguments.length > 2) throw new TypeError("Timing options cannot be combined with positional arguments");
            ({duration, curve: easing, options} = _timing(duration, 1, false, false, ARRANGE_OPTIONS));
        }
        this._assertMutable();
        let itemsValue;
        if (items instanceof GroupHandle) {
            itemsValue = this._handle(items);
        } else if (Array.isArray(items) && items.length) {
            itemsValue = `{${items.map((item) => this._handle(item)).join(",")}}`;
        } else {
            throw new TypeError("Arrange takes a group or a non-empty array of handles");
        }
        if (!options || (Object.getPrototypeOf(options) !== Object.prototype && Object.getPrototypeOf(options) !== null)) {
            throw new TypeError("Arrange options must be a plain object");
        }
        const unknown = Object.keys(options).find((key) => !ARRANGE_OPTIONS.includes(key));
        if (unknown) throw new TypeError(`Unknown Arrange option: ${unknown}`);
        if (options.gap !== undefined && !(Number.isFinite(options.gap) && options.gap >= 0)) {
            throw new RangeError("Arrange gap must be a finite number of zero or more");
        }
        for (const key of ["direction", "center"]) {
            const value = options[key];
            if (value !== undefined && !(Array.isArray(value) && (value.length === 2 || value.length === 3)
                && value.every(Number.isFinite))) {
                throw new TypeError(`Arrange ${key} must be [x, y] or [x, y, z] of finite numbers`);
            }
        }
        const value = Object.keys(options).length ? `,${_lua(options)}` : "";
        const durationValue = _lua(duration);
        const easingValue = _lua(easing);
        SCENE_STATES.get(this).lines.push(
            (scene) => `${scene}:arrange(${itemsValue},${durationValue},${easingValue}${value})`,
        );
        return this;
    }

    // Brings every object below target that is not a group to full opacity inside the
    // selection and to dim outside it: children first..last by number, a handle, an array of
    // handles, or nothing to bring everything back. Options come last.
    highlight(target, ...selection) {
        this._assertMutable();
        const targetName = this._handle(target);
        const last = selection[selection.length - 1];
        const options = last && (Object.getPrototypeOf(last) === Object.prototype || Object.getPrototypeOf(last) === null)
            ? selection.pop()
            : {};
        const unknown = Object.keys(options).find((key) => !HIGHLIGHT_OPTIONS.includes(key));
        if (unknown) throw new TypeError(`Unknown Highlight option: ${unknown}`);
        if (options.dim !== undefined && !(Number.isFinite(options.dim) && options.dim >= 0 && options.dim <= 1)) {
            throw new RangeError("Highlight dim must be an opacity from 0 to 1");
        }
        if (options.duration !== undefined && !(Number.isFinite(options.duration) && options.duration > 0)) {
            throw new RangeError("Highlight duration must be a positive number of seconds");
        }
        if (options.easing !== undefined && options.curve !== undefined) {
            throw new TypeError("Highlight takes easing or curve, not both");
        }
        const shape = "Highlight selects children by number, a handle, or a non-empty array of handles";
        const picked = [];
        if (typeof selection[0] === "number") {
            if (selection.length > 2 || !selection.every(Number.isInteger)) {
                throw new TypeError("Highlight first and last must be whole child numbers");
            }
            const [first, end = first] = selection;
            if (first < 1 || end < first) {
                throw new RangeError("Highlight first and last count children from 1, first no later than last");
            }
            picked.push(_lua(first), _lua(end));
        } else if (selection.length === 1 && Array.isArray(selection[0])) {
            if (!selection[0].length) throw new TypeError(shape);
            picked.push(`{${selection[0].map((handle) => this._handle(handle)).join(",")}}`);
        } else if (selection.length === 1) {
            picked.push(this._handle(selection[0]));
        } else if (selection.length) {
            throw new TypeError(shape);
        }
        const values = [targetName, ...picked];
        if (Object.keys(options).length) values.push(_lua(options));
        SCENE_STATES.get(this).lines.push((scene) => `${scene}:highlight(${values.join(",")})`);
        return this;
    }

    _motionOptions(options, allowed, name) {
        if (!options || (Object.getPrototypeOf(options) !== Object.prototype && Object.getPrototypeOf(options) !== null)) {
            throw new TypeError(`${name} options must be a plain object`);
        }
        const unknown = Object.keys(options).filter((key) => !allowed.includes(key));
        if (unknown.length) throw new TypeError(`Unknown ${name} option: ${unknown[0]}`);
        _timingValues(options, 1, false, name);
        if (options.arc !== undefined && !(Number.isFinite(options.arc) && Math.abs(options.arc) < 2 * Math.PI)) {
            throw new RangeError(`${name} arc must be a finite angle between -2*pi and 2*pi radians`);
        }
        for (const key of ["from", "to"]) {
            if (options[key] !== undefined && !(Number.isFinite(options[key]) && options[key] >= 0 && options[key] <= 1)) {
                throw new RangeError(`${name} ${key} must be a fraction of the path length between 0 and 1`);
            }
        }
        if (options.orient !== undefined && typeof options.orient !== "boolean") {
            throw new TypeError(`${name} orient must be a boolean`);
        }
        const entries = [];
        for (const key of Object.keys(options).sort()) {
            const value = options[key];
            if (value === undefined) continue;
            if (key === "about" && value instanceof Handle) {
                entries.push(`["about"]=${this._handle(value)}`);
                continue;
            }
            if (key === "about" && typeof value === "string" && !PIVOT_ANCHORS.has(value)) {
                throw new TypeError(`${name} about must be an anchor name, a point, or a handle`);
            }
            entries.push(`[${_string(key)}]=${_lua(value)}`);
        }
        return `{${entries.join(",")}}`;
    }

    rotate(target, radians, options = {}) {
        this._assertMutable();
        this._assertTransformable(target);
        if (!Number.isFinite(radians)) throw new TypeError("Rotate angle must be a finite number of radians");
        const targetName = this._target(target);
        const angle = _lua(radians);
        const value = this._motionOptions(options, ROTATE_OPTIONS, "Rotate");
        SCENE_STATES.get(this).lines.push((scene) => `${scene}:rotate(${targetName},${angle},${value})`);
        return this;
    }

    scale(target, factor, options = {}) {
        this._assertMutable();
        this._assertTransformable(target);
        if (typeof factor === "number" ? !Number.isFinite(factor) : !factor || typeof factor !== "object") {
            throw new TypeError("Scale factor must be a finite number or a vector");
        }
        const targetName = this._target(target);
        const factorValue = _lua(factor);
        const value = this._motionOptions(options, SCALE_OPTIONS, "Scale");
        SCENE_STATES.get(this).lines.push((scene) => `${scene}:scale(${targetName},${factorValue},${value})`);
        return this;
    }

    // Connector paths follow their sampled ends; ordinary paths keep a scheduled copy.
    moveAlong(target, path, options = {}) {
        this._assertMutable();
        this._assertTransformable(target);
        const targetName = this._target(target);
        const pathName = this._handle(path);
        const value = this._motionOptions(options, MOVE_ALONG_OPTIONS, "MoveAlong");
        SCENE_STATES.get(this).lines.push((scene) => `${scene}:move_along(${targetName},${pathName},${value})`);
        return this;
    }

    // Sample the plane map in target-parent space and emit its images; z stays unchanged.
    // The grid must cover every target point. Native spline interpolation preserves quadratic maps.
    warp(target, fn, duration = 1, easing = "smooth", options = undefined) {
        this._assertMutable();
        if (duration !== null && typeof duration === "object") {
            if (arguments.length > 3) {
                throw new TypeError("Warp takes (target, fn, options) or (target, fn, duration, curve, options)");
            }
            ({duration, curve: easing, options} = _timing(duration, 1, false, false, WARP_OPTIONS, "Warp"));
        }
        const targetName = this._target(target);
        if (typeof fn !== "function") throw new TypeError("Warp fn must be a function of x and y that returns [x, y]");
        if (!options || (Object.getPrototypeOf(options) !== Object.prototype && Object.getPrototypeOf(options) !== null)) {
            throw new TypeError("Warp requires options with x_range and y_range, the samples the builder calls fn at");
        }
        const unknown = Object.keys(options).filter((key) => !WARP_OPTIONS.includes(key));
        if (unknown.length) throw new TypeError(`Unknown Warp option: ${unknown[0]}`);
        if (options.x_range === undefined) throw new TypeError("Warp requires x_range");
        if (options.y_range === undefined) throw new TypeError("Warp requires y_range");
        const columns = _warpSamples(options.x_range, "Warp x_range");
        const rows = _warpSamples(options.y_range, "Warp y_range");
        if (columns.samples.length * rows.samples.length > WARP_SAMPLE_LIMIT) {
            throw new RangeError(
                `Warp samples hold at most ${WARP_SAMPLE_LIMIT} images; widen the steps of x_range and y_range`,
            );
        }
        if (options.step !== undefined && !(Number.isFinite(options.step) && options.step > 0)) {
            throw new RangeError("Warp step must be a positive length");
        }
        const timing = `${_lua(duration)},${_lua(easing)}`;
        const state = SCENE_STATES.get(this);
        const values = [];
        state.sampleActive = true;
        try {
            for (const y of rows.samples) {
                for (const x of columns.samples) values.push(_sampleVector(fn, x, y, "Warp fn").map(_single));
            }
        } finally {
            state.sampleActive = false;
        }
        const samples = `{["values"]=${_lua(values)},["x_range"]=${_lua(columns.range)},["y_range"]=${_lua(rows.range)}}`;
        const settings = options.step === undefined ? "" : `,{["step"]=${_lua(options.step)}}`;
        state.lines.push((scene) => `${scene}:warp(${targetName},${samples},${timing}${settings})`);
        return this;
    }

    transform(target, matrix, duration = 1, easing = "smooth") {
        if (typeof duration === "object") {
            if (arguments.length > 3) throw new TypeError("Timing options cannot be combined with positional arguments");
            ({duration, curve: easing} = _timing(duration));
        }
        this._assertTransformable(target);
        return this._animate("transform", target, [matrix, duration, easing]);
    }

    fade(target, opacity, duration = 1, easing = "smooth") {
        if (typeof duration === "object") {
            if (arguments.length > 3) throw new TypeError("Timing options cannot be combined with positional arguments");
            ({duration, curve: easing} = _timing(duration));
        }
        return this._animate("fade", target, [opacity, duration, easing]);
    }

    transition(from, to, duration = 1, easing = "smooth") {
        if (typeof duration === "object") {
            if (arguments.length > 3) throw new TypeError("Timing options cannot be combined with positional arguments");
            ({duration, curve: easing} = _timing(duration));
        }
        this._assertMutable();
        const fromName = this._target(from);
        const toName = this._target(to);
        const durationValue = _lua(duration);
        const easingValue = _lua(easing);
        SCENE_STATES.get(this).lines.push(
            (scene) => `${scene}:transition(${fromName},${toName},${durationValue},${easingValue})`,
        );
        return this;
    }

    stroke(target, color, duration = 1, easing = "smooth") {
        if (typeof duration === "object") {
            if (arguments.length > 3) throw new TypeError("Timing options cannot be combined with positional arguments");
            ({duration, curve: easing} = _timing(duration));
        }
        return this._animate("stroke", target, [color, duration, easing]);
    }

    fill(target, color, duration = 1, easing = "smooth") {
        if (typeof duration === "object") {
            if (arguments.length > 3) throw new TypeError("Timing options cannot be combined with positional arguments");
            ({duration, curve: easing} = _timing(duration));
        }
        return this._animate("fill", target, [color, duration, easing]);
    }

    play(specification, duration = 1, easing = "smooth", lag = 0) {
        if (typeof duration === "object") {
            if (arguments.length > 2) throw new TypeError("Timing options cannot be combined with positional arguments");
            ({duration, curve: easing, lag} = _timing(duration, 1, true));
        }
        const timing = SCENE_TRACKERS.get(this);
        const { anchor, time } = timing.clock;
        this._assertMutable();
        const descriptors = this._descriptors(specification, "play");
        const durationValue = _lua(duration);
        const easingValue = _lua(easing);
        const lagValue = _lua(lag);
        SCENE_STATES.get(this).lines.push(
            (scene) => `${scene}:play({${descriptors.join(",")}},${durationValue},${easingValue},${lagValue})`,
        );
        // A play lasts its duration and the lag of each later target, as the scene counts it.
        if (Number.isFinite(duration) && duration > 0 && Number.isFinite(lag) && lag >= 0) {
            const span = _decimal(duration) * (1 + _decimal(lag) * (descriptors.length - 1));
            timing.clock = { anchor, time: Math.fround(_decimal(time) + span) };
        }
        return this;
    }

    set(specification) {
        this._assertMutable("instant");
        return this._set(specification);
    }

    _set(specification) {
        const descriptors = this._descriptors(specification, "set");
        SCENE_STATES.get(this).lines.push((scene) => `${scene}:set({${descriptors.join(",")}})`);
        return this;
    }

    // Records the state each target shows now, with the objects below it, on a stack of its own.
    save(target) {
        this._assertMutable("instant");
        const targetName = this._target(target);
        SCENE_STATES.get(this).lines.push((scene) => `${scene}:save(${targetName})`);
        return this;
    }

    // Brings each target back to the newest state save recorded for it: over duration seconds,
    // each later target lag of the duration later, or at once without a duration.
    restore(target, duration = 0, easing = "smooth", lag = 0) {
        if (typeof duration === "object") {
            if (arguments.length > 2) throw new TypeError("Timing options cannot be combined with positional arguments");
            ({duration, curve: easing, lag} = _timing(duration, 0, true, true));
        }
        this._assertMutable();
        const targetName = this._target(target);
        if (!(Number.isFinite(duration) && duration >= 0)) {
            throw new RangeError("Restore duration must be zero or a positive number of seconds");
        }
        if (!(Number.isFinite(lag) && lag >= 0)) throw new RangeError("Restore lag must be zero or positive");
        const durationValue = _lua(duration);
        const easingValue = _lua(easing);
        const lagValue = _lua(lag);
        SCENE_STATES.get(this).lines.push(
            (scene) => `${scene}:restore(${targetName},${durationValue},${easingValue},${lagValue})`,
        );
        return this;
    }

    // A number for objects to follow, starting at value.
    tracker(value) {
        if (typeof value !== "number" || !Number.isFinite(value)) {
            throw new TypeError("Tracker value must be a finite number");
        }
        const tracker = Object.freeze(new Tracker());
        TRACKERS.set(tracker, { owner: this, value, windows: [] });
        return tracker;
    }

    _trackerEntry(tracker) {
        const entry = TRACKERS.get(tracker);
        if (!entry) throw new TypeError("Expected a tracker from scene.tracker");
        if (entry.owner !== this) throw new TypeError("Tracker belongs to another scene");
        return entry;
    }

    // Makes target follow the trackers through fn, which maps their values to a state, and
    // applies the state they give now as set does.
    bind(target, trackers, fn) {
        this._assertQuiet();
        if (typeof fn !== "function") throw new TypeError("Bind requires a function");
        const list = TRACKERS.has(trackers) ? [trackers] : trackers;
        if (!Array.isArray(list) || !list.length) {
            throw new TypeError("Bind requires a tracker or an array of trackers");
        }
        for (const tracker of list) this._trackerEntry(tracker);
        const tracks = SCENE_TRACKERS.get(this);
        if (target instanceof Handle && target.owner === this && tracks.bound.has(target.name)) {
            throw new TypeError("An object can follow only one binding");
        }
        const binding = { target, trackers: [...list], fn, shift: [0, 0, 0], windows: [] };
        this._set(_boundDescriptor(binding, _boundState(binding)));
        tracks.bound.add(target.name);
        tracks.bindings.push(binding);
        return target;
    }

    // Moves trackers to their values over duration seconds, eased by curve: the objects bound to
    // them play one linear segment a frame through the states their functions give.
    // Takes (tracker, value, duration, curve) or ([[tracker, value], ...], duration, curve). The
    // trackers move and the functions run at once, but the segments wait for the next call that
    // adds or reads clips, so a later track of other trackers that overlaps this one on a binding
    // merges with it there, as in a Lua scene.
    track(first, ...rest) {
        this._assertQuiet();
        let moves;
        let duration;
        let curve;
        if (TRACKERS.has(first)) {
            moves = [[first, rest[0]]];
            [, duration, curve] = rest;
        } else {
            if (!Array.isArray(first) || !first.length) {
                throw new TypeError("Track requires a tracker or an array of [tracker, value]");
            }
            moves = first;
            [duration, curve] = rest;
        }
        if (typeof duration === "object") {
            if (curve !== undefined) throw new TypeError("Timing options cannot be combined with positional arguments");
            ({duration, curve} = _timing(duration));
        }
        if (typeof duration !== "number" || !(duration > 0) || duration === Infinity) {
            throw new RangeError("Track duration must be a positive finite number");
        }
        const moving = moves.map((move) => {
            const [tracker, target] = Array.isArray(move) ? move : [];
            const entry = this._trackerEntry(tracker);
            if (typeof target !== "number" || !Number.isFinite(target)) {
                throw new TypeError("Track target must be a finite number");
            }
            return { tracker, entry, from: entry.value, to: target, values: [entry.value] };
        });
        const ease = _curveEase(curve);
        const timing = SCENE_TRACKERS.get(this);
        const affected = timing.bindings.filter((binding) =>
            binding.trackers.some((tracker) => moving.some((move) => move.tracker === tracker)),
        );
        const steps = Math.max(1, Math.ceil(duration * timing.fps - 1e-6));
        if (affected.length && steps * affected.length > TRACKER_CLIP_LIMIT) {
            throw new RangeError("Scene animation limit exceeded: track needs one sample per binding per frame within the 65536-clip budget");
        }
        if (!affected.length) {
            for (const move of moving) move.entry.value = move.to;
            return this.wait(duration);
        }
        const { anchor, time: start } = timing.clock;
        const finish = start + duration;
        for (const move of moving) {
            for (const window of move.entry.windows) {
                if (
                    window.anchor === anchor &&
                    _overlaps(start, finish, window) &&
                    window.affected.some((binding) => affected.includes(binding))
                ) {
                    throw new TypeError(
                        "This track and an earlier one move the same tracker at once; " +
                            "a tracker has one value at a time",
                    );
                }
            }
        }
        for (const binding of affected) {
            if (binding.windows.some((window) => window.anchor === anchor && _overlaps(start, finish, window))) {
                throw new TypeError(
                    "This track overlaps a track whose segments have already played on the same object; " +
                        "author tracks that overlap with only wait, at, parallel, mark, bind, tracker and " +
                        "factories between them",
                );
            }
        }
        // The waiting tracks this one overlaps on each binding, whose samples it merges with there.
        const overlapping = new Map();
        for (const binding of affected) {
            const others = timing.pending.filter(
                (other) =>
                    other.anchor === anchor && _overlaps(start, finish, other) && other.affected.includes(binding),
            );
            if (others.length) overlapping.set(binding, others);
        }
        const snapshot = new Map();
        for (const binding of affected) {
            for (const tracker of binding.trackers) snapshot.set(tracker, TRACKERS.get(tracker).value);
        }
        const bounds = [start];
        const states = [];
        try {
            for (let step = 1; step <= steps; step++) {
                const u = ease(step / steps);
                for (const move of moving) {
                    move.entry.value = step === steps ? move.to : move.from + (move.to - move.from) * u;
                    move.values[step] = move.entry.value;
                }
                // A binding that merges samples when its tracks play, at the times of them all.
                const row = [];
                const checks = [];
                affected.forEach((binding, index) => {
                    if (overlapping.has(binding)) return;
                    row[index] = _boundState(binding);
                    checks.push(_boundDescriptor({ target: binding.target, shift: [0, 0, 0] }, row[index]));
                });
                if (checks.length) this._descriptors(checks, "play");
                states.push(row);
                bounds.push(Math.fround(start + duration * (step / steps)));
            }
        } catch (error) {
            for (const move of moving) move.entry.value = move.from;
            throw error;
        }
        const track = {
            order: ++timing.authored, anchor, start, finish: bounds[steps], steps, duration, ease, bounds,
            moves: moving, affected, states, snapshot, groups: new Map(), slot: 0,
            declares: false,
        };
        for (const [binding, others] of overlapping) {
            let group = null;
            for (const other of others) {
                let joined = other.groups.get(binding);
                if (!joined) {
                    joined = { binding, tracks: [other] };
                    other.groups.set(binding, joined);
                }
                if (!group) {
                    group = joined;
                } else if (joined !== group) {
                    for (const member of joined.tracks) {
                        group.tracks.push(member);
                        member.groups.set(binding, group);
                    }
                }
            }
            group.tracks.push(track);
            track.groups.set(binding, group);
        }
        for (const move of moving) move.entry.windows.push({ anchor, start, finish: track.finish, affected });
        timing.clock = { anchor, time: track.finish };
        const lines = SCENE_STATES.get(this).lines;
        for (let step = 1; step <= steps; step++) lines.push((name) => _trackStep(name, track, step));
        timing.pending.push(track);
        timing.waiting += steps * affected.length;
        if (timing.waiting > TRACKER_PENDING_LIMIT) _flushTracks(this);
        return this;
    }

    _descriptors(specification, verb) {
        const specifications = Array.isArray(specification) ? specification : [specification];
        if (!specifications.length) throw new TypeError(`Animation ${verb} requires at least one descriptor`);

        const targets = new Set();
        const counts = [];
        const cuts = [];
        const descriptors = specifications.map((descriptor) => {
            if (
                !descriptor ||
                (Object.getPrototypeOf(descriptor) !== Object.prototype && Object.getPrototypeOf(descriptor) !== null)
            ) {
                throw new TypeError("Animation descriptors must be plain objects");
            }
            const unknown = Object.keys(descriptor).filter((key) => key !== "target" && !PLAY_PROPERTIES.includes(key));
            if (unknown.length) throw new TypeError(`Unknown animation property: ${unknown[0]}`);
            const targetName = this._handle(descriptor.target);
            if (targets.has(targetName)) throw new TypeError(`Animation ${verb} cannot contain duplicate targets`);
            targets.add(targetName);

            const properties = PLAY_PROPERTIES.filter((key) => Object.hasOwn(descriptor, key));
            if (!properties.length) throw new TypeError("Animation descriptors require at least one property");
            if (Object.hasOwn(descriptor, "shift") && Object.hasOwn(descriptor, "transform")) {
                throw new TypeError("Animation descriptors cannot combine shift and transform");
            }
            if (
                Object.hasOwn(descriptor, "arc") &&
                !Object.hasOwn(descriptor, "shift") &&
                !Object.hasOwn(descriptor, "transform")
            ) {
                throw new TypeError("Animation descriptor arc requires shift or transform");
            }
            if (verb === "set" && Object.hasOwn(descriptor, "arc")) {
                throw new TypeError("Set applies values at once, so its descriptors cannot use arc");
            }
            if (
                (Object.hasOwn(descriptor, "shift") || Object.hasOwn(descriptor, "transform")) &&
                HANDLE_TYPES.get(descriptor.target) === "connector"
            ) {
                throw new TypeError("Connector geometry follows its endpoints and cannot be shifted or transformed");
            }
            if (Object.hasOwn(descriptor, "text")) {
                const type = HANDLE_TYPES.get(descriptor.target);
                if (type === "group") {
                    throw new TypeError(
                        "Animation text changes one text, and a group or spans row holds several; target one of its texts",
                    );
                }
                if (type === "number") throw new TypeError("Animation text cannot change a number; animate its value");
                if (type !== "text" && type !== "label") throw new TypeError("Animation text needs a text or label");
                if (typeof descriptor.text !== "string") throw new TypeError("Animation text must be a string");
            }
            if (Object.hasOwn(descriptor, "retype")) {
                if (!Object.hasOwn(descriptor, "text")) throw new TypeError("Animation retype needs text");
                if (typeof descriptor.retype !== "boolean") throw new TypeError("Animation retype must be a boolean");
            }
            if (Object.hasOwn(descriptor, "layer")) {
                if (!Number.isInteger(descriptor.layer)) throw new TypeError("Animation layer must be an integer");
                if (descriptor.layer < -(2 ** 31) || descriptor.layer >= 2 ** 31) {
                    throw new RangeError("Animation layer must fit in 32 bits");
                }
            }
            if (typeof descriptor.gradient === "boolean") {
                throw new TypeError("Animation gradient must be an end-stop color or gradient options");
            }
            const graph = this._geometry(descriptor, counts, cuts);
            const values = properties.flatMap((key) => {
                if (graph?.has(key)) return graph.get(key) ? [graph.get(key)] : [];
                const value = key === "gradient" ? _gradient(descriptor[key]) : descriptor[key];
                return [`${key === "trimDistance" ? "trim_distance" : key}=${_lua(value)}`];
            });
            return `{target=${targetName},${values.join(",")}}`;
        });
        for (const [target, count] of counts) POINT_COUNTS.set(target, count);
        for (const [target, cut] of cuts) PLOT_CUTS.set(target, cut);
        return descriptors;
    }

    // Checks the points of a shape a descriptor moves: each key must suit the kind of shape, and
    // points must keep the count the shape has, except on a plot, which takes a new graph (see
    // _replot). A count the builder learns here goes into counts, and a plot's cut into cuts,
    // which the caller keeps once every descriptor of the call is valid. Returns what _replot
    // returns for a plot, or null.
    _geometry(descriptor, counts, cuts) {
        const type = HANDLE_TYPES.get(descriptor.target);
        for (const [key, [types, needs]] of GEOMETRY_KEYS) {
            if (!Object.hasOwn(descriptor, key)) continue;
            if ((OBJECTS.includes(type) || type === "group" || type === "connector") && !types.includes(type)) {
                throw new TypeError(`Animation ${key} needs ${needs}`);
            }
        }
        if (type === "plot") return this._replot(descriptor, cuts);
        const key = PLOT_STATE_KEYS.find((name) => Object.hasOwn(descriptor, name));
        if (key !== undefined) throw new TypeError(`Animation ${key} needs a plot`);
        if (!Object.hasOwn(descriptor, "points")) return null;
        const points = descriptor.points;
        if (!Array.isArray(points)) throw new TypeError("Animation points must be an array of points");
        const count = POINT_COUNTS.get(descriptor.target);
        if (count === undefined) {
            counts.push([descriptor.target, points.length]);
        } else if (points.length !== count) {
            throw new RangeError(
                `Animation points must have ${count} points, as the ${type} has, not ${points.length}; ` +
                    "a shape keeps its point count",
            );
        }
        return null;
    }

    // Re-traces a contour child with its grid and level, emitting sampled scalars for Lua.
    _recontour(descriptor, cuts, kept) {
        const has = (key) => Object.hasOwn(descriptor, key);
        if (has("points") || has("discontinuities")) {
            throw new TypeError("A contour state takes fn or values, not points or discontinuities");
        }
        if (!has("fn") && !has("values")) {
            const key = ["level", "x_range", "y_range"].find(has);
            if (key) throw new TypeError(`Contour ${key} needs fn or values`);
            return null;
        }
        const xRange = has("x_range") ? descriptor.x_range : kept.xRange;
        const yRange = has("y_range") ? descriptor.y_range : kept.yRange;
        const level = has("level") ? descriptor.level : kept.level;
        if (!Number.isFinite(level) || Math.abs(level) > SINGLE_MAX) {
            throw new RangeError("Contour level must be a finite number");
        }
        const { samples, xRange: gridX, yRange: gridY } = this._contourSamples(descriptor.fn, descriptor.values, xRange, yRange);
        const values = `values=${_lua(samples)}`;
        const graph = new Map([["fn", values], ["values", values]]);
        cuts.push([descriptor.target, { contour: true, xRange: gridX, yRange: gridY, level }]);
        return graph;
    }

    // Materialize a plot descriptor using supplied or retained cuts. Emit endpoint and column
    // samples for functions, and record updated cuts. Null means no new geometry.
    _replot(descriptor, cuts) {
        const contour = PLOT_CUTS.get(descriptor.target);
        if (contour?.contour) return this._recontour(descriptor, cuts, contour);
        if (Object.hasOwn(descriptor, "level") || Object.hasOwn(descriptor, "values")) {
            throw new TypeError("Animation level and values need a contour child");
        }
        const has = (key) => Object.hasOwn(descriptor, key);
        if (has("x_range") && !has("fn")) throw new TypeError("Animation x_range needs fn");
        if (!has("fn") && !has("points")) {
            const key = ["y_range", "discontinuities"].find(has);
            if (key !== undefined) throw new TypeError(`Animation ${key} needs fn or points`);
            return null;
        }
        if (has("fn") && has("points")) throw new TypeError("Animation of a plot takes either points or fn, not both");
        const kept = PLOT_CUTS.get(descriptor.target) ?? { range: null, band: null, breaks: null };
        const band = has("y_range") ? _plotBand(descriptor.y_range) : kept.band;
        let breaks = kept.breaks;
        if (has("discontinuities")) {
            const list = descriptor.discontinuities;
            if (!Array.isArray(list) || !list.every(Number.isFinite)) {
                throw new TypeError("Plot discontinuities must be an array of finite numbers");
            }
            breaks = [...list].sort((a, b) => a - b);
        }
        let range = kept.range;
        let line;
        let stored = null;
        if (has("fn")) {
            if (typeof descriptor.fn !== "function") throw new TypeError("Animation fn must be a function of x");
            if (has("x_range")) range = _spaceRange(descriptor.x_range, "Plot x_range");
            else if (!range) throw new TypeError("Plot fn requires x_range");
            const grid = _plotGrid(range);
            const state = SCENE_STATES.get(this);
            let pieces;
            const samples = [];
            state.sampleActive = true;
            try {
                pieces = _plotPieces(descriptor.fn, grid, range[2], band, breaks ?? [], samples);
            } finally {
                state.sampleActive = false;
            }
            line = _plotLine(pieces);
            stored = _plotStored(pieces, samples);
        } else {
            if (!Array.isArray(descriptor.points)) throw new TypeError("Animation points must be an array of points");
            line = `{${descriptor.points.map(_plotPoint).join(",")}}`;
        }
        cuts.push([descriptor.target, { range, band, breaks }]);
        const graph = new Map([["fn", `fn=${stored}`], ["points", `points=${line}`],
            ["x_range", range ? `x_range=${_lua(range)}` : ""]]);
        if (has("y_range")) graph.set("y_range", `y_range=${_lua(band)}`);
        return graph;
    }

    // Moves the camera; a target that is a handle or an array of handles aims it at the center of
    // their authored bounds, as focus frames them, and the eye keeps its offset.
    look(camera, duration = 1, easing = "smooth") {
        if (typeof duration === "object" && arguments.length > 2) throw new TypeError("Timing options cannot be combined with positional arguments");
        return this._look(null, camera, duration, easing);
    }

    // The camera calls below move the scene's camera, or with a receiver the view of that name.
    _look(receiver, camera, duration, easing) {
        if (typeof duration === "object") ({duration, curve: easing} = _timing(duration));
        this._assertMutable();
        const cameraValue = this._camera(camera);
        const durationValue = _lua(duration);
        const easingValue = _lua(easing);
        SCENE_STATES.get(this).lines.push(
            (scene) => `${receiver ?? scene}:look(${cameraValue},${durationValue},${easingValue})`,
        );
        return this;
    }

    // Circles a 3D camera about its target by any angle, about the camera's up by default.
    orbit(radians, options = {}) {
        return this._orbit(null, radians, options);
    }

    _orbit(receiver, radians, options) {
        this._assertMutable();
        if (!Number.isFinite(radians) || radians === 0) {
            throw new TypeError("Orbit angle must be a finite, non-zero number of radians");
        }
        const angle = _lua(radians);
        const value = this._motionOptions(options, ORBIT_OPTIONS, "Orbit");
        SCENE_STATES.get(this).lines.push((scene) => `${receiver ?? scene}:orbit(${angle},${value})`);
        return this;
    }

    // Pans and zooms a 2D camera so the authored bounds of a handle or handles fill the frame,
    // or returns to the opening camera without one, from either view.
    focus(target = null, options = {}) {
        return this._focus(null, target, options);
    }

    _focus(receiver, target, options) {
        this._assertMutable();
        let targetName = "nil";
        if (Array.isArray(target)) {
            if (!target.length) throw new TypeError("Focus targets must be a handle or a non-empty array of handles");
            targetName = `{${target.map((item) => this._handle(item)).join(",")}}`;
        } else if (target !== null) {
            targetName = this._handle(target);
        }
        const { padding } = options && typeof options === "object" ? options : {};
        if (padding !== undefined && !(Number.isFinite(padding) && padding >= 0)) {
            throw new RangeError("Focus padding must be a non-negative number");
        }
        const value = this._motionOptions(options, FOCUS_OPTIONS, "Focus");
        SCENE_STATES.get(this).lines.push((scene) => `${receiver ?? scene}:focus(${targetName},${value})`);
        return this;
    }

    wait(duration = 1) {
        this._assertQuiet();
        const value = _lua(duration);
        SCENE_STATES.get(this).lines.push((scene) => `${scene}:wait(${value})`);
        _advance(this, duration);
        return this;
    }

    remove(target) {
        this._assertMutable("instant");
        const targetName = this._target(target);
        SCENE_STATES.get(this).lines.push((scene) => `${scene}:remove(${targetName})`);
        return this;
    }

    // Authors the callback from an absolute time or marker, then returns to the current time.
    at(time, callback) {
        this._assertQuiet();
        if (typeof callback !== "function") throw new TypeError("Scene at requires a callback");
        if (typeof time !== "string" && !(Number.isFinite(time) && time >= 0)) {
            throw new TypeError("Scene at requires a non-negative time or a marker name");
        }
        const state = SCENE_STATES.get(this);
        const timing = SCENE_TRACKERS.get(this);
        const resume = `times[${++state.timelines}]`;
        const timeValue = _lua(time);
        state.lines.push((scene) => `${resume}=${scene}:time();${scene}:seek(${timeValue})`);
        const back = timing.clock;
        // A marker keeps the clock it was set at; a number counts from the start of the scene.
        timing.clock = typeof time === "string"
            ? timing.markers.get(time) ?? { anchor: {}, time: 0 }
            : { anchor: timing.origin, time: Math.fround(time) };
        callback(this);
        state.lines.push((scene) => `${scene}:seek(${resume})`);
        timing.clock = back;
        return this;
    }

    // Starts every track at the current time and continues after the longest one.
    parallel(...tracks) {
        this._assertQuiet();
        const list = tracks.length === 1 && Array.isArray(tracks[0]) ? tracks[0] : tracks;
        if (!list.length) throw new TypeError("Scene parallel requires at least one track");
        if (list.some((track) => typeof track !== "function")) {
            throw new TypeError("Scene parallel tracks must be functions");
        }
        const state = SCENE_STATES.get(this);
        const timing = SCENE_TRACKERS.get(this);
        const start = `times[${++state.timelines}]`;
        const end = `times[${++state.timelines}]`;
        state.lines.push((scene) => `${start}=${scene}:time();${end}=${start}`);
        // Every branch starts at the clock of the parallel, which ends at the latest of theirs.
        const begin = timing.clock;
        let reached = null;
        for (const track of list) {
            state.lines.push((scene) => `${scene}:seek(${start})`);
            timing.clock = begin;
            track(this);
            const clock = timing.clock;
            if (!reached) {
                reached = clock;
            } else if (reached.anchor === clock.anchor) {
                reached = { anchor: clock.anchor, time: Math.max(reached.time, clock.time) };
            } else {
                reached = { anchor: {}, time: 0 };
            }
            state.lines.push((scene) => `${end}=math.max(${end},${scene}:time())`);
        }
        state.lines.push((scene) => `${scene}:seek(${end})`);
        timing.clock = reached;
        return this;
    }

    mark(name) {
        this._assertQuiet();
        if (typeof name !== "string" || !name) throw new TypeError("Marker names must be non-empty strings");
        const value = _lua(name);
        SCENE_STATES.get(this).lines.push((scene) => `${scene}:mark(${value})`);
        const timing = SCENE_TRACKERS.get(this);
        if (!timing.markers.has(name)) timing.markers.set(name, timing.clock);
        return this;
    }

    _compile(scene, depth) {
        return _compile(SCENE_STATES.get(this), scene, depth);
    }

    compile() {
        this._assertMutable("instant");
        return [...this._compile("scene", 0), "return scene", ""].join("\n");
    }
}

for (const method of FACTORIES) {
    SceneBuilder.prototype[method] = function (options = {}) {
        return this._object(method, options);
    };
}

// Lua's math.random draws from xoshiro256** over 64-bit integers, and math.randomseed(x, y) seeds
// it with {x, 0xff, y, 0} and throws the first 16 numbers away; this is the same generator, so a
// seed gives the numbers a Lua scene draws after math.randomseed with the same seed.
const RANDOM_BITS = (1n << 64n) - 1n;

function _rotateLeft(value, count) {
    return ((value << count) | (value >> (64n - count))) & RANDOM_BITS;
}

function _randomInteger(value, name) {
    if (!Number.isSafeInteger(value)) throw new RangeError(`${name} must be a safe integer`);
    return BigInt(value);
}

function _random(seed, seed2 = 0) {
    if (seed === undefined) throw new RangeError("vis.random needs a seed, such as vis.random(42)");
    const state = [
        BigInt.asUintN(64, _randomInteger(seed, "Random seed")),
        0xffn,
        BigInt.asUintN(64, _randomInteger(seed2, "Random seed")),
        0n,
    ];
    const next = () => {
        const [first, second] = state;
        const third = state[2] ^ first;
        const fourth = state[3] ^ second;
        const result = (_rotateLeft((second * 5n) & RANDOM_BITS, 7n) * 9n) & RANDOM_BITS;
        state[0] = first ^ fourth;
        state[1] = second ^ third;
        state[2] = (third ^ (second << 17n)) & RANDOM_BITS;
        state[3] = _rotateLeft(fourth, 45n);
        return result;
    };
    for (let i = 0; i < 16; i++) next();
    // A number from 0 to span: the low bits of a draw under the next power of two, drawing again
    // while they pass span, as Lua projects into a range.
    const project = (value, span) => {
        if ((span & ((span + 1n) & RANDOM_BITS)) === 0n) return value & span;
        let mask = span;
        for (const shift of [1n, 2n, 4n, 8n, 16n, 32n]) mask |= mask >> shift;
        let projected = value & mask;
        while (projected > span) projected = next() & mask;
        return projected;
    };
    return Object.freeze(function random(...bounds) {
        if (bounds.length > 2) throw new TypeError("random takes at most a lower and an upper bound");
        if (bounds.length === 0) return Number(next() >> 11n) * 2 ** -53;
        if (bounds.length === 1 && bounds[0] === 0) {
            throw new RangeError("random(0) draws a 64-bit integer, which a JavaScript number cannot hold");
        }
        const low = bounds.length === 2 ? _randomInteger(bounds[0], "Random lower bound") : 1n;
        const high = _randomInteger(bounds[bounds.length - 1], "Random upper bound");
        if (low > high) throw new RangeError("Random interval is empty");
        return Number(low + project(next(), high - low));
    });
}

export const vis = Object.freeze({
    springResponse: _springResponse,
    animCurve: Object.freeze({
        preset: _curvePreset,
        cubicBezier: _cubicBezier,
        reverse: _reverseCurve,
    }),
    random: _random,
    scene(config = {}) {
        return new SceneBuilder(config);
    },
});

export function compileScene(scene) {
    if (!(scene instanceof SceneBuilder)) throw new TypeError("Expected a viscript JS scene");
    return scene.compile();
}

export function evaluateScene(source, name = "scene.js") {
    if (typeof source !== "string" || !source) throw new TypeError("JavaScript source must be a non-empty string");
    const sourceName = String(name).replace(/[^a-zA-Z0-9._/-]/g, "_");
    const scene = Function("vis", `"use strict";\n${source}\n//# sourceURL=${sourceName}`)(vis);
    if (!(scene instanceof SceneBuilder)) throw new TypeError(`${name} must return a viscript scene`);
    if (SCENE_OWNERS.has(scene)) throw new TypeError(`${name} must return an owning root viscript scene`);
    return scene;
}
