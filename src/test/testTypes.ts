import {
    createVis,
    vis,
    type AnimCurve,
    type AnimationOptions,
    type TimingOptions,
    type AudioCueHandle,
    type AnimationSpec,
    type BBox,
    type GroupHandle,
    type Mat4,
    type LayoutVisualKind,
    type LayoutPathInfo,
    type ObjectHandle,
    type RandomGenerator,
    type RectangleOptions,
    type SceneTransitionOptions,
    type SpringResponseOptions,
    type TexHandle,
    type ThemeOptions,
    type VisInputSample,
    type VisAudio,
    type Tracker,
    type ViewHandle,
    type ViewportBounds,
} from "../bindings/js/client.js";

declare const runtimeAudio: VisAudio;
const clickVoice = runtimeAudio.play("click.wav", {bus: "ui", gain: 0.8, rate: 1.25});
runtimeAudio.active(clickVoice);
runtimeAudio.stop(clickVoice);
runtimeAudio.masterGain(0.75);
runtimeAudio.busGain("music", 0.5);
runtimeAudio.transport({time: 1.25, rate: 0.5, playing: true, seek: true});

const inputSample: VisInputSample = {
    position: {x: 60, y: 50},
    value: {x: 0.3, y: 0.5},
    rgba: [205, 0, 50, 255],
    object: {handle: 42, id: "gradient", type: "rectangle"},
};
void inputSample;

const customTheme: ThemeOptions = {
    preset: "pro_black",
    background: "#ffffff",
    text: {
        h1: {font: "Pretendard", size: 36, color: "#202124"},
        code: {font: "Pretendard", size: 14, color: "#9b3600"},
    },
    objects: ["#5e7a9b", "#b8915a", "#557b63"],
    object_width: 1.5,
    roundness: 0.1,
    gradient: true,
    end_gradient_stop: "#778899",
    axis: {x: "#9b3600", y: "#557b63", z: "#5e7a9b"},
    colors: {accent: "#4cc9f0", success: "#7bd88f", warning: "#ffd166", danger: "#ff6b6b"},
};
const namedTheme: ThemeOptions = {preset: "pro_black"};
void namedTheme;
const definition = vis.scene({
    width: 960,
    height: 540,
    loop: true,
    theme: customTheme,
    camera: {mode: "interactive", view: "2d", target: [0, 0], height: 6},
});
const music: AudioCueHandle = definition.sound({
    asset: "theme.ogg", bus: "music", begin: 0, end: 12, gain: 0.6, loop: true,
});
music.gain(0.2, 4, 0.5, "ease_out").gain(0.8, 5, 1, "gentle");
const world = definition.space({
    x: [-3, 3, 1], y: [-2, 2, 1], numbers: true,
    number_mode: "relative", number_size: 14, number_color: "#aabbcc",
});
const frame = world.space({matrix: [1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1]});
const halfPi = definition.tex({tex: "\\frac{\\pi}{2}"});
const axes = definition.space({x: [-4, 4, 1], y: [-2, 2, 1], ticks: {x: Math.PI / 2, y: true}, tick_length: 10});
axes.labels({x: [[Math.PI / 2, halfPi], [-Math.PI / 2, "-pi/2"]], gap: 6});
// @ts-expect-error a space places its labels with labels().
definition.space({labels: {x: [[1, "1"]]}});
definition.space({ticks: true});
const axisLabels: ObjectHandle[] = axes.labels({
    y: [[1, "1"], [-1, axes.text({text: "-1"})]], side: {y: "right"}, size: 18, color: "#ff0000",
});
// @ts-expect-error a label is [value, label].
axes.labels({x: [["half", halfPi]]});
// @ts-expect-error labels sit below or above the x axis.
axes.labels({x: [[1, "1"]], side: {x: "left"}});
// @ts-expect-error a tick step is a number or a boolean.
definition.space({ticks: {x: "1"}});
const arrow = frame.arrow({
    from: [-2, 0],
    to: [2, 0],
    tail: 12,
    tip: 16,
    color: "accent",
});
const point = world.point({point: [0, 1, -2], radius: 5});
definition.indicate(point, {color: "focus", duration: 0.2});
const circleCells: GroupHandle = world.cell(
    (x, y, time) => x * x / ((1 + time) ** 2) + y * y <= 4 ? "#0891b2" : "#00000000",
    {mode: "padd", padding: 0.08, duration: 1, fps: 24},
);
const voxels: GroupHandle = world.voxel(
    (x, y, z, time) => x * x + y * y + z * z < (1 + time) ** 2 ? "#2563eb" : "#00000000",
    {mode: "padd", padding: 0.08, duration: 1, fps: 24},
);
world.picture({asset: "pixels", width: 2, filter: "nearest"});
world.picture({pixels: ["#ff0000", "#00ff00"], size: [2, 1], width: 2, filter: "nearest"});
world.cell({size: [4, 2], texture: "pixels", source: [0, 0, 4, 2], mode: "padd"});
world.surface({
    points: [[-1, 0, -1], [1, 0, -1], [-1, 0, 1], [1, 0, 1]],
    size: [2, 2],
    mode: "solid",
});
world.surface({points: [[0, 0], [1, 0], [0, 1], [1, 1]], size: [2, 2], colors: "viridis"});
world.surface({points: [[0, 0], [1, 0], [0, 1], [1, 1]], size: [2, 2], mode: "solid_mesh", mesh: [2, 0]});
world.surface({points: [[0, 0], [1, 0], [0, 1], [1, 1]], size: [2, 2], light: {direction: [-1, 1, 1], ambient: 0.3}});
// @ts-expect-error A light needs a direction.
world.surface({points: [[0, 0], [1, 0], [0, 1], [1, 1]], size: [2, 2], light: {ambient: 0.3}});
// @ts-expect-error Mesh lines are counted across the columns and the rows.
world.surface({points: [[0, 0], [1, 0], [0, 1], [1, 1]], size: [2, 2], mode: "mesh", mesh: 2});
world.surface({
    points: [[0, 0], [1, 0], [0, 1], [1, 1]],
    size: [2, 2],
    colors: {map: "cividis", range: [1, 0], axis: [0, 0, 1]},
});
world.surface({
    points: [[0, 0], [1, 0], [0, 1], [1, 1]],
    size: [2, 2],
    colors: (x, y, z) => (z > 0 ? "#ff0000" : "accent"),
});
world.surface({points: [[0, 0], [1, 0], [0, 1], [1, 1]], size: [2, 2], colors: ["#000", "#fff", "#000", "#fff"]});
// @ts-expect-error A colormap must be one of the named scales.
world.surface({points: [[0, 0], [1, 0], [0, 1], [1, 1]], size: [2, 2], colors: "jet"});
// @ts-expect-error A colormap's axis is x, y, z or a vector.
world.surface({points: [[0, 0], [1, 0], [0, 1], [1, 1]], size: [2, 2], colors: {map: "magma", axis: "w"}});
const matrix: Mat4 = [
    0.72, -0.69, 0, 0,
    0.69, 0.72, 0, 0,
    0, 0, 1, 0,
    0, 0, 0, 1,
];
definition.create([arrow, point], 0.8, "ease_out", 0.1);
definition.create(arrow, 0.8, "ease_out", 0, "clockwise");
definition.uncreate(point, 0.6, "smooth", 0, "reverse");
definition.fillReveal(arrow, 0.3, "ease_out");
definition.drawBorderThenFill(arrow, 0.8, "ease_in_out", 0, "clockwise");
definition.write(arrow, 0.8, "gentle", 0, "forward");
definition.fadeIn(point, {shift: [0, 0.5], scale: 0.9, duration: 0.5, curve: "ease_out"});
definition.fadeOut(point, {shift: [0, -0.5], scale: 1.1});
definition.growFromCenter(point, 0.5, "ease_out");
definition.growFromEdge(point, "bottom", 0.5, "ease_out");
definition.shrinkToCenter(point, 0.5, "ease_in");
definition.indicate(point, {color: "warning", scale: 1.2, duration: 0.7});
definition.wiggle(point, {scale: 1.2, angle: 0.1, swings: 4, duration: 1, curve: "linear"});
definition.circumscribe(point, {shape: "ellipse", color: "focus", width: 2, padding: 0.2, window: 0.4});
definition.passingFlash(point, {color: "accent", width: 5, window: 0.2, duration: 0.8, curve: "linear"});
definition.flash(point, {lines: 8, size: 0.3, padding: 0.15, width: 2, curve: "smooth"});
definition.flash([1, 2], {color: "warning"});
definition.ripple(point, {color: "accent", width: 2, padding: 0.1, size: 0.5, duration: 0.8, curve: "snappy"});
definition.warp([point, arrow], (x, y) => [x * x - y * y, 2 * x * y], {x_range: [-4, 4, 0.5], y_range: {min: -3, max: 3, step: 0.5}});
definition.warp(point, (x, y) => [x, y + Math.sin(x)], 2, "linear", {x_range: [-4, 4, 0.1], y_range: [-3, 3, 0.1], step: 0.02});
// @ts-expect-error a warp needs the samples it calls fn at.
definition.warp(point, (x, y) => [x, y]);
// @ts-expect-error fn returns [x, y].
definition.warp(point, (x: number) => x, {x_range: [0, 1, 1], y_range: [0, 1, 1]});
// @ts-expect-error the samples need their rows.
definition.warp(point, (x, y) => [x, y], {x_range: [0, 1, 1]});
const morphSource = definition.polygon({points: [[-1, -1], [1, -1], [0, 1]]});
const morphTarget = definition.polygon({points: [[-1, 0], [1, -1], [1, 1]]});
definition.morph(morphSource, morphTarget, 0.8, "ease_in_out");
const morphReplacement = definition.polygon({points: [[-1, -1], [1, 0], [-1, 1]]});
definition.morph(morphTarget, morphReplacement, 0.8, "ease_in_out");
const fadeTarget = definition.text({text: "result", point: [0, 0]});
definition.fadeTransform(morphReplacement, fadeTarget, 0.25, "gentle");
const batchSource = definition.plot({points: [[-1, 0], [0, 1], [1, 0]]});
const batchTarget = definition.plot({points: [[-1, 0], [0, -1], [1, 0]]});
definition.morph([batchSource], [batchTarget], 0.8, "ease_in_out", 0.1);
definition.morph(batchSource, batchTarget, 1, "linear", {pairs: [[1, 1]], copy: true});
definition.morph([batchSource], [batchTarget], 0.8, "smooth", {lag: 0.1, copy: true});
// @ts-expect-error copy is a boolean.
definition.morph(morphSource, morphTarget, 0.8, "smooth", {copy: 1});
definition.transform(arrow, matrix, 1.2, "ease_in_out").wait(0.2);
definition.transition(arrow, point, 0.4, "smooth");
definition.transform(point, matrix);
definition.parallel(
    (scene) => scene.shift(point, [1, 0], 1),
    (scene) => scene.fade(arrow, 0.5, 0.5).mark("half"),
);
definition.parallel([(scene) => scene.wait(0.5)]);
definition.at("half", (scene) => scene.stroke(arrow, "#ff0000", 0.25));
definition.at(0.25, (scene) => scene.fill(point, "#00ff00", 0.25));

const group: GroupHandle = definition.group({id: "nodes", matrix});
const rectangleOptions: RectangleOptions = {
    center: [-1, 0], size: [1.5, 0.8], corner: 0.1, fill: "#264653",
};
const rectangle: ObjectHandle = group.rectangle(rectangleOptions);
const label = rectangle.label({text: "node", point: [-1, 0]});
const sibling = group.circle({center: [1, 0], radius: 0.5});
const connector = group.connector(rectangle, sibling, {padding: 0.1, tail: 8, tip: 12, stroke: "#e9c46a"});
const [brace, braceLabel] = group.brace({around: [rectangle, sibling], offset: 0.1, label: "nodes", size: 0.3});
const trimmedLine = definition.line({from: [0, 0], to: [4, 0], trimDistance: [0.5, 1]});
definition.arrow({from: [0, 0], to: [4, 0], trimDistance: 0.5});
definition.vector({value: [4, 0], trimDistance: [1, 0.5]});
definition.set({target: trimmedLine, trimDistance: 0.25});
definition.play({target: trimmedLine, trimDistance: [0.5, 0.25]}, {duration: 1});
// @ts-expect-error Endpoint distances are a number or a pair.
definition.line({from: [0, 0], to: [4, 0], trimDistance: [1, 2, 3]});
// @ts-expect-error Endpoint trimming uses trimDistance.
definition.line({from: [0, 0], to: [4, 0], buff: 1});
// @ts-expect-error Brace spacing uses offset.
group.brace({around: rectangle, buff: 1});
// @ts-expect-error Animation endpoint trimming uses trimDistance.
definition.play({target: trimmedLine, buff: 1});
const bracing: readonly ObjectHandle[] = [brace, braceLabel];
group.brace({from: [-1, 0], to: [1, 0], direction: [0, 1], depth: 0.2, gap: 0.1, label: sibling});
const [, noLabel] = definition.brace({around: rectangle, direction: "left", fill: "#e9c46a", role: "code"});
// @ts-expect-error a brace without a label returns none.
const missingLabel: ObjectHandle = noLabel;
// @ts-expect-error direction is a side or a vector.
definition.brace({around: rectangle, direction: "north"});
// @ts-expect-error a label is a string or a handle.
definition.brace({around: rectangle, label: 3});
definition.line({from: [-2, -1], to: [2, -1], dash: [8, 4], dash_offset: 2});
const flowing = definition.line({from: [-2, -1.5], to: [2, -1.5], dash: [8, 4]});
definition.play({target: flowing, dash_offset: -12}, 0.8, "linear");
rectangle.moveTo([-1.25, 0]).nextTo(sibling, [1, 0], 0.4).alignTo(sibling, [0, 1, 0]);
group.arrange().arrange([1, 0, 0], 0.3).arrangeGrid(2, 0.2, 0.4);
rectangle.group().rectangle().circle().point({point: [0, 0]});
definition.group({id: "palette", stroke: "#2a9d8f", fill: "#e9c46a", width: 3}).circle({radius: 0.5});
definition.point({point: [0, 0]});
definition.line({from: [-1, 0], to: [1, 0]});
definition.line({from: [-1, 0], to: [1, 0], gradient: "#fedcba"});
definition.circle({gradient: false});
definition.circle({gradient: {type: "radial", stops: ["accent", [0.5, "#fff"], [1, "warning"]], center: [0, 0], radius: 1, focal: [0.2, 0], focalRadius: 0.1}});
definition.rectangle({gradient: {type: "linear", stops: ["#f00", "#00f"], from: [-1, 0], to: [1, 0]}});
definition.circle({gradient: {type: "conic", center: [0, 0], angle: 90}});
// @ts-expect-error conic gradients have no radius
definition.circle({gradient: {type: "conic", radius: 1}});
// @ts-expect-error linear gradients have no center
definition.circle({gradient: {type: "linear", center: [0, 0]}});
// @ts-expect-error unknown gradient type
definition.circle({gradient: {type: "diamond"}});
// @ts-expect-error a gradient paints vector shapes, not texts.
definition.text({text: "x", gradient: true});
// @ts-expect-error a gradient paints vector shapes, not spaces.
definition.space({gradient: "#fedcba"});
definition.arrow({from: [-1, 0], to: [1, 0]});
definition.vector({value: [1, 2, 3]});
definition.rectangle({size: [2, 1]});
definition.polygon({points: [[-1, 0], [1, 0], [0, 1]]});
definition.plot({points: [[-1, 0], [1, 1]]});
definition.plot({points: [[-1, 0], [0, NaN], [1, 1]], y_range: [-1, 1], discontinuities: [0.5]});
definition.plot({fn: (x) => Math.tan(x), x_range: [-4, 4, 0.01], y_range: [-3, 3], width: 3});
definition.plot({fn: (x) => (x < 0 ? undefined : Math.sqrt(x)), x_range: {min: -1, max: 1, step: 0.1}});
definition.plot({fn: Math.floor, x_range: [-2, 2, 0.1], discontinuities: [-1, 0, 1]});
// @ts-expect-error fn needs the samples it is called at.
definition.plot({fn: Math.sin});
// @ts-expect-error a plot takes points or fn, not both.
definition.plot({points: [[0, 0], [1, 1]], fn: Math.sin, x_range: [0, 1, 0.1]});
// @ts-expect-error fn returns a number.
definition.plot({fn: (x: number) => [x, x], x_range: [0, 1, 0.1]});
// @ts-expect-error y_range is [lo, hi].
definition.plot({fn: Math.sin, x_range: [0, 1, 0.1], y_range: [0, 1, 2]});
definition.route({
    points: [[-2, 0], [0, 1], [2, 0]],
    tail: 8,
    tip: 14,
    dash: [8, 4],
    dash_offset: -2,
});
definition.path({commands: [
    {type: "move", to: [-2, 0]},
    {type: "line", to: [-1, 1]},
    {type: "quadratic", control: [0, 2], to: [1, 1]},
    {type: "cubic", control1: [1.5, 0.5], control2: [2, -0.5], to: [2.5, 0]},
    {type: "close"},
], samples: 24});
definition.curve({from: [-2, 0], control1: [-1, 2], control2: [1, -2], to: [2, 0], samples: 32});
definition.surface({points: [[-1, 0], [1, 0], [-1, 1], [1, 1]], size: [2, 2], mode: "mesh", shading: false});
definition.text({text: "root", orientation: "plane", size: 0.5, role: "code"});
definition.text({text: "leaning", slant: 0.2});
definition.text({text: "boxed", measure: true});
// @ts-expect-error measure is a flag.
definition.text({text: "boxed", measure: "yes"});
const counter: ObjectHandle = definition.number({value: 0, decimals: 1, separator: ",", prefix: "$"});
definition.play({target: counter, value: 99.5}, 2);
definition.set({target: counter, value: 3});
definition.play({target: counter, layer: 2, shift: [0, 1]}, 1);
definition.set({target: counter, layer: -1});
definition.save(counter).save([counter]);
definition.restore(counter).restore([counter], 1, "linear", 0.25);
// @ts-expect-error restore takes handles.
definition.restore("counter");
// @ts-expect-error decimals is a number.
definition.number({decimals: "2"});
const along: Tracker = definition.tracker(0);
const slope = definition.tracker(1);
const followed: ObjectHandle = definition.bind(counter, along, (v) => ({value: v, opacity: 1 - v / 4}));
definition.bind(point, [along, slope], (v, k) => ({shift: [v, k * v], radius: 0.1}));
definition.track(along, 2, 1, "linear").track([[along, 1], [slope, 2]], 0.5, {preset: "back", strength: 1});
// Tracks of different trackers may overlap on one object, as in the branches of a parallel.
definition.parallel(
    (scene) => scene.track(along, 3, 2, "linear"),
    (scene) => scene.wait(0.5).track([[slope, 0]], 1, "smooth"),
);
void followed;
void along.get();
// @ts-expect-error a bound state cannot use arc.
definition.bind(arrow, along, (v) => ({transform: matrix, arc: v}));
// @ts-expect-error a bound state cannot shift and transform at once.
definition.bind(arrow, along, (v) => ({shift: [v, 0], transform: matrix}));
// @ts-expect-error track needs its duration.
definition.track(along, 2);
const bent = definition.curve({from: [-2, 0], control1: [-1, 1], control2: [1, 1], to: [2, 0]});
definition.bind(bent, along, (v) => ({control2: [1, v], to: {x: 2, y: v / 2}}));
definition.bind(flowing, slope, (k) => ({from: [-2, k], width: 2 + k}));
definition.bind(morphSource, along, (v) => ({points: [[-1, -1], [1, -1], [0, 1 + v]]}));
definition.play({target: definition.vector({value: [1, 0]}), value: [0, 1], origin: [1, 1, 0]}, 1);
definition.set({target: morphTarget, points: [[-1, 0], [1, -1], [1, 2]]});
// @ts-expect-error points is a list of points.
definition.set({target: morphTarget, points: [1, 2]});
// @ts-expect-error a control point is a point.
definition.bind(bent, along, (v) => ({control1: v}));
const sentence: GroupHandle = definition.spans(["The ", {text: "brown", color: "#a0522d", rise: 0.1}, " fox"], {
    split: "words",
    wrap: 4,
    point: [0, 1, 0],
});
const words: ObjectHandle[] = sentence.children();
sentence.spans(["nested"]);
// @ts-expect-error spans hold only text, which has no stroke.
definition.spans(["a"], {stroke: "#ff0000"});
// @ts-expect-error a part needs its text.
definition.spans([{color: "#ff0000"}]);
// @ts-expect-error split cuts words or letters.
definition.spans(["a"], {split: "lines"});
const paragraph: GroupHandle = definition.group({typeset: true});
paragraph.spans(["first line"]);
const turned: GroupHandle = definition.spans(["b", " + ", "a"]);
definition.matchTransform(sentence, turned, 1.5, "smooth", {arc: Math.PI / 2, lag: 0.1, pairs: [[words[0], turned]], strings: false});
definition.matchTransform(sentence, turned, 1, "smooth", {diff: true, fade: 0.4});
definition.matchTransform(sentence, turned, 1, "smooth", {shapes: true});
definition.matchTransform(sentence, turned, 1, "smooth", {copy: true});
const formula: TexHandle = definition.tex({tex: "a^2+b^2=c^2", point: [0, 1], role: "h2", id: "eq"});
const terms: TexHandle = paragraph.tex({
    tex: ["a^2", "+", {text: "c^2", id: "hyp", color: "#ff0000", opacity: 0.5}],
    display: false,
    fill: "#ffffff",
});
const square: TexHandle = terms.part("c^2").part("2", 1);
definition.play({target: square, fill: "#00ff00"}, 0.5);
definition.matchTransform(formula, terms, 1, "smooth", {arc: 0.5, pairs: [[formula.part("c"), terms.part("c^2")]]});
// @ts-expect-error a formula draws filled glyphs and shapes, which have no stroke.
definition.tex({tex: "x", stroke: "#ff0000"});
// @ts-expect-error a part needs its TeX.
definition.tex({tex: [{color: "#ff0000"}]});
// @ts-expect-error a formula needs its TeX.
definition.tex({role: "h2"});
// @ts-expect-error part finds pieces by their TeX.
formula.part(2);
// With the tex module, a spans part holds a formula set inline; leading spaces the lines and
// baseline pins the first one, as it pins a text, a number or a formula.
const inline: GroupHandle = definition.spans(
    ["The sides satisfy ", {tex: "a^2+b^2=c^2", id: "pythagoras", color: "#ff0000"}, ", so ",
        {tex: ["c", "=", {text: "\\sqrt{a^2+b^2}", id: "root"}], rise: 0.1, display: false, size: 20}, "."],
    {split: "words", wrap: 6, leading: 1.5, baseline: true, point: [-3, 1]},
);
void inline;
definition.text({text: "Area", point: [0, 0], baseline: true});
definition.number({value: 3, baseline: true});
definition.tex({tex: "x^2", point: [0, 0], baseline: true});
// @ts-expect-error a formula draws filled glyphs and shapes, which have no stroke.
definition.spans([{tex: "x", stroke: "#ff0000"}]);
// @ts-expect-error leading is a multiple of the line height.
definition.spans(["a"], {leading: "2"});
// @ts-expect-error baseline is a boolean.
definition.text({text: "a", baseline: 1});
definition.rectangle({around: words, padding: [0.2, 0.05], fill: "#fde68a"});
definition.rectangle({around: words[0], padding: 0.1});
const shadedRegion: ObjectHandle = definition.area({fn: (x) => x * x, x_range: [0, 1, 0.1], base: (x) => x, fill: "#2563eb55"});
definition.area({fn: Math.sin, x_range: {min: 0, max: 3, step: 0.1}, base: -1});
const bars: GroupHandle = definition.riemann({fn: Math.sin, x_range: [0, 3, 0.5], sample: "midpoint", negative: "#ef4444"});
bars.children()[0];
definition.riemann({values: [1, 2], x_range: [0, 1, 0.5], fill: "#f59e0b99"});
// @ts-expect-error sample is left, right, or midpoint.
definition.riemann({fn: Math.sin, x_range: [0, 3, 0.5], sample: "center"});
// @ts-expect-error rectangles take no radius.
definition.riemann({fn: Math.sin, x_range: [0, 3, 0.5], radius: 1});
const contours: GroupHandle = definition.contour({fn: (x, y) => x*x + y*y, levels: [1],
    x_range: [-2, 2, 0.1], y_range: [-2, 2, 0.1], colors: "viridis",
    labels: {format: "%.1f", size: 0.12, spacing: 2, padding: 0.04, upright: true}});
paragraph.contour({values: [0, 1, 1, 2], levels: [0.5], x_range: [0, 1, 1], y_range: [0, 1, 1], colors: ["accent"]});
// @ts-expect-error contour label formats have zero to six fixed decimal places.
definition.contour({fn: (x, y) => x+y, levels: [0], x_range: [0, 1, 1], y_range: [0, 1, 1], labels: {format: "%s"}});
// @ts-expect-error a contour needs levels.
definition.contour({fn: (x, y) => x+y, x_range: [0, 1, 1], y_range: [0, 1, 1]});
// @ts-expect-error scalar samples are numbers.
definition.contour({values: [[1, 2]], levels: [0], x_range: [0, 1, 1], y_range: [0, 1, 1]});
// @ts-expect-error a contour takes fn or values.
definition.contour({fn: (x, y) => x+y, values: [0, 1, 1, 2], levels: [0], x_range: [0, 1, 1], y_range: [0, 1, 1]});
const contourAxis = definition.tracker(1);
definition.bind(contours.children()[0], contourAxis, (a: number) => ({fn: (x: number, y: number) => x*x/(a*a)+y*y}));
definition.play({target: contours.children()[0], fn: (x: number, y: number) => x*x+y*y, level: 2}, 1);
definition.set({target: contours.children()[0], values: [0, 1, 1, 2], x_range: [0, 1, 1], y_range: [0, 1, 1]});
// @ts-expect-error one contour child takes one scalar level.
definition.play({target: contours.children()[0], level: [1, 2]}, 1);
const arrows: GroupHandle = definition.vectorField({
    fn: (x, y) => [-y, x],
    x_range: [-2, 2, 0.5],
    y_range: {min: -1, max: 1, step: 0.5},
    anchor: "middle",
    colors: ["accent", "#ff0000"],
    magnitude: [0, 3],
    tip: 8,
    width: 2,
    id: "field",
});
arrows.children()[0];
definition.vectorField({values: [[1, 0]], x_range: [0, 0, 1], y_range: [0, 0, 1], scale: 0.5, color: "#ff0000"});
// @ts-expect-error the field gives [x, y].
definition.vectorField({fn: (x: number) => x, x_range: [0, 1, 1], y_range: [0, 1, 1]});
// @ts-expect-error an arrow stands on its tail or its middle.
definition.vectorField({fn: (x, y) => [x, y], x_range: [0, 1, 1], y_range: [0, 1, 1], anchor: "tip"});
// @ts-expect-error arrows take no fill.
definition.vectorField({fn: (x, y) => [x, y], x_range: [0, 1, 1], y_range: [0, 1, 1], fill: "#ff0000"});
// @ts-expect-error a field needs its rows.
definition.vectorField({fn: (x, y) => [x, y], x_range: [0, 1, 1]});
// @ts-expect-error area needs fn.
definition.area({x_range: [0, 1, 0.1]});
// @ts-expect-error base is a number or a function.
definition.area({fn: Math.sin, x_range: [0, 1, 0.1], base: "axis"});
// @ts-expect-error around takes handles.
definition.rectangle({around: "words"});
const leftDisk = definition.circle({center: [-0.5, 0], radius: 1});
const rightDisk = definition.circle({center: [0.5, 0], radius: 1});
const lens: ObjectHandle = definition.intersection({of: [leftDisk, rightDisk], fill: "#a855f7"});
paragraph.union({of: [leftDisk, rightDisk]});
definition.difference({of: [leftDisk, rightDisk, lens], stroke: "#111827"});
definition.exclusion({of: [leftDisk, rightDisk]});
// @ts-expect-error of lists handles.
definition.union({of: ["left", "right"]});
// @ts-expect-error a region needs its shapes.
definition.intersection({fill: "#a855f7"});
const shelf: GroupHandle = definition.group();
const shelfItems = [shelf.rectangle({size: [1, 1]}), shelf.rectangle({size: [1, 1]})];
definition.arrange(shelf, 0.9, {preset: "back", strength: 0.6}, {gap: 0.3});
definition.arrange(shelfItems, 1, "linear", {direction: [0, -1], center: [2, 0]});
definition.arrange(shelfItems);
// @ts-expect-error gap is a number.
definition.arrange(shelf, 1, "smooth", {gap: "wide"});
// @ts-expect-error arrange takes handles.
definition.arrange(["a", "b"]);
// @ts-expect-error fade is a share of the duration.
definition.matchTransform(sentence, turned, 1, "smooth", {fade: "half"});
// @ts-expect-error diff is a boolean.
definition.matchTransform(sentence, turned, 1, "smooth", {diff: "lines"});
// @ts-expect-error shapes is a boolean.
definition.matchTransform(sentence, turned, 1, "smooth", {shapes: 1});
// @ts-expect-error a pair holds two pieces.
definition.matchTransform(sentence, turned, 1, "smooth", {pairs: [[words[0]]]});
const listing: GroupHandle = definition.code({
    baseline: true, pitch: 36,
    text: "local x = 1\nprint(x)",
    language: "lua",
    leading: 1.2,
    colors: {keyword: "accent", comment: "#808080"},
    id: "listing",
});
const tokens: ObjectHandle[] = listing.children()[0].children();
definition.write(tokens, 1, "smooth", 0.1);
const secondLine: GroupHandle = listing.row(2);
const firstTokens: ObjectHandle[] = listing.column(1);
definition.fill(listing.entry(2, 1), "#ff0000", 0.5);
definition.highlight(listing, firstTokens);
secondLine.children();
// @ts-expect-error an entry needs its row and its column.
listing.entry(2);
const entries: GroupHandle = definition.matrix({
    rows: [[2, -1, 0], [1, 3, "x"]],
    brackets: "round",
    gap: [0.5, 0.2],
    justify: ["left", "center", "right"],
    decimals: 1,
    size: 24,
    color: "accent",
    id: "m",
});
paragraph.matrix({rows: [[1]], gap: 0.3, justify: "right", stroke: "#ff0000", width: 3});
definition.play({target: entries.entry(2, 1), value: 7}, 1);
definition.rectangle({around: entries.row(2), fill: "#dbeafe", layer: -1});
// @ts-expect-error the brackets are one of four kinds.
definition.matrix({rows: [[1]], brackets: "curly"});
// @ts-expect-error entries are numbers or strings.
definition.matrix({rows: [[true]]});
// @ts-expect-error brackets take no dash.
definition.matrix({rows: [[1]], dash: [1, 1]});
// @ts-expect-error a matrix needs its rows.
definition.matrix({size: 24});
const ledger: GroupHandle = definition.table({
    header: ["n", "n2"],
    rows: [[1, 1], [2, 4], [3, 9]],
    padding: [0.3, 0.1],
    justify: "right",
    stripes: true,
    rules: "grid",
    stroke: "#ff0000",
    id: "t",
});
paragraph.table({rows: [["a"]], stripes: "surface", padding: 0, rules: "none"});
definition.play({target: ledger.entry(4, 2), value: 16}, 1);
definition.rectangle({around: ledger.row(3), fill: "#dbeafe", layer: -1});
// @ts-expect-error the rules are one of five kinds.
definition.table({rows: [[1]], rules: "dots"});
// @ts-expect-error stripes are on, off or a color.
definition.table({rows: [[1]], stripes: 2});
// @ts-expect-error a table has no brackets.
definition.table({rows: [[1]], brackets: "round"});
// @ts-expect-error a table needs its rows.
definition.table({header: ["n"]});
paragraph.code({text: "a = b"});
definition.highlight(listing, 1, 2, {dim: 0.2, duration: 0.4, curve: "linear"});
definition.highlight(listing, 2);
definition.highlight(listing, tokens[0], {curve: "smooth"});
definition.highlight(listing, tokens);
definition.highlight(listing);
// @ts-expect-error a highlight takes its curve as curve, not easing.
definition.highlight(listing, {easing: "smooth"});
// @ts-expect-error the selection is a number, a handle or handles.
definition.highlight(listing, "line");
// @ts-expect-error code holds only text, which has no stroke.
definition.code({text: "x", stroke: "#ff0000"});
// @ts-expect-error the language is one viscript splits.
definition.code({text: "x", language: "rust"});
// @ts-expect-error colors name token classes.
definition.code({text: "x", colors: {bold: "#ffffff"}});
// @ts-expect-error code baseline is a boolean.
definition.code({text: "x", baseline: 1});
// @ts-expect-error code pitch is a number.
definition.code({text: "x", pitch: "36"});
// @ts-expect-error code needs its text.
definition.code({language: "lua"});
// @ts-expect-error Label is an Object-only child Text convenience.
definition.label({text: "scene label"});
definition.picture({asset: "pixels"});
definition.picture({pixels: ["#ff0000"], size: [1, 1]});
definition.cell({size: [2, 2]});
definition.connector(rectangle, sibling);
const animation: AnimationSpec = {target: rectangle, shift: [0.5, 0], opacity: 0.5, fill: "#f4a261"};
definition.play(animation, 0.8, "ease_out", 0.1);
definition.play([
    {target: sibling, transform: matrix},
    {target: connector, stroke: "#e76f51", tail: 10, tip: 16},
] as const, 1.2, "ease_in_out", 0.2);
const backCurve: AnimCurve = vis.animCurve.preset("back", 1.35);
const customCurve: AnimCurve = vis.animCurve.cubicBezier(0.2, 0.9, 0.3, 1, 0.8);
const reversedCurve: AnimCurve = vis.animCurve.reverse(backCurve);
definition.shift(sibling, [0.5, 0], 0.8, backCurve);
definition.shift(sibling, [-0.5, 0], 0.8, reversedCurve);
definition.fadeIn(point, {duration: 0.5, curve: customCurve});
const random: RandomGenerator = vis.random(42, 7);
definition.point({point: [2 * random() - 1, random(-2, 2)], radius: random(4)});
// @ts-expect-error A generator takes at most a lower and an upper bound.
random(1, 2, 3);
// @ts-expect-error A generator needs a seed.
vis.random();
definition.rotate(rectangle, Math.PI / 2, {about: "bottom_left", duration: 0.5});
definition.rotate([rectangle, sibling], Math.PI, {about: [0, 0], axis: [0, 1, 0], curve: "linear"});
definition.rotate(sibling, Math.PI, {about: rectangle, shift: [1, 0], arc: Math.PI / 2, lag: 0.1});
definition.scale(rectangle, 1.5, {about: "top"});
definition.scale(sibling, [2, 0.5], {about: [1, 1], curve: backCurve});
definition.moveAlong(point, sibling, {from: 1, to: 0, orient: true, duration: 2});
definition.moveAlong([point, label], sibling, {lag: 0.25, curve: "linear"});
definition.orbit(2 * Math.PI, {axis: [0, 1, 0], duration: 6, curve: "linear"});
definition.orbit(-Math.PI);
const foundHandle: ObjectHandle | null = definition.find("card");
const childHandles: ObjectHandle[] = rectangle.children();
const clonedGroup: GroupHandle = definition.clone(group, {id: "nodes-copy", stroke: "#ff0000", shift: [0, -1]});
const clonedHandle: ObjectHandle = definition.clone(rectangle, {parent: clonedGroup, opacity: 0.5});
// @ts-expect-error A clone takes the style options, a parent and a shift, not a size.
definition.clone(rectangle, {size: [1, 1]});
// @ts-expect-error An orbit turns about the camera's target, not a pivot.
definition.orbit(Math.PI, {about: [0, 0, 0]});
definition.focus(rectangle, {padding: 0.3, duration: 0.8, curve: backCurve});
definition.focus([rectangle, sibling]).focus(null, {duration: 0.5}).focus();
definition.focus(rectangle, {curve: "linear"});
// @ts-expect-error All timing options take their curve as curve, not easing.
definition.focus(rectangle, {easing: "linear"});
// @ts-expect-error padding is a share of the span.
definition.focus(rectangle, {padding: "wide"});
definition.look({target: rectangle, height: 3}, 1, "linear");
definition.look({target: [rectangle, sibling], eye: [4, -6, 5]});
const inset: ViewHandle = definition.view({x: 0.6, y: 0.05, width: 0.35, height: 0.35}, {
    camera: {target: point, height: 1.5}, follow: point, hide: [label], layer: 0,
    background: "#05080d", optical: true, opacity: 0,
});
inset.fade(1, 0.5).look({height: 0.75}, 1, "linear").focus(rectangle, {padding: 0.2}).focus();
definition.view({x: 0, y: 0, width: 0.25, height: 0.25}, {camera: {view: "3d", eye: [0, -8, 6]}, hide: point})
    .orbit(Math.PI / 2, {axis: [0, 0, 1], duration: 2});
// @ts-expect-error A view takes no zoom; its camera's height sets the magnification.
definition.view({x: 0, y: 0, width: 0.5, height: 0.5}, {zoom: 4});
// @ts-expect-error A view is not a scene.
inset.wait(1);
definition.play({target: rectangle, shift: [1, 0], arc: Math.PI / 2}, 1);
definition.play({target: sibling, transform: matrix, arc: -Math.PI / 4}, 1);
definition.play([
    {target: rectangle, size: [2, 1], corner: 0.2, width: 4},
    {target: point, radius: 8, progress: 0.5},
    {target: label, size: 32},
    {target: sibling, gradient: {type: "linear", stops: ["accent", "warning"]}},
] as const, 1);
definition.play({target: sibling, gradient: "#00ff00"}, 1);
definition.set([{target: rectangle, opacity: 0.5}, {target: sibling, shift: [1, 0], radius: 0.75}] as const);
definition.group({id: "isolated-card", isolate: true, opacity: 0.5});
const maskTarget = definition.circle({radius: 0.5});
const wipeTarget = definition.rectangle({size: [1, 1]});
definition.rectangle({size: [2, 2], mask: maskTarget});
definition.rectangle({size: [2, 2], mask: {target: wipeTarget, mode: "inverse_alpha"}}).mask(null);
definition.circle({radius: 0.5, blend: "multiply"});
const shadowed = definition.rectangle({size: [2, 1], blur: 2, shadow: {color: "#00000066", offset: [0, -4], blur: 8}});
definition.play({target: shadowed, blur: 0, shadow: {offset: [0, -12]}}, 1);
// @ts-expect-error Shadow options are color, offset and blur.
definition.rectangle({size: [2, 1], shadow: {spread: 2}});
// @ts-expect-error Blend modes are named.
definition.circle({radius: 0.5, blend: "burn"});
// @ts-expect-error Mask modes are named.
definition.rectangle({size: [2, 2], mask: {target: maskTarget, mode: "glow"}});

// @ts-expect-error Effect options take their curve as curve, not easing.
definition.fadeOut(point, {easing: "smooth"});
// @ts-expect-error Picture sources use either an asset or an inline pixel buffer.
definition.picture({asset: "pixels", pixels: ["#fff"], size: [1, 1]});
// @ts-expect-error Inline picture pixels require their buffer dimensions.
definition.picture({pixels: ["#fff"]});
// @ts-expect-error A picture keeps its source colors and has no outline to dash.
definition.picture({asset: "pixels", dash: [4, 2]});
// @ts-expect-error A cell is painted by its color, texture and patches, with no outline.
definition.cell({size: [2, 2], width: 2});
// @ts-expect-error Animation curve preset names are closed.
vis.animCurve.preset("warp");

// @ts-expect-error Group matrices must be complete Mat4 values.
definition.group({matrix: [1, 0]});
// @ts-expect-error A group draws nothing itself, so it has no color of its own.
definition.group({color: "#ff0000"});
// @ts-expect-error Rectangle size must be a two-dimensional vector.
definition.rectangle({size: [1, 2, 3]});
// @ts-expect-error Stroke dash patterns contain at most four values.
definition.line({from: [0, 0], to: [1, 0], dash: [1, 2, 3, 4, 5]});
// @ts-expect-error A quadratic Path command requires one control point.
definition.path({commands: [{type: "move", to: [0, 0]}, {type: "quadratic", to: [1, 1]}]});
// @ts-expect-error Curve endpoints and controls use world vectors.
definition.curve({from: [0, 0], control1: [0, 1], control2: [1, 1], to: "end"});
// @ts-expect-error Connector endpoints must be object handles.
definition.connector(rectangle, {point: [0, 0]});
// @ts-expect-error Connector endpoints belong in positional arguments, not options.
definition.connector(rectangle, sibling, {from: rectangle});
// @ts-expect-error Relative layout targets must be object handles.
rectangle.nextTo({point: [0, 0]}, [1, 0]);
// @ts-expect-error Only GroupHandle provides arrange.
rectangle.arrange();
// @ts-expect-error Only SpaceHandle provides voxel sampling.
rectangle.voxel(() => "#fff");
// @ts-expect-error ObjectHandle.cell accepts CellOptions, not a sampling callback.
rectangle.cell(() => "#fff");
// @ts-expect-error A cell callback returns a color string.
world.cell(() => 42);
// @ts-expect-error A voxel callback returns a color string.
world.voxel(() => 42);
// @ts-expect-error A play descriptor must animate at least one property.
definition.play({target: rectangle});
// @ts-expect-error shift and transform are mutually exclusive for one target.
definition.play({target: rectangle, shift: [1, 0], transform: matrix});
// @ts-expect-error play targets must be object handles.
definition.play({target: definition, opacity: 0.5});
// @ts-expect-error Unknown animation properties are rejected.
definition.play({target: rectangle, opacity: 0.5, spin: 1});
// @ts-expect-error A gradient cannot be switched on or off by an animation.
definition.play({target: rectangle, gradient: true});
// @ts-expect-error set applies values at once and takes no arc.
definition.set({target: rectangle, shift: [1, 0], arc: 1});
// @ts-expect-error Create direction is a closed string union.
definition.create(rectangle, 1, "smooth", 0, "diagonal");
// @ts-expect-error Fade scale must be numeric.
definition.fadeIn(rectangle, {scale: "small"});
// @ts-expect-error Fade options reject unknown properties.
definition.fadeOut(rectangle, {spin: 1});
// @ts-expect-error A travel arc bends only a shift or a transform.
definition.play({target: rectangle, opacity: 0.5, arc: 1});
// @ts-expect-error Pivot anchors are a closed string union.
definition.scale(rectangle, 2, {about: "middle"});
// @ts-expect-error Rotation options take their curve as curve, not easing.
definition.rotate(rectangle, 1, {easing: "linear"});
// @ts-expect-error Paths are object handles.
definition.moveAlong(rectangle, [0, 0]);
// @ts-expect-error Motion along a path has no pivot.
definition.moveAlong(rectangle, sibling, {about: "center"});
// @ts-expect-error Indicate color must be a color string.
definition.indicate(rectangle, {color: 42});
// @ts-expect-error Wiggle options take their curve as curve, not easing.
definition.wiggle(rectangle, {easing: "linear"});
// @ts-expect-error A circumscribe frame is a rectangle or an ellipse.
definition.circumscribe(rectangle, {shape: "circle"});
// @ts-expect-error A passing flash runs along the outline itself, with no padding.
definition.passingFlash(rectangle, {padding: 0.1});
// @ts-expect-error A flash bursts out of a handle or a point of two or three coordinates.
definition.flash([1]);
// @ts-expect-error A ripple grows from a handle's outline, not from a point.
definition.ripple([1, 2]);
// @ts-expect-error Morph targets must be object handles.
definition.morph(rectangle, definition);
// @ts-expect-error Theme presets are a closed string union.
vis.scene({theme: "editorial"});
// @ts-expect-error Theme object colors are strings.
vis.scene({theme: {objects: [42]}});
// @ts-expect-error Text roles are a closed string union.
definition.text({text: "caption", role: "caption"});

const detail = vis.scene({width: 480, height: 270, camera: {view: "3d"}});
detail.space().vector({value: [1, 2, 3]});
const overview = vis.scene({width: 480, height: 270});
overview.space().point({point: [0, 0]});
overview.viewport(detail, {x: 0.5, y: 0.5, width: 0.5, height: 0.5});
const overviewBounds: ViewportBounds = {x: 0, y: 0, width: 0.5, height: 1};
definition.viewport(overview, overviewBounds);

const transitionA = vis.scene({width: 480, height: 270});
transitionA.circle({id: "hero"});
const transitionB = vis.scene({width: 960, height: 540});
transitionB.rectangle({id: "hero"});
const transitionOptions: SceneTransitionOptions = {
    duration: 0.8, hold: [0.2, 0.4] as const, live: true, curve: "snappy",
    viewport: {x: 0, y: 0, width: 1, height: 1},
    style: "zoom_in", area: "hero", area_margin: 0,
};
definition.sceneTransition([transitionA, transitionB], transitionOptions);

// @ts-expect-error A viewport child must be another SceneBuilder.
definition.viewport(world, overviewBounds);
// @ts-expect-error All normalized viewport bounds are required.
definition.viewport(vis.scene(), {x: 0, y: 0, width: 1});
// @ts-expect-error Viewport bounds must contain numbers.
definition.viewport(vis.scene(), {x: 0, y: 0, width: "1", height: 1});
// @ts-expect-error Scene transitions require SceneBuilder stages.
definition.sceneTransition([vis.scene(), world]);
// @ts-expect-error Scene transition hold is a number or a list of numbers.
definition.sceneTransition([vis.scene(), vis.scene()], {hold: "short"});

async function preview(canvas: HTMLCanvasElement)
{
    const scene = await createVis(definition, "typed-scene.ts");
    const cameraHandled: boolean = scene.cameraInput("pan", 0.1, 0);
    void cameraHandled;
    // @ts-expect-error Runtime camera input accepts defined actions.
    scene.cameraInput("move");
    // @ts-expect-error Camera input uses the explicit runtime method name.
    scene.camera("pan", 0.1, 0);
    const loops: boolean = scene.loop;
    const bounds: BBox = scene.bounds("nodes", 0);
    const overlaps: boolean = scene.intersects("nodes", "target", 0, 4);
    const report = scene.layoutReport(0, 4);
    const containmentPassed: boolean = report.containments.every((item) => item.contained);
    const visibleVisualCount: number = report.visuals.filter((item) => item.visible).length;
    const effectiveVisualCount: number = report.visuals.filter((item) => item.visible && !item.occluded).length;
    const visualKind: LayoutVisualKind | undefined = report.visuals[0]?.kind;
    const occluder: number | null | undefined = report.visuals[0]?.occluder;
    const sampledPath: LayoutPathInfo | undefined = report.paths[0];
    const sampledPoint: Readonly<{x: number; y: number}> | undefined = sampledPath?.points[0];
    void loops;
    void bounds;
    void overlaps;
    void containmentPassed;
    void visibleVisualCount;
    void effectiveVisualCount;
    void visualKind;
    void occluder;
    void sampledPath;
    void sampledPoint;
    scene.asset("pixels", new Uint8Array([1, 2, 3]), "png");
    scene.draw(canvas, scene.duration);
    scene.draw(canvas, scene.duration, 2);
    // @ts-expect-error Canvas pixel ratio is numeric.
    scene.draw(canvas, scene.duration, "2");
    scene.destroy();

    const empty = await createVis();
    empty.font("Probe", new Uint8Array([0]));
    const loadedDuration: number = empty.load(definition, "typed-scene.js");
    const pixels: Uint8ClampedArray = empty.render(loadedDuration);
    void pixels;
    // @ts-expect-error Render always returns a copy, so it takes no copy flag.
    empty.render(0, true);
    // @ts-expect-error Builder scenes load through load.
    empty.loadScene(definition);
    empty.destroy();
}

void preview;
void label;

const physicalSpring: SpringResponseOptions = {mass: 1, stiffness: 40, damping: 2, displacement: 200};
const physicalDisplacement: number = vis.springResponse(7.5, {...physicalSpring, velocity: -5});
void physicalDisplacement;
// @ts-expect-error Spring response requires physical system parameters.
vis.springResponse(1, {displacement: 200});
// @ts-expect-error Physical spring parameters are numeric.
vis.springResponse(1, {...physicalSpring, mass: "1"});
// @ts-expect-error Observation duration belongs to the timeline.
vis.springResponse(1, {...physicalSpring, duration: 8});

const timing: TimingOptions = {duration: 0.4, curve: "linear"};
const stagger: AnimationOptions = {...timing, lag: 0.25};
definition.create([rectangle, sibling], stagger).write(rectangle, {...stagger, direction: "reverse"});
definition.shift(rectangle, [0, 1], timing).fade(rectangle, 0.3, timing);
definition.play({target: rectangle, opacity: 0.5}, stagger).restore(rectangle, stagger);
definition.morph(rectangle, sibling, {...stagger, copy: true});
definition.matchTransform(rectangle, sibling, {...stagger, copy: true});
definition.arrange([rectangle, sibling], {...timing, gap: 0.5});
definition.look({target: rectangle}, timing);
inset.look({target: rectangle}, timing).fade(0.5, timing);
music.gain(0.5, 0, timing);
// @ts-expect-error Simple timing does not stagger one shift.
definition.shift(rectangle, [0, 1], {duration: 1, lag: 0.2});
// @ts-expect-error Timing options take their curve as curve, not easing.
definition.create(rectangle, {easing: "smooth"});
// @ts-expect-error Named timing goes in the options form, not a positional options tail.
definition.morph(rectangle, sibling, 1, "linear", {duration: 0.5});
