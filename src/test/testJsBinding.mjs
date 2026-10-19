import assert from "node:assert/strict";
import { inflateSync } from "node:zlib";
import { copyFileSync, existsSync, mkdirSync, mkdtempSync, readFileSync, readdirSync, rmSync, writeFileSync } from "node:fs";
import { tmpdir } from "node:os";
import { dirname, join, resolve } from "node:path";
import { fileURLToPath, pathToFileURL } from "node:url";
import { spawnSync } from "node:child_process";
import { font, fontOptions, run } from "./testNode.mjs";

const bindingPath = process.argv[2];
const cliPath = process.argv[3];
const repoRoot = resolve(dirname(fileURLToPath(import.meta.url)), "../..");

const { compileScene, evaluateScene, vis, ViewHandle } = await import(pathToFileURL(bindingPath));
const objectCount = (scene) =>
    scene.objects.length + scene.viewports.reduce((count, viewport) => count + objectCount(viewport.scene), 0);
const boundsOverlap = (scene, firstId, secondId) => {
    const first = scene.objects.find((object) => object.id === firstId)?.bounds;
    const second = scene.objects.find((object) => object.id === secondId)?.bounds;
    assert.ok(first && second, `Missing layout bounds for ${firstId} or ${secondId}`);
    return (
        first.x < second.x + second.width &&
        second.x < first.x + first.width &&
        first.y < second.y + second.height &&
        second.y < first.y + first.height
    );
};
const scene = vis.scene({
    width: 320,
    height: 180,
    loop: true,
    background: "#0d1117",
    camera: { mode: "interactive", view: "2d", height: 6 },
});
const world = scene.space({ x: [-3, 3, 1], y: [-2, 2, 1], id: "world" });
const axis = world.arrow({
    from: [-2.8, 0],
    to: [2.8, 0],
    tail: 16,
    tip: 20,
    id: "double-arrow",
});
const point = world.point({ point: [2.2, 1.45], fill: "#ef5350", id: "target" });
scene.transform(axis, [0.72, -0.69, 0, 0, 0.69, 0.72, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1], 1.6, "ease_in_out");
scene.fade(point, 0.25, 0.5, "ease_out").wait(0.4);

const lua = compileScene(scene);
assert.match(lua, /objects\[1\]:arrow/);
assert.match(lua, /\["tail"\]=16/);
assert.match(lua, /scene:transform\(objects\[2\]/);
assert.match(lua, /\["loop"\]=true/);
assert.match(lua, /return scene\n$/);

const trimmed = vis.scene({ width: 200, height: 100, fps: 10,
    camera: { mode: "fixed", view: "2d", height: 10 } });
const trimmedLine = trimmed.line({ from: [-4, 0], to: [4, 0], trimDistance: [1, 2] });
trimmed.arrow({ from: [-4, 2], to: [4, 2], trimDistance: 1, tail: 5, tip: 8 });
trimmed.vector({ origin: [-4, -2], value: [8, 0], trimDistance: [2, 1] });
const trimmedX = trimmed.tracker(0);
trimmed.bind(trimmedLine, trimmedX, (t) => ({ to: [4, 6 * t], trimDistance: [1 + t, 2 - t] }));
trimmed.track(trimmedX, 1, 1, "linear");
trimmed.play({ target: trimmedLine, trimDistance: 0.5 }, 1, "linear");
assert.match(compileScene(trimmed), /\["trim_distance"\]=\{1,2\}/);
assert.match(compileScene(trimmed), /trim_distance=0.5/);
assert.throws(() => trimmed.play({target: trimmedLine, buff: 1}), /Unknown animation property: buff/);
const invalidTrim = vis.scene();
const invalidTrimLine = invalidTrim.line({from: [0, 0], to: [1, 0]});
assert.throws(() => invalidTrim.bind(invalidTrimLine, invalidTrim.tracker(0), (t) => ({buff: t})),
    /A bound state cannot set 'buff'/);
const trimmedScript = `
local s = vis.scene {width = 200, height = 100, fps = 10,
  camera = {mode = "fixed", view = "2d", height = 10}}
local l = s:line {from = {-4, 0}, to = {4, 0}, trim_distance = {1, 2}}
s:arrow {from = {-4, 2}, to = {4, 2}, trim_distance = 1, tail = 5, tip = 8}
s:vector {origin = {-4, -2}, value = {8, 0}, trim_distance = {2, 1}}
local x = s:tracker(0)
s:bind(l, x, function(t) return {to = {4, 6 * t}, trim_distance = {1 + t, 2 - t}} end)
s:track(x, 1, 1, "linear")
s:play({target = l, trim_distance = 0.5}, 1, "linear")
return s
`;

const sampleScene = vis.scene({ camera: { view: "3d" } });
const sampleSpace = sampleScene.space({ x: [-1, 1, 1], y: [0, 1, 1], z: [-1, 1, 1] });
const voxelCoordinates = [];
sampleSpace.voxel((x, y, z, time) => {
    voxelCoordinates.push([x, y, z, time]);
    const radius = 0.75 + time;
    return x * x + y * y + z * z <= radius * radius ? "#2563eb" : "#00000000";
}, { mode: "padd", padding: 0.1, duration: 0.5, fps: 2 });
const cellCoordinates = [];
sampleSpace.cell((x, y, time) => {
    cellCoordinates.push([x, y, time]);
    const axis = 1 + time;
    return x * x / (axis * axis) + y * y <= 1 ? "#0891b2" : "#00000000";
}, { mode: "padd", padding: 0.08, duration: 0.5, fps: 2 });
const sampleLua = compileScene(sampleScene);
assert.equal(voxelCoordinates.length, 36);
assert.deepEqual(voxelCoordinates[0], [-1, 0, -1, 0]);
assert.deepEqual(voxelCoordinates.at(-1), [1, 1, 1, 0.5]);
assert.equal(cellCoordinates.length, 12);
assert.deepEqual(cellCoordinates[0], [-1, 0, 0]);
assert.deepEqual(cellCoordinates.at(-1), [1, 1, 0.5]);
assert.match(sampleLua, /objects\[1\]:voxel\(function\(x,y,z,time\)/);
assert.match(sampleLua, /mode="padd",padding=0\.1,duration=0\.5,fps=2/);
assert.match(sampleLua, /objects\[1\]:cell\(function\(x,y,time\)/);
assert.doesNotMatch(sampleLua, /scene:morph/);

const invalidSampleScene = vis.scene();
const invalidSampleSpace = invalidSampleScene.space({ x: [-1, 1, 1], y: [-1, 1, 1], z: [-1, 1, 1] });
const invalidSampleBefore = compileScene(invalidSampleScene);
assert.throws(() => invalidSampleSpace.voxel(null), /must be a function/);
assert.throws(() => invalidSampleSpace.voxel(() => "#fff", { mode: "mesh" }), /full or padd/);
assert.throws(() => invalidSampleSpace.voxel(() => "#fff", { padding: 0.5 }), /between 0 and 0\.5/);
assert.throws(() => invalidSampleSpace.voxel(() => "#fff", { duration: -1 }), /duration/);
assert.throws(() => invalidSampleSpace.voxel(() => "#fff", { duration: "1" }), /duration/);
assert.throws(() => invalidSampleSpace.voxel(() => "#fff", { duration: 1, fps: 0 }), /fps/);
assert.throws(() => invalidSampleSpace.voxel(() => "#fff", { duration: 200, fps: 30 }), /frame limit/);
assert.throws(() => invalidSampleSpace.voxel(() => "#fff", { depth: 1 }), /Unknown voxel option/);
assert.throws(() => invalidSampleSpace.voxel(() => 42), /must return/);
assert.throws(() => invalidSampleSpace.cell(() => "#fff", { depth: 1 }), /Unknown cell option/);
assert.throws(() => invalidSampleSpace.cell(() => 42), /must return/);
assert.throws(
    () => invalidSampleSpace.voxel(() => {
        invalidSampleScene.point({ point: [0, 0] });
        return "#fff";
    }),
    /authoring is unavailable/,
);
assert.equal(compileScene(invalidSampleScene), invalidSampleBefore);
assert.equal(typeof invalidSampleSpace.voxel(() => "#fff").arrange, "function");
assert.equal(typeof invalidSampleSpace.cell(() => "#fff").arrange, "function");
assert.equal(typeof invalidSampleSpace.cell({ size: [1, 1] }).moveTo, "function");
const oversizedVoxel = vis.scene().space({
    x: [-100, 100, 1], y: [-100, 100, 1], z: [0, 0, 1],
});
assert.throws(() => oversizedVoxel.voxel(() => "#fff"), /limit/);
const oversizedTemporalCell = vis.scene().space({
    x: [-25, 25, 1], y: [-25, 25, 1], z: [0, 0, 1],
});
assert.throws(
    () => oversizedTemporalCell.cell(() => "#fff", { duration: 4, fps: 30 }),
    /sample limit/,
);
assert.equal(world.point({ point: [0, 0] }).voxel, undefined);

const themed = vis.scene({
    width: 320,
    height: 180,
    theme: {
        preset: "pro_black",
        background: "#ffffff",
        text: { h1: { font: "Pretendard", size: 36, color: "#202124" } },
        objects: ["#5e7a9b", "#b8915a", "#557b63"],
        object_width: 1.5,
        roundness: 0.25,
        gradient: true,
        end_gradient_stop: "#778899",
        axis: { x: "#9b3600", y: "#557b63", z: "#5e7a9b" },
        colors: {accent: "#4cc9f0", danger: "#ff6b6b", surface: "#161b22", result: "#edc948", focus: "#6ea8fe"},
    },
});
themed.text({ text: "Theme", role: "h1", fill: "accent" });
const themedLua = compileScene(themed);
assert.match(themedLua, /\["preset"\]="pro_black"/);
assert.match(themedLua, /\["objects"\]=\{"#5e7a9b","#b8915a","#557b63"\}/);
assert.match(themedLua, /\["object_width"\]=1\.5/);
assert.match(themedLua, /\["roundness"\]=0\.25/);
assert.match(themedLua, /\["gradient"\]=true/);
assert.match(themedLua, /\["end_gradient_stop"\]="#778899"/);
assert.match(themedLua, /\["colors"\]=\{\["accent"\]="#4cc9f0",\["danger"\]="#ff6b6b",\["focus"\]="#6ea8fe",\["result"\]="#edc948",\["surface"\]="#161b22"\}/);
assert.match(themedLua, /\["fill"\]="accent"/);
assert.match(themedLua, /\["role"\]="h1"/);

const freeform = vis.scene({ width: 480, height: 270 });
const groupMatrix = [1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1];
const nodes = freeform.group({ id: "nodes", matrix: groupMatrix });
const boxCenter = [-1, 0];
const box = nodes.rectangle({ center: boxCenter, size: [1.5, 0.8], corner: 0.15, fill: "#264653" });
box.label({ text: "A", point: [-1, 0], fill: "#ffffff" });
const circle = nodes.circle({ center: [1, 0], radius: 0.45, fill: "#2a9d8f" });
nodes.circle({center: [2, 0], radius: 0.45, gradient: "#fedcba"});
nodes.line({from: [1.5, -1], to: [2.5, -1], gradient: false});
const connectorOptions = {
    padding: 0.1,
    tail: 8,
    tip: 12,
    stroke: "#e9c46a",
    dash: [8, 4],
    dash_offset: 2,
};
const connector = nodes.connector(box, circle, connectorOptions);
box.moveTo([-1.25, 0]).nextTo(circle, [1, 0, 0], 0.4).alignTo(circle, [0, 1, 0]);
nodes.arrange([1, 0, 0], 0.3).arrangeGrid(2, 0.2, 0.4);
const shift = [0.5, 0.25];
const firstAnimation = { target: box, shift, opacity: 0.5, fill: "#f4a261" };
freeform.play(firstAnimation, 0.8, "ease_out", 0.1);
const transform = [1, 0, 0, 0.5, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1];
const parallelAnimations = [
    { target: circle, transform },
    { target: connector, stroke: "#e76f51", dash_offset: -12, tail: 10, tip: 16 },
];
freeform.play(parallelAnimations, 1.2, "ease_in_out", 0.2);
groupMatrix[0] = 99;
boxCenter[0] = 99;
connectorOptions.padding = 99;
shift[0] = 99;
transform[0] = 99;
firstAnimation.opacity = 0.99;
firstAnimation.target = circle;
parallelAnimations.reverse();

const freeformLua = compileScene(freeform);
assert.match(freeformLua, /objects\[1\] = scene:group/);
assert.match(freeformLua, /objects\[2\] = objects\[1\]:rectangle/);
assert.match(freeformLua, /objects\[3\] = objects\[2\]:label/);
assert.match(freeformLua, /objects\[4\] = objects\[1\]:circle/);
assert.match(freeformLua, /objects\[5\] = objects\[1\]:circle .*\["gradient"\]="#fedcba"/);
assert.match(freeformLua, /objects\[6\] = objects\[1\]:line .*\["gradient"\]=false/);

const gradients = vis.scene({ width: 320, height: 180 });
gradients.circle({
    radius: 1,
    gradient: { type: "radial", stops: ["accent", [0.5, "#ffffff"], [1, "warning"]], center: [0, 0], radius: 2, focal: [0.2, 0], focalRadius: 0.1 },
});
gradients.rectangle({ gradient: { type: "linear", stops: ["#ff0000", "#0000ff"], from: [-1, 0], to: [1, 0] } });
gradients.circle({ gradient: { type: "conic", angle: 45 } });
const gradientLua = compileScene(gradients);
assert.match(
    gradientLua,
    /\["gradient"\]=\{\["center"\]=\{0,0\},\["focal"\]=\{0\.2,0\},\["focal_radius"\]=0\.1,\["radius"\]=2,\["stops"\]=\{"accent",\{0\.5,"#ffffff"\},\{1,"warning"\}\},\["type"\]="radial"\}/,
);
assert.match(gradientLua, /\["gradient"\]=\{\["from"\]=\{-1,0\},\["stops"\]=\{"#ff0000","#0000ff"\},\["to"\]=\{1,0\},\["type"\]="linear"\}/);
assert.match(gradientLua, /\["gradient"\]=\{\["angle"\]=45,\["type"\]="conic"\}/);
assert.throws(() => gradients.circle({ gradient: { type: "diamond" } }), /gradient\.type/);
assert.throws(() => gradients.circle({ gradient: { type: "linear", center: [0, 0] } }), /Unknown linear gradient option: center/);
assert.throws(() => gradients.circle({ gradient: { type: "conic", focal_radius: 1 } }), /Unknown conic gradient option/);
assert.throws(() => gradients.circle({ gradient: { type: "linear", stops: ["#fff"] } }), /2 to 8 stops/);
assert.match(freeformLua, /objects\[7\] = objects\[1\]:connector \{from=objects\[2\],to=objects\[4\],/);
assert.match(freeformLua, /\["dash"\]=\{8,4\}/);
assert.match(freeformLua, /\["dash_offset"\]=2/);
assert.match(freeformLua, /objects\[2\]:move_to\(\{-1\.25,0\}\)/);
assert.match(freeformLua, /objects\[2\]:next_to\(objects\[4\],\{1,0,0\},0\.4\)/);
assert.match(freeformLua, /objects\[2\]:align_to\(objects\[4\],\{0,1,0\}\)/);
assert.match(freeformLua, /objects\[1\]:arrange\(\{1,0,0\},0\.3\)/);
assert.match(freeformLua, /objects\[1\]:arrange_grid\(2,0\.2,0\.4\)/);
assert.match(
    freeformLua,
    /scene:play\(\{\{target=objects\[2\],shift=\{0\.5,0\.25\},opacity=0\.5,fill="#f4a261"\}\},0\.8,"ease_out",0\.1\)/,
);
assert.match(
    freeformLua,
    /scene:play\(\{\{target=objects\[4\],transform=\{1,0,0,0\.5,0,1,0,0,0,0,1,0,0,0,0,1\}\},\{target=objects\[7\],stroke="#e76f51",dash_offset=-12,tail=10,tip=16\}\},1\.2,"ease_in_out",0\.2\)/,
);
assert.doesNotMatch(freeformLua, /99/);
for (const factory of [
    "group",
    "space",
    "point",
    "line",
    "arrow",
    "vector",
    "circle",
    "rectangle",
    "polygon",
    "plot",
    "route",
    "path",
    "curve",
    "surface",
    "text",
    "picture",
    "cell",
    "connector",
]) {
    assert.equal(typeof freeform[factory], "function", `SceneBuilder.${factory} is missing`);
    assert.equal(typeof box[factory], "function", `ObjectHandle.${factory} is missing`);
}

const angleTwin = vis.scene({width: 200, height: 100, fps: 10, camera: {height: 10, view: "2d"}});
const nativeArc = angleTwin.arc({center: [-2, 0], radius: 1, start: 0.2, angle: Math.PI, dash: [4, 2]});
const nativeAngle = angleTwin.angle({vertex: [0, 0], from: [-1, 0.2], to: [-1, -0.2], radius: 0.5,
    right: "never", label_distance: 1.5, label: {text: "∠APB = 35°", size: 12, id: "angle-label"}});
assert.equal(nativeAngle.children().length, 1);
assert.equal(angleTwin.find("angle-label"), nativeAngle.children()[0]);
const angleTracker = angleTwin.tracker(0);
angleTwin.bind(nativeArc, angleTracker, (v) => ({center: [-2 + v, 0], radius: 1 + v, start: 0.2 + v, angle: Math.PI - v}));
angleTwin.bind(nativeAngle, angleTracker, (v) => ({vertex: [v, 0], from: [-1 + v, 0.2], to: [-1 + v, -0.2]}));
angleTwin.track(angleTracker, 1, 1, "linear");
angleTwin.play({target: nativeAngle, radius: 0.75, vertex: [2, 0], from: [1, -0.2], to: [1, 0.2]}, 0.5, "linear");
const angleTwinScript = `
local s = vis.scene {width = 200, height = 100, fps = 10, camera = {height = 10, view = "2d"}}
local a = s:arc {center = {-2, 0}, radius = 1, start = 0.2, angle = math.pi, dash = {4, 2}}
local m = s:angle {vertex = {0, 0}, from = {-1, 0.2}, to = {-1, -0.2}, radius = 0.5,
  right = "never", label_distance = 1.5, label = {text = "∠APB = 35°", size = 12, id = "angle-label"}}
local t = s:tracker(0)
s:bind(a, t, function(v) return {center = {-2 + v, 0}, radius = 1 + v, start = 0.2 + v, angle = math.pi - v} end)
s:bind(m, t, function(v) return {vertex = {v, 0}, from = {-1 + v, 0.2}, to = {-1 + v, -0.2}} end)
s:track(t, 1, 1, "linear")
s:play({target = m, radius = 0.75, vertex = {2, 0}, from = {1, -0.2}, to = {1, 0.2}}, 0.5, "linear")
return s
`;
assert.match(compileScene(angleTwin), /:arc /);
assert.match(compileScene(angleTwin), /:angle /);
assert.match(compileScene(angleTwin), /\["label_distance"\]=1.5/);
assert.match(compileScene(angleTwin), /\["vertex"\]=/);
const angleInvalid = vis.scene();
assert.throws(() => angleInvalid.play({target: angleInvalid.circle(), vertex: [0, 0]}), /vertex needs an angle/);

const validation = vis.scene();
const validationBox = validation.rectangle();
const validationCircle = validation.circle();
const validationConnector = validation.connector(validationBox, validationCircle);
const foreign = vis.scene().circle();
assert.throws(() => validation.connector(validationBox, foreign), /same scene/);
assert.throws(() => validationBox.connector(validationBox, foreign), /same scene/);
assert.throws(() => validationBox.nextTo(foreign, [1, 0]), /same scene/);
assert.throws(() => validationBox.alignTo(foreign, [1, 0]), /same scene/);
assert.throws(
    () => validation.connector(validationBox, validationCircle, { from: validationBox }),
    /passed as handles/,
);
const connectorValidationBefore = compileScene(validation);
assert.throws(() => validation.shift(validationConnector, [1, 0]), /cannot be shifted or transformed/);
assert.throws(
    () => validation.transform(validationConnector, [1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1]),
    /cannot be shifted or transformed/,
);
assert.throws(
    () => validation.play({ target: validationConnector, shift: [1, 0] }),
    /cannot be shifted or transformed/,
);
assert.equal(compileScene(validation), connectorValidationBefore);
const validBefore = compileScene(validation);
assert.throws(() => validation.play({ target: foreign, opacity: 0.5 }), /same scene/);
assert.throws(() => validation.play({ target: validationBox }), /at least one property/);
assert.throws(
    () =>
        validation.play({
            target: validationBox,
            shift: [1, 0],
            transform: [1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1],
        }),
    /cannot combine/,
);
assert.throws(
    () =>
        validation.play([
            { target: validationBox, opacity: 0.5 },
            { target: validationBox, fill: "#ffffff" },
        ]),
    /duplicate targets/,
);
assert.throws(() => validation.play([]), /at least one descriptor/);
assert.throws(() => validation.play({ target: validationBox, opacity: 0.5, spin: 1 }), /Unknown animation property/);
assert.throws(() => validation.play(null), /plain objects/);
assert.equal(compileScene(validation), validBefore);

const effects = vis.scene({ width: 320, height: 180 });
const effectSource = effects.polygon({
    id: "effect-source",
    points: [
        [-1, -0.5],
        [1, -0.5],
        [0, 1],
    ],
});
const effectAuxiliary = effects.rectangle({ id: "effect-auxiliary" });
const fadeShift = [0, 0.5];
const fadeOptions = { shift: fadeShift, scale: 0.9, duration: 0.6, curve: "ease_out" };
effects.create(effectSource, 0.5, "linear", 0, "clockwise");
effects.uncreate(effectSource, 0.4, "smooth", 0.1, "reverse");
effects.fillReveal(effectSource, 0.3, "ease_out", 0.05);
effects.drawBorderThenFill(effectSource, 0.6, "ease_in_out", 0, "counterclockwise");
effects.write(effectSource, 0.7, "gentle", 0.05, "forward");
effects.fadeIn(effectSource, fadeOptions);
effects.fadeOut(effectSource);
effects.growFromCenter(effectSource, 0.45, "ease_out");
effects.growFromEdge(effectSource, "bottom", 0.4, "ease_out");
effects.shrinkToCenter(effectSource, 0.35, "ease_in");
effects.indicate(effectSource, { color: "#4cc9f0", scale: 1.25, duration: 0.7, curve: "smooth" });
effects.indicate(effectAuxiliary);
const effectTarget = effects.polygon({
    id: "effect-target",
    points: [
        [-1, -1],
        [1, -1],
        [0, 1.25],
    ],
});
effects.morph(effectSource, effectTarget, 0.8, "ease_in_out");
const effectReplacement = effects.polygon({
    id: "effect-replacement",
    points: [
        [-1.2, 0],
        [0.6, -1],
        [0.6, 1],
    ],
});
effects.morph(effectTarget, effectReplacement, 0.9, "ease_out");
const effectLabel = effects.text({id: "effect-label", text: "result", point: [0, 0]});
effects.fadeTransform(effectReplacement, effectLabel, 0.25, "gentle");
const batchMorph = vis.scene();
const batchSourceA = batchMorph.plot({points: [[-1, 0], [0, 1], [1, 0]]});
const batchSourceB = batchMorph.plot({points: [[-1, 1], [0, 0], [1, 1]]});
const batchTargetA = batchMorph.plot({points: [[-1, 0], [0, -1], [1, 0]]});
const batchTargetB = batchMorph.plot({points: [[-1, -1], [0, 0], [1, -1]]});
batchMorph.morph(
    [batchSourceA, batchSourceB],
    [batchTargetA, batchTargetB],
    0.8,
    "ease_in_out",
    0.1,
);
assert.match(
    compileScene(batchMorph),
    /scene:morph\(\{objects\[1\],objects\[2\]\},\{objects\[3\],objects\[4\]\},0\.8,"ease_in_out",0\.1\)/,
);
const springOptions = { mass: 1, stiffness: (2 * Math.PI) ** 2, damping: 0.6 * Math.PI, displacement: 200 };
const springReference = (time) => {
    const omega = 2 * Math.PI;
    const damped = omega * Math.sqrt(1 - 0.15 ** 2);
    return 200 * Math.exp(-0.15 * omega * time)
        * (Math.cos(damped * time) + 0.15 * omega / damped * Math.sin(damped * time));
};
for (let frame = 240; frame >= 0; frame--) {
    const time = Math.max(0, frame / 30 - 0.5);
    assert.ok(Math.abs(vis.springResponse(time, springOptions) - springReference(time)) < 1e-10);
}
assert.equal(vis.springResponse(0, {...springOptions, displacement: -3.25, velocity: 5}), -3.25);
assert.ok(Math.abs(vis.springResponse(7.5, springOptions) + 0.13352159900600497) < 1e-10);
for (const time of [0.01, 0.25, 1, 4]) {
    assert.ok(Math.abs(vis.springResponse(time, {mass: 2, stiffness: 8, damping: 0, displacement: 3, velocity: 5})
        - (3 * Math.cos(2 * time) + 2.5 * Math.sin(2 * time))) < 1e-10);
    assert.ok(Math.abs(vis.springResponse(time, {mass: 1, stiffness: 16, damping: 10, displacement: 3, velocity: 5})
        - (29 * Math.exp(-2 * time) - 11 * Math.exp(-8 * time)) / 6) < 1e-10);
    const expected = (3 + 11 * time) * Math.exp(-2 * time);
    for (const damping of [8 - 1e-10, 8, 8 + 1e-10]) {
        assert.ok(Math.abs(vis.springResponse(time, {mass: 2, stiffness: 8, damping, displacement: 3, velocity: 5})
            - expected) < 1e-9);
    }
}
assert.ok(Math.abs(vis.springResponse(1e300, {mass: 1, stiffness: 1, damping: 1e300, displacement: 1})
    - Math.exp(-1)) < 1e-12);
assert.equal(vis.springResponse(1e300, {mass: 1, stiffness: 1, damping: 2, displacement: 1}), 0);
const springResidual = Math.exp(Math.log(1e308) + Math.log(751) - 750);
assert.ok(Math.abs(vis.springResponse(750, {mass: 1, stiffness: 1, damping: 2, displacement: 1e308})
    / springResidual - 1) < 1e-12);
const springUnderdamped = Math.exp(Math.log(1e308) - 750) * Math.cos(750);
assert.ok(Math.abs(vis.springResponse(750, {mass: 1, stiffness: 2, damping: 2, displacement: 1e308, velocity: -1e308})
    / springUnderdamped - 1) < 1e-12);
const springOverdamped = Math.exp(Math.log(1e308) + Math.log(4 / 3) - 750);
assert.ok(Math.abs(vis.springResponse(750, {mass: 1, stiffness: 4, damping: 5, displacement: 1e308})
    / springOverdamped - 1) < 1e-12);
const springTiny = Math.exp(Math.log(1e308) + Math.log(1431) - 1430);
assert.ok(springTiny > 0 && Math.abs(vis.springResponse(1430, {mass: 1, stiffness: 1, damping: 2, displacement: 1e308})
    / springTiny - 1) < 1e-12);
for (const sign of [-1, 1]) {
    const options = {mass: 1, stiffness: 1e-308, damping: 0, displacement: -sign * 1e308, velocity: sign * 1e308};
    assert.ok(Math.abs(vis.springResponse(2, options) / 1e308 - sign) < 1e-12);
    assert.equal(vis.springResponse(1, options), 0);
}
assert.throws(() => vis.springResponse(2, {mass: 1, stiffness: 1e-308, damping: 0,
    displacement: 1e308, velocity: 1e308}), /spring response is outside the finite numeric range/);
for (const seconds of [-1, Infinity, NaN, "1"]) {
    assert.throws(() => vis.springResponse(seconds, springOptions), /seconds must be a finite non-negative number/);
}
for (const key of ["mass", "stiffness", "damping", "displacement", "velocity"]) {
    for (const value of [Infinity, NaN, "1", null]) {
        assert.throws(() => vis.springResponse(0, {...springOptions, [key]: value}), new RegExp(`${key} must be a finite number`));
    }
}
for (const key of ["mass", "stiffness", "damping", "displacement"]) {
    const missing = {...springOptions};
    delete missing[key];
    assert.throws(() => vis.springResponse(0, missing), new RegExp(`${key} must be a finite number`));
}
for (const options of [{...springOptions, mass: 0}, {...springOptions, stiffness: -1}, {...springOptions, damping: -1}]) {
    assert.throws(() => vis.springResponse(0, options), /mass and stiffness must be positive and damping non-negative/);
}
assert.throws(() => vis.springResponse(0, {...springOptions, duration: 8}), /unknown option 'duration'/);
assert.throws(() => vis.springResponse(0, []), /spring response requires an options object/);
assert.throws(() => vis.springResponse(0, {...springOptions, mass: 1e-300, stiffness: 1e300}), /spring response is outside the finite numeric range/);
assert.throws(() => vis.springResponse(0, {...springOptions, mass: 1e300, stiffness: 1e-300}), /spring response is outside the finite numeric range/);
assert.throws(() => vis.springResponse(1e300, {mass: 1, stiffness: 1e300, damping: 0, displacement: 1}), /spring response is outside the finite numeric range/);

const physicalSpringScene = vis.scene({width: 960, height: 540, fps: 30,
    camera: {view: "2d", height: 540}});
const springSeconds = physicalSpringScene.tracker(0);
const springMass = physicalSpringScene.rectangle({id: "mass", size: [50, 50], fill: "#409cff", width: 0});
physicalSpringScene.bind(springMass, springSeconds, (time) => ({shift: [vis.springResponse(time, springOptions), 0]}));
physicalSpringScene.wait(0.5).track(springSeconds, 7.5, 7.5, "linear");
physicalSpringScene.at(8, () => {});
const physicalSpringScript = `
local s = vis.scene {width=960, height=540, fps=30, camera={view="2d", height=540}}
local seconds = s:tracker(0)
local mass = s:rectangle {id="mass", size={50,50}, fill="#409cff", width=0}
local omega = 2 * math.pi
s:bind(mass, seconds, function(time)
  return {shift={vis.spring_response(time, {mass=1, stiffness=omega*omega,
    damping=0.3*omega, displacement=200}), 0}}
end)
s:wait(0.5)
s:track(seconds, 7.5, 7.5, "linear")
s:seek(8)
return s
`;

const backCurve = vis.animCurve.preset("back", 1.4);
const customCurve = vis.animCurve.cubicBezier(0.2, 0.9, 0.3, 1, 0.75);
const reversedBackCurve = vis.animCurve.reverse(backCurve);
effects.shift(effectAuxiliary, [1, 0], 0.8, backCurve);
effects.shift(effectAuxiliary, [-1, 0], 0.8, reversedBackCurve);
effects.fadeIn(effectAuxiliary, { duration: 0.5, curve: customCurve });
fadeShift[0] = 99;
fadeOptions.scale = 99;
const effectsLua = compileScene(effects);
assert.match(effectsLua, /scene:create\(objects\[1\],0\.5,"linear",0,"clockwise"\)/);
assert.match(effectsLua, /scene:uncreate\(objects\[1\],0\.4,"smooth",0\.1,"reverse"\)/);
assert.match(effectsLua, /scene:fill_reveal\(objects\[1\],0\.3,"ease_out",0\.05\)/);
assert.match(effectsLua, /scene:draw_border_then_fill\(objects\[1\],0\.6,"ease_in_out",0,"counterclockwise"\)/);
assert.match(effectsLua, /scene:write\(objects\[1\],0\.7,"gentle",0\.05,"forward"\)/);
assert.match(
    effectsLua,
    /scene:fade_in\(objects\[1\],\{\["curve"\]="ease_out",\["duration"\]=0\.6,\["scale"\]=0\.9,\["shift"\]=\{0,0\.5\}\}\)/,
);
assert.match(
    effectsLua,
    /scene:fade_out\(objects\[1\],\{\["curve"\]="smooth",\["duration"\]=1,\["scale"\]=1,\["shift"\]=\{0,0,0\}\}\)/,
);
assert.match(effectsLua, /scene:grow_from_center\(objects\[1\],0\.45,"ease_out"\)/);
assert.match(effectsLua, /scene:grow_from_edge\(objects\[1\],"bottom",0\.4,"ease_out"\)/);
assert.match(effectsLua, /scene:shrink_to_center\(objects\[1\],0\.35,"ease_in"\)/);
assert.match(
    effectsLua,
    /scene:indicate\(objects\[1\],\{\["color"\]="#4cc9f0",\["curve"\]="smooth",\["duration"\]=0\.7,\["scale"\]=1\.25\}\)/,
);
assert.match(
    effectsLua,
    /scene:indicate\(objects\[2\],\{\["color"\]="focus",\["curve"\]="smooth",\["duration"\]=1,\["scale"\]=1\.2\}\)/,
);
assert.match(effectsLua, /scene:morph\(objects\[1\],objects\[3\],0\.8,"ease_in_out"\)/);
assert.match(effectsLua, /scene:morph\(objects\[3\],objects\[4\],0\.9,"ease_out"\)/);
assert.match(effectsLua, /scene:fade_transform\(objects\[4\],objects\[5\],0\.25,"gentle"\)/);
assert.match(effectsLua, /scene:shift\(objects\[2\],\{1,0\},0\.8,\{\["preset"\]="back",\["strength"\]=1\.4\}\)/);
assert.match(effectsLua, /scene:shift\(objects\[2\],\{-1,0\},0\.8,\{\["preset"\]="back",\["reverse"\]=true,\["strength"\]=1\.4\}\)/);
assert.match(
    effectsLua,
    /scene:fade_in\(objects\[2\],\{\["curve"\]=\{\["bezier"\]=\{0\.2,0\.9,0\.3,1\},\["strength"\]=0\.75\},\["duration"\]=0\.5,\["scale"\]=1,\["shift"\]=\{0,0,0\}\}\)/,
);
assert.doesNotMatch(effectsLua, /99/);
assert(Object.isFrozen(backCurve));
assert(Object.isFrozen(reversedBackCurve));
assert.deepEqual(vis.animCurve.reverse(reversedBackCurve), backCurve);
assert(Object.isFrozen(customCurve.bezier));

const invalidEffects = vis.scene();
const invalidEffectSource = invalidEffects.circle();
const invalidEffectTarget = invalidEffects.rectangle();
const invalidEffectForeign = vis.scene().circle();
const invalidEffectsBefore = compileScene(invalidEffects);
assert.throws(() => invalidEffects.create(invalidEffectSource, 1, "smooth", 0, "diagonal"), /Create direction/);
assert.throws(() => invalidEffects.uncreate(invalidEffectSource, 1, "smooth", 0, "diagonal"), /Create direction/);
assert.throws(
    () => invalidEffects.drawBorderThenFill(invalidEffectSource, 1, "smooth", 0, "diagonal"),
    /Create direction/,
);
assert.throws(() => invalidEffects.fadeIn(invalidEffectSource, null), /plain object/);
assert.throws(() => invalidEffects.fadeOut(invalidEffectSource, []), /plain object/);
assert.throws(() => invalidEffects.fadeIn(invalidEffectSource, { scale: 0 }), /finite positive/);
assert.throws(() => invalidEffects.fadeOut(invalidEffectSource, { scale: Infinity }), /finite positive/);
assert.throws(() => invalidEffects.fadeIn(invalidEffectSource, { spin: 1 }), /Unknown FadeIn option/);
assert.throws(() => invalidEffects.growFromEdge(invalidEffectSource, "center"), /Growth edge/);
assert.throws(() => invalidEffects.indicate(invalidEffectSource, { scale: 1 }), /greater than 1/);
assert.throws(() => invalidEffects.indicate(invalidEffectSource, { scale: Number.NaN }), /greater than 1/);
assert.throws(() => invalidEffects.indicate(invalidEffectSource, { flash: true }), /Unknown Indicate option/);
assert.throws(
    () => invalidEffects.fadeIn(invalidEffectSource, { easing: "smooth" }),
    /Unknown FadeIn option: easing/,
);
assert.throws(() => vis.animCurve.preset("warp"), /Unknown animation curve preset/);
assert.throws(() => vis.animCurve.preset("back", 2.1), /between 0 and 2/);
assert.throws(() => vis.animCurve.cubicBezier(-0.1, 0, 0.5, 1), /x in 0\.\.1/);
assert.throws(() => vis.animCurve.reverse({preset: "back", reverse: 1}), /reverse must be a boolean/);

// vis.random draws what Lua's math.random draws after math.randomseed with the same seed; the
// numbers are those of Lua 5.4's xoshiro256**, worked out apart from Lua and from this generator.
const drawn = (random) => [random(), random(), random(), random(1, 100), random(1000000), random(-5, 5), random(3, 3), random(1, 6)];
const seededDraws = [
    [[42], [0.93081217803956817, 0.45178389935924312, 0.54688311243421495, 86, 545718, 1, 3, 1]],
    [[42, 7], [0.83191383383816586, 0.97213906090725322, 0.16884000478541139, 16, 25461, 4, 3, 2]],
    [[-3], [0.3196627899223482, 0.57617040687375631, 0.82167927079365166, 32, 910274, 0, 3, 6]],
    [[0], [0.2469118419609948, 0.23482927841848023, 0.069275939529084729, 22, 158896, 2, 3, 1]],
];
for (const [seeds, expected] of seededDraws) assert.deepEqual(drawn(vis.random(...seeds)), expected, `seed ${seeds}`);
assert.deepEqual(drawn(vis.random(42)), drawn(vis.random(42, 0)));
assert.notDeepEqual(drawn(vis.random(42)), drawn(vis.random(43)));
const unitDraws = vis.random(5);
for (let i = 0; i < 1000; i++) {
    const value = unitDraws();
    assert.ok(value >= 0 && value < 1);
    const face = unitDraws(1, 6);
    assert.ok(Number.isInteger(face) && face >= 1 && face <= 6);
}
assert.ok(Number.isSafeInteger(vis.random(9)(Number.MIN_SAFE_INTEGER, Number.MAX_SAFE_INTEGER)));
assert.ok(Object.isFrozen(vis.random(1)));
assert.throws(() => vis.random(), /vis\.random needs a seed, such as vis\.random\(42\)/);
assert.throws(() => vis.random(1.5), /Random seed must be a safe integer/);
assert.throws(() => vis.random(1, 2 ** 53), /Random seed must be a safe integer/);
assert.throws(() => vis.random("1"), /Random seed must be a safe integer/);
const checkedDraws = vis.random(1);
assert.throws(() => checkedDraws(0), /random\(0\) draws a 64-bit integer, which a JavaScript number cannot hold/);
assert.throws(() => checkedDraws(0.5), /Random upper bound must be a safe integer/);
assert.throws(() => checkedDraws(2, 1), /Random interval is empty/);
assert.throws(() => checkedDraws(-1), /Random interval is empty/);
assert.throws(() => checkedDraws(1.5, 2), /Random lower bound must be a safe integer/);
assert.throws(() => checkedDraws(1, 2, 3), /at most a lower and an upper bound/);
assert.deepEqual([checkedDraws(), checkedDraws(1, 6)], (() => {
    const fresh = vis.random(1);
    return [fresh(), fresh(1, 6)];
})());
assert.throws(() => invalidEffects.morph(invalidEffectSource, invalidEffectSource), /distinct handles/);
assert.throws(() => invalidEffects.morph(invalidEffectSource, invalidEffectForeign), /same scene/);
assert.throws(() => invalidEffects.morph([invalidEffectSource], invalidEffectTarget), /both be handles or arrays/);
assert.throws(() => invalidEffects.morph([], []), /non-empty and equally sized/);
assert.throws(
    () => invalidEffects.morph([invalidEffectSource], [invalidEffectTarget, invalidEffectSource]),
    /non-empty and equally sized/,
);
assert.throws(
    () => invalidEffects.morph(
        [invalidEffectSource, invalidEffectSource],
        [invalidEffectTarget, invalidEffectTarget],
    ),
    /distinct across all pairs/,
);
assert.throws(() => invalidEffects.morph(invalidEffectForeign, invalidEffectTarget), /same scene/);
assert.throws(() => invalidEffects.fadeTransform(invalidEffectSource, invalidEffectSource), /distinct handles/);
assert.throws(() => invalidEffects.fadeTransform(invalidEffectSource, invalidEffectForeign), /same scene/);
assert.equal(compileScene(invalidEffects), invalidEffectsBefore);

// Named emphasis effects compile to their Lua calls with every option spelled out, and a wiggle
// may begin while a shift runs.
const emphasis = vis.scene({ width: 160, height: 90 });
const wiggled = emphasis.rectangle({ size: [1, 1], id: "wiggled" });
emphasis.parallel(
    (scene) => scene.shift(wiggled, [2, 0], 3, "linear"),
    (scene) => scene.wait(0.5).wiggle(wiggled, { scale: 1.2, angle: 0.1, swings: 4, duration: 1, curve: "linear" }),
);
emphasis.wiggle(wiggled);
assert.throws(() => emphasis.wiggle(wiggled, { scale: 0 }), /Wiggle scale must be a finite positive number/);
assert.throws(() => emphasis.wiggle(wiggled, { angle: Infinity }), /Wiggle angle must be a finite number/);
assert.throws(() => emphasis.wiggle(wiggled, { swings: 0 }), /Wiggle swings must be a finite positive number/);
assert.throws(() => emphasis.wiggle(wiggled, { spin: 1 }), /Unknown Wiggle option: spin/);
emphasis.circumscribe(wiggled, { shape: "ellipse", color: "#ff0000", window: 0.5, curve: "linear" });
assert.throws(() => emphasis.circumscribe(wiggled, { shape: "circle" }), /Circumscribe shape must be/);
assert.throws(() => emphasis.circumscribe(wiggled, { window: 0 }), /Circumscribe window must be greater than 0/);
assert.throws(() => emphasis.circumscribe(wiggled, { width: 0 }), /Circumscribe width must be a finite positive/);
assert.throws(() => emphasis.circumscribe(wiggled, { padding: -1 }), /Circumscribe padding must be a finite/);
assert.throws(() => emphasis.circumscribe(wiggled, { scale: 2 }), /Unknown Circumscribe option: scale/);
emphasis.passingFlash(wiggled, { width: 4, window: 0.5 });
assert.throws(() => emphasis.passingFlash(wiggled, { window: 2 }), /PassingFlash window must be greater than 0/);
assert.throws(() => emphasis.passingFlash(wiggled, { width: NaN }), /PassingFlash width must be a finite positive/);
assert.throws(() => emphasis.passingFlash(wiggled, { padding: 1 }), /Unknown PassingFlash option: padding/);
emphasis.flash(wiggled, { lines: 8 });
emphasis.flash([1, -1], { size: 0.3, curve: "linear" });
assert.throws(() => emphasis.flash(wiggled, { lines: 0 }), /Flash lines must be a whole number from 1 to 64/);
assert.throws(() => emphasis.flash(wiggled, { lines: 2.5 }), /Flash lines must be a whole number from 1 to 64/);
assert.throws(() => emphasis.flash(wiggled, { size: -1 }), /Flash size must be a finite positive/);
assert.throws(() => emphasis.flash([1], {}), /Flash takes a handle or a finite point/);
assert.throws(() => emphasis.flash([0, NaN]), /Flash takes a handle or a finite point/);
emphasis.ripple(wiggled, { size: 0.6 });
assert.throws(() => emphasis.ripple(wiggled, { size: 0 }), /Ripple size must be a finite positive/);
assert.throws(() => emphasis.ripple(wiggled, { padding: -1 }), /Ripple padding must be a finite/);
assert.throws(() => emphasis.ripple(wiggled, { width: Infinity }), /Ripple width must be a finite positive/);
assert.throws(() => emphasis.ripple(wiggled, { window: 0.5 }), /Unknown Ripple option: window/);
const emphasisLua = compileScene(emphasis);
assert.match(
    emphasisLua,
    /scene:wiggle\(objects\[1\],\{\["angle"\]=0\.1,\["curve"\]="linear",\["duration"\]=1,\["scale"\]=1\.2,\["swings"\]=4\}\)/,
);
assert.match(
    emphasisLua,
    /scene:wiggle\(objects\[1\],\{\["angle"\]=0\.0628318531,\["curve"\]="smooth",\["duration"\]=2,\["scale"\]=1\.1,\["swings"\]=6\}\)/,
);
assert.match(
    emphasisLua,
    /scene:circumscribe\(objects\[1\],\{\["color"\]="#ff0000",\["curve"\]="linear",\["duration"\]=1,\["padding"\]=0\.1,\["shape"\]="ellipse",\["width"\]=3,\["window"\]=0\.5\}\)/,
);
assert.match(
    emphasisLua,
    /scene:passing_flash\(objects\[1\],\{\["color"\]="focus",\["curve"\]="smooth",\["duration"\]=1,\["width"\]=4,\["window"\]=0\.5\}\)/,
);
assert.match(
    emphasisLua,
    /scene:flash\(objects\[1\],\{\["color"\]="focus",\["curve"\]="smooth",\["duration"\]=1,\["lines"\]=8,\["padding"\]=0\.1,\["size"\]=0\.2,\["width"\]=3\}\)/,
);
assert.match(
    emphasisLua,
    /scene:flash\(\{1,-1\},\{\["color"\]="focus",\["curve"\]="linear",\["duration"\]=1,\["lines"\]=12,\["padding"\]=0\.1,\["size"\]=0\.3,\["width"\]=3\}\)/,
);
assert.match(
    emphasisLua,
    /scene:ripple\(objects\[1\],\{\["color"\]="focus",\["curve"\]="snappy",\["duration"\]=1,\["padding"\]=0,\["size"\]=0\.6,\["width"\]=3\}\)/,
);

// Named timing compiles to the same timeline as positional calls and leaves shared options intact.
const sharedTiming = Object.freeze({duration: 0.4, curve: "linear"});
const sharedStagger = Object.freeze({...sharedTiming, lag: 0.25});
const namedTimingCalls = [
    (s, a, b, named) => s.create([a, b], ...(named ? [sharedStagger] : [0.4, "linear", 0.25])),
    (s, a, b, named) => s.uncreate([a, b], ...(named ? [sharedStagger] : [0.4, "linear", 0.25])),
    (s, a, b, named) => s.fillReveal([a, b], ...(named ? [sharedStagger] : [0.4, "linear", 0.25])),
    (s, a, b, named) => s.drawBorderThenFill([a, b], ...(named ? [sharedStagger] : [0.4, "linear", 0.25])),
    (s, a, b, named) => s.write([a, b], ...(named ? [sharedStagger] : [0.4, "linear", 0.25])),
    (s, a, b, named) => s.growFromCenter(a, ...(named ? [sharedTiming] : [0.4, "linear"])),
    (s, a, b, named) => s.growFromEdge(a, "left", ...(named ? [sharedTiming] : [0.4, "linear"])),
    (s, a, b, named) => s.shrinkToCenter(a, ...(named ? [sharedTiming] : [0.4, "linear"])),
    (s, a, b, named) => s.shift(a, [0, 1], ...(named ? [sharedTiming] : [0.4, "linear"])),
    (s, a, b, named) => s.fade(a, 0.3, ...(named ? [sharedTiming] : [0.4, "linear"])),
    (s, a, b, named) => s.stroke(a, "accent", ...(named ? [sharedTiming] : [0.4, "linear"])),
    (s, a, b, named) => s.fill(a, "accent", ...(named ? [sharedTiming] : [0.4, "linear"])),
    (s, a, b, named) => s.fadeTransform(a, b, ...(named ? [sharedTiming] : [0.4, "linear"])),
    (s, a, b, named) => s.morph(a, b, ...(named ? [{...sharedStagger, copy: true}] : [0.4, "linear", {lag: 0.25, copy: true}])),
    (s, a, b, named) => s.matchTransform(a, b, ...(named ? [{...sharedStagger, copy: true}] : [0.4, "linear", {lag: 0.25, copy: true}])),
    (s, a, b, named) => s.arrange([a, b], ...(named ? [{...sharedTiming, gap: 0.5}] : [0.4, "linear", {gap: 0.5}])),
    (s, a, b, named) => s.play([{target: a, shift: [0, 1]}, {target: b, opacity: 0.3}],
        ...(named ? [sharedStagger] : [0.4, "linear", 0.25])),
    (s, a, b, named) => s.save(a).shift(a, [0, 1]).restore(a, ...(named ? [sharedStagger] : [0.4, "linear", 0.25])),
    (s, a, b, named) => s.look({target: [1, 0], height: 4}, ...(named ? [sharedTiming] : [0.4, "linear"])),
    (s, a, b, named) => {
        const t = s.tracker(0);
        s.bind(a, t, (x) => ({shift: [x, 0]}));
        s.track(t, 1, ...(named ? [sharedTiming] : [0.4, "linear"]));
    },
    (s, a, b, named) => {
        const t = s.tracker(0);
        s.bind(a, t, (x) => ({shift: [x, 0]}));
        s.track([[t, 1]], ...(named ? [sharedTiming] : [0.4, "linear"]));
    },
    (s, a, b, named) => {
        const v = s.view({x: 0.5, y: 0, width: 0.5, height: 0.5});
        v.look({target: [1, 0]}, ...(named ? [sharedTiming] : [0.4, "linear"]));
        v.fade(0.3, ...(named ? [sharedTiming] : [0.4, "linear"]));
    },
    (s, a, b, named) => {
        const block = s.code({text: "x = 1", id: "code"});
        s.edit(block, {replace: "1", with: "2"}, ...(named
            ? [{...sharedTiming, fade: 0.5}] : [0.4, "linear", {fade: 0.5}]));
    },
    (s, a, b, named) => s.sound({asset: "tone.wav", begin: 0, end: 1}).gain(0.3, 0,
        ...(named ? [sharedTiming] : [0.4, "linear"])),
    (s, a, b, named) => s.warp(a, (x, y) => [x + 0.2, y], ...(named
        ? [{...sharedTiming, x_range: [-2, 2, 1], y_range: [-2, 2, 1]}]
        : [0.4, "linear", {x_range: [-2, 2, 1], y_range: [-2, 2, 1]}])),
];
for (const author of namedTimingCalls) {
    const source = (named) => {
        const s = vis.scene({width: 96, height: 64});
        const a = s.rectangle({id: "a", center: [-1, 0], size: [1, 1]});
        const b = s.rectangle({id: "b", center: [1, 0], size: [1, 1]});
        author(s, a, b, named);
        return compileScene(s);
    };
    assert.equal(source(true), source(false), author.toString());
}
const timingValidation = vis.scene();
const timingTarget = timingValidation.rectangle({});
const timingBefore = compileScene(timingValidation);
for (const options of [{duration: 0}, {duration: -1}, {duration: Infinity}, {curve: "unknown"},
    {easing: "smooth"}, {lag: 0.25}, {seconds: 1}]) {
    assert.throws(() => timingValidation.shift(timingTarget, [0, 1], options));
}
assert.throws(() => timingValidation.create(timingTarget, {lag: -0.1}), /Lag/);
assert.throws(() => timingValidation.shift(timingTarget, [0, 1], sharedTiming, "linear"), /positional arguments/);
assert.throws(() => timingValidation.look({target: [0, 0]}, sharedTiming, "linear"), /positional arguments/);
assert.equal(compileScene(timingValidation), timingBefore);

// warp calls fn at every sample of x_range and y_range, each running on until it reaches its
// max, and hands Lua the images row by row from the bottom left, each number in the fewest
// digits that give back the single the scene keeps.
const warping = vis.scene({ width: 480, height: 270, camera: { view: "2d", height: 9 } });
const warpedPlane = warping.space({ id: "plane", x: [-4, 4, 1], y: [-3, 3, 1] });
const warpedDot = warping.point({ id: "dot", point: [1, 1] });
const squared = (x, y) => [x * x - y * y, 2 * x * y];
warping.warp([warpedPlane, warpedDot], squared, { x_range: [-4, 4, 0.5], y_range: [-3, 3, 0.5] });
warping.warp(warpedDot, (x, y) => [x + 0.1, y / 2], 0.5, "linear", {
    x_range: [-1, 1, 0.75],
    y_range: [1, 2.5, 1],
    step: 0.1,
});
const warpRegion = { x_range: [0, 1, 1], y_range: [0, 1, 1] };
assert.throws(() => warping.warp(warpedDot, squared), /Warp requires options with x_range and y_range/);
assert.throws(() => warping.warp(warpedDot, squared, 1, "smooth"), /Warp requires options with x_range and y_range/);
assert.throws(() => warping.warp(warpedDot, squared, warpRegion, "linear"),
    /Warp takes \(target, fn, options\) or \(target, fn, duration, curve, options\)/);
assert.throws(() => warping.warp(warpedDot, [1, 2], warpRegion), /Warp fn must be a function of x and y that returns \[x, y\]/);
assert.throws(() => warping.warp(vis.scene().point({ point: [0, 0] }), squared, warpRegion),
    /Animation targets must be handles from the same scene/);
assert.throws(() => warping.warp(warpedDot, squared, { y_range: [0, 1, 1] }), /Warp requires x_range/);
assert.throws(() => warping.warp(warpedDot, squared, { x_range: [0, 1, 1] }), /Warp requires y_range/);
assert.throws(() => warping.warp(warpedDot, squared, { ...warpRegion, spacing: 0.1 }), /Unknown Warp option: spacing/);
assert.throws(() => warping.warp(warpedDot, squared, { x_range: [0, 0, 1], y_range: [0, 1, 1] }),
    /Warp x_range must run from min to a greater max in at most 65536 samples/);
assert.throws(() => warping.warp(warpedDot, squared, { x_range: [0, 300, 1], y_range: [0, 300, 1] }),
    /Warp samples hold at most 65536 images; widen the steps of x_range and y_range/);
assert.throws(() => warping.warp(warpedDot, squared, { ...warpRegion, step: 0 }), /Warp step must be a positive length/);
assert.throws(() => warping.warp(warpedDot, (x) => x, warpRegion),
    /Warp fn must return \[x, y\] of finite numbers; at \(0, 0\) it did not/);
assert.throws(() => warping.warp(warpedDot, (x, y) => [x / 0, y], warpRegion),
    /Warp fn must return \[x, y\] of finite numbers; at \(0, 0\) it did not/);
assert.throws(() => warping.warp(warpedDot, (x, y) => {
    warping.wait(1);
    return [x, y];
}, warpRegion), /Scene authoring is unavailable inside a sampling callback/);
warping.wait(0.5);
const warpingLua = compileScene(warping);
const warpCalls = warpingLua.split("\n").filter((line) => line.startsWith("scene:warp("));
assert.equal(warpCalls.length, 2);
assert.match(warpCalls[0], /^scene:warp\(\{objects\[1\],objects\[2\]\},\{\["values"\]=\{\{7,24\},\{3\.25,21\},/);
assert.match(warpCalls[0], /\{7,24\}\},\["x_range"\]=\{-4,4,0\.5\},\["y_range"\]=\{-3,3,0\.5\}\},1,"smooth"\)$/);
assert.equal(warpCalls[0].match(/\{-?[\d.]+,-?[\d.]+\}/g).length, 17 * 13);
assert.equal(
    warpCalls[1],
    'scene:warp(objects[2],{["values"]={{-0.9,0.5},{-0.15,0.5},{0.6,0.5},{1.35,0.5},{-0.9,1},{-0.15,1},{0.6,1},{1.35,1},'
        + '{-0.9,1.5},{-0.15,1.5},{0.6,1.5},{1.35,1.5}},["x_range"]={-1,1.25,0.75},["y_range"]={1,3,1}},0.5,"linear",'
        + '{["step"]=0.1})',
);
// The scene cannot know where the targets stand, so samples that miss one fail when it runs.
const uncovered = vis.scene();
uncovered.warp(uncovered.point({ point: [3, 0] }), squared, { x_range: [-1, 1, 0.5], y_range: [-1, 1, 0.5] });
const uncoveredLua = compileScene(uncovered);

// plot calls fn where Lua plot would, halving the gap between two samples 40 times to find an
// edge, and cuts the graph as it does: where fn is undefined, on the edges of y_range and on
// either side of each discontinuity. It hands Lua the pieces as points with a break between
// each two, a point that is not a number, and passes y_range and discontinuities on.
const plotting = vis.scene({ width: 320, height: 180, camera: { mode: "fixed", view: "2d", height: 6 } });
const plotAxes = plotting.space({ x: [-3, 3, 1], y: [-2, 2, 1] });
const endpointPlot = vis.scene({width: 320, height: 180, camera: {view: "2d", height: 6}});
const endpointGraph = endpointPlot.plot({id: "graph", fn: (x) => 2 * x,
    x_range: [-2, 2, 1], y_range: [-1, 1], endpoint_radius: 3, width: 0, progress: 0});
endpointPlot.clone(endpointGraph, {id: "copy", shift: [0, 2]});
endpointPlot.wait(1).play({target: endpointGraph, fn: (x) => 2 * x + 1}, 1, "linear");
endpointPlot.wait(1).set({target: endpointGraph, points: [[-1, 0], [1, 0]]});
endpointPlot.wait(1).set({target: endpointGraph, points: []});
endpointPlot.wait(1);
const endpointScript = `
local s=vis.scene{width=320,height=180,camera={view="2d",height=6}}
local graph=s:plot{id="graph",fn=function(x) return 2*x end,
    x_range={-2,2,1},y_range={-1,1},endpoint_radius=3,width=0,progress=0}
s:clone(graph,{id="copy",shift={0,2}})
s:wait(1); s:play({target=graph,fn=function(x) return 2*x+1 end},1,"linear")
s:wait(1); s:set{target=graph,points={{-1,0},{1,0}}}
s:wait(1); s:set{target=graph,points={}}
s:wait(1)
return s
`;
assert.match(compileScene(endpointPlot), /\["endpoint_radius"\]=3/);
for (const endpoint_radius of [-1, NaN, Infinity, 1e100, true, "3"]) {
    assert.throws(() => endpointPlot.plot({points: [[0, 0], [1, 1]], endpoint_radius}),
        /Plot endpoint_radius must be a finite number of zero or more/);
}

const endpointProgress = vis.scene({width: 320, height: 180, camera: {view: "2d", height: 6}});
const progressPlot = (receiver, id, y, progress) => receiver.plot({id, points: [[-2, y], [2, y]],
    endpoint_radius: 4, width: 0, progress});
const endpointDown = progressPlot(endpointProgress, "down", -1.5, 1);
const endpointUp = progressPlot(endpointProgress, "up", -0.5, 0);
const endpointStay = progressPlot(endpointProgress, "stay", 0.5, 0);
const endpointGroup = endpointProgress.group({});
progressPlot(endpointGroup, "grouped", 1.5, 0);
const endpointSpace = endpointProgress.space({x: [-3, 3, 1], y: [-3, 3, 1],
    stroke: "#00000000", axis_x: "#00000000", axis_y: "#00000000"});
progressPlot(endpointSpace, "spaced", 2.5, 0);
const endpointReset = progressPlot(endpointProgress, "reset", -2.5, 1);
const endpointFuture = endpointProgress.plot({id: "future", points: [[-1, 0], [1, 0]],
    endpoint_radius: 4, width: 0, progress: 0});
endpointProgress.uncreate(endpointReset, 1, "linear");
endpointProgress.at(1, (s) => s.play([{target: endpointDown, progress: 0},
    {target: endpointUp, progress: 1}, {target: endpointStay, progress: 0}], 1, "linear"));
endpointProgress.at(1, (s) => s.create([endpointGroup, endpointSpace], 1, "linear"));
endpointProgress.at(1.5, (s) => s.play({target: endpointReset, progress: 0}, 1, "linear"));
endpointProgress.at(2, (s) => s.uncreate([endpointGroup, endpointSpace], 1, "linear"));
endpointProgress.at(1, (s) => s.play({target: endpointFuture, progress: 0}, 1, "linear"));
endpointProgress.at(3, (s) => s.create(endpointFuture, 1, "linear"));
endpointProgress.at(3.5, (s) => s.create(endpointReset, 1, "linear"));
endpointProgress.at(4, (s) => s.create([endpointGroup, endpointSpace], 1, "linear"));
endpointProgress.wait(4);
const endpointProgressScript = `
local s=vis.scene{width=320,height=180,camera={view="2d",height=6}}
local down=s:plot{id="down",points={{-2,-1.5},{2,-1.5}},endpoint_radius=4,width=0,progress=1}
local up=s:plot{id="up",points={{-2,-0.5},{2,-0.5}},endpoint_radius=4,width=0,progress=0}
local stay=s:plot{id="stay",points={{-2,0.5},{2,0.5}},endpoint_radius=4,width=0,progress=0}
local group=s:group{}
local grouped=group:plot{id="grouped",points={{-2,1.5},{2,1.5}},endpoint_radius=4,width=0,progress=0}
local space=s:space{x={-3,3,1},y={-3,3,1},stroke="#00000000",axis_x="#00000000",axis_y="#00000000"}
local spaced=space:plot{id="spaced",points={{-2,2.5},{2,2.5}},endpoint_radius=4,width=0,progress=0}
local reset=s:plot{id="reset",points={{-2,-2.5},{2,-2.5}},endpoint_radius=4,width=0,progress=1}
local future=s:plot{id="future",points={{-1,0},{1,0}},endpoint_radius=4,width=0,progress=0}
s:uncreate(reset,1,"linear")
s:at(1,function()
    s:play({{target=down,progress=0},{target=up,progress=1},{target=stay,progress=0}},1,"linear")
end)
s:at(1,function() s:create({group,space},1,"linear") end)
s:at(1.5,function() s:play({target=reset,progress=0},1,"linear") end)
s:at(2,function() s:uncreate({group,space},1,"linear") end)
s:at(1,function() s:play({target=future,progress=0},1,"linear") end)
s:at(3,function() s:create(future,1,"linear") end)
s:at(3.5,function() s:create(reset,1,"linear") end)
s:at(4,function() s:create({group,space},1,"linear") end)
s:seek(5)
return s
`;

const plotSamples = [];
const hyperbola = (x) => {
    plotSamples.push(x);
    return 1 / x;
};
plotAxes.plot({ fn: hyperbola, x_range: [-1, 1, 0.5], y_range: [-1.5, 1.5], id: "hyperbola" });
plotAxes.plot({ fn: Math.floor, x_range: [-1, 1, 0.5], discontinuities: [0], width: 3 });
plotAxes.plot({ fn: Math.sqrt, x_range: [-1, 1, 0.5] });
plotAxes.plot({ points: [[0, 0], [1, 1], [2, NaN], [3, 1], { x: 4, y: -Infinity }, [5, 0], [6, 1]], y_range: [-1, 2] });
plotAxes.plot({ points: [[0, 0], [1, 1]], x_range: [0, 1, 1] });
// The grid, then 40 halvings toward each edge of the band; 1/0 at 0 and 2 at 0.5 lie outside.
assert.deepEqual(plotSamples.slice(0, 2), [-1, -0.5]);
assert.equal(plotSamples.length, 5 + 2 * 40);
assert.ok([0, 0.5, 1].every((x) => plotSamples.includes(x)));
assert.throws(() => plotAxes.plot({ fn: 1, x_range: [0, 1, 0.5] }), /Plot fn must be a function of x/);
assert.throws(() => plotAxes.plot({ fn: Math.sin, x_range: [0, 1, 0.5], points: [[0, 0], [1, 1]] }),
    /Plot accepts either points or fn, not both/);
assert.throws(() => plotAxes.plot({ fn: Math.sin }), /Plot fn requires x_range/);
assert.throws(() => plotAxes.plot({ id: "nothing" }), /Plot requires points, or fn with x_range/);
assert.throws(() => plotAxes.plot([[0, 0], [1, 1]]), /Plot options must be a plain object/);
assert.throws(() => plotAxes.plot({ fn: Math.sin, x_range: [0, 0.5, 1] }), /Plot x_range must hold from 2 to 1000000 samples/);
assert.throws(() => plotAxes.plot({ fn: Math.sin, x_range: [0, 2e6, 1] }), /Plot x_range must hold from 2 to 1000000 samples/);
assert.throws(() => plotAxes.plot({ fn: Math.sin, x_range: [0, 1] }), /Plot x_range must contain min, max, and step/);
assert.throws(() => plotAxes.plot({ fn: Math.sin, x_range: [0, 1, 0.5], y_range: [1] }), /Plot y_range must be \[lo, hi\]/);
assert.throws(() => plotAxes.plot({ fn: Math.sin, x_range: [0, 1, 0.5], y_range: [-1, Infinity] }),
    /Plot y_range must hold two finite numbers/);
assert.throws(() => plotAxes.plot({ fn: Math.sin, x_range: [0, 1, 0.5], y_range: [1, 1] }), /Plot y_range must have lo below hi/);
assert.throws(() => plotAxes.plot({ points: [[0, 0], [1, 1]], y_range: [2, 1] }), /Plot y_range must have lo below hi/);
assert.throws(() => plotAxes.plot({ fn: Math.sin, x_range: [0, 1, 0.5], discontinuities: 0.5 }),
    /Plot discontinuities must be an array of finite numbers/);
assert.throws(() => plotAxes.plot({ fn: Math.sin, x_range: [0, 1, 0.5], discontinuities: [0.5, NaN] }),
    /Plot discontinuities must be an array of finite numbers/);
assert.throws(() => plotAxes.plot({ fn: () => "1", x_range: [0, 1, 0.5] }),
    /Plot fn must return a number, or undefined where it is undefined; at 0 it returned string/);
assert.throws(() => plotAxes.plot({ fn: (x) => (x < 1 ? undefined : 1), x_range: [0, 1, 0.5] }),
    /Plot has nothing to draw: fn needs two defined values in a row/);
assert.throws(() => plotAxes.plot({ fn: () => 5, x_range: [0, 1, 0.5], y_range: [-1, 1] }),
    /Plot has nothing to draw inside y_range/);
assert.throws(() => plotAxes.plot({
    fn: (x) => {
        plotting.wait(1);
        return x;
    },
    x_range: [0, 1, 0.5],
}), /Scene authoring is unavailable inside a sampling callback/);
plotting.wait(0.5);
const plotLines = compileScene(plotting).split("\n").filter((line) => line.includes(":plot "));
assert.deepEqual(plotLines, [
    'objects[2] = objects[1]:plot {["fn"]={points={{-1,-1},{-0.6666667,-1.5},{0/0,0/0},'
        + '{0.6666667,1.5},{1,1}},samples={{-1,-1},{-0.5,-2},{0,0/0},{0.5,2},{1,1}}},'
        + '["id"]="hyperbola",["x_range"]={-1,1,0.5},["y_range"]={-1.5,1.5}}',
    'objects[3] = objects[1]:plot {["discontinuities"]={0},["fn"]={points={{-1,-1},{-0.5,-1},{-0.0005,-1},{0/0,0/0},'
        + '{0.0005,0},{0.5,0},{1,1}},samples={{-1,-1},{-0.5,-1},{-0.0005,-1},{0/0,0/0},{0.0005,0},{0.5,0},{1,1}}},'
        + '["width"]=3,["x_range"]={-1,1,0.5}}',
    'objects[4] = objects[1]:plot {["fn"]={points={{0,0},{0.5,0.70710677},{1,1}},'
        + 'samples={{-1,0/0},{-0.5,0/0},{0,0},{0.5,0.70710677},{1,1}}},["x_range"]={-1,1,0.5}}',
    'objects[5] = objects[1]:plot {["points"]={{0,0},{1,1},{0/0,0/0},{3,1},{0/0,0/0},{5,0},{6,1}},["y_range"]={-1,2}}',
    'objects[6] = objects[1]:plot {["points"]={{0,0},{1,1}},["x_range"]={0,1,1}}',
]);
// A plot takes points in any number, which the scene cuts as Lua plot cuts them, so the builder
// leaves their count to the scene: the broken plot keeps 4 points and the banded one 4, two on
// each side of the peak it cuts off, and the whole one takes 2 in place of its 3.
const recut = vis.scene();
const recutBroken = recut.plot({ points: [[0, 0], [1, 1], [2, NaN], [3, 1], [4, 0]] });
const recutBanded = recut.plot({ points: [[0, 0], [1, 3], [2, 0]], y_range: [-1, 2] });
const recutWhole = recut.plot({ points: [[0, 0], [1, 1], [2, 0]] });
recut.play([
    { target: recutBroken, points: [[0, 1], [1, 2], [3, 2], [4, 1]] },
    { target: recutBanded, points: [[0, 1], [0.5, 2], [1.5, 2], [2, 1]] },
], 1);
recut.play({ target: recutWhole, points: [[0, 0], [1, 1]] }, 1);
const recutLua = compileScene(recut);
// Graphs with poles, jumps and an edge of their domain, drawn on, which render as their Lua
// twin's do; the steps are powers of two, so every sample is exact in single precision.
const plotTwin = vis.scene({ width: 320, height: 180, camera: { mode: "fixed", view: "2d", height: 6 } });
const plotTwinAxes = plotTwin.space({ x: [-3, 3, 1], y: [-2, 2, 1] });
plotTwin.create([
    plotTwinAxes.plot({ fn: (x) => 1 / (x * x - 1), x_range: [-3, 3, 1 / 16], y_range: [-2, 2], width: 4 }),
    plotTwinAxes.plot({
        fn: Math.floor, x_range: [-3, 3, 1 / 8], discontinuities: [2, -2, -1, 0, 1], y_range: [-2.5, 2.5], color: "#ff0000",
    }),
    plotTwinAxes.plot({ fn: Math.sqrt, x_range: [-1, 3, 1 / 4], color: "#00aa00" }),
    // Jumps on the first and the last sample, which the discontinuities there cut.
    plotTwinAxes.plot({
        fn: (x) => (x < 0 ? Math.ceil(x) : Math.floor(x)), x_range: [-2, 2, 1 / 8], discontinuities: [-2, -1, 1, 2],
        color: "#0000ff",
    }),
], 1, "linear");
const plotTwinScript = [
    'local s = vis.scene {width = 320, height = 180, camera = {mode = "fixed", view = "2d", height = 6}}',
    "local w = s:space {x = {-3, 3, 1}, y = {-2, 2, 1}}",
    "s:create({",
    "    w:plot {fn = function(x) return 1 / (x * x - 1) end, x_range = {-3, 3, 1 / 16}, y_range = {-2, 2}, width = 4},",
    "    w:plot {fn = math.floor, x_range = {-3, 3, 1 / 8}, discontinuities = {2, -2, -1, 0, 1}, y_range = {-2.5, 2.5},",
    '        color = "#ff0000"},',
    '    w:plot {fn = math.sqrt, x_range = {-1, 3, 1 / 4}, color = "#00aa00"},',
    "    w:plot {fn = function(x) if x < 0 then return math.ceil(x) end return math.floor(x) end, x_range = {-2, 2, 1 / 8},",
    '        discontinuities = {-2, -1, 1, 2}, color = "#0000ff"},',
    '}, 1, "linear")',
    "return s",
    "",
].join("\n");

// A descriptor gives a plot a new graph as in Lua: fn, which the builder samples and cuts as
// plot() does, over the x_range and to the band and discontinuities the plot keeps, or points in
// any number. The builder emits the graph as points, with a break between each two pieces and
// none for a graph cut to nothing, and passes y_range and discontinuities on.
const regraph = vis.scene();
const functionPlay = vis.scene({ width: 320, height: 180, camera: { mode: "fixed", view: "2d", height: 6 } });
const functionGraph = functionPlay.plot({ fn: () => 0, x_range: [-2, 2, 0.5], y_range: [-1, 1], width: 4 });
const functionCalls = [];
functionPlay.play({ target: functionGraph, fn: (x) => { functionCalls.push(x); return 2 * x * x; } }, 1, "linear");
functionPlay.play({ target: functionGraph, fn: () => 2 }, 1, "linear");
functionPlay.play({ target: functionGraph, fn: () => 0 }, 1, "linear");
assert.deepEqual(functionCalls.filter((x) => [-2, -1.5, -1, -0.5, 0, 0.5, 1, 1.5, 2].includes(x)),
    [-2, -1.5, -1, -0.5, 0, 0.5, 1, 1.5, 2]);
assert.match(compileScene(functionPlay), /samples=\{\{-2,8\},\{-1.5,4.5\},\{-1,2\},\{-0.5,0.5\},\{0,0\}/);
const functionPlayScript = [
    'local s = vis.scene {width = 320, height = 180, camera = {mode = "fixed", view = "2d", height = 6}}',
    'local p = s:plot {fn = function() return 0 end, x_range = {-2, 2, 0.5}, y_range = {-1, 1}, width = 4}',
    's:play({target = p, fn = function(x) return 2 * x * x end}, 1, "linear")',
    's:play({target = p, fn = function() return 2 end}, 1, "linear")',
    's:play({target = p, fn = function() return 0 end}, 1, "linear")',
    'return s',
].join("\n");
const animatedPoles = vis.scene({ width: 320, height: 180, fps: 10,
    camera: { mode: "fixed", view: "2d", height: 9 } });
const polePhase = animatedPoles.tracker(0);
let poleCalls = 0;
const shiftedPole = (phase) => (x) => { poleCalls++; return Math.tan(x - phase); };
const movingPole = animatedPoles.plot({ id: "graph", fn: shiftedPole(0), x_range: [-3, 3, 0.125],
    y_range: [-4, 4], width: 4 });
animatedPoles.bind(movingPole, polePhase, (phase) => ({ fn: shiftedPole(phase) }));
animatedPoles.track(polePhase, 0.5, 1, "linear");
const animatedPolesLua = compileScene(animatedPoles);
const bakedPoleCalls = poleCalls;
assert.equal(compileScene(animatedPoles), animatedPolesLua);
assert.equal(poleCalls, bakedPoleCalls, "compiled plots must retain their sampled data");
const animatedPolesScript = `
local s = vis.scene {width = 320, height = 180, fps = 10,
    camera = {mode = "fixed", view = "2d", height = 9}}
local phase = s:tracker(0)
local function shifted(p) return function(x) return math.tan(x-p) end end
local graph = s:plot {id = "graph", fn = shifted(0), x_range = {-3,3,0.125}, y_range = {-4,4}, width = 4}
s:bind(graph,phase,function(p) return {fn = shifted(p)} end)
s:track(phase,0.5,1,"linear")
return s`;
const regraphPlot = regraph.plot({ fn: (x) => 1 / x, x_range: [-1, 1, 0.5], y_range: [-1.5, 1.5] });
regraph.play({ target: regraphPlot, fn: (x) => 1 / (x - 0.25) }, 1);
const regraphSteps = regraph.plot({ points: [[0, 0], [1, 1]] });
regraph.play({ target: regraphSteps, fn: Math.floor, x_range: [0, 2, 0.5], discontinuities: [1] }, 1);
regraph.play({ target: regraphSteps, fn: Math.ceil }, 1);
regraph.set({ target: regraphSteps, points: [[0, 0], [1, 1], [2, NaN], [3, 0]] });
regraph.play({ target: regraphPlot, fn: () => 5, y_range: [-1, 1] }, 1);
assert.deepEqual(compileScene(regraph).split("\n").filter((line) => line.includes("play(") || line.includes("set(")), [
    'scene:play({{target=objects[1],fn={points={{-1,-0.8},{-0.5,-1.3333334},{-0.41666666,-1.5},{0/0,0/0},{0.9166667,1.5},'
        + '{1,1.3333334}},samples={{-1,-0.8},{-0.5,-1.3333334},{0,-4},{0/0,0/0},{0.5,4},{1,1.3333334}}}}},1,"smooth",0)',
    'scene:play({{target=objects[2],fn={points={{0,0},{0.5,0},{0.9995,0},{0/0,0/0},{1.0005,1},{1.5,1},{2,2}},'
        + 'samples={{0,0},{0.5,0},{0.9995,0},{0/0,0/0},{1.0005,1},{1.5,1},{2,2}}},'
        + 'x_range={0,2,0.5},discontinuities={1}}},1,"smooth",0)',
    'scene:play({{target=objects[2],fn={points={{0,0},{0.5,1},{0.9995,1},{0/0,0/0},{1.0005,2},{1.5,2},{2,2}},'
        + 'samples={{0,0},{0.5,1},{0.9995,1},{0/0,0/0},{1.0005,2},{1.5,2},{2,2}}}}},1,"smooth",0)',
    "scene:set({{target=objects[2],points={{0,0},{1,1},{0/0,0/0},{3,0}}}})",
    'scene:play({{target=objects[1],fn={points={},samples={{-1,5},{-0.5,5},{0,5},{0.5,5},{1,5}}},y_range={-1,1}}},1,"smooth",0)',
]);
const regraphFresh = regraph.plot({ points: [[0, 0], [1, 1]] });
const regraphTriangle = regraph.polygon({ points: [[0, 0], [1, 0], [0, 1]] });
assert.throws(() => regraph.play({ target: regraphSteps, x_range: [0, 1, 0.5] }, 1), /Animation x_range needs fn/);
assert.throws(() => regraph.play({ target: regraphSteps, y_range: [0, 1] }, 1), /Animation y_range needs fn or points/);
assert.throws(() => regraph.play({ target: regraphSteps, discontinuities: [1] }, 1),
    /Animation discontinuities needs fn or points/);
assert.throws(() => regraph.play({ target: regraphSteps, fn: Math.sin, points: [[0, 0], [1, 1]] }, 1),
    /Animation of a plot takes either points or fn, not both/);
assert.throws(() => regraph.play({ target: regraphSteps, fn: 3 }, 1), /Animation fn must be a function of x/);
assert.throws(() => regraph.play({ target: regraphSteps, fn: Math.sin, y_range: [1, 0] }, 1),
    /Plot y_range must have lo below hi/);
assert.throws(() => regraph.play({ target: regraphFresh, fn: Math.sin }, 1), /Plot fn requires x_range/);
assert.throws(() => regraph.play({ target: regraphTriangle, fn: Math.sin }, 1), /Animation fn needs a plot/);
assert.throws(() => regraph.set({ target: regraphTriangle, x_range: [0, 1, 0.5] }), /Animation x_range needs a plot/);
// A call that fails leaves the plots their cuts, as one that fails in Lua ends the script.
assert.throws(() => regraph.play([
    { target: regraphFresh, fn: Math.sin, x_range: [0, 1, 0.5] },
    { target: regraphTriangle, fn: Math.sin },
], 1), /Animation fn needs a plot/);
assert.throws(() => regraph.play({ target: regraphFresh, fn: Math.sin }, 1), /Plot fn requires x_range/);
assert.throws(() => regraph.play({
    target: regraphSteps,
    fn: (x) => {
        regraph.wait(1);
        return x;
    },
}, 1), /Scene authoring is unavailable inside a sampling callback/);
// A graph whose pole a tracker moves, bound and tracked, steps cut at discontinuities and to a band
// and then to nothing, points of other counts, and a copy that takes fn alone, which render as
// their Lua twin's do. The pole and the steps of x_range are powers of two, so every sample is
// exact in single precision.
const replot = vis.scene({ width: 320, height: 180, fps: 10, camera: { mode: "fixed", view: "2d", height: 6 } });
const replotAxes = replot.space({ x: [-3, 3, 1], y: [-2, 2, 1] });
const replotPole = replot.tracker(-2);
const replotHyperbola = (p) => (x) => 1 / (x - p);
const replotGraph = replotAxes.plot({ fn: replotHyperbola(-2), x_range: [-3, 3, 1 / 16], y_range: [-2, 2], width: 4 });
replot.bind(replotGraph, replotPole, (p) => ({ fn: replotHyperbola(p) }));
const replotSteps = replotAxes.plot({ points: [[-3, -1], [3, 1]], color: "#ff0000" });
replot.set({ target: replotSteps, points: [[-3, -1], [0, 1], [3, -1]] });
replot.track(replotPole, 2, 1, "linear");
replot.play({
    target: replotSteps, fn: Math.floor, x_range: [-2, 2, 1 / 8], y_range: [-1.5, 1.5], discontinuities: [1, -1, 0],
}, 1);
replot.play({ target: replotSteps, fn: Math.ceil }, 1);
const replotCopy = replot.clone(replotGraph);
replot.play([{ target: replotCopy, fn: replotHyperbola(0.5) }, { target: replotSteps, fn: () => 5 }], 1);
replot.play({ target: replotSteps, points: [[-3, 0], [-1, 1], [1, -1], [3, 0]] }, 1);
const replotScript = [
    'local s = vis.scene {width = 320, height = 180, fps = 10, camera = {mode = "fixed", view = "2d", height = 6}}',
    "local w = s:space {x = {-3, 3, 1}, y = {-2, 2, 1}}",
    "local pole = s:tracker(-2)",
    "local function hyperbola(p) return function(x) return 1 / (x - p) end end",
    "local graph = w:plot {fn = hyperbola(-2), x_range = {-3, 3, 1 / 16}, y_range = {-2, 2}, width = 4}",
    "s:bind(graph, pole, function(p) return {fn = hyperbola(p)} end)",
    'local steps = w:plot {points = {{-3, -1}, {3, 1}}, color = "#ff0000"}',
    "s:set {target = steps, points = {{-3, -1}, {0, 1}, {3, -1}}}",
    's:track(pole, 2, 1, "linear")',
    "s:play({target = steps, fn = math.floor, x_range = {-2, 2, 1 / 8}, y_range = {-1.5, 1.5}, discontinuities = {1, -1, 0}}, 1)",
    "s:play({target = steps, fn = math.ceil}, 1)",
    "local copy = s:clone(graph)",
    "s:play({{target = copy, fn = hyperbola(0.5)}, {target = steps, fn = function() return 5 end}}, 1)",
    "s:play({target = steps, points = {{-3, 0}, {-1, 1}, {1, -1}, {3, 0}}}, 1)",
    "return s",
    "",
].join("\n");

// A space takes ticks and labels as Lua space does, and labels() places more and returns their
// handles in order: a new text below the space for each string, and each handle given as it
// is, which the script names.
const graphing = vis.scene({ width: 320, height: 180, camera: { mode: "fixed", view: "2d", height: 6 } });
const tag = graphing.rectangle({ size: [0.5, 0.25], id: "tag" });
const graph = graphing.space({
    x: [-3, 3, 1], y: [-2, 2, 1], ticks: { x: Math.PI / 2, y: true }, tick_length: 10,
    labels: { x: [[Math.PI / 2, "half"], [-Math.PI / 2, tag]], gap: 6 }, id: "graph",
});
const graphLabels = graph.labels({ y: [[1, "one"], [-1, tag]], side: { y: "right" }, size: 18, color: "#ff0000" });
assert.deepEqual(graphLabels.map((handle) => handle.name), ["objects[4][1]", "objects[1]"]);
assert.equal(graphLabels[1], tag);
assert.deepEqual(graph.children().map((handle) => handle.name), ["objects[3][1]", "objects[4][1]"]);
assert.throws(() => graph.labels(5), /Space labels must be a plain object of x, y and z lists of \[value, label\]/);
assert.throws(() => graphing.space({ labels: [] }), /Space labels must be a plain object/);
assert.throws(() => graph.labels({ x: [[1, "a"]], w: [] }), /Unknown space labels option: w/);
assert.throws(() => graph.labels({ x: 5 }), /Space labels x must be an array of \[value, label\]/);
for (const entry of [5, null, [1], [1, "a", 2], [NaN, "a"], [1, 2], ["1", "a"]]) {
    assert.throws(() => graph.labels({ y: [[1, "a"], entry] }),
        /Space labels y entry 2 must be \[value, label\], a finite number and a string or handle/);
}
assert.throws(() => graph.labels({ z: [[1, vis.scene().rectangle({})]] }), /Object references must be handles from the same scene/);
const graphingLines = compileScene(graphing).split("\n");
assert.deepEqual(graphingLines.slice(3, 6), [
    'objects[2] = scene:space {["id"]="graph",["tick_length"]=10,["ticks"]={["x"]=1.5707963267948966,["y"]=true},'
        + '["x"]={-3,3,1},["y"]={-2,2,1}}',
    'objects[3] = objects[2]:labels {["gap"]=6,["x"]={{1.5707963267948966,"half"},{-1.5707963267948966,objects[1]}}}',
    'objects[4] = objects[2]:labels {["color"]="#ff0000",["side"]={["y"]="right"},["size"]=18,["y"]={{1,"one"},{-1,objects[1]}}}',
]);
assert.equal(graphingLines.filter((line) => line.includes(":labels ")).length, 2);
const screenLabels = vis.scene({width: 320, height: 180,
    camera: {mode: "fixed", view: "2d", height: 6}});
const screenAxes = screenLabels.space({id: "screen-axes", x: [-2, 2, 1], y: [-1, 1, 1],
    labels: {placement: "screen", x: [[-2, "x"]], gap: 10, size: 18}});
const suppliedScreen = screenAxes.text({id: "screen-supplied", text: "y", size: 24});
assert.equal(screenAxes.labels({placement: "screen", y: [[1, suppliedScreen]], gap: 10})[0], suppliedScreen);
assert.throws(() => screenAxes.labels({placement: "other"}), /Space labels placement must be 'world' or 'screen'/);
assert.throws(() => screenAxes.labels({placement: "screen", side: {x: "above"}}), /Screen labels do not take side/);
screenLabels.look({height: 4}, 1, "linear");
screenLabels.warp(screenAxes, (x, y) => [x + 3, y], 1, "linear", {x_range: [-2, 2, 1], y_range: [-1, 1, 1]});
const screenLabelLua = compileScene(screenLabels);
assert.match(screenLabelLua, /\["placement"\]="screen"/);
// Generated ids continue independently on each axis and preserve supplied handle ids.
const labelIds = vis.scene({ camera: { mode: "fixed", view: "2d", height: 6 } });
const keptLabel = labelIds.text({ id: "axes/label/x/1", text: "kept" });
const namedAxes = labelIds.space({ id: "axes", x: [-2, 2, 1], y: [-2, 2, 1], z: [-2, 2, 1],
    labels: { x: [[1, keptLabel], [-1, "first"]], y: [[1, "up"]], z: [[1, "depth"]] } });
const firstLabel = namedAxes.children()[0];
assert.equal(labelIds.find("axes/label/x/2"), firstLabel);
const moreLabels = namedAxes.labels({ x: [[1, "second"], [-1, keptLabel]], y: [[-1, "down"]] });
const lastLabel = namedAxes.labels({ x: [[2, "third"]] })[0];
assert.equal(labelIds.find("axes/label/x/1"), keptLabel);
assert.equal(moreLabels[1], keptLabel);
assert.equal(labelIds.find("axes/label/x/2"), firstLabel);
assert.equal(labelIds.find("axes/label/x/3"), moreLabels[0]);
assert.equal(labelIds.find("axes/label/x/4"), lastLabel);
assert.equal(labelIds.find("axes/label/y/1"), namedAxes.children()[1]);
assert.equal(labelIds.find("axes/label/y/2"), moreLabels[2]);
assert.equal(labelIds.find("axes/label/z/1"), namedAxes.children()[2]);
const anonymousAxes = labelIds.space({ labels: { x: [[1, "anonymous"]] } });
assert.equal(anonymousAxes.labels({ x: [[2, "another"]] }).length, 1);
assert.equal(namedAxes.children().length, 6);
// labels() makes and lays out objects, so the tracks that wait keep waiting: tracks of two
// trackers on either side of it in a parallel merge on the dot they both move, as in the Lua
// twin, where a call that wrote the waiting segments would make the second track fail.
const labeled = vis.scene({ width: 160, height: 90, fps: 10, camera: { mode: "fixed", view: "2d", height: 6 } });
const labeledX = labeled.tracker(0);
const labeledZ = labeled.tracker(1);
const labeledAxes = labeled.space({ x: [-3, 3, 1], y: [-2, 2, 1], ticks: true });
const labeledDot = labeled.circle({ center: [-2, 1], radius: 0.2, fill: "#ffffff", stroke: "#00000000" });
labeled.bind(labeledDot, [labeledX, labeledZ], (x, z) => ({ shift: [x, 0], radius: 0.2 * z }));
labeled.parallel(
    (scene) => scene.track(labeledX, 4, 4, "linear"),
    (scene) => {
        scene.wait(1);
        labeledAxes.labels({ x: [[1, "one"], [-1, "minus one"]] });
        scene.track(labeledZ, 3, 1, "linear");
    },
);
const labeledScript = [
    'local s = vis.scene {width = 160, height = 90, fps = 10, camera = {mode = "fixed", view = "2d", height = 6}}',
    "local x = s:tracker(0)",
    "local z = s:tracker(1)",
    "local axes = s:space {x = {-3, 3, 1}, y = {-2, 2, 1}, ticks = true}",
    'local dot = s:circle {center = {-2, 1}, radius = 0.2, fill = "#ffffff", stroke = "#00000000"}',
    "s:bind(dot, {x, z}, function(a, b) return {shift = {a, 0}, radius = 0.2 * b} end)",
    's:parallel(function() s:track(x, 4, 4, "linear") end, function()',
    "    s:wait(1)",
    '    axes:labels {x = {{1, "one"}, {-1, "minus one"}}}',
    '    s:track(z, 3, 1, "linear")',
    "end)",
    "return s",
].join("\n");

const dashboard = vis.scene({ width: 320, height: 180 });
dashboard.space({ id: "dashboard" });
const panelConfig = { width: 160, height: 90, camera: { view: "2d", height: 6 } };
const panel = vis.scene(panelConfig);
const panelWorld = panel.space({ id: "panel" });
const panelPoint = panelWorld.point({ point: [0, 0], id: "panel-point" });
panel.create(panelPoint, 0.5);
const insetConfig = { width: 80, height: 45 };
const inset = vis.scene(insetConfig);
const insetWorld = inset.space({ id: "inset" });
insetWorld.circle({ radius: 0.5, id: "inset-circle" });
panel.viewport(inset, { x: 0.5, y: 0, width: 0.5, height: 0.5 });
dashboard.viewport(panel, { x: 0, y: 0, width: 0.5, height: 1 });
panelConfig.width = 999;
panelConfig.camera.height = 9;
insetConfig.height = 999;
assert.throws(() => {
    panel.config = { width: 999 };
}, TypeError);
assert.throws(() => {
    panel.lines = [() => "wait(7)"];
}, TypeError);

const viewportLua = compileScene(dashboard);
assert.doesNotMatch(viewportLua, /999|:wait\([79]\)/);
assert.match(viewportLua, /objects\[1\] = scene:space/);
assert.match(viewportLua, /do\n    local viewportScene1 = vis\.scene/);
assert.match(viewportLua, /    objects\[1\] = viewportScene1:space/);
assert.match(viewportLua, /    viewportScene1:create\(objects\[2\]/);
assert.match(viewportLua, /    do\n        local viewportScene2 = vis\.scene/);
assert.match(viewportLua, /        objects\[1\] = viewportScene2:space/);
assert.match(
    viewportLua,
    /        viewportScene1:viewport\(viewportScene2,\{\["height"\]=0\.5,\["width"\]=0\.5,\["x"\]=0\.5,\["y"\]=0\}\)/,
);
assert.match(
    viewportLua,
    /    scene:viewport\(viewportScene1,\{\["height"\]=1,\["width"\]=0\.5,\["x"\]=0,\["y"\]=0\}\)/,
);

const rect = { x: 0, y: 0, width: 1, height: 1 };
const self = vis.scene();
assert.throws(() => self.viewport(self, rect), /own viewport child/);
assert.throws(() => self.viewport({}, rect), /viscript JS scene/);
const invalidBoundsChild = vis.scene();
assert.throws(() => self.viewport(invalidBoundsChild, { x: -0.1, y: 0, width: 1, height: 1 }), /normalized/);
assert.throws(() => self.viewport(invalidBoundsChild, { x: 0, y: 0, width: 0, height: 1 }), /positive/);
assert.throws(() => self.viewport(invalidBoundsChild, { x: 0.5, y: 0, width: 0.6, height: 1 }), /normalized/);
const firstOwner = vis.scene();
const secondOwner = vis.scene();
const reused = vis.scene();
const reusedGroup = reused.group();
const reusedBox = reusedGroup.rectangle();
firstOwner.viewport(reused, rect);
assert.throws(() => firstOwner.viewport(reused, rect), /already owned/);
assert.throws(() => secondOwner.viewport(reused, rect), /already owned/);
assert.throws(() => reused.wait(1), /sealed/);
assert.throws(() => reused.space(), /sealed/);
assert.throws(() => reused.point({ point: [0, 0] }), /sealed/);
assert.throws(() => reusedBox.point({ point: [0, 0] }), /sealed/);
assert.throws(() => reusedBox.moveTo([0, 0]), /sealed/);
assert.throws(() => reusedGroup.arrange(), /sealed/);
assert.throws(() => reused.play({ target: reusedBox, opacity: 0.5 }), /sealed/);
const ancestor = vis.scene();
const descendant = vis.scene();
ancestor.viewport(descendant, rect);
assert.throws(() => descendant.viewport(ancestor, rect), /cycle/);

const transitionRoot = vis.scene({ width: 320, height: 180 });
const transitionFirst = vis.scene({ width: 160, height: 90 });
transitionFirst.circle({ center: [-1, 0], radius: 0.7, fill: "#4cc9f0", id: "hero" });
const transitionSecond = vis.scene({ width: 320, height: 180 });
transitionSecond.rectangle({ center: [1, 0], size: [1.8, 1.2], fill: "#f72585", id: "hero" });
transitionRoot.sceneTransition([transitionFirst, transitionSecond], {
    duration: 0.8,
    hold: 0.25,
    curve: vis.animCurve.preset("snappy", 0.8),
    viewport: { x: 0.1, y: 0.1, width: 0.8, height: 0.8 },
});
const transitionLua = compileScene(transitionRoot);
assert.match(transitionLua, /local transitionScene1_1 = vis\.scene/);
assert.match(transitionLua, /local transitionScene1_2 = vis\.scene/);
assert.match(transitionLua, /scene:scene_transition\(\{transitionScene1_1,transitionScene1_2\}/);
assert.match(transitionLua, /\["duration"\]=0\.8/);
assert.throws(() => transitionFirst.wait(1), /sealed/);
assert.throws(() => vis.scene().sceneTransition([], {}), /at least two/);
assert.throws(() => vis.scene().sceneTransition([vis.scene(), {}]), /viscript JS scenes/);
const duplicateTransitionStage = vis.scene();
assert.throws(() => vis.scene().sceneTransition([duplicateTransitionStage, duplicateTransitionStage]), /unique/);
assert.throws(() => vis.scene().sceneTransition([vis.scene(), vis.scene()], { duration: 0 }), /positive/);
assert.throws(() => vis.scene().sceneTransition([vis.scene(), vis.scene()], { hold: -1 }), /non-negative/);
assert.throws(
    () => vis.scene().sceneTransition([vis.scene(), vis.scene()], { easing: "smooth" }),
    /Unknown scene transition option: easing/,
);
const invalidTransitionRoot = vis.scene();
const invalidTransitionFirst = vis.scene();
const invalidTransitionSecond = vis.scene();
assert.throws(
    () => invalidTransitionRoot.sceneTransition([invalidTransitionFirst, invalidTransitionSecond], { curve() {} }),
    /plain objects/,
);
invalidTransitionFirst.wait(0.1);
invalidTransitionRoot.sceneTransition([invalidTransitionFirst, invalidTransitionSecond]);
assert.throws(() => invalidTransitionRoot.sceneTransition([vis.scene(), vis.scene()]), /only one/);

const liveConfig = { width: 120, height: 60, background: "#000000", camera: { height: 4 }, antialiasing: false };
const liveRoot = vis.scene(liveConfig);
const liveFirst = vis.scene(liveConfig);
const liveDot = liveFirst.circle({ center: [-1, 0], radius: 0.2, fill: "#ffffff", stroke: "#00000000" });
liveFirst.shift(liveDot, [4, 0], 4, "linear");
const liveSecond = vis.scene(liveConfig);
const liveNext = liveSecond.circle({ center: [-1, 0], radius: 0.2, fill: "#ffffff", stroke: "#00000000" });
liveSecond.shift(liveNext, [1, 0], 0.5, "linear");
liveRoot.sceneTransition([liveFirst, liveSecond], { hold: [2, 1], duration: 1, style: "slide", curve: "linear" });
const liveLua = compileScene(liveRoot);
assert.match(liveLua, /\["hold"\]=\{2,1\}/);
const frozenRoot = vis.scene();
frozenRoot.sceneTransition([vis.scene(), vis.scene()], { live: false });
assert.match(compileScene(frozenRoot), /\["live"\]=false/);
for (const hold of [[1], [1, -1], [1, Infinity], [1, "slow"], Array(2)]) {
    assert.throws(() => vis.scene().sceneTransition([vis.scene(), vis.scene()], { hold }), /hold/);
}
assert.throws(() => vis.scene().sceneTransition([vis.scene(), vis.scene()], { live: 1 }), /boolean/);

// Styled transitions compile to the options of scene_transition and run as the script does.
const styledStage = (background, x) => {
    const stage = vis.scene({ width: 200, height: 100, background, camera: { height: 4 } });
    stage.rectangle({ center: [x, 0], size: [1, 1], fill: "#ffffff", id: "card" });
    return stage;
};
const styledRoot = vis.scene({ width: 200, height: 100, background: "#ff0000" });
styledRoot.sceneTransition([styledStage("#000000", -2), styledStage("#202020", 0), styledStage("#404040", 2)], {
    duration: 1,
    curve: "linear",
    viewport: { x: 0.1, y: 0.2, width: 0.8, height: 0.6 },
    styles: ["fade", { style: "match" }],
});
const styledLua = compileScene(styledRoot);
assert.match(styledLua, /\["styles"\]=\{\{\["style"\]="fade"\},\{\["style"\]="match"\}\}/);
const fadeRoot = vis.scene();
fadeRoot.sceneTransition([vis.scene(), vis.scene()], { style: "fade" });
assert.match(compileScene(fadeRoot), /\["style"\]="fade"/);
const slideRoot = vis.scene({ width: 200, height: 100, background: "#ff0000" });
slideRoot.sceneTransition([styledStage("#000000", -2), styledStage("#ffffff", 2)], {
    duration: 1,
    curve: "linear",
    viewport: { x: 0.1, y: 0.2, width: 0.8, height: 0.6 },
    style: "slide",
    from: "top",
});
const slideLua = compileScene(slideRoot);
assert.match(slideLua, /\["style"\]="slide"/);
assert.match(slideLua, /\["from"\]="top"/);
const slidesRoot = vis.scene();
slidesRoot.sceneTransition([vis.scene(), vis.scene(), vis.scene()], {
    styles: [{ style: "slide", from: "left" }, "match"],
});
assert.match(compileScene(slidesRoot), /\{\["from"\]="left",\["style"\]="slide"\}/);
const zoomRoot = vis.scene({ width: 200, height: 100, background: "#ff0000" });
zoomRoot.sceneTransition([styledStage("#000000", -2), styledStage("#ffffff", 2), styledStage("#000000", 0)], {
    duration: 1,
    curve: "linear",
    viewport: { x: 0.1, y: 0.2, width: 0.8, height: 0.6 },
    styles: [{ style: "zoom_in", area: { x: 0.5, y: 0.25, width: 0.25, height: 0.25 } }, { style: "zoom_out", area: "card" }],
});
const zoomLua = compileScene(zoomRoot);
assert.match(zoomLua, /\{\["area"\]=\{\["height"\]=0\.25,\["width"\]=0\.25,\["x"\]=0\.5,\["y"\]=0\.25\},\["style"\]="zoom_in"\}/);
assert.match(zoomLua, /\{\["area"\]="card",\["style"\]="zoom_out"\}/);
const exactZooms = [];
for (const style of ["zoom_in", "zoom_out"]) {
    for (const width of [0, 3, 18]) {
        const build = (byObject) => {
            const config = { width: 200, height: 100, antialiasing: false, camera: { height: 100 } };
            const stages = [vis.scene({ ...config, background: "#000000" }), vis.scene({ ...config, background: "#ffffff" })];
            const owner = stages[style === "zoom_in" ? 0 : 1];
            owner.rectangle({ id: "card", center: [50, 20], size: [50, 25], fill: "#00ff00", stroke: "#ff0000", width });
            const root = vis.scene(config);
            const region = byObject ? { area: "card", area_margin: 0 }
                                    : { area: { x: 0.625, y: 0.175, width: 0.25, height: 0.25 } };
            // Cover the shared style and per-transition style forms.
            const transition = { style, ...region };
            root.sceneTransition(stages, { duration: 1, curve: "linear",
                ...(style === "zoom_in" ? transition : { styles: [transition] }) });
            assert.throws(() => owner.wait(1), /sealed/);
            return compileScene(root);
        };
        const objectLua = build(true);
        assert.match(objectLua, /\["area_margin"\]=0/);
        exactZooms.push({ objectLua, areaLua: build(false) });
    }
}
const styleError = (options, pattern, count = 2) =>
    assert.throws(() => vis.scene().sceneTransition(Array.from({ length: count }, () => vis.scene()), options), pattern);
styleError({ style: "wipe" }, /Scene transition style must be match, fade, slide, zoom_in or zoom_out/);
styleError({ style: "zoom_in" }, /zoom needs an area/);
styleError({ styles: ["zoom_out"] }, /zoom needs an area/);
styleError({ style: "fade", area: "card" }, /area applies only to the zoom styles/);
styleError({ style: "zoom_in", area: { x: 0, y: 0, width: 1 } }, /area must be an object id or/);
styleError({ style: "zoom_in", area: { x: 0, y: 0, width: 1, height: 1, depth: 1 } }, /area must be an object id or/);
styleError({ area: "card", styles: ["fade"] }, /either area or styles/);
for (const area_margin of [-1, Infinity, NaN, 1e300, true, "0"]) {
    styleError({ style: "zoom_in", area: "card", area_margin }, /area_margin must be a finite non-negative number/);
}
styleError({ style: "zoom_in", area: { x: 0, y: 0, width: 1, height: 1 }, area_margin: 0 }, /area_margin applies only to an object zoom area/);
styleError({ style: "fade", area_margin: 0 }, /area_margin applies only to an object zoom area/);
styleError({ area_margin: 0, styles: ["fade"] }, /either area_margin or styles/);
styleError({ styles: [{ style: "zoom_out", area: "card", area_margin: -1 }] }, /area_margin must be a finite non-negative number/);
const failedMarginStages = [styledStage("#000000", -2), styledStage("#ffffff", 2)];
const failedMarginRoot = vis.scene();
assert.throws(() => failedMarginRoot.sceneTransition(failedMarginStages,
    { style: "zoom_in", area: "card", area_margin: -1 }), /area_margin/);
assert.doesNotThrow(() => failedMarginStages[0].wait(0.25));
assert.doesNotThrow(() => failedMarginRoot.sceneTransition(failedMarginStages,
    { style: "zoom_in", area: "card", area_margin: 0.1 }));
styleError({ style: "fade", match: true }, /match applies only to the slide and zoom styles/);
styleError({ style: "slide", match: 1 }, /match must be a boolean/);
const pairRoot = vis.scene({ width: 200, height: 100, background: "#ff0000" });
pairRoot.sceneTransition([styledStage("#000000", -2), styledStage("#ffffff", 2)], {
    duration: 1,
    curve: "linear",
    styles: [{ style: "slide", from: "bottom", match: true }],
});
const pairLua = compileScene(pairRoot);
assert.match(pairLua, /\{\["from"\]="bottom",\["match"\]=true,\["style"\]="slide"\}/);
styleError({ style: "slide", from: "up" }, /from must be left, right, top or bottom/);
styleError({ style: "fade", from: "left" }, /from applies only to the slide style/);
styleError({ from: "left", styles: ["slide"] }, /either from or styles/);
styleError({ style: "fade", styles: ["fade"] }, /either style or styles/);
styleError({ styles: ["fade"] }, /one style for each of the 2 transitions/, 3);
styleError({ styles: [7] }, /style name or a plain object/);
styleError({ styles: [{ style: "fade", speed: 2 }] }, /Unknown scene transition style option: speed/);

const evaluated = evaluateScene(
    `
const scene = vis.scene({width: 64, height: 64});
const world = scene.space();
const frame = world.space({id: "frame"});
frame.point({point: [0, 0], id: "origin"});
frame.picture({asset: "pixels", center: [0, 0], width: 2, filter: "nearest"});
frame.picture({pixels: ["#ff0000", "#00ff00"], size: [2, 1], width: 2, filter: "nearest"});
frame.cell({size: [4, 2], texture: "pixels", source: [0, 0, 4, 2], mode: "padd"});
const row = scene.group({id: "row"});
const card = row.rectangle({size: [1.5, 0.8], corner: 0.1});
card.text({text: "card"});
row.arrange();
scene.play({target: card, opacity: 0.75});
const child = vis.scene({width: 32, height: 32});
child.space().point({point: [0, 0], id: "child-origin"});
scene.viewport(child, {x: 0.5, y: 0.5, width: 0.5, height: 0.5});
return scene;
`,
    "evaluated.js",
);
assert.match(compileScene(evaluated), /\["id"\]="origin"/);
assert.match(compileScene(evaluated), /objects\[1\]:space/);
assert.match(compileScene(evaluated), /objects\[2\]:picture/);
assert.match(compileScene(evaluated), /\["pixels"\]=\{"#ff0000","#00ff00"\}/);
assert.match(compileScene(evaluated), /objects\[2\]:cell/);
assert.match(compileScene(evaluated), /scene:group/);
assert.match(compileScene(evaluated), /:rectangle/);
assert.match(compileScene(evaluated), /:arrange\(/);
assert.match(compileScene(evaluated), /scene:play\(\{\{target=/);
assert.match(compileScene(evaluated), /scene:viewport\(viewportScene1/);

const isolatedScene = vis.scene();
isolatedScene.group({id: "card", isolate: true, opacity: 0.5}).rectangle({size: [2, 1]});
assert.match(compileScene(isolatedScene), /scene:group \{\["id"\]="card",\["isolate"\]=true,\["opacity"\]=0\.5\}/);

const maskedScene = vis.scene();
const hole = maskedScene.circle({id: "hole", radius: 0.5});
const wipe = maskedScene.rectangle({id: "wipe", size: [1, 1]});
const glow = maskedScene.circle({id: "glow", radius: 0.25});
maskedScene.rectangle({id: "image", size: [2, 2], mask: hole});
maskedScene.rectangle({id: "revealed", size: [2, 1], mask: {target: wipe, mode: "inverse_alpha"}});
const lit = maskedScene.rectangle({id: "lit", size: [1, 1]});
lit.mask(glow, "luma");
const maskedSource = compileScene(maskedScene);
assert.match(maskedSource, /scene:rectangle \{\["id"\]="image",\["mask"\]=objects\[1\],\["size"\]=\{2,2\}\}/);
assert.match(maskedSource, /\["mask"\]=\{\["mode"\]="inverse_alpha",\["target"\]=objects\[2\]\}/);
assert.match(maskedSource, /objects\[6\]:mask\(objects\[3\],"luma"\)/);
maskedScene.circle({id: "shade", radius: 0.5, blend: "multiply"});
const soft = maskedScene.rectangle({id: "soft", size: [2, 1], blur: 2, shadow: {offset: [0, -4], blur: 8}});
maskedScene.play({target: soft, blur: 0, shadow: {color: "#ff000080"}}, 1);
const softSource = compileScene(maskedScene);
assert.match(softSource, /\["blur"\]=2,\["id"\]="soft",\["shadow"\]=\{\["blur"\]=8,\["offset"\]=\{0,-4\}\}/);
assert.match(softSource, /scene:play\(\{\{target=objects\[8\],blur=0,shadow=\{\["color"\]="#ff000080"\}\}\}/);
assert.match(compileScene(maskedScene), /scene:circle \{\["blend"\]="multiply",\["id"\]="shade",\["radius"\]=0\.5\}/);
assert.throws(() => maskedScene.rectangle({mask: {target: hole, mode: "glow"}}), /Mask mode must be/);
assert.throws(() => maskedScene.rectangle({mask: {mode: "alpha"}}), /Mask requires a target/);
assert.throws(() => maskedScene.rectangle({mask: {target: hole, soft: true}}), /Unknown mask option: soft/);
assert.throws(() => maskedScene.rectangle({mask: 1}), /Mask must be a handle/);
assert.throws(() => lit.mask(vis.scene().circle({})), /same scene/);
assert.throws(() => lit.mask(glow, "bright"), /Mask mode must be/);
assert.throws(
    () =>
        vis
            .scene()
            .space()
            .point({ point: [Number.NaN, 0] }),
    /finite/,
);
assert.equal(typeof vis.scene().point, "function");
assert.equal(vis.scene().grid, undefined);
assert.equal(world.grid, undefined);
assert.throws(() => vis.scene().fade(point, 0), /same scene/);
assert.throws(
    () =>
        evaluateScene(
            "const scene=vis.scene(); scene.viewport(scene,{x:0,y:0,width:1,height:1}); return scene",
            "self-viewport.js",
        ),
    /own viewport child/,
);
assert.throws(
    () =>
        evaluateScene(
            "const root=vis.scene(); const child=vis.scene(); root.viewport(child,{x:0,y:0,width:1,height:1}); return child",
            "owned-viewport.js",
        ),
    /owning root/,
);
assert.throws(() => evaluateScene("return 42", "invalid.js"), /must return/);

const audioScene = vis.scene();
const music = audioScene.sound({
    asset: "theme.ogg", bus: "music", begin: 0.25, end: 8, gain: 0.6, loop: true,
});
music.gain(0.2, 4, 0.5, "ease_out").gain(0.8, 5, 1, "smooth");
audioScene.wait(8);
const audioLua = compileScene(audioScene);
// vis.audio exists only with the audio module, so a cue and its gains are guarded by it.
assert.deepEqual(audioLua.split("\n").slice(1, 5), [
    "local cues = {}",
    'if vis.audio then cues[1]=vis.audio.cue(scene,{["asset"]="theme.ogg",["begin"]=0.25,["bus"]="music",'
        + '["end"]=8,["gain"]=0.6,["loop"]=true}) end',
    'if cues[1] then cues[1]:gain(0.2,4,0.5,"ease_out") end',
    'if cues[1] then cues[1]:gain(0.8,5,1,"smooth") end',
]);
assert.throws(() => vis.scene().sound({asset: ""}), /asset/);
assert.throws(() => vis.scene().sound({asset: "hit.wav", bus: "voice"}), /bus/);
assert.throws(() => vis.scene().sound({asset: "hit.wav", gain: 5}), /gain/);
assert.throws(() => vis.scene().sound({asset: "hit.wav", begin: 2, end: 1}), /end/);
assert.throws(() => vis.scene().sound({asset: "hit.wav", unknown: true}), /Unknown/);
assert.throws(() => vis.scene().sound({asset: "hit.wav"}).gain(1, -1), /begin/);

const tracks = vis.scene({ width: 160, height: 90 });
const runner = tracks.circle({ radius: 0.4, id: "runner" });
const badge = tracks.rectangle({ size: [1, 1], id: "badge" });
tracks.parallel(
    (scene) => scene.shift(runner, [2, 0], 3, "linear"),
    (scene) => scene.fadeIn(badge, { duration: 1 }).mark("badge"),
);
tracks.at("badge", (scene) => scene.fill(runner, "#ff0000", 0.5));
tracks.at(4, (scene) => scene.fade(runner, 0.5, 1));
tracks.parallel([(scene) => scene.shift(badge, [0, 1], 1)]);
const tracksLua = compileScene(tracks);
assert.match(tracksLua, /times\[1\]=scene:time\(\);times\[2\]=times\[1\]/);
assert.match(tracksLua, /scene:mark\("badge"\)/);
assert.match(tracksLua, /scene:seek\("badge"\)/);
assert.throws(() => vis.scene().parallel(), /at least one track/);
assert.throws(() => vis.scene().parallel(() => {}, 1), /functions/);
assert.throws(() => vis.scene().at(-1, () => {}), /time/);
assert.throws(() => vis.scene().at(1), /callback/);
assert.throws(() => vis.scene().mark(""), /Marker/);

// A look that turns into 3D and changes the lens adds to a running pan.
const lensed = vis.scene({ width: 160, height: 90 });
lensed.rectangle({ size: [1, 1] });
lensed.parallel(
    (scene) => scene.look({ target: [2, 0] }, 2, "linear"),
    (scene) => scene.wait(0.5).look({ view: "3d", eye: [2, -8, 6], projection: "perspective", fov: 0.6 }, 1),
);
const lensedLua = compileScene(lensed);

// Bound objects follow a tracker: bind sets the state its function gives, and track plays one
// linear segment a frame through the states the eased values give, as Lua scenes do.
const trail = vis.scene({ width: 160, height: 90, fps: 4 });
const walker = trail.circle({ radius: 0.2, id: "walker" });
const meter = trail.number({ value: 0, id: "meter" });
const spare = trail.circle({ radius: 0.1, id: "spare" });
const pace = trail.tracker(1);
const gain = trail.tracker(0.5);
const walked = [];
assert.equal(trail.bind(walker, pace, (v) => (walked.push(v), { shift: [v, 1], opacity: v / 4 })), walker);
trail.bind(meter, [pace, gain], (v, k) => ({ value: v * k }));
assert.equal(trail.track(pace, 3, 1, "linear"), trail);
assert.deepEqual(walked, [1, 1.5, 2, 2.5, 3]);
assert.equal(pace.get(), 3);
trail.track([[gain, 2]], 0.5, "linear");
trail.track(pace, 1, 0.5);
assert.deepEqual(walked, [1, 1.5, 2, 2.5, 3, 2, 1]);
trail.track(trail.tracker(0), 1, 0.25);
const trailBefore = compileScene(trail);
const otherTracker = vis.scene().tracker(0);
assert.throws(() => trail.tracker(Number.NaN), /Tracker value must be a finite number/);
assert.throws(() => trail.tracker("1"), /Tracker value must be a finite number/);
assert.throws(() => trail.bind(spare, pace, null), /Bind requires a function/);
assert.throws(() => trail.bind(spare, [], () => ({})), /Bind requires a tracker or an array of trackers/);
assert.throws(() => trail.bind(spare, [pace, {}], () => ({})), /Expected a tracker from scene\.tracker/);
assert.throws(() => trail.bind(spare, otherTracker, () => ({})), /Tracker belongs to another scene/);
assert.throws(() => trail.bind(walker, gain, () => ({ opacity: 1 })), /An object can follow only one binding/);
assert.throws(() => trail.bind(spare, pace, () => 1), /A bound function must return a state object/);
assert.throws(() => trail.bind(spare, pace, () => ({ spin: 1 })), /A bound state cannot set 'spin'/);
assert.throws(() => trail.bind(spare, pace, () => ({ shift: [1, 0], arc: 1 })), /A bound state cannot set 'arc'/);
assert.throws(
    () => trail.bind(spare, pace, () => ({ shift: [1, 0], transform: [1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1] })),
    /A bound state cannot contain both shift and transform/,
);
assert.throws(() => trail.bind(spare, pace, () => ({ shift: 1 })), /A bound shift must be an array of numbers/);
for (const duration of [undefined, 0, -1, Infinity, Number.NaN, "1"]) {
    assert.throws(() => trail.track(pace, 2, duration), /Track duration must be a positive finite number/);
}
assert.throws(() => trail.track(pace, Number.NaN, 1), /Track target must be a finite number/);
assert.throws(() => trail.track([], 1), /Track requires a tracker or an array of \[tracker, value\]/);
assert.throws(() => trail.track(1, 2, 1), /Track requires a tracker or an array of \[tracker, value\]/);
assert.throws(() => trail.track([[pace, 2], [{}, 2]], 1), /Expected a tracker from scene\.tracker/);
assert.throws(() => trail.track([[otherTracker, 2]], 1), /Tracker belongs to another scene/);
assert.throws(() => trail.track(pace, 2, 1, "wobble"), /Unknown animation curve preset: wobble/);
assert.equal(pace.get(), 1);
assert.equal(compileScene(trail), trailBefore);
trail.bind(spare, gain, (k) => ({ opacity: k / 2 }));
const trailLua = compileScene(trail);
assert.match(trailLua, /scene:set\(\{\{target=objects\[1\],shift=\{1,1,0\},opacity=0\.25\}\}\)/);
assert.match(trailLua, /scene:set\(\{\{target=objects\[2\],value=0\.5\}\}\)/);
assert.match(
    trailLua,
    /scene:play\(\{\{target=objects\[1\],shift=\{0\.5,0,0\},opacity=0\.375\},\{target=objects\[2\],value=0\.75\}\},times\[2\]-times\[1\],"linear",0\)/,
);
assert.match(trailLua, /scene:play\(\{\{target=objects\[2\],value=3\.75\}\},times\[7\]-times\[6\],"linear",0\)/);
assert.match(trailLua, /scene:play\(\{\{target=objects\[1\],shift=\{-1,0,0\},opacity=0\.5\},\{target=objects\[2\],value=4\}\},times\[10\]-times\[9\],"linear",0\)/);
assert.match(trailLua, /scene:wait\(0\.25\)/);
assert.match(trailLua, /scene:set\(\{\{target=objects\[3\],opacity=1\}\}\)/);
assert.equal(trailLua.match(/scene:play\(/g).length, 8);

// A track takes one segment a frame, at least one; 0.28 * 25 is a hair over 7.
const stepping = vis.scene({ fps: 25 });
const tick = stepping.tracker(0);
stepping.bind(stepping.circle({ radius: 0.2 }), tick, (v) => ({ opacity: v }));
const segments = () => compileScene(stepping).match(/scene:play\(/g).length;
stepping.track(tick, 1, 0.28);
assert.equal(segments(), 7);
stepping.track(tick, 0, 0.001);
assert.equal(segments(), 8);
stepping.track(tick, 1, 20);
assert.equal(segments(), 508);
assert.match(compileScene(stepping), /scene:seek\(times\[1\]\+0\.04\)/);

const longTrack = vis.scene({ width: 160, height: 90, fps: 30 });
const longValue = longTrack.tracker(0);
const longNumber = longTrack.number({ value: 0, decimals: 0, id: "n" });
const longLine = longTrack.line({ from: [0, 0], to: [1, 0], id: "line" });
let longSamples = 0;
longTrack.bind(longNumber, longValue, (v) => {
    longSamples++;
    return { value: Math.floor(v * 600 + 0.5) };
});
longTrack.bind(longLine, longValue, (v) => ({ to: [1, v * v] }));
longTrack.track(longValue, 1, 20, "linear");
assert.equal(longSamples, 601);
const longTrackScript = `
local s=vis.scene{width=160,height=90,fps=30}
local t=s:tracker(0)
local n=s:number{value=0,decimals=0,id="n"}
local line=s:line{from={0,0},to={1,0},id="line"}
s:bind(n,t,function(v) return {value=math.floor(v*600+0.5)} end)
s:bind(line,t,function(v) return {to={1,v*v}} end)
s:track(t,1,20,"linear")
return s
`;
const minuteTrack = vis.scene({ width: 160, height: 90, fps: 60 });
const minuteValue = minuteTrack.tracker(0);
let minuteSamples = 0;
minuteTrack.bind(minuteTrack.number({ value: 0, decimals: 0, id: "n" }), minuteValue, (v) => {
    minuteSamples++;
    return { value: Math.floor(v * 3600 + 0.5) };
});
minuteTrack.track(minuteValue, 1, 60, "linear");
assert.equal(minuteSamples, 3601);
const minuteTrackScript = `
local s=vis.scene{width=160,height=90,fps=60}
local t=s:tracker(0)
local n=s:number{value=0,decimals=0,id="n"}
s:bind(n,t,function(v) return {value=math.floor(v*3600+0.5)} end)
s:track(t,1,60,"linear")
return s
`;
assert.throws(() => longTrack.track(longValue, 0, 1e30), /65536-clip budget/);
assert.equal(longValue.get(), 1);

// Linear tracker callbacks keep double precision before a binding stores its retained state.
const preciseTrack = vis.scene({ fps: 30 });
const preciseCount = preciseTrack.tracker(0);
let preciseStep = 0;
preciseTrack.bind(preciseTrack.number({ value: 0, id: "n" }), preciseCount, (value) => {
    assert.equal(value, 2000 * (preciseStep++ / 240));
    return { value: Math.floor(value) };
});
preciseTrack.wait(1).track(preciseCount, 2000, 8, "linear").wait(1);
assert.equal(preciseStep, 241);
for (const curve of [{ preset: "linear", reverse: true }, { preset: "smooth", strength: 0 },
    { bezier: [0.2, 0.3, 0.8, 0.9], strength: 0, reverse: true }]) {
    const scene = vis.scene({ fps: 30 });
    const value = scene.tracker(0);
    let step = 0;
    scene.bind(scene.circle({ radius: 1 }), value, (v) => {
        assert.equal(v, 2000 * (step++ / 240));
        return { opacity: 0.5 };
    });
    scene.track(value, 2000, 8, curve);
}

const timedTracks = [24, 30, 60].map((fps) => {
    const scene = vis.scene({ fps });
    const seconds = scene.tracker(0);
    scene.bind(scene.number({ value: 0 }), seconds, (value) => ({ value }));
    scene.wait(0.5).track(seconds, 7.5, 7.5, "linear").mark("end");
    return [fps, scene];
});

// Scenes a Lua twin mirrors line for line: one that tracks through every kind of curve and then
// focuses, one whose bound shift rides a running scale, and one that records the values its
// function sees.
const twin = vis.scene({ width: 160, height: 90, fps: 10 });
const twinDot = twin.circle({ center: [0, 0], radius: 0.2, id: "dot" });
const twinLine = twin.plot({ points: Array.from({ length: 21 }, (_, i) => [i / 10, i / 10]), id: "line" });
const twinX = twin.tracker(0);
twin.bind(twinDot, twinX, (v) => ({ shift: [v, 0], opacity: 0.6 + v / 10 }));
twin.bind(twinLine, twinX, (v) => ({ progress: 0.25 + v / 4 }));
twin.track(twinX, 2, 1, "linear").track([[twinX, 1]], 0.5).track(twinX, 1.7, 0.7, "back");
twin.track(twinX, 0.2, 0.9, "elastic").track(twinX, 1.3, 0.6, { bezier: [0.3, -0.4, 0.6, 1.6], strength: 0.8 });
twin.track(twinX, 0.5, 0.45, { preset: "ease_in_out", reverse: true }).track(twin.tracker(0), 1, 0.25);
twin.focus(twinDot, { padding: 0.5, duration: 0.25 }).focus([twinDot, twinLine]).focus(null, { duration: 0.5 });
const twinScript = [
    "local s = vis.scene {width = 160, height = 90, fps = 10}",
    "local points = {}",
    "for i = 0, 20 do points[#points + 1] = {i / 10, i / 10} end",
    'local dot = s:circle {center = {0, 0}, radius = 0.2, id = "dot"}',
    'local line = s:plot {points = points, id = "line"}',
    "local x = s:tracker(0)",
    "s:bind(dot, x, function(v) return {shift = {v, 0}, opacity = 0.6 + v / 10} end)",
    "s:bind(line, x, function(v) return {progress = 0.25 + v / 4} end)",
    's:track(x, 2, 1, "linear") s:track({{x, 1}}, 0.5) s:track(x, 1.7, 0.7, "back")',
    's:track(x, 0.2, 0.9, "elastic") s:track(x, 1.3, 0.6, {bezier = {0.3, -0.4, 0.6, 1.6}, strength = 0.8})',
    's:track(x, 0.5, 0.45, {preset = "ease_in_out", reverse = true}) s:track(s:tracker(0), 1, 0.25)',
    "s:focus(dot, {padding = 0.5, duration = 0.25}) s:focus({dot, line}) s:focus(nil, {duration = 0.5})",
    "return s",
].join("\n");
const ride = vis.scene({ width: 160, height: 80, camera: { mode: "fixed", view: "2d", height: 4 } });
const rider = ride.rectangle({ center: [-2, 0], size: [1, 1], fill: "#ffffff", stroke: "#00000000", id: "dot" });
const rideX = ride.tracker(0);
ride.bind(rider, rideX, (v) => ({ shift: [v, 0] }));
ride.parallel(
    (scene) => scene.track(rideX, 4, 2, "linear"),
    (scene) => scene.wait(0.5).scale(rider, 2, { duration: 1, curve: "linear" }),
);
const rideScript = [
    'local s = vis.scene {width = 160, height = 80, camera = {mode = "fixed", view = "2d", height = 4}}',
    'local dot = s:rectangle {center = {-2, 0}, size = {1, 1}, fill = "#ffffff", stroke = "#00000000", id = "dot"}',
    "local x = s:tracker(0)",
    "s:bind(dot, x, function(v) return {shift = {v, 0}} end)",
    's:parallel(function() s:track(x, 4, 2, "linear") end,',
    '    function() s:wait(0.5) s:scale(dot, 2, {duration = 1, curve = "linear"}) end)',
    "return s",
].join("\n");
const heardCurves = [
    [undefined, "nil", 0.7],
    ["snappy", '"snappy"', 0.7],
    ["bounce", '"bounce"', 0.7],
    [{ preset: "elastic", strength: 1.3 }, '{preset = "elastic", strength = 1.3}', 2.5],
    [{ preset: "back", reverse: true }, '{preset = "back", reverse = true}', 0.7],
    [{ bezier: [0.17, 0.67, 0.83, 0.67], reverse: true }, "{bezier = {0.17, 0.67, 0.83, 0.67}, reverse = true}", 0.7],
];
const listener = vis.scene({ width: 64, height: 36, fps: 24 });
const listenerLevel = listener.tracker(-1.3);
const heard = [];
listener.bind(listener.circle({ radius: 0.1 }), listenerLevel, (v) => (heard.push(v), { opacity: 0.5 }));
heardCurves.forEach(([curve, , duration], index) => {
    listener.track(listenerLevel, index % 2 ? -1.3 : 2.9, duration, curve);
});
const listenerScript = [
    "local s = vis.scene {width = 64, height = 36, fps = 24}",
    "local level = s:tracker(-1.3)",
    "local heard = {}",
    "s:bind(s:circle {radius = 0.1}, level, function(v) heard[#heard + 1] = v return {opacity = 0.5} end)",
    ...heardCurves.map(([, curve, duration], index) => {
        return `s:track(level, ${index % 2 ? -1.3 : 2.9}, ${duration}, ${curve})`;
    }),
    'for i, v in ipairs(heard) do s:point {point = {0, 0}, id = string.format("%d:%.17g", i, v)} end',
    "return s",
].join("\n");

// A bound state moves the points of a shape under the names its factory takes, and play and set
// take the same keys, as in the Lua twin.
const shaped = vis.scene({ width: 160, height: 90, fps: 10, camera: { mode: "fixed", view: "2d", height: 6 } });
const bow = shaped.curve({ from: [-3, 0], control1: [-1, 0], control2: [1, 0], to: [3, 0], width: 4, id: "bow" });
const beam = shaped.line({ from: [-3, -2], to: [3, -2], width: 4, id: "beam" });
const hand = shaped.vector({ value: [1, 0], origin: [-2, 1], id: "hand" });
const wave = shaped.plot({ points: [[-3, 2], [0, 2], [3, 2]], width: 4, id: "wave" });
const bend = shaped.tracker(0);
shaped.bind(bow, bend, (v) => ({ control2: [1, v] }));
shaped.bind(beam, bend, (v) => ({ to: [3, -2 + v] }));
shaped.bind(hand, bend, (v) => ({ value: [1 - v / 4, v / 2] }));
shaped.bind(wave, bend, (v) => ({ points: [[-3, 2], [0, 2 + v / 2], [3, 2]], opacity: 1 - v / 4 }));
shaped.track(bend, 2, 1, "smooth");
const wedge = shaped.polygon({ points: [[0, 0], [1, 0], [0, 1]], fill: "#ffffff", id: "wedge" });
shaped.play({ target: wedge, points: [[0, 0], [2, 0], [0, 2]] }, 0.5).set({ target: beam, from: [-3, -1] });
const shapedScript = [
    'local s = vis.scene {width = 160, height = 90, fps = 10, camera = {mode = "fixed", view = "2d", height = 6}}',
    'local bow = s:curve {from = {-3, 0}, control1 = {-1, 0}, control2 = {1, 0}, to = {3, 0}, width = 4, id = "bow"}',
    'local beam = s:line {from = {-3, -2}, to = {3, -2}, width = 4, id = "beam"}',
    'local hand = s:vector {value = {1, 0}, origin = {-2, 1}, id = "hand"}',
    'local wave = s:plot {points = {{-3, 2}, {0, 2}, {3, 2}}, width = 4, id = "wave"}',
    "local bend = s:tracker(0)",
    "s:bind(bow, bend, function(v) return {control2 = {1, v}} end)",
    "s:bind(beam, bend, function(v) return {to = {3, -2 + v}} end)",
    "s:bind(hand, bend, function(v) return {value = {1 - v / 4, v / 2}} end)",
    "s:bind(wave, bend, function(v) return {points = {{-3, 2}, {0, 2 + v / 2}, {3, 2}}, opacity = 1 - v / 4} end)",
    's:track(bend, 2, 1, "smooth")',
    'local wedge = s:polygon {points = {{0, 0}, {1, 0}, {0, 1}}, fill = "#ffffff", id = "wedge"}',
    's:play({target = wedge, points = {{0, 0}, {2, 0}, {0, 2}}}, 0.5, "smooth")',
    "s:set {target = beam, from = {-3, -1}}",
    "return s",
].join("\n");
const shapedLua = compileScene(shaped);
assert.match(shapedLua, /scene:set\(\{\{target=objects\[1\],control2=\{1,0\}\}\}\)/);
assert.match(shapedLua, /scene:set\(\{\{target=objects\[4\],opacity=1,points=\{\{-3,2\},\{0,2\},\{3,2\}\}\}\}\)/);
assert.match(shapedLua, /scene:play\(\{\{target=objects\[1\],control2=\{1,2\}\},\{target=objects\[2\],to=\{3,0\}\},/);
assert.match(shapedLua, /scene:set\(\{\{target=objects\[2\],from=\{-3,-1\}\}\}\)/);
assert.equal(shapedLua.match(/scene:play\(/g).length, 11);
// Each key needs a shape that takes it, and a shape keeps its point count, also from one sample
// of a binding to the next; a call that fails leaves the counts the builder knows as they were.
const misshaped = vis.scene();
const tri = misshaped.polygon({ points: [[0, 0], [1, 0], [0, 1]] });
const rod = misshaped.line({ from: [0, 0], to: [1, 0] });
const pin = misshaped.vector({ value: [1, 0] });
const ring = misshaped.circle({ radius: 1 });
const outline = misshaped.path({
    commands: [
        { type: "move", to: [0, 0] },
        { type: "line", to: [1, 0] },
        { type: "line", to: [1, 1] },
    ],
});
const reach = misshaped.tracker(0);
const misshapedBefore = compileScene(misshaped);
assert.throws(
    () => misshaped.play({ target: tri, points: [[0, 0], [1, 1]] }, 1),
    /Animation points must have 3 points, as the polygon has, not 2; a shape keeps its point count/,
);
assert.throws(
    () => misshaped.play({ target: rod, points: [[0, 0], [1, 1]] }, 1),
    /Animation points needs a polygon, plot, route or path of one contour/,
);
assert.throws(() => misshaped.set({ target: rod, origin: [0, 0] }), /Animation origin needs a vector/);
assert.throws(() => misshaped.set({ target: pin, control1: [0, 0] }), /Animation control1 needs a curve/);
assert.throws(
    () => misshaped.play({ target: ring, from: [0, 0] }, 1),
    /Animation from needs a line, arrow, curve or angle; move other objects with shift/,
);
assert.throws(
    () => misshaped.play({ target: tri, points: { x: 0, y: 0 } }, 1),
    /Animation points must be an array of points/,
);
assert.throws(
    () => misshaped.bind(tri, reach, (v) => ({ points: [[0, 0], [1, v]] })),
    /Animation points must have 3 points, as the polygon has, not 2/,
);
assert.throws(
    () => misshaped.play(
        [{ target: outline, points: [[0, 0], [1, 0], [1, 1], [0, 1]] }, { target: pin, from: [0, 0] }],
        1,
    ),
    /Animation from needs a line, arrow, curve or angle/,
);
assert.equal(compileScene(misshaped), misshapedBefore);
misshaped.bind(outline, reach, (v) => ({
    points: v < 0.5 ? [[0, 0], [1, 0], [1, 1 + v]] : [[0, 0], [1, 0], [1, 1], [0, 1]],
}));
assert.throws(
    () => misshaped.track(reach, 1, 1),
    /Animation points must have 3 points, as the path has, not 4; a shape keeps its point count/,
);

// A copy, and each copy below a copied group, keeps the point count of its source. A copy made
// while a tracker runs takes the points its source shows then, and a warp after the track maps
// the points the track leaves. The builder leaves warps to the scene, which refuses new points
// for a warped shape when it loads.
const copied = vis.scene({ width: 160, height: 90, fps: 10, camera: { mode: "fixed", view: "2d", height: 6 } });
const rib = copied.line({ from: [-2, -2], to: [2, -2], width: 2, id: "rib" });
const lift = copied.tracker(0);
copied.bind(rib, lift, (v) => ({ to: [2, -2 + v] }));
copied.track(lift, 2, 1, "linear");
copied.at(0.5, (scene) => scene.clone(rib, { id: "rib2" }));
copied.warp(rib, (x, y) => [x, y + 1], 1, "linear", { x_range: [-3, 3, 1], y_range: [-3, 1, 1] });
const fan = copied.group({ id: "fan" });
const blade = fan.polygon({ points: [[0, 0], [1, 0], [0, 1]], id: "fan/blade" });
const fanCopy = copied.clone(fan, { id: "fan2" });
for (const target of [copied.clone(blade), copied.find("fan2/blade")]) {
    assert.throws(
        () => copied.play({ target, points: [[0, 0], [1, 1]] }, 1),
        /Animation points must have 3 points, as the polygon has, not 2/,
    );
}
copied.play({ target: fanCopy.children()[0], points: [[0, 0], [2, 0], [0, 2]] }, 0.5);
const copiedLua = compileScene(copied);
assert.match(copiedLua, /scene:warp\(objects\[1\],\{\["values"\]=/);
const rewarped = vis.scene({ width: 160, height: 90 });
const rewarpedBeam = rewarped.line({ from: [0, 0], to: [1, 0], id: "beam" });
rewarped.warp(rewarpedBeam, (x, y) => [x, y + 1], 1, "linear", { x_range: [-1, 2, 1], y_range: [-1, 1, 1] });
rewarped.play({ target: rewarpedBeam, to: [1, 1] }, 1);
const rewarpedLua = compileScene(rewarped);
// Tracks of different trackers that overlap on one binding merge there, as in Lua. The twin
// overlaps two tracks in a parallel at frames that interleave, overlays a third with at after a
// play, keeps a binding of one tracker beside them and tracks on after them; its functions see
// the values the Lua functions see, to the last bit.
const merging = vis.scene({ width: 160, height: 90, fps: 10, camera: { mode: "fixed", view: "2d", height: 6 } });
const mergeX = merging.tracker(0);
const mergeZ = merging.tracker(1);
const mergeW = merging.tracker(0);
const mergeDot = merging.circle({ center: [-3, 0], radius: 0.2, fill: "#ffffff", stroke: "#00000000", id: "dot" });
const mergeBar = merging.rectangle({
    center: [0, -2], size: [1, 0.4], fill: "#ffffff", stroke: "#00000000", id: "bar",
});
const mergeRing = merging.circle({ center: [2, 1], radius: 0.5, stroke: "#ffffff", id: "ring" });
const mergeHeard = [];
merging.bind(mergeDot, [mergeX, mergeZ], (x, z) => ({ shift: [x, 0], radius: 0.2 * z }));
merging.bind(mergeBar, mergeX, (x) => ({ opacity: 1 - x / 8 }));
merging.bind(mergeRing, [mergeX, mergeW], (x, w) => (mergeHeard.push(x, w), { width: 1 + x, opacity: 1 - w / 2 }));
merging.play({ target: mergeBar, size: [2, 0.4] }, 0.5, "linear");
merging.parallel(
    (scene) => scene.track(mergeX, 4, 4, "linear"),
    (scene) => scene.wait(1.05).track(mergeZ, 3, 1, "smooth"),
);
merging.at(2.33, (scene) => scene.track(mergeW, 1, 1.5, "ease_in"));
merging.track(mergeX, 2, 1, "linear");
// The functions of the tracks that merge on the ring run once for each frame of the tracks
// before them, and the ring waits for the save to sample at the frames of both of its tracks.
assert.equal(mergeHeard.length, 2 * (1 + 40 + 10));
merging.save(mergeDot);
assert.equal(mergeHeard.length, 2 * (1 + 40 + 10 + 56));
const mergeScript = [
    'local s = vis.scene {width = 160, height = 90, fps = 10, camera = {mode = "fixed", view = "2d", height = 6}}',
    "local x = s:tracker(0)",
    "local z = s:tracker(1)",
    "local w = s:tracker(0)",
    'local dot = s:circle {center = {-3, 0}, radius = 0.2, fill = "#ffffff", stroke = "#00000000", id = "dot"}',
    'local bar = s:rectangle {center = {0, -2}, size = {1, 0.4}, fill = "#ffffff", stroke = "#00000000", id = "bar"}',
    'local ring = s:circle {center = {2, 1}, radius = 0.5, stroke = "#ffffff", id = "ring"}',
    "local heard = {}",
    "s:bind(dot, {x, z}, function(a, b) return {shift = {a, 0}, radius = 0.2 * b} end)",
    "s:bind(bar, x, function(a) return {opacity = 1 - a / 8} end)",
    "s:bind(ring, {x, w}, function(a, b)",
    "    heard[#heard + 1] = a",
    "    heard[#heard + 1] = b",
    "    return {width = 1 + a, opacity = 1 - b / 2}",
    "end)",
    's:play({target = bar, size = {2, 0.4}}, 0.5, "linear")',
    's:parallel(function() s:track(x, 4, 4, "linear") end, function() s:wait(1.05) s:track(z, 3, 1, "smooth") end)',
    's:at(2.33, function() s:track(w, 1, 1.5, "ease_in") end)',
    's:track(x, 2, 1, "linear") s:save(dot)',
    "return s",
].join("\n");
const mergeHeardScript = mergeScript.replace(
    "return s",
    'for i, v in ipairs(heard) do s:point {point = {0, 0}, id = string.format("%d:%.17g", i, v)} end\nreturn s',
);
const mergeLua = compileScene(merging);
// The tracks seek from their starts where they were authored and record the times
// their frames end in the times table, after the marks of the parallel and the at; every
// segment then plays from its own time, and the cursor returns. The script declares no local
// of its own for them.
const recordsFrames = /:seek\([^)]*\);times\[\d+\]=\w+:time\(\)/;
assert.match(mergeLua, /\ntimes\[4\]=scene:time\(\);scene:seek\(times\[4\]\+0\.1\);times\[5\]=scene:time\(\)\n/);
assert.equal(mergeLua.match(/^local /gm).length, 3);
assert.match(mergeLua, /\ntimes\[83\]=scene:time\(\)\n/);
assert.match(mergeLua, /\nscene:seek\(times\[4\]\);scene:play\(\{\{target=objects\[1\],/);
assert.match(mergeLua, /\nscene:seek\(times\[14\]\);scene:play\(\{\{target=objects\[1\],[^\n]*\},times\[45\]-times\[14\],"linear",0\)\n/);
assert.match(mergeLua, /\nscene:seek\(times\[4\]\);scene:play\(\{\{target=objects\[2\],opacity=/);
assert.match(mergeLua, /\nscene:seek\(times\[83\]\)\nscene:save\(objects\[1\]\)\n/);
assert.match(compileScene(twin), recordsFrames);

// One tracker cannot move in two tracks at once, and a track cannot merge with one whose
// segments a call in between has played. A track that fails leaves its trackers where they
// were, and a merged sample that fails fails the call that plays it, and every later one.
const clash = vis.scene();
const clashX = clash.tracker(0);
clash.bind(clash.circle({ radius: 1 }), clashX, (x) => ({ opacity: x }));
assert.throws(
    () => clash.parallel((scene) => scene.track(clashX, 1, 2), (scene) => scene.wait(1).track(clashX, 0, 2)),
    /This track and an earlier one move the same tracker at once; a tracker has one value at a time/,
);
const replayed = vis.scene();
const replayedX = replayed.tracker(0);
const replayedZ = replayed.tracker(0);
replayed.bind(replayed.circle({ radius: 1 }), [replayedX, replayedZ], (x, z) => ({ opacity: x, radius: 1 + z }));
replayed.track(replayedX, 1, 2).fade(replayed.circle({ radius: 0.5 }), 0.5, 1);
assert.throws(
    () => replayed.at(1, (scene) => scene.track(replayedZ, 1, 1)),
    new RegExp(
        "This track overlaps a track whose segments have already played on the same object; author tracks " +
            "that overlap with only wait, at, parallel, mark, bind, tracker and factories between them",
    ),
);
assert.equal(replayedZ.get(), 0);
const broken = vis.scene({ fps: 10 });
const brokenX = broken.tracker(0);
const brokenZ = broken.tracker(0);
broken.bind(broken.circle({ radius: 1 }), [brokenX, brokenZ], (x, z) => (x + z > 1.5 ? 1 : { opacity: x }));
const brokenBefore = compileScene(broken);
assert.throws(() => broken.track([[brokenX, 2]], 1), /A bound function must return a state object/);
assert.equal(brokenX.get(), 0);
assert.equal(compileScene(broken), brokenBefore);
broken.parallel((scene) => scene.track(brokenX, 1, 2), (scene) => scene.wait(1).track(brokenZ, 1, 1));
assert.throws(() => compileScene(broken), /A bound function must return a state object/);
assert.throws(() => broken.fade(broken.circle({ radius: 0.5 }), 0.5, 1), /A bound function must return a state object/);

// A parallel merges its branches also after a call whose length only the scene computes, and a
// viewport child merges its tracks before its parent takes it. Tracks the builder cannot time
// against each other, a track after such a call and one an at puts at a time from the start,
// stay apart, and the scene reports their overlap.
const later = vis.scene({ width: 160, height: 90, fps: 10 });
const laterX = later.tracker(0);
const laterZ = later.tracker(1);
const laterDot = later.circle({ radius: 0.2, id: "dot" });
later.fadeIn(laterDot);
later.bind(laterDot, [laterX, laterZ], (x, z) => ({ shift: [x, 0], radius: 0.2 * z }));
later.parallel((scene) => scene.track(laterX, 2, 2, "linear"), (scene) => scene.wait(0.55).track(laterZ, 2, 1));
const nested = vis.scene({ width: 160, height: 90, fps: 10 });
const nestedX = nested.tracker(0);
const nestedZ = nested.tracker(1);
const nestedDot = nested.circle({ radius: 0.2, id: "inner" });
nested.bind(nestedDot, [nestedX, nestedZ], (x, z) => ({ shift: [x, 0], radius: 0.2 * z }));
nested.parallel((scene) => scene.track(nestedX, 2, 2), (scene) => scene.wait(0.5).track(nestedZ, 2, 1));
later.viewport(nested, { x: 0, y: 0, width: 0.5, height: 0.5 });
const laterLua = compileScene(later);
assert.match(laterLua, /\ntimes\[3\]=scene:time\(\);scene:seek\(times\[3\]\+0\.1\);times\[4\]=scene:time\(\)\n/);
assert.match(laterLua, /\n    local times = \{\}\n/);
assert.match(
    laterLua,
    /\n    times\[3\]=viewportScene1:time\(\);viewportScene1:seek\(times\[3\]\+0\.1\);times\[4\]=viewportScene1:time\(\)\n/,
);
const laterScript = [
    "local s = vis.scene {width = 160, height = 90, fps = 10}",
    "local x = s:tracker(0)",
    "local z = s:tracker(1)",
    'local dot = s:circle {radius = 0.2, id = "dot"}',
    "s:fade_in(dot)",
    "s:bind(dot, {x, z}, function(a, b) return {shift = {a, 0}, radius = 0.2 * b} end)",
    's:parallel(function() s:track(x, 2, 2, "linear") end, function() s:wait(0.55) s:track(z, 2, 1) end)',
    "local inner = vis.scene {width = 160, height = 90, fps = 10}",
    "local ix = inner:tracker(0)",
    "local iz = inner:tracker(1)",
    'inner:bind(inner:circle {radius = 0.2, id = "inner"}, {ix, iz}, function(a, b) return {shift = {a, 0}, radius = 0.2 * b} end)',
    "inner:parallel(function() inner:track(ix, 2, 2) end, function() inner:wait(0.5) inner:track(iz, 2, 1) end)",
    "s:viewport(inner, {x = 0, y = 0, width = 0.5, height = 0.5})",
    "return s",
].join("\n");
const apart = vis.scene({ fps: 10 });
const apartX = apart.tracker(0);
const apartZ = apart.tracker(1);
const apartDot = apart.circle({ radius: 0.2, id: "dot" });
apart.fadeIn(apartDot);
apart.bind(apartDot, [apartX, apartZ], (x, z) => ({ shift: [x, 0], radius: 0.2 * z }));
apart.track(apartX, 2, 2).at(1.5, (scene) => scene.track(apartZ, 2, 1));
const apartLua = compileScene(apart);
assert.match(apartLua, recordsFrames);

// A clone, the moves of a view, a view and a warp write the segments that wait first, also
// those a merge moves to the end of its tracks, so a copy made inside two merging tracks
// takes the points they give then, the views aim at the dot where the tracks leave it, and the
// warp lifts the line the tracks lay flat, frame for frame as in the Lua twin.
const pending = vis.scene({ width: 160, height: 90, fps: 10, camera: { mode: "fixed", view: "2d", height: 6 } });
const pendingX = pending.tracker(0);
const pendingZ = pending.tracker(0);
const pendingBow = pending.curve({ from: [-3, 0], control1: [-1, 0], control2: [1, 0], to: [3, 0], width: 4, id: "bow" });
pending.bind(pendingBow, [pendingX, pendingZ], (a, b) => ({ control1: [-1, a], control2: [1, b] }));
const pendingDot = pending.circle({ radius: 0.25, fill: "#ffffff", id: "dot" });
pending.bind(pendingDot, pendingX, (a) => ({ shift: [a, -1] }));
const pendingBeam = pending.line({ from: [-2, 0.5], to: [2, 0.5], width: 4, id: "beam" });
pending.bind(pendingBeam, pendingZ, (b) => ({ to: [2, 0.5 + b] }));
const pendingLens = pending.view({ x: 0.5, y: 0.5, width: 0.5, height: 0.5 }, { camera: { height: 2 } });
const pendingOverlap = (a, b) => pending.parallel(
    (scene) => scene.track(pendingX, a, 2, "linear"),
    (scene) => scene.wait(1).track(pendingZ, b, 2, "linear"),
);
pendingOverlap(2, 1);
pending.at(1.5, (scene) => scene.clone(pendingBow, { id: "copy" }));
pendingOverlap(3, 0);
pendingLens.look({ target: pendingDot }, 0.5);
pendingOverlap(4, 1);
pending.view({ x: 0, y: 0, width: 0.5, height: 0.5 }, { camera: { target: pendingDot, height: 2 } });
pendingOverlap(4.5, 0);
pending.warp(pendingBeam, (x, y) => [x, y + 1], 1, "linear", { x_range: [-3, 3, 1], y_range: [-1, 3, 1] });
const pendingLua = compileScene(pending);
for (const call of [
    /objects\[\d+\],objects\[\d+\] = scene:clone\(objects\[1\],\{\["id"\]="copy"\}\)/,
    /view1:look\(\{\["target"\]=objects\[2\]\},0\.5,"smooth"\)/,
    /local view2=scene:view\(/,
    /scene:warp\(objects\[3\],/,
]) {
    assert.match(pendingLua, new RegExp(`\\nscene:seek\\(times\\[\\d+\\]\\)\\n${call.source}`), String(call));
}
const pendingScript = [
    'local s = vis.scene {width = 160, height = 90, fps = 10, camera = {mode = "fixed", view = "2d", height = 6}}',
    "local x = s:tracker(0)",
    "local z = s:tracker(0)",
    'local bow = s:curve {from = {-3, 0}, control1 = {-1, 0}, control2 = {1, 0}, to = {3, 0}, width = 4, id = "bow"}',
    "s:bind(bow, {x, z}, function(a, b) return {control1 = {-1, a}, control2 = {1, b}} end)",
    'local dot = s:circle {radius = 0.25, fill = "#ffffff", id = "dot"}',
    "s:bind(dot, x, function(a) return {shift = {a, -1}} end)",
    'local beam = s:line {from = {-2, 0.5}, to = {2, 0.5}, width = 4, id = "beam"}',
    "s:bind(beam, z, function(b) return {to = {2, 0.5 + b}} end)",
    "local lens = s:view({x = 0.5, y = 0.5, width = 0.5, height = 0.5}, {camera = {height = 2}})",
    "local function overlap(a, b)",
    '    s:parallel(function() s:track(x, a, 2, "linear") end, function() s:wait(1) s:track(z, b, 2, "linear") end)',
    "end",
    "overlap(2, 1)",
    's:at(1.5, function() s:clone(bow, {id = "copy"}) end)',
    "overlap(3, 0)",
    "lens:look({target = dot}, 0.5)",
    "overlap(4, 1)",
    "s:view({x = 0, y = 0, width = 0.5, height = 0.5}, {camera = {target = dot, height = 2}})",
    "overlap(4.5, 0)",
    's:warp(beam, function(px, py) return px, py + 1 end, 1, "linear")',
    "return s",
].join("\n");
// A formula, like the other factories, and the lookup of its parts leave the tracks waiting, so
// tracks on either side of them still merge.
const quiet = vis.scene({ fps: 10 });
const quietX = quiet.tracker(0);
const quietZ = quiet.tracker(0);
quiet.bind(quiet.circle({ radius: 1 }), [quietX, quietZ], (x, z) => ({ opacity: x, radius: 1 + z }));
quiet.track(quietX, 1, 2);
quiet.tex({ tex: "x^2" }).part("x");
quiet.at(1, (scene) => scene.track(quietZ, 1, 1));
assert.match(compileScene(quiet), recordsFrames);

// A transform that turns and a morph add their change to a running shift, and a morph that
// would end before the shift fails.
const layers = vis.scene({ width: 160, height: 80, camera: { mode: "fixed", view: "2d", height: 4 } });
const bar = layers.rectangle({ center: [-2, 0], size: [2, 0.5], fill: "#ffffff", stroke: "#00000000", id: "bar" });
const blob = layers.circle({ center: [-2, -1], radius: 0.5, fill: "#ffffff", stroke: "#00000000", id: "blob" });
layers.parallel(
    (scene) => scene.shift(bar, [4, 0], 2, "linear"),
    (scene) => scene.wait(0.5).transform(bar, [0, -1, 0, 2, 1, 0, 0, 2, 0, 0, 1, 0, 0, 0, 0, 1], 1, "linear"),
    (scene) => scene.shift(blob, [4, 0], 2, "linear"),
    (scene) => {
        scene.wait(1);
        const square = scene.rectangle({ center: [2, 0], size: [1, 1], fill: "#ffffff", stroke: "#00000000" });
        scene.morph(blob, square, 1, "linear");
    },
);
const layersLua = compileScene(layers);
const hasty = vis.scene({ width: 160, height: 80 });
const hastyDot = hasty.circle({ radius: 0.5, id: "dot" });
hasty.shift(hastyDot, [1, 0], 2).at(1, (scene) => scene.morph(hastyDot, scene.rectangle({ size: [1, 1] }), 0.5));
const hastyLua = compileScene(hasty);

const shapes = vis.scene({ width: 160, height: 90 });
const disc = shapes.circle({ radius: 0.5, id: "disc" });
const sizedPanel = shapes.rectangle({ size: [1, 1], id: "panel" });
const caption = shapes.text({ text: "size", size: 20, id: "caption" });
const strip = shapes.rectangle({
    size: [4, 1],
    fill: "#ffffff",
    gradient: { type: "linear", stops: ["#ff0000", "#0000ff"] },
    id: "strip",
});
shapes.play(
    [
        { target: disc, radius: 1.5, width: 4 },
        { target: sizedPanel, size: [3, 2], corner: 0.5, progress: 0.5 },
        { target: caption, size: 40 },
        { target: strip, gradient: { type: "linear", stops: ["#00ff00", "#ffff00"] } },
    ],
    1,
);
const shapesLua = compileScene(shapes);
assert.match(shapesLua, /\{target=objects\[1\],width=4,radius=1\.5\}/);
assert.match(shapesLua, /\{target=objects\[2\],progress=0\.5,size=\{3,2\},corner=0\.5\}/);
assert.match(shapesLua, /\{target=objects\[3\],size=40\}/);
assert.match(shapesLua, /gradient=\{\["stops"\]=\{"#00ff00","#ffff00"\},\["type"\]="linear"\}/);
shapes.set([{ target: disc, opacity: 0.5 }, { target: sizedPanel, corner: 0.25 }]);
assert.match(compileScene(shapes), /scene:set\(\{\{target=objects\[1\],opacity=0\.5\},\{target=objects\[2\],corner=0\.25\}\}\)/);
assert.throws(() => shapes.set([]), /Animation set requires at least one descriptor/);
assert.throws(() => shapes.set({ target: disc, shift: [1, 0], arc: 1 }), /cannot use arc/);
assert.throws(
    () => shapes.set([{ target: disc, opacity: 1 }, { target: disc, fill: "#ffffff" }]),
    /set cannot contain duplicate targets/,
);
assert.throws(() => shapes.play({ target: strip, gradient: true }), /end-stop color or gradient options/);
assert.throws(
    () => shapes.play({ target: strip, gradient: { type: "linear", center: [0, 0] } }),
    /Unknown linear gradient option: center/,
);

const motion = vis.scene({ width: 160, height: 90 });
const wheel = motion.rectangle({ size: [1, 1], id: "wheel" });
const orbit = motion.circle({ radius: 1.5, id: "orbit" });
const moon = motion.circle({ center: [1.5, 0], radius: 0.1, id: "moon" });
motion.parallel(
    (scene) => scene.rotate(wheel, -Math.PI, { about: "center", shift: [Math.PI * 0.5, 0], duration: 2 }),
    (scene) => scene.moveAlong(moon, orbit, { orient: true, from: 0, to: 0.5, curve: "linear" }),
);
motion.rotate(moon, Math.PI, { about: wheel });
motion.scale([wheel, orbit], [2, 1], { about: "bottom_left", lag: 0.5 });
motion.play({ target: moon, shift: [-1, 0], arc: Math.PI / 2 });
const motionLua = compileScene(motion);
assert.match(motionLua, /scene:rotate\(objects\[1\],-3\.14159[0-9]*,\{\["about"\]="center",\["duration"\]=2,/);
assert.match(motionLua, /scene:move_along\(objects\[3\],objects\[2\],\{\["curve"\]="linear",\["from"\]=0,\["orient"\]=true,\["to"\]=0\.5\}\)/);
assert.match(motionLua, /scene:rotate\(objects\[3\],3\.14159[0-9]*,\{\["about"\]=objects\[1\]\}\)/);
assert.match(motionLua, /scene:scale\(\{objects\[1\],objects\[2\]\},\{2,1\},\{\["about"\]="bottom_left",\["lag"\]=0\.5\}\)/);
assert.match(motionLua, /arc=1\.5707/);
assert.throws(() => vis.scene().rotate(wheel, 1), /handles from the same scene/);
assert.throws(() => motion.rotate(wheel, Number.NaN), /angle/);
assert.throws(() => motion.rotate(wheel, 1, { about: "middle" }), /anchor/);
assert.throws(() => motion.rotate(wheel, 1, { arc: 7 }), /arc/);
assert.throws(() => motion.rotate(wheel, 1, { spin: true }), /Unknown Rotate option/);
assert.throws(() => motion.scale(wheel, "big"), /factor/);
assert.throws(() => motion.moveAlong(moon, orbit, { from: 2 }), /fraction/);
assert.throws(() => motion.moveAlong(moon, orbit, { orient: 1 }), /boolean/);
assert.throws(() => motion.play({ target: moon, opacity: 0.5, arc: 1 }), /arc requires/);

const connected = vis.scene({ width: 200, height: 200, camera: { height: 10 } });
const connectedNode = connected.group({});
connectedNode.circle({ radius: 1 });
connectedNode.text({ text: "node" });
const connectedSink = connected.circle({ center: [3, 3], radius: 0.5 });
const connectedEdge = connected.connector(connectedNode, connectedSink, { id: "edge", padding: 0.1 });
connected.create(connectedEdge, 0.5, "linear");
connected.fadeIn(connectedNode, { duration: 0.5 });
const connectedToken = connected.circle({ radius: 0.1, id: "token" });
connected.parallel(
    (s) => s.shift(connectedSink, [1, 0], 1, "linear"),
    (s) => s.moveAlong(connectedToken, connectedEdge, { from: 1, to: 0, duration: 1, curve: "linear" }),
);
assert.match(compileScene(connected), /scene:connector \{from=objects\[1\],to=objects\[4\]/);
assert.match(compileScene(connected), /scene:move_along\(objects\[6\],objects\[5\]/);
const connectedScript = `
local s = vis.scene {width = 200, height = 200, camera = {height = 10}}
local node = s:group {}
node:circle {radius = 1}
node:text {text = "node"}
local sink = s:circle {center = {3, 3}, radius = 0.5}
local edge = s:connector {from = node, to = sink, id = "edge", padding = 0.1}
s:create(edge, 0.5, "linear")
s:fade_in(node, {duration = 0.5})
local token = s:circle {radius = 0.1, id = "token"}
s:parallel(
    function() s:shift(sink, {1, 0}, 1, "linear") end,
    function() s:move_along(token, edge, {from = 1, to = 0, duration = 1, curve = "linear"}) end)
return s
`;

const tree = vis.scene({ width: 160, height: 90 });
const card = tree.group({ id: "card" });
const cardBody = card.rectangle({ size: [2, 1], id: "body" });
const cardTitle = card.text({ text: "T" });
const cardLink = tree.connector(cardBody, cardTitle, { id: "link" });
assert.deepEqual(card.children(), [cardBody, cardTitle]);
assert.deepEqual(cardBody.children(), []);
assert.equal(tree.find("body"), cardBody);
assert.equal(tree.find("link"), cardLink);
assert.equal(tree.find("missing"), null);
assert.throws(() => tree.find(3), /id string/);
card.children().pop();
assert.equal(card.children().length, 2);
// The rows of a group are the groups directly below it; other children lie between uncounted.
const rowA = card.group({ id: "rowA" });
card.circle({ radius: 0.2 });
const rowB = card.group();
const rowA1 = rowA.text({ text: "a1" });
const rowA2 = rowA.text({ text: "a2" });
const rowB1 = rowB.text({ text: "b1" });
assert.equal(card.row(1), rowA);
assert.equal(card.row(2), rowB);
assert.equal(card.entry(1, 2), rowA2);
assert.deepEqual(card.column(1), [rowA1, rowB1]);
assert.deepEqual(card.column(2), [rowA2]);
tree.play({ target: card.entry(2, 1), opacity: 0.5 }, 1);
assert.ok(compileScene(tree).includes(`{{target=${rowB1.name},opacity=0.5}}`));
assert.throws(() => card.row(3), /Row 3 is past the last of the group's 2 rows/);
assert.throws(() => card.row(0), /Row takes a whole number from 1/);
assert.throws(() => card.entry(1.5, 1), /Entry row takes a whole number from 1/);
assert.throws(() => card.entry(2, 2), /Entry 2 is past the last of the 1 objects in row 2/);
assert.throws(() => card.column(3), /No row of the group has an object 3/);
assert.throws(() => rowA.row(1), /The group has no rows; its rows are the groups directly below it/);

const turntable = vis.scene({
    width: 160,
    height: 90,
    camera: { mode: "fixed", view: "3d", eye: [0, 0, 10], projection: "perspective" },
});
turntable.point({ point: [2, 0, 0] });
turntable.orbit(2 * Math.PI, { duration: 4, curve: "linear" });
turntable.orbit(Math.PI, { axis: [1, 0, 0] });
const turntableLua = compileScene(turntable);
assert.match(turntableLua, /scene:orbit\(6\.28318[0-9]*,\{\["curve"\]="linear",\["duration"\]=4\}\)/);
assert.match(turntableLua, /scene:orbit\(3\.14159[0-9]*,\{\["axis"\]=\{1,0,0\}\}\)/);
assert.throws(() => turntable.orbit(0), /non-zero/);
assert.throws(() => turntable.orbit(1, { about: [0, 0, 0] }), /Unknown Orbit option/);

// Focus frames handles in a 2D camera with the options of Lua scene:focus, and the scene checks
// the camera when it loads.
const framing = vis.scene({ width: 160, height: 90 });
const near = framing.circle({ radius: 0.2, id: "near" });
const far = framing.circle({ center: [3, 1], radius: 0.4, id: "far" });
assert.equal(framing.focus(near, { padding: 0.5, duration: 0.25 }), framing);
framing.focus([near, far]).focus(null, { duration: 0.5, curve: "linear" }).focus();
const framingLua = compileScene(framing);
assert.match(framingLua, /scene:focus\(objects\[1\],\{\["duration"\]=0\.25,\["padding"\]=0\.5\}\)/);
assert.match(framingLua, /scene:focus\(\{objects\[1\],objects\[2\]\},\{\}\)/);
assert.match(framingLua, /scene:focus\(nil,\{\["curve"\]="linear",\["duration"\]=0\.5\}\)/);
assert.match(framingLua, /scene:focus\(nil,\{\}\)/);
assert.throws(() => framing.focus([]), /Focus targets must be a handle or a non-empty array of handles/);
assert.throws(() => framing.focus(vis.scene().circle({ radius: 1 })), /handles from the same scene/);
assert.throws(() => framing.focus([near, "far"]), /handles from the same scene/);
assert.throws(() => framing.focus(near, { easing: "linear" }), /Unknown Focus option: easing/);
assert.throws(() => framing.focus(near, null), /Focus options must be a plain object/);
for (const padding of [-0.1, Infinity, "1"]) {
    assert.throws(() => framing.focus(near, { padding }), /Focus padding must be a non-negative number/);
}
for (const duration of [0, -1, Number.NaN]) {
    assert.throws(() => framing.focus(near, { duration }), /Focus duration must be a finite positive number/);
}
assert.throws(() => framing.focus(near, { curve: "wobble" }), /Unknown animation curve preset: wobble/);
assert.equal(compileScene(framing), framingLua);
const lookingAround = vis.scene({ camera: { view: "3d" } });
lookingAround.focus(lookingAround.circle({ radius: 1 }));
const lookingAroundLua = compileScene(lookingAround);

// A look aims at handles as focus frames them and keeps the camera's offset, also during a pan;
// the twin script pans to the centers by number.
const centering = vis.scene({ width: 160, height: 90, camera: { mode: "fixed", view: "2d", height: 4 } });
const hub = centering.rectangle({ center: [3, 1], size: [2, 1], fill: "#ffffff", stroke: "#00000000", id: "hub" });
const spoke = centering.circle({ center: [-2, -1], radius: 0.5, fill: "#ff0000", id: "spoke" });
centering.parallel(
    (scene) => scene.look({ target: [-4, 0] }, 2, "linear"),
    (scene) => scene.wait(1).look({ target: hub }, 2, "linear"),
);
centering.look({ target: [hub, spoke], height: 6 });
const centeringLua = compileScene(centering);
assert.match(centeringLua, /scene:look\(\{\["target"\]=objects\[1\]\},2,"linear"\)/);
assert.match(centeringLua, /scene:look\(\{\["height"\]=6,\["target"\]=\{objects\[1\],objects\[2\]\}\},1,"smooth"\)/);
assert.match(compileScene(vis.scene().look({ target: [1, 2] })), /scene:look\(\{\["target"\]=\{1,2\}\},1,"smooth"\)/);
assert.throws(() => centering.look({ target: vis.scene().circle({ radius: 1 }) }), /handles from the same scene/);
assert.throws(() => centering.look({ target: [hub, [1, 2]] }), /handles from the same scene/);
const centerScript = [
    'local s = vis.scene {width = 160, height = 90, camera = {mode = "fixed", view = "2d", height = 4}}',
    's:rectangle {center = {3, 1}, size = {2, 1}, fill = "#ffffff", stroke = "#00000000", id = "hub"}',
    's:circle {center = {-2, -1}, radius = 0.5, fill = "#ff0000", id = "spoke"}',
    's:parallel(function() s:look({target = {-4, 0}}, 2, "linear") end,',
    '    function() s:wait(1) s:look({target = {3, 1}}, 2, "linear") end)',
    's:look({target = {0.75, 0}, height = 6}, 1, "smooth")',
    "return s",
].join("\n");
const aiming = vis.scene({ camera: { mode: "fixed", view: "3d", eye: [0, 0, 10], projection: "perspective" } });
const rim = aiming.line({ from: [1, 0, -2], to: [3, 2, 0] });
aiming.parallel(
    (scene) => scene.orbit(2 * Math.PI, { duration: 4, curve: "linear" }),
    (scene) => scene.wait(1).look({ target: rim }, 2, "linear"),
);
const aimingLua = compileScene(aiming);

// focus() returns to the opening camera from the 3D view too: the twin script looks back to the
// opening pose by number, and a scene that opened in 2D ends in it again.
const homing = vis.scene({
    width: 160,
    height: 90,
    camera: {
        mode: "fixed",
        view: "3d",
        eye: [4, -6, 5],
        target: [1, 0, 0],
        up: [0, 0, 1],
        projection: "perspective",
        fov: 0.6,
    },
});
homing.rectangle({ center: [1, 0], size: [2, 1], fill: "#ffffff", stroke: "#00000000" });
homing.circle({ center: [3, 1], radius: 0.5, fill: "#ff0000" });
homing.orbit(Math.PI / 2).look({ fov: 0.9 }, 0.5).focus(null, { duration: 1, curve: "linear" });
const homeScript = [
    "local s = vis.scene {width = 160, height = 90,",
    '    camera = {mode = "fixed", view = "3d", eye = {4, -6, 5}, target = {1, 0, 0}, up = {0, 0, 1},',
    '              projection = "perspective", fov = 0.6}}',
    's:rectangle {center = {1, 0}, size = {2, 1}, fill = "#ffffff", stroke = "#00000000"}',
    's:circle {center = {3, 1}, radius = 0.5, fill = "#ff0000"}',
    "s:orbit(math.pi / 2)",
    's:look({fov = 0.9}, 0.5, "smooth")',
    's:look({eye = {4, -6, 5}, target = {1, 0, 0}, up = {0, 0, 1}, fov = 0.6}, 1, "linear")',
    "return s",
].join("\n");
const flattening = vis.scene({ width: 160, height: 90 });
flattening.look({ view: "3d", eye: [0, -8, 6], projection: "perspective" }).focus();
const flatteningLua = compileScene(flattening);
assert.match(flatteningLua, /scene:focus\(nil,\{\}\)/);

// A view draws the scene again in a rectangle through a camera of its own. Its handle takes the
// scene's camera calls and a fade, a view made in an at block can be moved after it, and the
// twin script makes the same views by hand.
const magnified = vis.scene({ width: 160, height: 90, camera: { mode: "fixed", view: "2d", height: 9 } });
const speck = magnified.circle({ center: [1, 1], radius: 0.25, fill: "#ff0000", stroke: "#00000000", id: "speck" });
const cover = magnified.rectangle({
    center: [-3, 2], size: [1, 1], fill: "#0000ff", stroke: "#00000000", layer: 1, id: "cover",
});
const lens = magnified.view({ x: 0.5, y: 0, width: 0.5, height: 0.5 }, {
    camera: { target: speck, height: 3 }, follow: speck, hide: cover, layer: 0, background: "#05080d", opacity: 0,
});
assert.ok(lens instanceof ViewHandle);
assert.ok(Object.isFrozen(lens));
assert.equal(lens.fade(1, 0.5), lens);
assert.equal(lens.look({ height: 1.5 }, 1, "linear").focus(speck, { padding: 0.5, duration: 0.5 }).focus(), lens);
let overview;
magnified.at(0.25, (scene) => {
    overview = scene.view({ x: 0, y: 0.5, width: 0.25, height: 0.5 }, {
        camera: { target: [2, 0], height: 4 }, optical: false,
    });
});
overview.look({ target: [cover, speck] }, 0.5).fade(0.5, 0.25, "linear");
const magnifiedLua = compileScene(magnified);
assert.match(magnifiedLua, new RegExp([
    'local view1=scene:view\\(\\{\\["height"\\]=0\\.5,\\["width"\\]=0\\.5,\\["x"\\]=0\\.5,\\["y"\\]=0\\},',
    '\\{\\["background"\\]="#05080d",\\["camera"\\]=\\{\\["height"\\]=3,\\["target"\\]=objects\\[1\\]\\},',
    '\\["follow"\\]=objects\\[1\\],\\["hide"\\]=\\{objects\\[2\\]\\},\\["layer"\\]=0,\\["opacity"\\]=0\\}\\)\n',
    'view1:fade\\(1,0\\.5,"smooth"\\)\n',
    'view1:look\\(\\{\\["height"\\]=1\\.5\\},1,"linear"\\)\n',
    'view1:focus\\(objects\\[1\\],\\{\\["duration"\\]=0\\.5,\\["padding"\\]=0\\.5\\}\\)\n',
    'view1:focus\\(nil,\\{\\}\\)\n',
    'times\\[1\\]=scene:time\\(\\);scene:seek\\(0\\.25\\)\n',
    'local view2=scene:view\\(\\{\\["height"\\]=0\\.5,\\["width"\\]=0\\.25,\\["x"\\]=0,\\["y"\\]=0\\.5\\},',
    '\\{\\["camera"\\]=\\{\\["height"\\]=4,\\["target"\\]=\\{2,0\\}\\},\\["optical"\\]=false\\}\\)\n',
    'scene:seek\\(times\\[1\\]\\)\n',
    'view2:look\\(\\{\\["target"\\]=\\{objects\\[2\\],objects\\[1\\]\\}\\},0\\.5,"smooth"\\)\n',
    'view2:fade\\(0\\.5,0\\.25,"linear"\\)\n',
].join("")));
const lensScript = [
    'local s = vis.scene {width = 160, height = 90, camera = {mode = "fixed", view = "2d", height = 9}}',
    'local speck = s:circle {center = {1, 1}, radius = 0.25, fill = "#ff0000", stroke = "#00000000", id = "speck"}',
    "local cover = s:rectangle {center = {-3, 2}, size = {1, 1},",
    '    fill = "#0000ff", stroke = "#00000000", layer = 1, id = "cover"}',
    "local lens = s:view({x = 0.5, y = 0, width = 0.5, height = 0.5}, {camera = {target = speck, height = 3},",
    '    follow = speck, hide = cover, layer = 0, background = "#05080d", opacity = 0})',
    'lens:fade(1, 0.5) lens:look({height = 1.5}, 1, "linear")',
    "lens:focus(speck, {padding = 0.5, duration = 0.5}) lens:focus()",
    "local overview",
    "s:at(0.25, function()",
    "    overview = s:view({x = 0, y = 0.5, width = 0.25, height = 0.5}, {camera = {target = {2, 0}, height = 4},",
    "        optical = false})",
    "end)",
    'overview:look({target = {cover, speck}}, 0.5) overview:fade(0.5, 0.25, "linear")',
    "return s",
].join("\n");

// The builder refuses what the script would refuse, before the view takes a name.
const viewRect = { x: 0, y: 0, width: 0.5, height: 0.5 };
const stranger = vis.scene().circle({ radius: 1 });
assert.throws(() => magnified.view({ x: 0.5, y: 0, width: 0.6, height: 0.5 }), /normalized/);
assert.throws(() => magnified.view(viewRect, null), /View options must be a plain object/);
assert.throws(() => magnified.view(viewRect, { zoom: 4 }), /Unknown View option: zoom/);
assert.throws(() => magnified.view(viewRect, { camera: [0, 0] }), /View camera must be a look camera object/);
assert.throws(() => magnified.view(viewRect, { optical: 1 }), /View optical must be a boolean/);
assert.throws(() => magnified.view(viewRect, { background: 0 }), /View background must be a color/);
for (const opacity of [-0.1, 1.5, Number.NaN, "1"]) {
    assert.throws(() => magnified.view(viewRect, { opacity }), /View opacity must be between 0 and 1/);
    assert.throws(() => lens.fade(opacity), /View opacity must be between 0 and 1/);
}
for (const layer of [0.5, 2 ** 31, -(2 ** 31) - 1, "1"]) {
    assert.throws(() => magnified.view(viewRect, { layer }), /View layer must be an integer that fits in 32 bits/);
}
assert.throws(() => magnified.view(viewRect, { hide: stranger }), /handles from the same scene/);
assert.throws(() => magnified.view(viewRect, { hide: [speck, lens] }), /handles from the same scene/);
assert.throws(() => magnified.view(viewRect, { follow: stranger }), /handles from the same scene/);
assert.throws(() => magnified.view(viewRect, { camera: { target: stranger } }), /handles from the same scene/);
assert.throws(() => magnified.view(viewRect, { hide: Array(257).fill(speck) }), /A view hides at most 256 objects/);
for (const duration of [0, -1, Number.NaN]) {
    assert.throws(() => lens.fade(0.5, duration), /View fade duration must be a positive number/);
}
assert.throws(() => lens.fade(0.5, 1, "wobble"), /Unknown animation curve preset: wobble/);
assert.throws(() => lens.orbit(0), /non-zero/);
assert.throws(() => lens.focus([]), /Focus targets must be a handle or a non-empty array of handles/);
assert.throws(() => lens.focus(speck, { easing: "linear" }), /Unknown Focus option: easing/);
assert.throws(() => lens.look({ target: stranger }), /handles from the same scene/);
assert.equal(compileScene(magnified), magnifiedLua);
const crowded = vis.scene();
const crowd = crowded.circle({ radius: 1 });
assert.throws(() => crowded.view(viewRect, { hide: [crowd, stranger] }), /handles from the same scene/);
crowded.view(viewRect, { hide: Array(256).fill(crowd) });
for (let index = 1; index < 8; index++) crowded.view(viewRect);
assert.throws(() => crowded.view(viewRect), /A scene holds at most 8 views, as each draws the scene again/);
assert.match(compileScene(crowded), /\["hide"\]=\{objects\[1\](,objects\[1\]){255}\}/);
assert.match(compileScene(crowded), /local view8=scene:view\(\{\["height"\]=0\.5,\["width"\]=0\.5,\["x"\]=0,\["y"\]=0\},\{\}\)/);
assert.doesNotMatch(compileScene(crowded), /view9/);
const sealed = vis.scene();
const sealedView = sealed.view(viewRect);
vis.scene().viewport(sealed, viewRect);
assert.throws(() => sealedView.look({ height: 2 }), /sealed/);
assert.throws(() => sealedView.fade(0), /sealed/);
assert.throws(() => sealed.view(viewRect), /sealed/);

// The script checks the timing: the fades of a view follow one another. A 3D view orbits.
const hurried = vis.scene();
const hurriedView = hurried.view(viewRect, { opacity: 0 });
hurried.parallel(() => hurriedView.fade(1, 1), () => hurriedView.fade(0.5, 1));
const hurriedLua = compileScene(hurried);
const turning = vis.scene({ camera: { mode: "fixed", view: "3d", eye: [0, 0, 10], projection: "perspective" } });
turning.point({ point: [2, 0, 0] });
turning.view(viewRect, { camera: { eye: [0, -8, 6] } }).orbit(Math.PI, { duration: 2, curve: "linear" });
const turningLua = compileScene(turning);
assert.match(turningLua, /view1:orbit\(3\.14159[0-9]*,\{\["curve"\]="linear",\["duration"\]=2\}\)/);

const measured = vis.scene({ width: 320, height: 180 });
const measuredCaption = measured.text({ text: "measured", measure: true, id: "caption" });
measured.rectangle({ size: [0.5, 0.5], id: "beside" }).nextTo(measuredCaption, [1, 0], 0.2);
const measuredLua = compileScene(measured);
assert.match(measuredLua, /\["measure"\]=true/);

// A number counts as play and set animate its value.
const reserved = vis.scene({ width: 320, height: 180, camera: { height: 180 } });
reserved.space({ x: [-100, 100, 50], y: [-70, 70, 50], width: 1, axis_width: 2, ticks: true });
const reservedNumber = reserved.number({ tabular: true, value: 1.5, decimals: 2, reserve: "-000.00", align: [1, 0.5], id: "n" });
reserved.matrix({ tabular: true, rows: [[1, 2], [3, 4]], reserve: ["-000.00", "00"], justify: "right", id: "m" });
reserved.text({ tabular: true, text: "x1", reserve: "long text", point: [0, -50] });
reserved.play({ target: reservedNumber, value: -3 }, 1, "linear");
assert.throws(() => reserved.number({ reserve: false }), /Text reserve must be a string/);
assert.throws(() => reserved.number({ tabular: 1 }), /Text tabular must be a boolean/);
assert.throws(() => reserved.matrix({ rows: [[1]], tabular: "yes" }), /Matrix tabular must be a boolean/);
for (const axis_width of [-1, NaN, Infinity, "2"]) {
    assert.throws(() => reserved.space({ axis_width }), /Space axis_width must be zero or more pixels/);
}
const reservedLua = compileScene(reserved);
assert.match(reservedLua, /\["tabular"\]=true/);
assert.match(reservedLua, /\["axis_width"\]=2/);
assert.match(reservedLua, /\["reserve"\]="-000.00"/);
assert.match(reservedLua, /\["reserve"\]=\{"-000.00","00"\}/);
assert.throws(() => reserved.matrix({ rows: [[1, 2]], reserve: ["0"] }), /Matrix reserve must be a string or one string per column/);
const counting = vis.scene({ width: 320, height: 180 });
const counted = counting.number({ value: 0, separator: ",", prefix: "n = ", id: "count" });
counting.set({ target: counted, value: 5 });
counting.play({ target: counted, value: 1000 }, 2, "linear");
const countingLua = compileScene(counting);
assert.match(countingLua, /objects\[1\] = scene:number \{\["id"\]="count",\["prefix"\]="n = ",\["separator"\]=",",\["value"\]=0\}/);
assert.match(countingLua, /scene:set\(\{\{target=objects\[1\],value=5\}\}\)/);
assert.match(countingLua, /scene:play\(\{\{target=objects\[1\],value=1000\}\},2,"linear",0\)/);

// A text or label changes its string as play and set animate text, beside a move.
const retitled = vis.scene({ width: 320, height: 180 });
const title = retitled.text({ text: "cat", id: "title" });
const retitledCaption = retitled.rectangle({ size: [1, 1] }).label({ text: "Hello world", id: "caption" });
retitled.set({ target: title, text: "draft" });
retitled.play([{ target: title, text: "horse", shift: [1, 0] }, { target: retitledCaption, text: "Hello there", retype: true }], 1, "linear");
const retitledLua = compileScene(retitled);
assert.match(retitledLua, /scene:set\(\{\{target=objects\[1\],text="draft"\}\}\)/);
assert.match(
    retitledLua,
    /scene:play\(\{\{target=objects\[1\],shift=\{1,0\},text="horse"\},\{target=objects\[3\],text="Hello there",retype=true\}\},1,"linear",0\)/,
);
assert.throws(() => retitled.play({ target: title, text: 5 }), /Animation text must be a string/);
assert.throws(() => retitled.play({ target: title, text: "a\0b" }), /Strings cannot contain zero bytes/);
assert.throws(() => retitled.play({ target: title, opacity: 0, retype: true }), /Animation retype needs text/);
assert.throws(() => retitled.play({ target: title, text: "a", retype: 1 }), /Animation retype must be a boolean/);
assert.throws(() => retitled.play({ target: retitled.number({ value: 1 }), text: "a" }),
    /Animation text cannot change a number; animate its value/);
assert.throws(() => retitled.play({ target: retitled.spans(["a ", "b"]), text: "c" }),
    /Animation text changes one text, and a group or spans row holds several; target one of its texts/);
assert.throws(() => retitled.set({ target: retitled.circle({ radius: 1 }), text: "a" }),
    /Animation text needs a text or label/);

// play and set change an object's draw layer at a point in time, beside a move.
const layered = vis.scene({ width: 320, height: 180 });
const layeredBack = layered.rectangle({ size: [1, 1], id: "back" });
const layeredFront = layered.circle({ radius: 0.5, layer: 1, id: "front" });
layered.set({ target: layeredBack, layer: 2 });
layered.play({ target: layeredBack, layer: 0, shift: [1, 0] }, 1, "linear");
layered.set({ target: layeredFront, layer: -1 });
const layeredLua = compileScene(layered);
assert.match(layeredLua, /scene:set\(\{\{target=objects\[1\],layer=2\}\}\)/);
assert.match(layeredLua, /scene:play\(\{\{target=objects\[1\],shift=\{1,0\},layer=0\}\},1,"linear",0\)/);
assert.match(layeredLua, /scene:set\(\{\{target=objects\[2\],layer=-1\}\}\)/);
assert.throws(() => layered.play({ target: layeredBack, layer: 1.5 }), /Animation layer must be an integer/);
assert.throws(() => layered.set({ target: layeredBack, layer: 2 ** 31 }), /Animation layer must fit in 32 bits/);
assert.throws(() => layered.bind(layeredFront, layered.tracker(0), () => ({ layer: 1 })),
    /A bound state cannot set 'layer'/);

// save records the state of a handle or handles, and restore brings the newest back over a
// duration or, by default, at once; a restore that finds nothing changed waits instead.
const saving = vis.scene({ width: 160, height: 80, camera: { height: 4 } });
const savingCard = saving.rectangle({ center: [-2, 0], size: [1, 1], id: "card" });
const savingDot = saving.circle({ center: [2, 0], radius: 0.5, id: "dot" });
saving.save([savingCard, savingDot]);
saving.play([{ target: savingCard, shift: [1, 1] }, { target: savingDot, layer: 3 }], 1);
saving.restore(savingCard, 1, "linear");
saving.restore(savingDot);
saving.save(savingCard);
saving.restore(savingCard, 0.5);
const savingLua = compileScene(saving);
assert.match(savingLua, /scene:save\(\{objects\[1\],objects\[2\]\}\)/);
assert.match(savingLua, /scene:restore\(objects\[1\],1,"linear",0\)/);
assert.match(savingLua, /scene:restore\(objects\[2\],0,"smooth",0\)/);
assert.match(savingLua, /scene:save\(objects\[1\]\)\nscene:restore\(objects\[1\],0\.5,"smooth",0\)/);
assert.throws(() => saving.restore(savingCard, -1), /Restore duration must be zero or a positive number of seconds/);
assert.throws(() => saving.restore(savingCard, 1, "linear", -0.5), /Restore lag must be zero or positive/);
assert.throws(() => saving.save([]), /Animation targets must be handles from the same scene/);
assert.throws(() => saving.restore(layeredBack), /Animation targets must be handles from the same scene/);
const unsaved = vis.scene({ width: 160, height: 80 });
unsaved.restore(unsaved.circle({ radius: 0.5, id: "lonely" }), 1);
const unsavedLua = compileScene(unsaved);

// Spans name every text they make, in the order the scene sets them.
const spanned = vis.scene({ width: 320, height: 180 });
const sentence = spanned.spans(["The quick ", { text: "brown", color: "#a0522d", id: "brown" }, " fox"], {
    split: "words",
    point: [0, 1, 0],
    id: "sentence",
});
const letters = spanned.group({ id: "holder" }).spans(["ab c"], { split: "letters" });
const formula = spanned.spans(["a", { text: "2", rise: 0.35, size: 14 }, " + b\nnext"]);
const block = spanned.group({ typeset: true, id: "block" });
block.spans(["stacked"], { point: [0, -2, 0] });
assert.equal(sentence.children().length, 4);
assert.equal(letters.children().length, 3);
assert.equal(formula.children().length, 4);
assert.equal(spanned.find("sentence"), sentence);
assert.equal(spanned.find("brown"), sentence.children()[2]);
assert.equal(spanned.find("sentence/1"), sentence.children()[0]);
assert.equal(spanned.find("sentence/4"), sentence.children()[3]);
assert.equal(spanned.find("sentence/3"), null);
spanned.write(sentence.children(), 1, "smooth", 0.2);
spanned.fill(spanned.find("brown"), "#ff0000", 0.5);
assert.throws(() => spanned.spans([]), /non-empty array/);
assert.throws(() => spanned.spans("text"), /non-empty array/);
assert.throws(() => spanned.spans([3]), /string or a plain object/);
const spannedLua = compileScene(spanned);
assert.match(spannedLua, /(objects\[\d+\]) = scene:spans \{"The quick ",\{\["color"\]="#a0522d",\["id"\]="brown",\["text"\]="brown"\}," fox",\["id"\]="sentence",\["point"\]=\{0,1,0\},\["split"\]="words"\}\n(objects\[\d+\]) = \1:children\(\)/);
assert.match(spannedLua, /scene:write\(\{objects\[2\]\[1\],objects\[2\]\[2\],objects\[2\]\[3\],objects\[2\]\[4\]\},1,"smooth",0\.2\)/);
assert.match(spannedLua, /scene:fill\(objects\[2\]\[3\],"#ff0000",0\.5,"smooth"\)/);
assert.match(spannedLua, /scene:group \{\["id"\]="block",\["typeset"\]=true\}/);

// A match pairs the pieces of two rows: texts that read the same travel, given pairs cross-fade.
const matching = vis.scene({ width: 320, height: 180 });
const firstRow = matching.spans(["a²", " + ", "b²", " = ", "c²"], { id: "first" });
matching.wait(0.5);
const secondRow = matching.spans(["c²", " = ", "a²", " + ", "b²"], { id: "second" });
matching.matchTransform(firstRow, secondRow, 1.5, "smooth", { arc: Math.PI / 2 });
const rootRow = matching.spans(["c", " = ", "√(", "a²", " + ", "b²", ")"], { id: "root" });
matching.matchTransform(secondRow, rootRow, 1, "linear", {
    pairs: [[matching.find("second/1"), matching.find("root/1")]],
    lag: 0.1,
});
assert.throws(() => matching.matchTransform(rootRow, rootRow), /distinct handles/);
assert.throws(() => matching.matchTransform(firstRow, rootRow, 1, "smooth", { spin: 1 }), /Unknown MatchTransform option: spin/);
assert.throws(() => matching.matchTransform(firstRow, rootRow, 1, "smooth", { arc: 7 }), /arc must be a finite angle/);
assert.throws(() => matching.matchTransform(firstRow, rootRow, 1, "smooth", { lag: -1 }), /lag must be a finite number/);
assert.throws(() => matching.matchTransform(firstRow, rootRow, 1, "smooth", { pairs: [[firstRow]] }), /pairs must be an array/);
assert.throws(() => matching.matchTransform(firstRow, rootRow, 1, "smooth", { strings: "no" }), /strings must be a boolean/);
const plain = vis.scene({ width: 320, height: 180 });
const plainFrom = plain.spans(["x"]);
const plainTo = plain.spans(["x"]);
plain.matchTransform(plainFrom, plainTo, 1, "smooth", { strings: false });
assert.match(compileScene(plain), /scene:match_transform\(objects\[1\],objects\[3\],1,"smooth",\{\["strings"\]=false\}\)/);
assert.throws(() => matching.matchTransform(firstRow, rootRow, 1, "smooth", { diff: 1 }), /diff must be a boolean/);
const edits = vis.scene({ width: 320, height: 180 });
const before = edits.code({ text: "a b\nc", language: "lua" });
const after = edits.code({ text: "c\nb a", language: "lua" });
edits.matchTransform(before, after, 1, "smooth", { diff: true, fade: 0.4 });
assert.match(compileScene(edits), /scene:match_transform\(objects\[1\],objects\[\d+\],1,"smooth",\{\["diff"\]=true,\["fade"\]=0\.4\}\)/);
assert.throws(() => matching.matchTransform(firstRow, rootRow, 1, "smooth", { fade: 0 }), /fade must be a share of the duration/);
const bars = vis.scene({ width: 320, height: 180 });
const twoBars = bars.group();
twoBars.rectangle({ center: [-1, 0], size: [2, 1] });
const fourBars = bars.group();
fourBars.rectangle({ center: [-1.5, 0], size: [0.8, 1] });
bars.matchTransform(twoBars, fourBars, 1, "smooth", { shapes: true });
assert.match(compileScene(bars), /scene:match_transform\(objects\[1\],objects\[3\],1,"smooth",\{\["shapes"\]=true\}\)/);
assert.throws(() => bars.matchTransform(twoBars, fourBars, 1, "smooth", { shapes: "yes" }), /shapes must be a boolean/);
const matchingLua = compileScene(matching);
assert.match(matchingLua, /scene:match_transform\(objects\[1\],objects\[3\],1\.5,"smooth",\{\["arc"\]=1\.5707963267948966\}\)/);
assert.match(
    matchingLua,
    /scene:match_transform\(objects\[3\],objects\[5\],1,"linear",\{\["lag"\]=0\.1,\["pairs"\]=\{\{objects\[4\]\[1\],objects\[6\]\[1\]\}\}\}\)/,
);

// Code names its lines and tokens as the native factory splits them, blank lines too.
const coded = vis.scene({ width: 320, height: 180 });
const listing = coded.code({
    text: "--[[ a\n  b ]] local x = 1\n\nprint(x, 'y') -- z\n",
    language: "lua",
    id: "listing",
    size: 16,
});
const nestedCode = coded.group({ id: "frame" }).code({ text: "a  =\tb" });
assert.equal(listing.children().length, 4);
assert.deepEqual(listing.children().map((line) => line.children().length), [1, 5, 0, 7]);
assert.equal(coded.find("listing/2"), listing.children()[1]);
assert.equal(coded.find("listing/2/1").name, coded.find("listing/2").children()[0].name);
assert.equal(coded.find("listing/4/7"), listing.children()[3].children()[6]);
assert.equal(coded.find("listing/4/8"), null);
assert.equal(nestedCode.children()[0].children().length, 3);
coded.write(listing.children()[3].children(), 1, "smooth", 0.1);
coded.fill(coded.find("listing/2/4"), "#ff0000", 0.5);
coded.highlight(listing, 2, 4, { dim: 0.3, duration: 1 });
coded.highlight(listing, coded.find("listing/2/2"));
coded.highlight(listing, [coded.find("listing/4/1"), coded.find("listing/4/3")], { curve: "linear" });
coded.highlight(listing);
assert.throws(() => coded.highlight(listing, 0), /first and last count children from 1/);
assert.throws(() => coded.highlight(listing, 3, 2), /first no later than last/);
assert.throws(() => coded.highlight(listing, 1.5), /whole child numbers/);
assert.throws(() => coded.highlight(listing, []), /non-empty array of handles/);
assert.throws(() => coded.highlight(listing, 1, { spin: 1 }), /Unknown Highlight option: spin/);
assert.throws(() => coded.highlight(listing, { dim: 2 }), /dim must be an opacity from 0 to 1/);
assert.throws(() => coded.highlight(listing, { duration: 0 }), /positive number of seconds/);
assert.throws(() => coded.highlight(listing, { easing: "linear" }), /Unknown Highlight option: easing/);
assert.throws(() => coded.code("x"), /plain object/);
assert.throws(() => coded.code({ language: "lua" }), /text string/);
assert.throws(() => coded.code({ text: "x", language: "rust" }), /Code language must be plain, lua/);
// A rectangle takes its place and size around handles, which it names as the script does.
coded.rectangle({ id: "band", around: listing.children().slice(1, 2), padding: [0.2, 0.05], fill: "#fde68a" });
coded.rectangle({ id: "box", around: coded.find("listing/4/1") });
assert.throws(() => coded.rectangle({ around: [] }), /Around must be a handle or a non-empty array of handles/);
assert.throws(() => coded.rectangle({ around: "listing" }), /Around must be a handle or a non-empty array of handles/);
const codedLua = compileScene(coded);
assert.match(
    codedLua,
    /objects\[\d+\] = scene:rectangle \{\["around"\]=\{objects\[2\]\[2\]\},\["fill"\]="#fde68a",\["id"\]="band",\["padding"\]=\{0\.2,0\.05\}\}/,
);
assert.match(codedLua, /objects\[\d+\] = scene:rectangle \{\["around"\]=objects\[3\]\[4\]\[1\],\["id"\]="box"\}/);
assert.match(
    codedLua,
    /(objects\[\d+\]) = scene:code \{\["id"\]="listing",\["language"\]="lua",\["size"\]=16,\["text"\]="--\[\[ a\\n  b \]\] local x = 1\\n\\nprint\(x, 'y'\) -- z\\n"\}\n(objects\[\d+\]) = \1:children\(\)\n(objects\[\d+\]) = \{\} for i, line in ipairs\(\2\) do \3\[i\] = line:children\(\) end/,
);
assert.match(codedLua, /scene:write\(\{objects\[3\]\[4\]\[1\],objects\[3\]\[4\]\[2\],objects\[3\]\[4\]\[3\],objects\[3\]\[4\]\[4\],objects\[3\]\[4\]\[5\],objects\[3\]\[4\]\[6\],objects\[3\]\[4\]\[7\]\},1,"smooth",0\.1\)/);
assert.match(codedLua, /scene:fill\(objects\[3\]\[2\]\[4\],"#ff0000",0\.5,"smooth"\)/);
assert.match(codedLua, /objects\[5\] = objects\[4\]:code \{\["text"\]="a  =\\tb"\}/);
assert.match(codedLua, /scene:highlight\(objects\[1\],2,4,\{\["dim"\]=0\.3,\["duration"\]=1\}\)/);
assert.match(codedLua, /scene:highlight\(objects\[1\],objects\[3\]\[2\]\[2\]\)/);
assert.match(codedLua, /scene:highlight\(objects\[1\],\{objects\[3\]\[4\]\[1\],objects\[3\]\[4\]\[3\]\},\{\["curve"\]="linear"\}\)/);
assert.match(codedLua, /scene:highlight\(objects\[1\]\)\n/);

// Code passes a pinned baseline and a world-unit pitch to Lua.
const pinnedCode = vis.scene({ width: 320, height: 180, camera: {height: 180} });
const pinnedBlock = pinnedCode.code({text: "iii WWW\n\nMM", point: [-120, 60], align: [0, 1],
    size: 18, baseline: true, pitch: 36, id: "pin"});
pinnedCode.fade(pinnedBlock, 0.5, 1, "linear");
const pinnedCodeScript = `
local s = vis.scene {width = 320, height = 180, camera = {height = 180}}
local b = s:code {text = "iii WWW\\n\\nMM", point = {-120, 60}, align = {0, 1},
    size = 18, baseline = true, pitch = 36, id = "pin"}
s:fade(b, 0.5, 1, "linear")
return s
`;
assert.match(compileScene(pinnedCode), /\["baseline"\]=true/);
assert.match(compileScene(pinnedCode), /\["pitch"\]=36/);
assert.throws(() => pinnedCode.code({text: "a", baseline: 1}), /baseline must be a boolean/);
for (const pitch of [0, -1, Infinity, NaN, "3"]) {
    assert.throws(() => pinnedCode.code({text: "a", pitch}), /pitch must be a positive distance/);
}
assert.throws(() => pinnedCode.code({text: "a", pitch: 3, leading: 1}), /pitch or leading, not both/);

// A search finds the tokens that a string or a RegExp covers, as the handles code named.
const sums = vis.scene({ width: 320, height: 180 });
const totals = sums.code({
    id: "sum",
    language: "lua",
    size: 16,
    text: "local total = 0\nfor i = 1, 10 do\n  total = total + i\nend\nprint(total)\n",
});
const found = (...ids) => ids.map((id) => sums.find(id));
assert.deepEqual(totals.search("total"), found("sum/1/2"));
assert.deepEqual(totals.search("total", { match: "last" }), found("sum/5/3"));
assert.deepEqual(totals.search("total", { match: 3 }), found("sum/3/3"));
assert.deepEqual(totals.search("total", { match: "all" }), found("sum/1/2", "sum/3/1", "sum/3/3", "sum/5/3"));
assert.deepEqual(totals.search("ota"), found("sum/1/2"));
assert.deepEqual(totals.search("1, 10"), found("sum/2/4", "sum/2/5", "sum/2/6"));
assert.deepEqual(totals.search("do\n  total"), found("sum/2/7", "sum/3/1"));
assert.deepEqual(totals.search("total", { lines: [3, 5], match: "all" }), found("sum/3/1", "sum/3/3", "sum/5/3"));
assert.deepEqual(totals.search("i", { whole: true, match: "all" }), found("sum/2/2", "sum/3/5"));
assert.deepEqual(totals.search(/\d+/, { match: "all" }), found("sum/1/4", "sum/2/4", "sum/2/6"));
assert.deepEqual(totals.search(/^\s*total/m, { lines: 3 }), found("sum/3/1"));
assert.deepEqual(totals.search(/l\b/g, { match: 5 }), found("sum/5/3"));
const holder = sums.group();
const word = holder.code({ id: "word", language: "python", text: "café = 'naïve'\nprint(café)" });
assert.deepEqual(word.search("é", { match: "all" }), found("word/1/1", "word/2/3"));
assert.deepEqual(word.search(/ï/u), found("word/1/3"));
sums.rectangle({ id: "box", around: totals.search("1, 10"), padding: 0.1 });
sums.wait(0.5);
sums.highlight(totals, totals.search(/total/, { match: "all" }));
sums.indicate(totals.search("total", { match: "all" }), { duration: 0.5 });
// An edit applies its changes in order in the builder and hands the script the edited text.
const second = sums.edit(totals, [
    { replace: "10", with: "n" },
    { insert: "local n = 10", before: 1 },
    { remove: 6 },
    { append: "return total" },
]);
assert.equal(sums.find("sum.2"), second);
assert.deepEqual(second.children().map((line) => line.children().length), [4, 4, 7, 5, 1, 2]);
const third = sums.edit(second, [
    { insert: " * i", after: "+ i" },
    { replace: /\btotal\b/, with: "sum", match: "all" },
    { replace: [1, 2], with: "local n, sum = 10, 0" },
    { prepend: "-- squares" },
    { insert: "  -- add", after: 3 },
], 2, "linear", { fade: 0.5 });
assert.equal(sums.find("sum.3"), third);
assert.deepEqual(third.children().map((line) => line.children().length), [1, 8, 7, 1, 7, 1, 2]);
const last = sums.edit(third, [{ remove: [4, 4] }, { remove: " * i" }, { replace: "sum", with: "total", match: "last" }],
    0.5, "smooth", { id: "final" });
assert.equal(sums.find("final"), last);
assert.deepEqual(last.children().map((line) => line.children().length), [1, 8, 7, 5, 1, 2]);
assert.deepEqual(last.search("total"), found("final/6/2"));
// The edited block joins the group of the block it replaces.
const reworded = sums.edit(word, { replace: "café", with: "cafe", match: "all" });
assert.equal(holder.children().at(-1), reworded);
assert.equal(sums.find("word.2/2/3"), reworded.entry(2, 3));
// Blank lines that end a text with no line feed of its own stay lines in the script.
const spaced = sums.edit(reworded, { insert: "\n\n", after: "print(cafe)" }, 0.5);
assert.equal(spaced.children().length, 3);
assert.equal(sums.find("word.3/3"), spaced.row(3));
const sumsLua = compileScene(sums);
assert.match(sumsLua, /scene:highlight\(objects\[1\],\{objects\[3\]\[1\]\[2\],objects\[3\]\[3\]\[1\],objects\[3\]\[3\]\[3\],objects\[3\]\[5\]\[3\]\}\)/);
assert.match(
    sumsLua,
    /scene:indicate\(\{objects\[3\]\[1\]\[2\],objects\[3\]\[3\]\[1\],objects\[3\]\[3\]\[3\],objects\[3\]\[5\]\[3\]\},\{\["color"\]="focus",\["curve"\]="smooth",\["duration"\]=0\.5,\["scale"\]=1\.2\}\)/,
);
assert.match(sumsLua, /objects\[8\] = scene:rectangle \{\["around"\]=\{objects\[3\]\[2\]\[4\],objects\[3\]\[2\]\[5\],objects\[3\]\[2\]\[6\]\},\["id"\]="box",\["padding"\]=0\.1\}/);
assert.match(
    sumsLua,
    /objects\[9\] = scene:edit\(objects\[1\],\{\{\["replace"\]=\{1,5\},\["with"\]="local n = 10\\nlocal total = 0\\nfor i = 1, n do\\n  total = total \+ i\\nend\\nreturn total\\n"\}\},1,"smooth",\{\["id"\]="sum\.2"\}\)\nobjects\[10\] = objects\[9\]:children\(\)/,
);
assert.match(
    sumsLua,
    /objects\[12\] = scene:edit\(objects\[9\],\{\{\["replace"\]=\{1,6\},\["with"\]="-- squares\\nlocal n, sum = 10, 0\\nfor i = 1, n do\\n  -- add\\n  sum = sum \+ i \* i\\nend\\nreturn sum\\n"\}\},2,"linear",\{\["id"\]="sum\.3",\["fade"\]=0\.5\}\)/,
);
assert.match(sumsLua, /objects\[15\] = scene:edit\(objects\[12\],\{\{\["replace"\]=\{1,7\},\["with"\]="-- squares\\nlocal n, sum = 10, 0\\nfor i = 1, n do\\n  sum = sum \+ i\\nend\\nreturn total\\n"\}\},0\.5,"smooth",\{\["id"\]="final"\}\)/);
assert.match(sumsLua, /objects\[18\] = scene:edit\(objects\[5\],\{\{\["replace"\]=\{1,2\},\["with"\]="cafe = 'naïve'\\nprint\(cafe\)\\n"\}\},1,"smooth",\{\["id"\]="word\.2"\}\)/);
assert.match(sumsLua, /objects\[21\] = scene:edit\(objects\[18\],\{\{\["replace"\]=\{1,2\},\["with"\]="cafe = 'naïve'\\nprint\(cafe\)\\n\\n\\n"\}\},0\.5,"smooth",\{\["id"\]="word\.3"\}\)/);
const failing = vis.scene({ width: 320, height: 180 });
const small = failing.code({ id: "b", text: "a = 1\nb = a" });
assert.throws(() => totals.search(""), /Search needs a non-empty string or a RegExp to find/);
assert.throws(() => totals.search("zzz"), /The code has no 'zzz'/);
assert.throws(() => totals.search("print", { lines: 1 }), /Line 1 of the code has no 'print'/);
assert.throws(() => totals.search(/zz/, { lines: [1, 2] }), /Lines 1 to 2 of the code have no \/zz\//);
assert.throws(() => totals.search("total", { match: 9 }), /Match 9 is past the last of the 4 matches of 'total'/);
assert.throws(() => totals.search("total", { match: "most" }), /Match must be "first", "last", "all" or a match number from 1/);
assert.throws(() => totals.search("total", { lines: [3, 1] }), /Lines takes a line number or \[first, last\], counting from 1/);
assert.throws(() => totals.search("total", { lines: 9 }), /Line 9 is past the last of the code's 5 lines/);
assert.throws(() => totals.search("total", { spin: 1 }), /Unknown Search option: spin/);
assert.throws(() => totals.search("ota", { whole: true }), /The code has no 'ota' made of whole tokens/);
assert.throws(() => totals.search("  "), /'  ' matches only blanks, which no token holds/);
assert.throws(() => totals.search(/x*/), /The code has no \/x\*\//);
assert.throws(() => failing.group().search("a"), /Search needs a block that code made/);
assert.throws(() => small.row(1).search("a"), /Search needs a block that code made/);
assert.throws(() => failing.indicate([]), /Animation targets must be handles from the same scene/);
assert.throws(() => failing.edit(small, {}), /An edit takes one of replace, insert, remove, append and prepend/);
assert.throws(() => failing.edit(small, []), /Edit takes an edit or an array of 1 to 1024 edits/);
assert.throws(() => failing.edit(small, { replace: "a" }), /Replace needs with, the text to put in its place/);
assert.throws(() => failing.edit(small, { replace: "z", with: "c" }), /The code has no 'z'/);
assert.throws(() => failing.edit(small, [{ append: "c" }, { remove: /z/ }]), /Edit 2: The code has no \/z\//);
assert.throws(() => failing.edit(small, { replace: "a", with: "c", match: 3 }), /Match 3 is past the last of the 2 matches of 'a'/);
assert.throws(() => failing.edit(small, { insert: "x" }), /Insert needs one of before and after/);
assert.throws(() => failing.edit(small, { insert: "x", before: 1, after: 1 }), /Insert needs one of before and after/);
assert.throws(() => failing.edit(small, { insert: "x", after: [1, 2] }), /Before and after take a text to find or a line number/);
assert.throws(() => failing.edit(small, { remove: 3 }), /Line 3 is past the last of the code's 2 lines/);
assert.throws(() => failing.edit(small, { remove: [2, 1] }), /Remove takes a text to find, a line number or \[first, last\]/);
assert.throws(() => failing.edit(small, { remove: 1, whole: true }), /Match, lines and whole need a text to find, not line numbers/);
assert.throws(() => failing.edit(small, { remove: "a", with: "c" }), /Unknown remove option: with/);
assert.throws(() => failing.edit(small, { append: 1 }), /Append takes a string/);
assert.throws(() => failing.edit(small, { remove: "" }), /The text to find cannot be empty/);
assert.throws(() => failing.edit(small, "a"), /An edit must be a plain object/);
assert.throws(() => failing.edit(small, { append: "c" }, 1, "smooth", { spin: 1 }), /Unknown Edit option: spin/);
assert.throws(() => failing.edit(small, { append: "c" }, 1, "smooth", { id: 1 }), /Edit id must be a string/);
assert.throws(() => failing.edit(small, { append: "c" }, 1, "smooth", { fade: 0 }), /Edit fade must be a share of the duration/);
assert.throws(() => failing.edit(failing.circle(), { append: "c" }), /Edit needs a block that code made/);

// Area calls its functions in the builder and emits the polygon Lua area makes.
const shaded = vis.scene({ width: 320, height: 180 });
const shadedAxes = shaded.space({ x: [0, 5, 1], y: [-1, 3, 1] });
shadedAxes.area({ fn: (x) => 1 + x, x_range: [0, 1, 0.5], fill: "#2563eb55", id: "under" });
shadedAxes.area({ fn: () => 2, x_range: [0, 1, 0.5], base: (x) => x, id: "between" });
shadedAxes.area({ fn: () => 2, x_range: [0, 1, 0.5], base: -1 });
const shadedLua = compileScene(shaded);
assert.match(
    shadedLua,
    /objects\[1\]:polygon \{\["fill"\]="#2563eb55",\["id"\]="under",\["points"\]=\{\{0,1\},\{0\.5,1\.5\},\{1,2\},\{1,0\},\{0,0\}\}\}/,
);
assert.match(shadedLua, /\["id"\]="between",\["points"\]=\{\{0,2\},\{0\.5,2\},\{1,2\},\{1,1\},\{0\.5,0\.5\},\{0,0\}\}/);
assert.match(shadedLua, /\["points"\]=\{\{0,2\},\{0\.5,2\},\{1,2\},\{1,-1\},\{0,-1\}\}/);
assert.throws(() => shadedAxes.area({ x_range: [0, 1, 0.5] }), /Area requires fn, a function/);
assert.throws(() => shadedAxes.area({ fn: (x) => x }), /Area requires x_range/);
assert.throws(() => shadedAxes.area({ fn: (x) => x, x_range: [0, 1, 0.5], base: "axis" }), /base must be a number or a function/);
assert.throws(() => shadedAxes.area({ fn: () => NaN, x_range: [0, 1, 0.5] }), /Area fn must return a finite number/);
assert.throws(() => shadedAxes.area({ fn: (x) => x, x_range: [1, 1, 0.5] }), /Area x_range must cover from 1 to/);
// A Riemann sum calls fn where Lua riemann would, emits the heights, and names its bars.
const sum = shadedAxes.riemann({ fn: (x) => 1 + x, x_range: [0, 2, 0.5], sample: "midpoint", fill: "#f59e0b99", id: "sum" });
assert.equal(sum.children().length, 4);
assert.equal(shaded.find("sum/3"), sum.children()[2]);
const given = shadedAxes.riemann({ values: [1, -1], x_range: [0, 1, 0.5], negative: "#ef4444" });
const sumLua = compileScene(shaded);
assert.match(
    sumLua,
    /(objects\[\d+\]) = objects\[1\]:riemann \{\["fill"\]="#f59e0b99",\["id"\]="sum",\["values"\]=\{1\.25,1\.75,2\.25,2\.75\},\["x_range"\]=\{0,2,0\.5\}\}\n(objects\[\d+\]) = \1:children\(\)/,
);
assert.match(sumLua, /objects\[1\]:riemann \{\["negative"\]="#ef4444",\["values"\]=\{1,-1\},\["x_range"\]=\{0,1,0\.5\}\}/);
shaded.fadeIn(given.children()[1]);
assert.match(compileScene(shaded), /scene:fade_in\(objects\[\d+\]\[2\]/);
assert.throws(() => shadedAxes.riemann({ fn: (x) => x }), /Riemann requires x_range/);
assert.throws(() => shadedAxes.riemann({ x_range: [0, 1, 0.5] }), /Riemann requires either fn or values/);
assert.throws(() => shadedAxes.riemann({ values: [1], x_range: [0, 1, 0.5] }), /one finite height for each of the 2 steps/);
assert.throws(() => shadedAxes.riemann({ values: [1, 2], x_range: [0, 1, 0.5], sample: "left" }), /sample needs fn/);
assert.throws(() => shadedAxes.riemann({ fn: (x) => x, x_range: [0, 1, 0.5], sample: "center" }), /left, right, or midpoint/);
assert.throws(() => shadedAxes.riemann({ fn: () => Infinity, x_range: [0, 1, 0.5] }), /Riemann fn must return a finite number/);
assert.throws(() => shadedAxes.riemann({ fn: (x) => x, x_range: [0, 2000, 1] }), /cover from 1 to 1024 steps/);
// A surface colored by a function emits the colors the builder samples at its points, in order;
// a colormap's name, a colormap object and a list of colors pass through as they are.
const colored = vis.scene({ width: 320, height: 180, camera: { view: "3d", eye: [3, -4, 3], up: [0, 0, 1] } });
const bowl = [[0, 0, 0], [1, 0, 0.25], { x: 0, y: 1, z: 0.25 }, [1, 1]];
const heights = [];
colored.surface({
    points: bowl, size: [2, 2], id: "by-function",
    colors: (x, y, z) => {
        heights.push(z);
        return z > 0.2 ? "#ff0000" : "accent";
    },
});
assert.deepEqual(heights, [0, 0.25, 0.25, 0]);
colored.surface({ points: bowl, size: [2, 2], colors: "viridis" });
colored.surface({ points: bowl, size: [2, 2], colors: { map: "magma", range: [1, 0], axis: [0, 1, 0] } });
colored.surface({ points: bowl, size: [2, 2], colors: ["#000", "#fff", "#000", "danger"] });
colored.surface({ points: bowl, size: [2, 2], mode: "solid_mesh", mesh: [2, 0] });
colored.surface({ points: bowl, size: [2, 2], light: { direction: [-1, 1, 1], ambient: 0.3 } });
// Two surfaces on one grid morph point by point, their colormap read again between them.
const fromBowl = colored.surface({ points: bowl, size: [2, 2], colors: "viridis" });
const toSaddle = colored.surface({ points: [[0, 0, 0], [1, 0, 0.25], [0, 1, -0.25], [1, 1, 0]], size: [2, 2], colors: "viridis" });
colored.morph(fromBowl, toSaddle, 1, "linear");
const surfaceLua = compileScene(colored);
assert.match(surfaceLua, /scene:morph\(objects\[\d+\],objects\[\d+\],1,"linear"\)/);
assert.match(surfaceLua, /scene:surface \{\["light"\]=\{\["ambient"\]=0\.3,\["direction"\]=\{-1,1,1\}\},/);
assert.match(surfaceLua, /scene:surface \{\["mesh"\]=\{2,0\},\["mode"\]="solid_mesh",/);
assert.match(surfaceLua, /scene:surface \{\["colors"\]=\{"accent","#ff0000","#ff0000","accent"\},\["id"\]="by-function"/);
assert.match(surfaceLua, /scene:surface \{\["colors"\]="viridis",/);
assert.match(surfaceLua, /scene:surface \{\["colors"\]=\{\["axis"\]=\{0,1,0\},\["map"\]="magma",\["range"\]=\{1,0\}\},/);
assert.match(surfaceLua, /scene:surface \{\["colors"\]=\{"#000","#fff","#000","danger"\},/);
assert.throws(() => colored.surface({ points: bowl, size: [2, 2], colors: () => 1 }),
    /Surface colors must return a hex color or Theme color role; at \(0, 0, 0\) it did not/);
assert.throws(() => colored.surface({ points: [[0, 0, 0], [1]], size: [2, 1], colors: () => "#000" }),
    /Surface points must contain 2 or 3 finite numbers/);
assert.throws(() => colored.surface({ size: [2, 2], colors: () => "#000" }), /Surface requires points/);
assert.throws(() => colored.surface({ points: bowl, size: [2, 2], colors: () => colored.circle() }),
    /unavailable inside a sampling callback/);

// Level labels retain native text children while the builder keeps one handle per level.
const labelledContours = vis.scene({ width: 320, height: 180, fps: 4, camera: { mode: "fixed", height: 6 } });
const labelledContourGroup = labelledContours.contour({ id: "labels", fn: (x, y) => x+y, levels: [0],
    x_range: [-2, 2, 0.25], y_range: [-2, 2, 0.25],
    labels: { format: "%.2f", size: 0.2, spacing: 0, padding: 0.05, upright: false } });
const labelledContourLevel = labelledContourGroup.children()[0];
const labelTracker = labelledContours.tracker(0);
labelledContours.bind(labelledContourLevel, labelTracker, (level) => ({ fn: (x, y) => x+y, level }));
labelledContours.track(labelTracker, 1, 1, "linear");
assert.equal(labelledContourGroup.children().length, 1);
assert.match(compileScene(labelledContours), /\["labels"\]=\{\["format"\]="%.2f",\["padding"\]=0.05,\["size"\]=0.2,\["spacing"\]=0,\["upright"\]=false\}/);
const labelledContoursScript = `
local s = vis.scene {width=320,height=180,fps=4,camera={mode="fixed",height=6}}
local c = s:contour {id="labels",fn=function(x,y) return x+y end,levels={0},
  x_range={-2,2,0.25},y_range={-2,2,0.25},
  labels={format="%.2f",size=0.2,spacing=0,padding=0.05,upright=false}}
local a=s:tracker(0)
s:bind(c:children()[1],a,function(v) return {fn=function(x,y) return x+y end,level=v} end)
s:track(a,1,1,"linear")
return s
`;
const invalidLabels = vis.scene();
const contourWithLabels = (labels) => invalidLabels.contour({ fn: (x, y) => x+y, levels: [0],
    x_range: [-1, 1, 1], y_range: [-1, 1, 1], labels });
assert.throws(() => contourWithLabels(1), /labels must be a boolean or a plain object/);
assert.throws(() => contourWithLabels({ format: "%n" }), /format must be %.0f through %.6f/);
assert.throws(() => contourWithLabels({ size: 0 }), /size must be positive/);
assert.throws(() => contourWithLabels({ spacing: -1 }), /spacing must be zero or more/);
assert.throws(() => contourWithLabels({ padding: Infinity }), /padding must be zero or more/);
assert.throws(() => contourWithLabels({ upright: 1 }), /upright must be a boolean/);
assert.throws(() => contourWithLabels({ extra: true }), /Unknown Contour labels option: extra/);
contourWithLabels(true);
contourWithLabels(false);

// Contours emit scalar samples in grid order and expose one plot handle per level.
const contourTwin = vis.scene({ width: 320, height: 180 });
const contourAxes = contourTwin.space({ stroke: "#00000000", axis_x: "#00000000", axis_y: "#00000000", axis_z: "#00000000" });
const contourGroup = contourAxes.contour({ fn: (x, y) => x + y, levels: [0.5, 1.5],
    x_range: [0, 1, 1], y_range: [0, 1, 1], colors: ["#ff0000", "#00ff00"], id: "contours", width: 3 });
assert.equal(contourGroup.children().length, 2);
assert.equal(contourTwin.find("contours/1"), contourGroup.children()[0]);
assert.equal(contourTwin.find("contours/2"), contourGroup.children()[1]);
assert.match(compileScene(contourTwin), /:contour \{.*\["levels"\]=\{0.5,1.5\},.*\["values"\]=\{0,1,1,2\}/);
assert.doesNotMatch(compileScene(contourTwin), /\["fn"\]/);
const contourOptions = { values: [0, 1, 1, 2], levels: [0.5], x_range: [0, 1, 1], y_range: [0, 1, 1] };
const invalidContour = vis.scene();
assert.throws(() => invalidContour.contour({ ...contourOptions, levels: [] }), /levels must hold from 1/);
assert.throws(() => invalidContour.contour({ ...contourOptions, levels: [NaN] }), /levels must hold from 1/);
assert.throws(() => invalidContour.contour({ ...contourOptions, x_range: [0, 0, 1] }), /at least two samples/);
assert.throws(() => invalidContour.contour({ ...contourOptions, x_range: [0, 300, 1], y_range: [0, 300, 1] }), /at most 65536 samples/);
assert.throws(() => invalidContour.contour({ ...contourOptions, values: [0, 1] }), /each of the 4 samples/);
assert.throws(() => invalidContour.contour({ ...contourOptions, values: [0, 1, 1, Infinity] }), /at \(1, 1\)/);
assert.throws(() => invalidContour.contour({ ...contourOptions, fn: (x, y) => x + y }), /requires either fn or values/);
assert.throws(() => invalidContour.contour({ ...contourOptions, colors: "rainbow" }), /one color per level or name a colormap/);
assert.throws(() => invalidContour.contour({ ...contourOptions, colors: [] }), /one color per level/);
assert.throws(() => invalidContour.contour({ ...contourOptions, labels: "yes" }), /labels must be a boolean or a plain object/);
assert.throws(() => invalidContour.contour({ ...contourOptions, values: undefined, fn: () => NaN }), /at \(0, 0\)/);
assert.throws(() => invalidContour.contour({ ...contourOptions, values: undefined,
    fn: () => { invalidContour.circle(); return 0; } }), /Scene authoring is unavailable inside a sampling callback/);
assert.throws(() => invalidContour.contour({ ...contourOptions, levels: Array(16).fill(0),
    x_range: [0, 255, 1], y_range: [0, 255, 1] }), /samples times levels/);
contourTwin.create(contourGroup.children()[0], 0.2);
contourTwin.indicate(contourGroup.children()[0], { duration: 0.2 });
contourTwin.passingFlash(contourGroup.children()[0], { duration: 0.2 });
contourTwin.fadeIn(contourGroup, { duration: 0.2 });
const contourTwinScript = `
local s = vis.scene {width=320, height=180}
local axes = s:space {stroke="#00000000", axis_x="#00000000", axis_y="#00000000", axis_z="#00000000"}
local c = axes:contour {fn=function(x,y) return x+y end, levels={0.5,1.5},
  x_range={0,1,1}, y_range={0,1,1}, colors={"#ff0000","#00ff00"}, id="contours", width=3}
local p = c:children()[1]
s:create(p, 0.2)
s:indicate(p, {duration = 0.2})
s:passing_flash(p, {duration = 0.2})
s:fade_in(c, {duration = 0.2})
return s`;

// Contour children re-trace fields at tracker samples, emitting grids for the Lua geometry path.
const changingContours = vis.scene({ width: 320, height: 180, fps: 4 });
const axisLength = changingContours.tracker(0.75);
const movingEllipse = changingContours.contour({ id: "ellipse", levels: [0],
    fn: (x, y) => x*x/(0.75*0.75)+y*y-1, x_range: [-2, 2, 0.125], y_range: [-1.5, 1.5, 0.125] });
const ellipseChild = movingEllipse.children()[0];
changingContours.bind(ellipseChild, axisLength, (a) => ({ fn: (x, y) => x*x/(a*a)+y*y-1 }));
changingContours.track(axisLength, 1.5, 1, "linear");
const movingLevel = changingContours.tracker(0.5);
const wellField = (x, y) => (x*x-1)*(x*x-1)+y*y;
const changingTopology = changingContours.contour({ id: "topology", fn: wellField, levels: [0.5],
    x_range: [-2, 2, 0.125], y_range: [-1.5, 1.5, 0.125] });
changingContours.bind(changingTopology.children()[0], movingLevel, (level) => ({ fn: wellField, level }));
changingContours.track(movingLevel, 1.1, 0.25, "linear");
changingContours.track(movingLevel, -0.2, 0.25, "linear");
changingContours.track(movingLevel, 0.5, 0.25, "linear");
const contourCopy = changingContours.clone(ellipseChild, { id: "copy", opacity: 0 });
changingContours.set({ target: contourCopy, values: [0, 1, 2, 3], level: 1,
    x_range: [0, 1, 1], y_range: [0, 1, 1] });
changingContours.play({ target: contourCopy, values: [0, 1, 2, 3], level: 2 }, 0.25, "linear");
const movingContoursLua = compileScene(changingContours);
assert.match(movingContoursLua, /scene:set\(\{\{target=objects\[2\]\[1\],values=\{/);
assert.match(movingContoursLua, /level=2,values=\{0,1,2,3\}/);
assert.doesNotMatch(movingContoursLua, /fn=/);
const invalidContourStates = vis.scene();
const validContourState = invalidContourStates.contour(contourOptions).children()[0];
assert.throws(() => invalidContourStates.set({ target: validContourState, level: 1 }), /level needs fn or values/);
assert.throws(() => invalidContourStates.set({ target: validContourState, points: [[0, 0], [1, 1]] }),
    /not points or discontinuities/);
assert.throws(() => invalidContourStates.set({ target: validContourState, values: [0, 1, 1, 2], level: NaN }),
    /level must be a finite number/);
assert.throws(() => invalidContourStates.set({ target: validContourState, fn: (x, y) => x+y, values: [0, 1, 1, 2] }),
    /requires either fn or values/);
assert.throws(() => invalidContourStates.set({ target: validContourState, values: [0, 1, 1, Infinity] }), /at \(1, 1\)/);
assert.throws(() => invalidContourStates.set({ target: validContourState, fn: () => {
    invalidContourStates.circle(); return 0;
} }), /Scene authoring is unavailable inside a sampling callback/);
assert.throws(() => invalidContourStates.set({ target: invalidContourStates.plot({ points: [[0, 0], [1, 1]] }),
    values: [0, 1, 1, 2] }), /level and values need a contour child/);
const changingContoursScript = `
local s=vis.scene{width=320,height=180,fps=4}
local function samples(fn)
  local values={}
  for y=0,24 do for x=0,32 do values[#values+1]=fn(-2+x*0.125,-1.5+y*0.125) end end
  return values
end
local function ellipse(a) return samples(function(x,y) return x*x/(a*a)+y*y-1 end) end
local a=s:tracker(0.75)
local e=s:contour{id="ellipse",values=ellipse(0.75),levels={0},x_range={-2,2,0.125},y_range={-1.5,1.5,0.125}}
s:bind(e:children()[1],a,function(v) return {values=ellipse(v)} end)
s:track(a,1.5,1,"linear")
local k=s:tracker(0.5)
local values=samples(function(x,y) return (x*x-1)*(x*x-1)+y*y end)
local t=s:contour{id="topology",values=values,levels={0.5},x_range={-2,2,0.125},y_range={-1.5,1.5,0.125}}
s:bind(t:children()[1],k,function(v) return {values=values,level=v} end)
s:track(k,1.1,0.25,"linear")
s:track(k,-0.2,0.25,"linear")
s:track(k,0.5,0.25,"linear")
local copy=s:clone(e:children()[1],{id="copy",opacity=0})
s:set{target=copy,values={0,1,2,3},level=1,x_range={0,1,1},y_range={0,1,1}}
s:play({target=copy,values={0,1,2,3},level=2},0.25,"linear")
return s`;

// A vector field calls fn where Lua vector_field would, emits the vectors, and names its arrows
// by column and row, leaving out the sample where the field vanishes.
const field = shadedAxes.vectorField({ fn: (x, y) => [-y, x], x_range: [-1, 1, 1], y_range: [-1, 1, 1], anchor: "middle", id: "field" });
assert.equal(field.children().length, 8);
assert.equal(shaded.find("field/1/1"), field.children()[0]);
assert.equal(shaded.find("field/3/2"), field.children()[4]);
assert.equal(shaded.find("field/2/2"), null);
const steady = shadedAxes.vectorField({ values: [[1, 0], [0, 0]], x_range: [0, 1, 1], y_range: [2, 2, 1], scale: 0.5, color: "#ff0000" });
assert.equal(steady.children().length, 1);
const fieldLua = compileScene(shaded);
assert.match(
    fieldLua,
    /(objects\[\d+\]) = objects\[1\]:vector_field \{\["anchor"\]="middle",\["id"\]="field",\["values"\]=\{\{1,-1\},\{1,0\},\{1,1\},\{0,-1\},\{0,0\},\{0,1\},\{-1,-1\},\{-1,0\},\{-1,1\}\},\["x_range"\]=\{-1,1,1\},\["y_range"\]=\{-1,1,1\}\}\n(objects\[\d+\]) = \1:children\(\)/,
);
assert.match(fieldLua, /vector_field \{\["color"\]="#ff0000",\["scale"\]=0\.5,\["values"\]=\{\{1,0\},\{0,0\}\}/);
assert.throws(() => shadedAxes.vectorField({ fn: (x, y) => [x, y], y_range: [0, 1, 1] }), /Vector field requires x_range/);
assert.throws(() => shadedAxes.vectorField({ fn: (x, y) => [x, y], x_range: [0, 1, 1] }), /Vector field requires y_range/);
assert.throws(() => shadedAxes.vectorField({ x_range: [0, 1, 1], y_range: [0, 1, 1] }), /Vector field requires either fn or values/);
assert.throws(() => shadedAxes.vectorField({ values: [[1, 0]], x_range: [0, 1, 1], y_range: [0, 1, 1] }),
    /one \[x, y\] for each of the 4 samples of x_range and y_range/);
assert.throws(() => shadedAxes.vectorField({ fn: (x) => x, x_range: [0, 1, 1], y_range: [0, 1, 1] }),
    /Vector field fn must return \[x, y\] of finite numbers; at \(0, 0\) it did not/);
assert.throws(() => shadedAxes.vectorField({ fn: (x, y) => [x, y], x_range: [0, 100, 1], y_range: [0, 100, 1] }), /at most 1024 arrows/);
assert.throws(() => shadedAxes.vectorField({ fn: (x, y) => [x, y], x_range: [0, 2000, 1], y_range: [0, 0, 1] }),
    /Vector field x_range must hold from 1 to 1024 samples/);
assert.throws(() => shadedAxes.vectorField([]), /Vector field options must be a plain object/);
// The builder leaves out the arrows the scene does: the speck beside the origin that a grid
// symmetric about zero samples, and any under a thousandth of the length or the longest arrow.
assert.equal(shadedAxes.vectorField({ fn: (x) => [x, 0], x_range: [-4.2, 4.2, 0.7], y_range: [0, 0, 1] }).children().length, 12);
assert.equal(shadedAxes.vectorField({ values: [[1e-4, 0], [1, 0]], x_range: [0, 1, 1], y_range: [0, 0, 1], length: 1 }).children().length, 1);
assert.equal(shadedAxes.vectorField({ values: [[1e-4, 0], [1, 0]], x_range: [0, 1, 1], y_range: [0, 0, 1], scale: 2 }).children().length, 1);
assert.throws(() => shadedAxes.vectorField({ values: [[1, 0]], x_range: [0, 0, 1], y_range: [0, 0, 1], length: 1, scale: 1 }),
    /Vector field length and scale cannot be used together/);
assert.throws(() => shadedAxes.vectorField({ values: [[1, 0]], x_range: [0, 0, 1], y_range: [0, 0, 1], length: 0 }),
    /Vector field length must be a positive number of world units/);
assert.throws(() => shadedAxes.vectorField({ values: [[1, 0]], x_range: [0, 0, 1], y_range: [0, 0, 1], scale: -1 }),
    /Vector field scale must be a positive number/);

// A region names the shapes it combines as the script does.
const venn = vis.scene({ width: 320, height: 180 });
const leftDisk = venn.circle({ center: [-0.5, 0], radius: 1, id: "left" });
const rightDisk = venn.circle({ center: [0.5, 0], radius: 1, id: "right" });
const vennGroup = venn.group({ id: "pair" });
const vennSquare = vennGroup.rectangle({ size: [2, 2] });
const vennHole = vennGroup.circle({ radius: 0.5 });
venn.intersection({ of: [leftDisk, rightDisk], fill: "#a855f7", id: "lens" });
venn.union({ of: [leftDisk, rightDisk], id: "both" });
venn.difference({ of: [leftDisk, rightDisk], id: "crescent" });
venn.exclusion({ of: [leftDisk, rightDisk], id: "apart" });
vennGroup.difference({ of: [vennSquare, vennHole], id: "ring" });
assert.throws(() => venn.union({ of: [leftDisk] }), /Of must be an array of 2 to 64 shape handles/);
assert.throws(() => venn.union({ of: [leftDisk, "right"] }), /Of must be an array of 2 to 64 shape handles/);
assert.throws(() => venn.union({ of: [leftDisk, shaded.find("under")] }), /handles from the same scene/);
const vennLua = compileScene(venn);
assert.match(vennLua, /objects\[\d+\] = scene:intersection \{\["fill"\]="#a855f7",\["id"\]="lens",\["of"\]=\{objects\[1\],objects\[2\]\}\}/);
assert.match(vennLua, /scene:union \{\["id"\]="both",\["of"\]=\{objects\[1\],objects\[2\]\}\}/);
assert.match(vennLua, /scene:difference \{\["id"\]="crescent",\["of"\]=\{objects\[1\],objects\[2\]\}\}/);
assert.match(vennLua, /scene:exclusion \{\["id"\]="apart",\["of"\]=\{objects\[1\],objects\[2\]\}\}/);
assert.match(vennLua, /objects\[3\]:difference \{\["id"\]="ring",\["of"\]=\{objects\[4\],objects\[5\]\}\}/);

// Arrange names a group or a list of handles and passes its options as Lua arrange takes them.
const shelf = vis.scene({ width: 320, height: 180 });
const shelfRow = shelf.group({ id: "row" });
const shelfA = shelfRow.rectangle({ id: "a", size: [1, 1] });
const shelfB = shelfRow.rectangle({ id: "b", size: [1, 1] });
shelfRow.arrange([1, 0], 0.5);
shelf.wait(0.5);
const shelfC = shelfRow.rectangle({ id: "c", size: [1, 1] });
shelf.arrange([shelfA, shelfC, shelfB], 1, "linear", { gap: 0.5 });
shelf.arrange(shelfRow, 0.5, { preset: "back", strength: 0.6 }, { direction: [0, -1], gap: 0, center: [2, 0] });
assert.throws(() => shelf.arrange([]), /Arrange takes a group or a non-empty array of handles/);
assert.throws(() => shelf.arrange(shelfA), /Arrange takes a group or a non-empty array of handles/);
assert.throws(() => shelf.arrange(shelfRow, 1, "smooth", { spin: 1 }), /Unknown Arrange option: spin/);
assert.throws(() => shelf.arrange(shelfRow, 1, "smooth", { gap: -1 }), /Arrange gap must be a finite number of zero or more/);
assert.throws(() => shelf.arrange(shelfRow, 1, "smooth", { center: [0] }), /Arrange center must be \[x, y\]/);
const shelfLua = compileScene(shelf);
assert.match(shelfLua, /scene:arrange\(\{objects\[2\],objects\[4\],objects\[3\]\},1,"linear",\{\["gap"\]=0\.5\}\)/);
assert.match(
    shelfLua,
    /scene:arrange\(objects\[1\],0\.5,\{\["preset"\]="back",\["strength"\]=0\.6\},\{\["center"\]=\{2,0\},\["direction"\]=\{0,-1\},\["gap"\]=0\}\)/,
);

// A matrix names its rows, entries and brackets as the native factory does, and its rows,
// entries and columns are the handles the builder tracks.
const grid = vis.scene({ width: 320, height: 180 });
const matrix = grid.matrix({ rows: [[2, -1, 0], [1, 3, "x"]], size: 24, id: "m", justify: ["left", "center", "right"] });
const unnamed = grid.group().matrix({ rows: [[1, 2]], brackets: "none" });
assert.equal(matrix.children().length, 4);
assert.equal(grid.find("m/2/3"), matrix.entry(2, 3));
assert.equal(grid.find("m/1"), matrix.row(1));
assert.equal(grid.find("m/right"), matrix.children()[3]);
assert.deepEqual(matrix.column(2), [grid.find("m/1/2"), grid.find("m/2/2")]);
assert.equal(unnamed.children().length, 1);
grid.play({ target: matrix.entry(2, 1), value: 7 }, 1);
grid.rectangle({ around: matrix.row(2), fill: "#dbeafe", layer: -1, id: "band" });
assert.throws(() => grid.matrix({}), /Matrix rows must be an array of rows, each an array of entries/);
assert.throws(() => grid.matrix({ rows: [[1], []] }), /Matrix row 2 must be an array of at least one entry/);
assert.throws(() => grid.matrix({ rows: [[1, 2], [3]] }), /as many entries as the first, 2; row 2 holds 1/);
assert.throws(() => grid.matrix({ rows: [[true]] }), /A matrix entry must be a number or a string/);
assert.throws(() => grid.matrix({ rows: [[Infinity]] }), /Matrix entries must be finite numbers/);
assert.throws(() => grid.matrix({ rows: [[1]], brackets: "curly" }), /Matrix brackets must be square, round, bar, or none/);
assert.throws(() => grid.matrix({ rows: Array.from({ length: 33 }, () => Array(32).fill(0)) }), /at most 1024 entries/);
assert.throws(() => grid.matrix("rows"), /Matrix options must be a plain object/);
const gridLua = compileScene(grid);
assert.match(gridLua, /objects\[1\] = scene:matrix \{\["id"\]="m",\["justify"\]=\{"left","center","right"\},\["rows"\]=\{\{2,-1,0\},\{1,3,"x"\}\},\["size"\]=24\}/);
assert.match(gridLua, /objects\[3\] = \{\} for i, row in ipairs\(objects\[2\]\) do objects\[3\]\[i\] = row:children\(\) end/);
assert.match(gridLua, /scene:play\(\{\{target=objects\[3\]\[2\]\[1\],value=7\}\},1,"smooth",0\)/);
assert.match(gridLua, /scene:rectangle \{\["around"\]=objects\[2\]\[2\],\["fill"\]="#dbeafe",\["id"\]="band",\["layer"\]=-1\}/);

// A table names its rows and entries with the header as the first row, and tracks its stripes
// and rules in the order the native factory adds them.
const ledger = vis.scene({ width: 320, height: 180 });
const sheet = ledger.table({ header: ["n", "n2"], rows: [[1, 1], [2, 4], [3, 9]], stripes: true, rules: "grid", id: "t" });
const loose = ledger.group().table({ rows: [["a", "b"], ["c", "d"]], stripes: "#00ff00", rules: "columns" });
// Four rows, a stripe behind the second row under the header, and the rule under the header,
// two between rows, one between columns and four around.
assert.equal(sheet.children().length, 13);
assert.equal(ledger.find("t/1/2"), sheet.entry(1, 2));
assert.equal(ledger.find("t/4"), sheet.row(4));
assert.deepEqual(sheet.column(1), [1, 2, 3, 4].map((r) => ledger.find(`t/${r}/1`)));
assert.equal(loose.children().length, 4);
assert.equal(ledger.group().table({ rows: [[1]], header: ["x"] }).children().length, 3);
ledger.play({ target: sheet.entry(4, 2), value: 16 }, 1);
ledger.rectangle({ around: sheet.row(3), fill: "#dbeafe", layer: -1, id: "band" });
assert.throws(() => ledger.table({}), /Table rows must be an array of rows, each an array of entries/);
assert.throws(() => ledger.table({ rows: [[1, 2]], header: ["a"] }), /Table header must be an array of one entry for each of its 2 columns/);
assert.throws(() => ledger.table({ rows: [[1]], header: [null] }), /A table entry must be a number or a string/);
assert.throws(() => ledger.table({ rows: [[1]], rules: "dots" }), /Table rules must be header, rows, columns, grid, or none/);
assert.throws(() => ledger.table({ rows: [[1]], stripes: 2 }), /Table stripes must be true or a color/);
assert.throws(() => ledger.table({ rows: Array.from({ length: 32 }, () => Array(32).fill(0)), header: Array(32).fill("h") }),
    /A table holds at most 1024 entries/);
assert.throws(() => ledger.table(null), /Table options must be a plain object/);
const ledgerLua = compileScene(ledger);
assert.match(ledgerLua, /objects\[1\] = scene:table \{\["header"\]=\{"n","n2"\},\["id"\]="t",\["rows"\]=\{\{1,1\},\{2,4\},\{3,9\}\},\["rules"\]="grid",\["stripes"\]=true\}/);
assert.match(ledgerLua, /objects\[3\] = \{\} for i = 1, 4 do objects\[3\]\[i\] = objects\[2\]\[i\]:children\(\) end/);
assert.match(ledgerLua, /scene:play\(\{\{target=objects\[3\]\[4\]\[2\],value=16\}\},1,"smooth",0\)/);
assert.match(ledgerLua, /scene:rectangle \{\["around"\]=objects\[2\]\[3\],\["fill"\]="#dbeafe",\["id"\]="band",\["layer"\]=-1\}/);

// A clone copies a handle and the handles below it. The copy's handles mirror the source's
// through the table the script returns with the copy, and with an id find() knows the copy and
// the ids below it: a nested id takes the copy's id in place of the source's, and any other
// takes the copy's id and a slash before it.
const cloning = vis.scene({ width: 160, height: 90 });
const pair = cloning.group({ id: "pair" });
const pairA = pair.circle({ id: "pair/a", radius: 0.5 });
pair.rectangle({ id: "b", size: [1, 1] });
const cloneWord = cloning.text({ id: "word", text: "hot" });
const twinCopy = cloning.clone(pair, { id: "twin", stroke: "#ff0000", shift: [0, -2] });
assert.equal(typeof twinCopy.arrange, "function");
assert.deepEqual(twinCopy.children().map((handle) => handle.name), ["objects[6][objects[2]]", "objects[6][objects[3]]"]);
assert.equal(cloning.find("twin"), twinCopy);
assert.equal(cloning.find("twin/a"), twinCopy.children()[0]);
assert.equal(cloning.find("twin/b"), twinCopy.children()[1]);
// Without an id the copy takes its id while the script runs, so find() does not know it.
const wordCopy = cloning.clone(cloneWord);
assert.equal(wordCopy.name, "objects[7]");
assert.equal(cloning.find("word:copy"), null);
assert.equal(typeof wordCopy.arrange, "undefined");
const inner = cloning.clone(pairA, { parent: twinCopy, opacity: 0.5 });
assert.deepEqual(twinCopy.children(), [cloning.find("twin/a"), cloning.find("twin/b"), inner]);
cloning.play({ target: cloning.find("twin/b"), opacity: 0.5 }, 1);
cloning.at(0.5, (scene) => scene.clone(cloneWord, { id: "half" }));
const cloningLua = compileScene(cloning);
assert.match(cloningLua, /objects\[5\],objects\[6\] = scene:clone\(objects\[1\],\{\["id"\]="twin",\["shift"\]=\{0,-2\},\["stroke"\]="#ff0000"\}\)/);
assert.match(cloningLua, /objects\[7\],objects\[8\] = scene:clone\(objects\[4\]\)/);
assert.match(cloningLua, /objects\[9\],objects\[10\] = scene:clone\(objects\[2\],\{\["opacity"\]=0\.5,\["parent"\]=objects\[5\]\}\)/);
assert.match(cloningLua, /scene:play\(\{\{target=objects\[6\]\[objects\[3\]\],opacity=0\.5\}\},1,"smooth",0\)/);
assert.match(cloningLua, /scene:seek\(0\.5\)\s*objects\[11\],objects\[12\] = scene:clone\(objects\[4\],\{\["id"\]="half"\}\)/);
const refusing = vis.scene({ width: 160, height: 90 });
const refusingSpace = refusing.space();
const refusingCell = refusingSpace.cell({ position: [0, 0] });
const refusingHolder = refusing.group();
refusingHolder.space();
const refusingBox = refusing.rectangle({ size: [1, 1] });
const refusingLink = refusing.connector(refusingBox, refusing.circle({ radius: 0.5 }));
const refusingBefore = compileScene(refusing);
assert.throws(() => refusing.clone(refusingSpace), /A space cannot be cloned yet/);
assert.throws(() => refusing.clone(refusingCell), /A cell cannot be cloned yet/);
assert.throws(() => refusing.clone(refusingHolder), /A space cannot be cloned yet/);
assert.throws(() => refusing.clone(refusingLink, { shift: [1, 0] }), /cannot be shifted or transformed/);
assert.throws(() => refusing.clone(pair), /handles from the same scene/);
assert.throws(() => refusing.clone(refusingBox, { parent: pair }), /handles from the same scene/);
assert.throws(() => refusing.clone(refusingBox, null), /Clone options must be a plain object/);
assert.throws(() => refusing.clone(refusingBox, [1]), /Clone options must be a plain object/);
assert.equal(compileScene(refusing), refusingBefore);

// A morph takes its lag or {lag, copy}, and a match takes copy, to transform a copy of the
// source and leave the source in place.
const copying = vis.scene({ width: 160, height: 90 });
const copySquare = copying.rectangle({ size: [1, 1] });
const copyDisc = copying.circle({ radius: 0.5 });
copying.morph(copySquare, copyDisc, 1, "linear", { copy: true });
const copyLeft = copying.circle({ radius: 0.25 });
const copyRight = copying.circle({ radius: 0.25 });
// With copy, one source may give a copy to several targets.
copying.morph([copySquare, copySquare], [copyLeft, copyRight], 1, "smooth", { lag: 0.5, copy: true });
const copyFrom = copying.group();
copyFrom.circle({ radius: 0.5 });
copying.wait(1);
const copyTo = copying.group();
copyTo.circle({ radius: 0.5 });
copying.matchTransform(copyFrom, copyTo, 1, "smooth", { copy: true, shapes: true });
const copyingLua = compileScene(copying);
assert.match(copyingLua, /scene:morph\(objects\[1\],objects\[2\],1,"linear",\{\["copy"\]=true\}\)/);
assert.match(copyingLua, /scene:morph\(\{objects\[1\],objects\[1\]\},\{objects\[3\],objects\[4\]\},1,"smooth",\{\["copy"\]=true,\["lag"\]=0\.5\}\)/);
assert.match(copyingLua, /scene:match_transform\(objects\[5\],objects\[7\],1,"smooth",\{\["shapes"\]=true,\["copy"\]=true\}\)/);
const copyingBefore = compileScene(copying);
assert.throws(() => copying.morph(copySquare, copyDisc, 1, "smooth", { copy: 1 }), /Morph copy must be a boolean/);
assert.throws(() => copying.morph(copySquare, copyDisc, 1, "smooth", { spin: 1 }), /Unknown morph option: spin/);
assert.throws(() => copying.morph(copySquare, copyDisc, 1, "smooth", { lag: -1 }), /Morph lag must be a finite number/);
assert.throws(() => copying.morph(copySquare, copyDisc, 1, "smooth", [1]), /Morph options must be a number or a plain object/);
assert.throws(() => copying.morph([copySquare, copySquare], [copyLeft, copyRight], 1, "smooth", { copy: false }),
    /distinct across all pairs/);
assert.throws(() => copying.morph([copySquare, copyDisc], [copyLeft, copyLeft], 1, "smooth", { copy: true }),
    /distinct across all pairs/);
assert.throws(() => copying.morph([copySquare, copyLeft], [copyLeft, copyRight], 1, "smooth", { copy: true }),
    /distinct across all pairs/);
assert.throws(() => copying.matchTransform(copyFrom, copyTo, 1, "smooth", { copy: "yes" }), /MatchTransform copy must be a boolean/);
assert.equal(compileScene(copying), copyingBefore);

// A retained guide adds the same detour to each pair without changing its endpoints.
const matchGuideScene = vis.scene({ width: 320, height: 180, camera: { height: 6 } });
const matchGuide = matchGuideScene.route({ points: [[0, 0], [0, 2], [2, 2]], opacity: 0, id: "guide" });
const guidedFrom = matchGuideScene.group({ id: "from" });
guidedFrom.circle({ center: [-2, -1], radius: 0.2, id: "a" });
guidedFrom.circle({ center: [0, -1], radius: 0.2, id: "b" });
matchGuideScene.wait(1);
const guidedTo = matchGuideScene.group({ id: "to" });
guidedTo.circle({ center: [0, 1], radius: 0.2, id: "c" });
guidedTo.circle({ center: [2, 1], radius: 0.2, id: "d" });
matchGuideScene.matchTransform(guidedFrom, guidedTo, 1, "linear", { path: matchGuide, copy: true, shapes: true });
matchGuideScene.shift(matchGuide, [1, -1], 1);
const guidedLua = compileScene(matchGuideScene);
assert.match(guidedLua, /\["path"\]=objects\[1\]/);
const guidedBefore = compileScene(matchGuideScene);
assert.throws(() => matchGuideScene.matchTransform(guidedFrom, guidedTo, 1, "linear", { path: matchGuide, arc: 1 }),
    /path and arc cannot be used together/);
assert.throws(() => matchGuideScene.matchTransform(guidedFrom, guidedTo, 1, "linear", { path: copyFrom }),
    /handles from the same scene/);
assert.throws(() => matchGuideScene.matchTransform(guidedFrom, guidedTo, 1, "linear", { path: [] }),
    /handle/);
assert.equal(compileScene(matchGuideScene), guidedBefore);
const guidedTwin = `
local s = vis.scene {width=320,height=180,camera={height=6}}
local p = s:route {points={{0,0},{0,2},{2,2}},opacity=0,id="guide"}
local a=s:group {id="from"}
a:circle {center={-2,-1},radius=0.2,id="a"}
a:circle {center={0,-1},radius=0.2,id="b"}
s:wait(1)
local b=s:group {id="to"}
b:circle {center={0,1},radius=0.2,id="c"}
b:circle {center={2,1},radius=0.2,id="d"}
s:match_transform(a,b,1,"linear",{path=p,copy=true,shapes=true})
s:shift(p,{1,-1},1)
return s
`;

// A measured scaled billboard retains its painted bounds through a clone and match.
const scaledMatchScripts = [false, true].flatMap((copy) => [false, true].map((guided) => {
    const scene = vis.scene({ width: 960, height: 540, camera: { height: 540 } });
    const source = scene.text({ text: "copy", font: "Pretendard", size: 40, measure: true,
        point: [-200, 0], id: "source" });
    scene.wait(0.1).scale(source, 0.2, { duration: 0.2, curve: "linear" }).wait(0.2);
    const snapshot = scene.clone(source, { id: "snapshot" });
    const target = scene.text({ text: "copy", font: "Pretendard", size: 40, measure: true,
        point: [200, 0], id: "target" });
    const path = scene.route({ points: [[-200, 0], [0, 80], [200, 0]], opacity: 0 });
    scene.matchTransform(snapshot, target, 1, "linear", { copy, ...(guided ? { path } : {}) });
    return compileScene(scene);
}));

// Contour indices override automatic pairing across paths and plots, including copied sources
// and array lag. The Lua twin retains exactly the same geometry and clips.
const pieceMorph = vis.scene({ width: 320, height: 180, camera: { height: 6 } });
const pieceFrom = pieceMorph.plot({ points: [[-2, 0], [-1, 0], [NaN, 0], [1, 0], [2, 0]], id: "source" });
const pieceTo = pieceMorph.path({ commands: [
    { type: "move", to: [-2, 2] }, { type: "line", to: [-1, 2] },
    { type: "move", to: [1, 2] }, { type: "line", to: [2, 2] },
], id: "first" });
const pieceLast = pieceMorph.plot({ points: [[-2, -2], [-1, -2], [NaN, 0], [1, -2], [2, -2]], id: "second" });
pieceMorph.morph([pieceFrom, pieceFrom], [pieceTo, pieceLast], 1, "linear",
    { copy: true, lag: 0.25, pairs: [[1, 2]] });
const pieceMorphLua = compileScene(pieceMorph);
assert.match(pieceMorphLua, /\["pairs"\]=\{\{1,2\}\}/);
const pieceMorphBefore = compileScene(pieceMorph);
assert.throws(() => pieceMorph.morph(pieceFrom, pieceTo, 1, "linear", { pairs: [[0, 1]] }), /positive integer/);
assert.throws(() => pieceMorph.morph(pieceFrom, pieceTo, 1, "linear", { pairs: [[1.5, 1]] }), /positive integer/);
assert.throws(() => pieceMorph.morph(pieceFrom, pieceTo, 1, "linear", { pairs: [[1]] }), /positive integer/);
assert.throws(() => pieceMorph.morph(pieceFrom, pieceTo, 1, "linear", { pairs: [[1, 1], [1, 2]] }), /at most once/);
assert.throws(() => pieceMorph.morph(pieceFrom, pieceTo, 1, "linear", { pairs: [[1, 1], [2, 1]] }), /at most once/);
assert.throws(() => pieceMorph.morph(pieceFrom, pieceTo, 1, "linear", { pairs: [[1, ,]] }), /positive integer/);
assert.equal(compileScene(pieceMorph), pieceMorphBefore);
const pieceMorphScript = `
local s = vis.scene {width = 320, height = 180, camera = {height = 6}}
local a = s:plot {points={{-2,0},{-1,0},{0/0,0},{1,0},{2,0}},id="source"}
local b = s:path {commands={{type="move",to={-2,2}},{type="line",to={-1,2}},
    {type="move",to={1,2}},{type="line",to={2,2}}},id="first"}
local c = s:plot {points={{-2,-2},{-1,-2},{0/0,0},{1,-2},{2,-2}},id="second"}
s:morph({a,a},{b,c},1,"linear",{copy=true,lag=0.25,pairs={{1,2}}})
return s
`;

// A formula set from parts names the groups of its parts as the tex module does; part() finds
// parts, glyphs and shapes by their TeX.
const typeset = vis.scene({ width: 320, height: 180 });
const pythagoras = typeset.tex({ tex: "a^2+b^2=c^2", point: [0, 1.5], role: "h2", id: "eq" });
const terms = typeset.group({ id: "holder" }).tex({
    tex: ["a^2", "+", { text: "b^2", color: "#ff0000" }, "=", { text: "c^2", id: "hyp", fill: "#00ff00" }],
    id: "sum",
});
const anonymous = typeset.tex({ tex: ["x", "y"], point: [2, -1.5] });
assert.equal(pythagoras.children().length, 0);
assert.equal(terms.children().length, 5);
assert.equal(anonymous.children().length, 2);
assert.equal(typeset.find("eq"), pythagoras);
assert.equal(typeset.find("sum/1"), terms.children()[0]);
assert.equal(typeset.find("sum/3"), terms.children()[2]);
assert.equal(typeset.find("hyp"), terms.children()[4]);
assert.equal(typeset.find("sum/5"), null);
const hypotenuse = terms.part("c^2");
typeset.write(pythagoras, 1);
typeset.play({ target: pythagoras.part("c"), fill: "#0000ff" }, 0.5);
typeset.play({ target: hypotenuse.part("2"), fill: "#ffff00" }, 0.5);
typeset.indicate(typeset.find("sum/3"));
const swapped = typeset.tex({ tex: ["c^2", "=", "a^2", "+", "b^2"], id: "swapped", point: [0, -1.5] });
typeset.matchTransform(terms, swapped, 1, "smooth", { arc: 0.5 });
assert.throws(() => typeset.tex(null), /Tex options must be a plain object/);
assert.throws(() => typeset.tex({ role: "h2" }), /Tex requires tex: a string, or a non-empty array/);
assert.throws(() => typeset.tex({ tex: [] }), /Tex requires tex/);
assert.throws(() => typeset.tex({ tex: ["a", 3] }), /A tex part must be a string or a plain object with a text string/);
assert.throws(() => typeset.tex({ tex: [{ color: "#ff0000" }] }), /A tex part must be/);
assert.throws(() => pythagoras.part(2), /Tex part requires the TeX of what it finds as a string/);
assert.throws(() => pythagoras.part("c", 0), /Tex part takes a whole number from 1/);
const typesetLua = compileScene(typeset);
assert.match(typesetLua, /objects\[1\] = scene:tex \{\["id"\]="eq",\["point"\]=\{0,1\.5\},\["role"\]="h2",\["tex"\]="a\^2\+b\^2=c\^2"\}/);
assert.match(typesetLua, /objects\[3\] = objects\[2\]:tex \{\["id"\]="sum",\["tex"\]=\{"a\^2","\+",\{\["color"\]="#ff0000",\["text"\]="b\^2"\},"=",\{\["fill"\]="#00ff00",\["id"\]="hyp",\["text"\]="c\^2"\}\}\}\nobjects\[4\] = objects\[3\]:children\(\)/);
assert.match(typesetLua, /objects\[7\] = objects\[3\]:part\("c\^2",1\)/);
assert.match(typesetLua, /objects\[8\] = objects\[1\]:part\("c",1\)\nscene:play\(\{\{target=objects\[8\],fill="#0000ff"\}\},0\.5,"smooth",0\)/);
assert.match(typesetLua, /objects\[9\] = objects\[7\]:part\("2",1\)/);
assert.match(typesetLua, /scene:indicate\(objects\[4\]\[3\],/);
assert.match(typesetLua, /scene:match_transform\(objects\[3\],objects\[10\],1,"smooth",\{\["arc"\]=0\.5\}\)/);

// A brace names what it spans as the script does and returns the brace and the label at its
// tip: the text the script makes of a string, named after the brace, or the handle given.
const bracing = vis.scene({ width: 400, height: 300, camera: { view: "2d", height: 300 } });
const discriminant = bracing.text({ id: "term", text: "b^2 - 4ac", point: [0, 40], size: 32, measure: true });
const [underBrace, underLabel] = bracing.brace({ id: "under", around: discriminant, offset: 6, label: "discriminant" });
const braceNote = bracing.text({ id: "note", text: "x", point: [0, 0], measure: true });
const [sideBrace, sideLabel] = bracing.brace({ around: [discriminant], direction: "right", label: braceNote, gap: 4 });
const braceHolder = bracing.group({ id: "holder" });
const [spanBrace, noLabel] = braceHolder.brace({
    id: "span", from: [-100, -80], to: [100, -80], direction: [0, 1], depth: 10, fill: "#e76f51",
});
assert.equal(bracing.find("under"), underBrace);
assert.equal(bracing.find("under/label"), underLabel);
assert.equal(sideLabel, braceNote);
assert.equal(noLabel, undefined);
assert.deepEqual(braceHolder.children(), [spanBrace]);
assert.notEqual(sideBrace, underBrace);
bracing.growFromCenter(underBrace);
bracing.write(underLabel, 0.5);
bracing.play({ target: underLabel, text: "b^2 - 4ac < 0" }, 0.5);
assert.throws(() => bracing.brace("term"), /Brace options must be a plain object/);
assert.throws(() => bracing.brace({ around: discriminant, label: 3 }), /Brace label must be a string or a handle/);
assert.throws(() => bracing.brace({ around: [] }), /Around must be a handle or a non-empty array of handles/);
assert.throws(() => bracing.brace({ around: discriminant, label: typeset.find("eq") }), /handles from the same scene/);
const bracingLua = compileScene(bracing);
assert.match(
    bracingLua,
    /objects\[2\],objects\[3\] = scene:brace \{\["around"\]=objects\[1\],\["id"\]="under",\["label"\]="discriminant",\["offset"\]=6\}/,
);
assert.match(
    bracingLua,
    /objects\[5\] = scene:brace \{\["around"\]=\{objects\[1\]\},\["direction"\]="right",\["gap"\]=4,\["label"\]=objects\[4\]\}/,
);
assert.match(
    bracingLua,
    /objects\[7\] = objects\[6\]:brace \{\["depth"\]=10,\["direction"\]=\{0,1\},\["fill"\]="#e76f51",\["from"\]=\{-100,-80\},\["id"\]="span",\["to"\]=\{100,-80\}\}/,
);
assert.match(bracingLua, /scene:grow_from_center\(objects\[2\],/);
assert.match(bracingLua, /scene:write\(objects\[3\],0\.5,/);

// A tex part of spans is a formula the row holds as a TexHandle, named as the row names its
// texts, and the groups of a formula set from parts are named after it; leading and baseline
// reach the row, and baseline the texts and formulas that take it.
const inlined = vis.scene({ width: 320, height: 180 });
const said = inlined.spans(["The sides ", { tex: "a^2+b^2=c^2", id: "eq" }, ", so ",
    { tex: ["c", "=", { text: "\\sqrt{a^2+b^2}", id: "root" }], rise: 0.1 }, "."],
{ split: "words", wrap: 2, leading: 1.5, baseline: true, point: [-1.5, 0.5], id: "said" });
assert.equal(said.children().length, 7);
assert.equal(inlined.find("eq"), said.children()[2]);
assert.equal(typeof said.children()[2].part, "function");
assert.equal(said.children()[3].part, undefined);
assert.equal(inlined.find("said/6"), said.children()[5]);
assert.equal(inlined.find("said/6").children().length, 3);
assert.equal(inlined.find("said/6/1"), inlined.find("said/6").children()[0]);
assert.equal(inlined.find("root"), inlined.find("said/6").children()[2]);
assert.equal(inlined.find("said/7"), said.children()[6]);
inlined.play({ target: inlined.find("eq").part("c"), fill: "#ff0000" }, 0.5);
inlined.text({ text: "Area", point: [0, -1], baseline: true, id: "area" });
inlined.number({ value: 2, point: [1, -1], baseline: true });
inlined.tex({ tex: "x^2", point: [1, -1], baseline: true, align: [0, 0.5], id: "square" });
assert.throws(() => inlined.spans([{ tex: 3 }]), /A spans tex part requires tex: a string, or a non-empty array/);
assert.throws(() => inlined.spans([{ tex: [] }]), /A spans tex part requires tex/);
assert.throws(() => inlined.spans([{ tex: [{ color: "#ff0000" }] }]), /A tex part must be a string or a plain object/);
assert.throws(() => inlined.spans([{ tex: "x", text: "y" }]), /A spans part sets text or tex, not both/);
const inlinedLua = compileScene(inlined);
assert.match(inlinedLua, /objects\[1\] = scene:spans \{"The sides ",\{\["id"\]="eq",\["tex"\]="a\^2\+b\^2=c\^2"\},", so ",\{\["rise"\]=0\.1,\["tex"\]=\{"c","=",\{\["id"\]="root",\["text"\]="\\\\sqrt\{a\^2\+b\^2\}"\}\}\},"\.",\["baseline"\]=true,\["id"\]="said",\["leading"\]=1\.5,\["point"\]=\{-1\.5,0\.5\},\["split"\]="words",\["wrap"\]=2\}\nobjects\[2\] = objects\[1\]:children\(\)\nobjects\[3\] = objects\[2\]\[6\]:children\(\)\n/);
assert.match(inlinedLua, /objects\[4\] = objects\[2\]\[3\]:part\("c",1\)\nscene:play\(\{\{target=objects\[4\],fill="#ff0000"\}\},0\.5,"smooth",0\)/);
assert.match(inlinedLua, /scene:text \{\["baseline"\]=true,\["id"\]="area",\["point"\]=\{0,-1\},\["text"\]="Area"\}/);
assert.match(inlinedLua, /scene:number \{\["baseline"\]=true,\["point"\]=\{1,-1\},\["value"\]=2\}/);
assert.match(inlinedLua, /scene:tex \{\["align"\]=\{0,0\.5\},\["baseline"\]=true,\["id"\]="square",\["point"\]=\{1,-1\},\["tex"\]="x\^2"\}/);

const directory = mkdtempSync(join(tmpdir(), "vis-js-binding-"));
try {
    // The CLI on scripts written to the directory: a run must exit with the expected status, an
    // inspect or a layout must succeed, and a refused script must fail.
    const cli = (args, expected = 0, options = {}) => run(cliPath, args, expected, options);
    const writeScript = (name, source) => {
        const path = join(directory, name);
        writeFileSync(path, source);
        return path;
    };
    const inspectScript = (name, source, fonts = []) => {
        const path = writeScript(name, source);
        return { path, scene: JSON.parse(cli(["inspect", path, ...fonts]).stdout) };
    };
    const layoutAt = (path, time, fonts = []) =>
        JSON.parse(cli(["layout", path, "--time", String(time), ...fonts]).stdout);
    // A report's numbers keep a float's 9 digits, so what the floats only approximate is compared
    // at 6.
    const rounded = (value) => {
        if (typeof value === "number") return Number(value.toPrecision(6));
        if (Array.isArray(value)) return value.map(rounded);
        if (value && typeof value === "object") {
            return Object.fromEntries(Object.entries(value).map(([key, entry]) => [key, rounded(entry)]));
        }
        return value;
    };
    const refusal = (name, source) => {
        const result = spawnSync(cliPath, ["inspect", writeScript(name, source)], { encoding: "utf8" });
        assert.notEqual(result.status, 0);
        return result.stderr;
    };
    // A twin that writes texts renders with the fonts it names as further arguments.
    const renderFrame = (path, time, fonts = []) => {
        const png = join(directory, "frame.png");
        cli(["render", path, "--time", String(time), ...fonts, "-o", png]);
        return readFileSync(png);
    };

    assert.equal(inspectScript("motion.lua", motionLua).scene.duration, 5.5);

    // A Lua function holds at most 200 locals, so a scene with more handles and marks than that
    // still loads, and a viewport child keeps its handles apart from its parent's.
    const crowded = vis.scene();
    const crowd = crowded.group({ id: "crowd" });
    for (let i = 0; i < 300; i++) crowd.circle({ center: [(i % 20) * 0.3 - 3, Math.floor(i / 20) * 0.3 - 2], radius: 0.1 });
    for (let i = 0; i < 250; i++) crowded.at(i * 0.01, () => {});
    const crowdedInset = vis.scene();
    for (let i = 0; i < 250; i++) crowdedInset.circle({ center: [(i % 25) * 0.2 - 2.5, Math.floor(i / 25) * 0.2 - 1], radius: 0.05 });
    crowded.viewport(crowdedInset, { x: 0.5, y: 0, width: 0.5, height: 0.5 });
    crowded.play({ target: crowd, opacity: 0.5 }, 1);
    const crowdedLua = compileScene(crowded);
    assert.match(crowdedLua, /^local scene = vis\.scene \{\}\nlocal objects = \{\}\nlocal times = \{\}\n/);
    assert.match(crowdedLua, /\ndo\n    local viewportScene1 = vis\.scene \{\}\n    local objects = \{\}\n/);
    assert.match(crowdedLua, /\nend\nscene:play\(\{\{target=objects\[1\],opacity=0\.5\}\},1,"smooth",0\)\n/);
    const crowdedScene = inspectScript("crowded.lua", crowdedLua).scene;
    assert.equal(crowdedScene.objects.length, 301);
    assert.equal(crowdedScene.viewports[0].scene.objects.length, 250);

    assert.equal(inspectScript("turntable.lua", turntableLua).scene.duration, 5);
    assert.equal(inspectScript("framing.lua", framingLua).scene.duration, 2.75);
    assert.match(refusal("looking-around.lua", lookingAroundLua), /focus requires a 2D camera; use look in 3D/);
    assert.equal(inspectScript("aiming.lua", aimingLua).scene.duration, 4);

    assert.equal(inspectScript("surface.lua", surfaceLua).scene.duration, 1);

    assert.equal(inspectScript("shapes.lua", compileScene(shapes)).scene.duration, 1);

    // A measured text measures in its font while the script runs, or in the default font without it.
    const measuredPath = writeScript("measured.lua", measuredLua);
    const screenLabelPath = writeScript("screen-labels.lua", screenLabelLua);
    for (const time of [0, 1, 0.5, 1]) {
        const report = layoutAt(screenLabelPath, time, ["--font", font]);
        const box = report.scene.objects.find((object) => object.id === "screen-axes/label/x/1")?.bounds;
        assert.ok(box);
        const anchor = 160 - 360 / (6 - 2 * time);
        assert.ok(Math.abs(box.x + box.width - (anchor - 14)) < 0.02, "Screen label lost its pixel gap during zoom");
    }
    for (const time of [2, 1.75, 1, 2]) {
        const box = layoutAt(screenLabelPath, time, ["--font", font]).scene.objects
            .find((object) => object.id === "screen-axes/label/x/1")?.bounds;
        assert.ok(box);
        const anchor = 160 + 45 * (-2 + 3 * (time - 1));
        assert.ok(Math.abs(box.x + box.width - (anchor - 14)) < 0.02, "Screen label used the unwarped Space origin");
    }
    cli(["inspect", measuredPath, ...fontOptions]);
    cli(["inspect", measuredPath]);

    // Lua seeded alike draws the same numbers, the draws a range throws away included; the script
    // hands them back in its error, which the CLI reports whole up to 2 KiB.
    const randomPath = writeScript("random.lua", [
        "math.randomseed(2024, 5)",
        "local values = {}",
        "for i = 1, 40 do",
        "    values[#values + 1] = string.format('%.17g', math.random())",
        "    values[#values + 1] = tostring(math.random(1, 6))",
        "    values[#values + 1] = tostring(math.random(1000))",
        "    values[#values + 1] = tostring(math.random(-1000000007, 1000000007))",
        "end",
        "error(table.concat(values, ' '), 0)",
    ].join("\n"));
    const randomResult = spawnSync(cliPath, ["inspect", randomPath], { encoding: "utf8" });
    const luaDraws = randomResult.stderr.match(/viscript: ([-0-9. e]+)/)?.[1].trim().split(" ").map(Number);
    const jsRandom = vis.random(2024, 5);
    const jsDraws = [];
    for (let i = 0; i < 40; i++) {
        jsDraws.push(jsRandom(), jsRandom(1, 6), jsRandom(1000), jsRandom(-1000000007, 1000000007));
    }
    assert.deepEqual(luaDraws, jsDraws, randomResult.stderr);

    assert.equal(inspectScript("counting.lua", countingLua).scene.duration, 2);

    assert.equal(inspectScript("retitled.lua", retitledLua).scene.duration, 1);

    // The back rectangle keeps its higher layer until the play ends, and the front circle
    // switches at the cursor.
    const layeredPath = writeScript("layered.lua", layeredLua);
    const layersAt = (time) =>
        Object.fromEntries(layoutAt(layeredPath, time).report.objects.map((object) => [object.id, object.layer]));
    assert.deepEqual(layersAt(0.5), { back: 2, front: 1 });
    assert.deepEqual(layersAt(1), { back: 0, front: -1 });

    // The card comes back over a second, the dot's layer switches back at once, and the last
    // restore waits half a second; 20 pixels per unit with the origin at pixel (80, 40).
    const saving = inspectScript("saving.lua", savingLua);
    const savedAt = (time) => Object.fromEntries(layoutAt(saving.path, time).report.objects.map((object) => {
        const bounds = object.paintBounds;
        return [object.id, [Math.round(bounds.x + bounds.width / 2), Math.round(bounds.y + bounds.height / 2),
            object.layer]];
    }));
    assert.deepEqual(savedAt(1), { card: [60, 20, 0], dot: [120, 40, 3] });
    assert.deepEqual(savedAt(1.5), { card: [50, 30, 0], dot: [120, 40, 3] });
    assert.deepEqual(savedAt(2), { card: [40, 40, 0], dot: [120, 40, 0] });
    assert.equal(saving.scene.duration, 2.5);
    assert.match(refusal("unsaved.lua", unsavedLua), /could not restore: 'lonely' has no saved state; save it before restoring/);

    const spannedObjects = inspectScript("spanned.lua", spannedLua, fontOptions).scene.objects;
    const sentenceHandle = spannedObjects.find((object) => object.id === "sentence").handle;
    assert.equal(spannedObjects.filter((object) => object.parent === sentenceHandle).length, 4);
    assert.equal(spannedObjects.find((object) => object.id === "brown").parent, sentenceHandle);
    assert.equal(spannedObjects.find((object) => object.id === "sentence/4").parent, sentenceHandle);
    assert.equal(spannedObjects.filter((object) => object.type === "text").length, 12);

    assert.equal(inspectScript("matching.lua", matchingLua, fontOptions).scene.duration, 3.6);

    // Every handle the builder names for a brace is the object the script returns.
    const bracing = inspectScript("bracing.lua", bracingLua, fontOptions).scene;
    const bracingObjects = bracing.objects;
    const bracingId = (id) => bracingObjects.find((object) => object.id === id);
    assert.equal(bracingId("under")?.type, "path");
    assert.equal(bracingId("under/label")?.type, "text");
    assert.equal(bracingId("span")?.parent, bracingId("holder").handle);
    assert.equal(bracingObjects.filter((object) => object.type === "path").length, 3);
    assert.equal(bracing.duration, 2);

    // Every token handle the builder names exists once the scene runs.
    const coded = inspectScript("coded.lua", codedLua, fontOptions);
    // Without its font, code is set in the default font, and the CLI says so.
    const missingCodeFont = cli(["inspect", coded.path, "--font", font]);
    assert.match(missingCodeFont.stderr, /the font 'JetBrainsMono' is not loaded, so its text is drawn in the default font/);
    assert.doesNotMatch(missingCodeFont.stderr, /'Pretendard'/);
    const codedObjects = coded.scene.objects;
    assert.equal(codedObjects.filter((object) => object.type === "text").length, 16);
    const lastLine = codedObjects.find((object) => object.id === "listing/4").handle;
    assert.equal(codedObjects.find((object) => object.id === "listing/4/7").parent, lastLine);
    assert.ok(codedObjects.find((object) => object.id === "band" && object.type === "rectangle"));
    assert.equal(coded.scene.duration, 4.6);

    // The script makes every token the builder named for a search, and every edited block, with
    // each token in its line; a block below a group edits into that group.
    const sums = inspectScript("sums.lua", sumsLua, fontOptions).scene;
    const sumsObjects = sums.objects;
    const sumsId = (id) => sumsObjects.find((object) => object.id === id);
    assert.equal(sumsId("sum/5/3").parent, sumsId("sum/5").handle);
    assert.equal(sumsId("word/2/3").parent, sumsId("word/2").handle);
    for (const id of ["sum.2", "sum.3", "final", "word.2"]) assert.equal(sumsId(id)?.type, "group");
    assert.equal(sumsId("final/6/2").parent, sumsId("final/6").handle);
    assert.equal(sumsId("sum.3/7/2").parent, sumsId("sum.3/7").handle);
    assert.equal(sumsId("final/7"), undefined);
    assert.equal(sumsId("word.2").parent, sumsId("word").parent);
    assert.equal(sumsId("word.3/3").parent, sumsId("word.3").handle);
    assert.equal(sumsId("word.3/4"), undefined);
    assert.equal(sumsId("box")?.type, "rectangle");
    assert.equal(sums.duration, 6.5);

    const shadedObjects = inspectScript("shaded.lua", compileScene(shaded)).scene.objects;
    assert.ok(shadedObjects.find((object) => object.id === "between" && object.type === "polygon"));
    const sumHandle = shadedObjects.find((object) => object.id === "sum").handle;
    assert.equal(shadedObjects.filter((object) => object.parent === sumHandle && object.type === "rectangle").length, 4);
    assert.ok(shadedObjects.find((object) => object.id === "sum/4"));
    const fieldHandle = shadedObjects.find((object) => object.id === "field").handle;
    assert.equal(shadedObjects.filter((object) => object.parent === fieldHandle && object.type === "vector").length, 8);
    assert.ok(shadedObjects.find((object) => object.id === "field/3/2"));

    assert.equal(inspectScript("shelf.lua", shelfLua).scene.duration, 2);

    const grid = inspectScript("grid.lua", gridLua, fontOptions).scene;
    const gridObjects = grid.objects;
    const secondRow = gridObjects.find((object) => object.id === "m/2").handle;
    assert.equal(gridObjects.find((object) => object.id === "m/2/3").parent, secondRow);
    assert.ok(gridObjects.find((object) => object.id === "m/left" && object.type === "path"));
    assert.ok(gridObjects.find((object) => object.id === "band" && object.type === "rectangle"));
    assert.equal(gridObjects.filter((object) => object.type === "text").length, 8);
    assert.equal(grid.duration, 1);

    const ledger = inspectScript("ledger.lua", ledgerLua, fontOptions).scene;
    const ledgerObjects = ledger.objects;
    const sheetHandle = ledgerObjects.find((object) => object.id === "t").handle;
    const sheetChildren = ledgerObjects.filter((object) => object.parent === sheetHandle).map((object) => object.type);
    assert.deepEqual(sheetChildren, [...Array(4).fill("group"), "rectangle", ...Array(8).fill("line")]);
    assert.equal(ledgerObjects.find((object) => object.id === "t/4/2").parent, ledgerObjects.find((object) => object.id === "t/4").handle);
    assert.equal(ledgerObjects.filter((object) => object.type === "text").length, 8 + 4 + 2);
    assert.equal(ledger.duration, 1);

    // Every part handle the builder names exists once the tex module sets the formulas; a build
    // without the module names the build option instead.
    const typesetResult = spawnSync(cliPath, ["inspect", writeScript("typeset.lua", typesetLua), ...fontOptions],
        { encoding: "utf8" });
    if (typesetResult.status === 0) {
        const typesetObjects = JSON.parse(typesetResult.stdout).objects;
        const handleOf = (id) => typesetObjects.find((object) => object.id === id)?.handle;
        assert.equal(typesetObjects.find((object) => object.id === "sum").parent, handleOf("holder"));
        const partTypes = typesetObjects.filter((object) => object.parent === handleOf("sum")).map((object) => object.type);
        assert.deepEqual(partTypes, Array(5).fill("group"));
        assert.equal(typesetObjects.find((object) => object.id === "hyp").parent, handleOf("sum"));
        assert.equal(typesetObjects.find((object) => object.id === "hyp/2").parent, handleOf("hyp"));
        assert.equal(typesetObjects.find((object) => object.id === "sum/3/2").parent, handleOf("sum/3"));
        assert.equal(typesetObjects.filter((object) => object.parent === handleOf("eq")).length, 8);
        assert.equal(JSON.parse(typesetResult.stdout).duration, 4);
    } else {
        assert.match(typesetResult.stderr, /tex needs the tex module: build viscript with -Dmodules=tex/);
    }

    // Every child the builder names in a row with formulas exists below it once the scene runs.
    const inlinedResult = spawnSync(cliPath, ["inspect", writeScript("inlined.lua", inlinedLua), ...fontOptions],
        { encoding: "utf8" });
    if (inlinedResult.status === 0) {
        const inlinedObjects = JSON.parse(inlinedResult.stdout).objects;
        const handleOf = (id) => inlinedObjects.find((object) => object.id === id)?.handle;
        const rowTypes = inlinedObjects.filter((object) => object.parent === handleOf("said")).map((object) => object.type);
        assert.deepEqual(rowTypes, ["text", "text", "group", "text", "text", "group", "text"]);
        assert.equal(inlinedObjects.find((object) => object.id === "eq").parent, handleOf("said"));
        assert.equal(inlinedObjects.find((object) => object.id === "said/6/1").parent, handleOf("said/6"));
        assert.equal(inlinedObjects.find((object) => object.id === "root").parent, handleOf("said/6"));
        assert.equal(inlinedObjects.find((object) => object.id === "square").type, "group");
        assert.equal(JSON.parse(inlinedResult.stdout).duration, 0.5);
    } else {
        assert.match(inlinedResult.stderr, /a tex part needs the tex module: build viscript with -Dmodules=tex/);
    }

    const vennObjects = inspectScript("venn.lua", vennLua).scene.objects;
    for (const id of ["lens", "both", "crescent", "apart", "ring"]) {
        assert.ok(vennObjects.find((object) => object.id === id && object.type === "path"), id);
    }

    const inspectedTracks = inspectScript("tracks.lua", tracksLua).scene;
    assert.equal(inspectedTracks.duration, 5);
    assert.deepEqual(inspectedTracks.markers, [{ name: "badge", time: 1 }]);

    assert.equal(inspectScript("lensed.lua", lensedLua).scene.duration, 2);

    // The copies take the ids the builder gave them, and the runtime derives the others.
    const cloningObjects = inspectScript("cloning.lua", cloningLua, fontOptions).scene.objects;
    const twinHandle = cloningObjects.find((object) => object.id === "twin").handle;
    for (const id of ["twin/a", "twin/b", "pair/a:copy"]) {
        assert.equal(cloningObjects.find((object) => object.id === id).parent, twinHandle, id);
    }
    for (const id of ["word:copy", "half"]) {
        assert.ok(cloningObjects.find((object) => object.id === id && object.type === "text"), id);
    }

    // Each transform from a copy adds the copy beside its source, which stays.
    const inspectedCopying = inspectScript("copying.lua", copyingLua).scene;
    assert.equal(inspectedCopying.duration, 4.5);
    assert.equal(inspectedCopying.objects.filter((object) => object.type === "rectangle").length, 4);
    assert.equal(inspectedCopying.objects.filter((object) => object.type === "group").length, 3);

    // A JS scene with trackers renders as its Lua twin does, and its functions see the values
    // the twin's functions see, to the last bit.
    for (const [index, script] of scaledMatchScripts.entries()) {
        const built = inspectScript(`scaled-match-${index}.lua`, script, ["--font", font]);
        const boundsAt = (time, id) =>
            layoutAt(built.path, time, ["--font", font]).scene.objects.find((object) => object.id === id).bounds;
        const before = boundsAt(0.499, "source");
        for (const time of [1.5, 0.5, 0.499, 0.5]) {
            const actual = boundsAt(time, time === 0.5 ? "target" : "source");
            for (const key of ["x", "y", "width", "height"]) {
                assert.ok(Math.abs(actual[key] - before[key]) < 0.01,
                    `Scaled billboard ${index} at ${time}: ${key} changed from ${before[key]} to ${actual[key]}`);
            }
        }
    }

    const guidedBuilt = inspectScript("match-guide-built.lua", guidedLua);
    const guidedAuthored = inspectScript("match-guide-authored.lua", guidedTwin);
    for (const time of [2, 1.5, 1, 1.25]) {
        assert.deepEqual(renderFrame(guidedBuilt.path, time), renderFrame(guidedAuthored.path, time));
    }
    const guidedObjects = layoutAt(guidedBuilt.path, 1.5).scene.objects;
    const guidedC = guidedObjects.find((object) => object.id === "c").bounds;
    assert.ok(Math.abs(guidedC.x + guidedC.width / 2 - 100) < 0.01);
    assert.ok(Math.abs(guidedC.y + guidedC.height / 2 - 60) < 0.01);
    for (const [fps, scene] of timedTracks) {
        const script = compileScene(scene).replace("return scene",
            'scene:point {point={0,0},id=string.format("duration:%.17g",scene:duration())}\nreturn scene');
        const inspected = inspectScript(`tracker-timing-${fps}.lua`, script).scene;
        assert.ok(inspected.objects.some((object) => object.id === "duration:8"));
        assert.deepEqual(inspected.markers, [{ name: "end", time: 8 }]);
    }
    const flatPng = inspectScript("flat-png.lua", compileScene(vis.scene({
        width: 960, height: 540, background: "#112233",
    })));
    const compressed = renderFrame(flatPng.path, 0);
    assert.ok(compressed.length < 10000, `Flat PNG occupies ${compressed.length} bytes`);
    assert.deepEqual(compressed, renderFrame(flatPng.path, 0));
    const chunks = [];
    for (let offset = 8; offset < compressed.length;) {
        const length = compressed.readUInt32BE(offset);
        if (compressed.toString("ascii", offset + 4, offset + 8) === "IDAT") {
            chunks.push(compressed.subarray(offset + 8, offset + 8 + length));
        }
        offset += length + 12;
    }
    const filtered = inflateSync(Buffer.concat(chunks));
    assert.equal(filtered.length, (960 * 4 + 1) * 540);
    const pixels = Buffer.alloc(960 * 540 * 4);
    for (let y = 0; y < 540; y += 1) {
        const row = y * (960 * 4 + 1);
        const filter = filtered[row];
        assert.ok(filter <= 4);
        for (let x = 0; x < 960 * 4; x += 1) {
            const index = y * 960 * 4 + x;
            const left = x >= 4 ? pixels[index - 4] : 0;
            const up = y ? pixels[index - 960 * 4] : 0;
            const corner = y && x >= 4 ? pixels[index - 960 * 4 - 4] : 0;
            const prediction = left + up - corner;
            const distances = [Math.abs(prediction - left), Math.abs(prediction - up), Math.abs(prediction - corner)];
            const paeth = distances[0] <= distances[1] && distances[0] <= distances[2] ? left
                : distances[1] <= distances[2] ? up : corner;
            const predictor = [0, left, up, Math.floor((left + up) / 2), paeth][filter];
            pixels[index] = filtered[row + 1 + x] + predictor;
            assert.equal(pixels[index], [0x11, 0x22, 0x33, 0xff][x % 4]);
        }
    }

    for (const [name, built, script, times, fonts] of [
        ["physical-spring", physicalSpringScene, physicalSpringScript, [0, 0.5, 16 / 30, 1, 8, 1]],
        ["long-track", longTrack, longTrackScript, [597 / 30, 598 / 30, 599 / 30, 597 / 30], ["--font", font]],
        ["minute-track", minuteTrack, minuteTrackScript, [3598 / 60], ["--font", font]],
        ["piece-morph", pieceMorph, pieceMorphScript, [0, 0.25, 0.5, 0.75, 1, 1.25, 0.5]],
        ["pinned-code", pinnedCode, pinnedCodeScript, [0, 0.5, 1, 0.5], fontOptions],
        ["angles", angleTwin, angleTwinScript, [0, 0.25, 0.5, 1, 1.25, 1.5, 0.5], fontOptions],
        ["connected", connected, connectedScript, [0.25, 0.5, 1, 1.25, 1.5, 1.75, 2, 1.25], fontOptions],
        ["trimmed", trimmed, trimmedScript, [0, 0.25, 0.5, 0.75, 1, 1.5, 2]],
        ["twin", twin, twinScript, [0, 0.3, 1, 1.4, 1.9, 2.4, 2.8, 3.1, 3.5, 3.9, 4.4, 4.5, 4.65, 5.1, 5.65, 5.9, 6.15]],
        ["shaped", shaped, shapedScript, [0, 0.25, 0.5, 0.75, 1, 1.25, 1.5]],
        ["ride", ride, rideScript, [0, 0.5, 0.75, 1, 1.25, 1.5, 2]],
        ["center", centering, centerScript, [0, 1, 1.5, 2, 2.5, 3, 3.5, 4]],
        ["home", homing, homeScript, [0, 0.5, 1, 1.25, 1.5, 1.75, 2, 2.25, 2.5]],
        ["lens", magnified, lensScript, [0, 0.25, 0.5, 1, 1.5, 1.75, 2, 2.5, 3, 3.25, 3.5, 3.75]],
        ["merge", merging, mergeScript, [0, 0.5, 1, 1.5, 1.55, 1.62, 2, 2.55, 2.83, 2.9, 3.5, 4, 4.33, 4.5, 5, 5.5]],
        ["pending", pending, pendingScript, [0, 1, 1.5, 2, 2.5, 3, 4, 4.5, 5, 6, 6.25, 6.5, 7.5, 8, 9.5, 10, 11, 12, 12.5]],
        ["plots", plotTwin, plotTwinScript, [0, 0.2, 0.45, 0.7, 1]],
        ["plot-endpoint", endpointPlot, endpointScript, [0, 0.999, 1, 1.5, 1.999, 2, 3, 4, 5, 0]],
        ["endpoint-progress", endpointProgress, endpointProgressScript, [0, 1, 1.25, 1.5, 2, 2.5, 3, 3.5, 4, 4.5, 5, 0]],
        ["contours", contourTwin, contourTwinScript, [0, 0.1, 0.2, 0.3, 0.4, 0.5, 0.6, 0.7, 0.8]],
        ["labelled-contours", labelledContours, labelledContoursScript, [0, 0.25, 0.5, 0.75, 1], ["--font", font]],
        ["bound-contours", changingContours, changingContoursScript, [0, 0.25, 0.5, 0.75, 1, 1.25, 1.5, 1.75, 2]],
        ["replot", replot, replotScript, [0, 0.25, 0.45, 0.55, 0.75, 1, 1.25, 1.75, 2.25, 2.75, 3.25, 3.75, 4.25, 4.75, 5]],
        ["function-play", functionPlay, functionPlayScript, [0, 0.25, 0.5, 0.75, 1, 1.25, 1.5, 1.75, 2, 2.25, 2.5, 2.75, 3]],
        ["animated-poles", animatedPoles, animatedPolesScript, Array.from({ length: 21 }, (_, i) => i / 20)],
        ["labeled", labeled, labeledScript, [0, 0.5, 1, 1.5, 2, 3, 4], fontOptions],
    ]) {
        const fromJs = inspectScript(`${name}-js.lua`, compileScene(built), fonts);
        const fromLua = inspectScript(`${name}-lua.lua`, script, fonts);
        assert.equal(fromJs.scene.duration, fromLua.scene.duration, name);
        if (name === "physical-spring") {
            assert.equal(fromJs.scene.duration, 8);
            for (const time of times) {
                for (const path of [fromJs.path, fromLua.path]) {
                    const mass = layoutAt(path, time).scene.objects.find((object) => object.id === "mass");
                    const displacement = mass.bounds.x + mass.bounds.width / 2 - 480;
                    assert.ok(Math.abs(displacement - springReference(Math.max(0, time - 0.5))) < 0.002);
                }
            }
        }
        if (name === "labelled-contours") {
            const label = fromJs.scene.objects.find((object) => object.id === "labels/1/label/1");
            assert.ok(label && label.type === "text", "contour labels must be real native Text children");
            const layout = layoutAt(fromJs.path, 0.5, fonts);
            const text = layout.scene.objects.find((object) => object.id === "labels/1/label/1");
            assert.equal(text.text, "0.50");
            const centerX = text.bounds.x + text.bounds.width / 2;
            const centerY = text.bounds.y + text.bounds.height / 2;
            assert.ok(Math.abs(centerX-centerY-85) / Math.sqrt(2) < 0.6, "label center must lie on its sampled curve");
            const curveIndex = layout.report.objects.findIndex((object) => object.id === "labels/1");
            const strokes = layout.report.paths.filter((path) => layout.report.visuals[path.visual].object === curveIndex);
            assert.equal(strokes.length, 2, "the stroke must leave a gap under its label");
        }
        for (const time of times) {
            if (name === "animated-poles") {
                const report = layoutAt(fromJs.path, time).report;
                const object = report.objects.findIndex((entry) => entry.id === "graph");
                const paths = report.paths.filter((path) => report.visuals[path.visual].object === object);
                assert.equal(paths.length, 3, `pole branch count at ${time}`);
                for (const path of paths) {
                    for (let i = 1; i < path.points.length; i++) {
                        assert.ok(Math.abs(path.points[i].y - path.points[i - 1].y) < 150,
                            `pole connector at ${time}`);
                    }
                }
            }
            assert.ok(renderFrame(fromJs.path, time, fonts).equals(renderFrame(fromLua.path, time, fonts)),
                `${name} at ${time}`);
        }
    }
    // The scene takes the points of the plots in the counts the builder left to it.
    const endpointPath = inspectScript("endpoint.lua", compileScene(endpointPlot)).path;
    for (const [time, x, y] of [[0, 145, 120], [1.5, 137.5, 120], [2, 130, 120], [3, 130, 90], [4, null, null], [0, 145, 120]]) {
        const graph = layoutAt(endpointPath, time).report.objects.find((object) => object.id === "graph");
        assert.equal(graph.visible, x !== null);
        if (x !== null) {
            assert.ok(Math.abs(graph.paintBounds.x + graph.paintBounds.width / 2 - x) < 0.1);
            assert.ok(Math.abs(graph.paintBounds.y + graph.paintBounds.height / 2 - y) < 0.1);
        }
    }
    const progressPath = inspectScript("endpoint-progress.lua", compileScene(endpointProgress)).path;
    for (const time of [0, 1, 1.25, 1.5, 2, 2.5, 3, 3.5, 4, 4.5, 5, 0]) {
        const objects = layoutAt(progressPath, time).report.objects;
        for (const id of ["down", "up", "stay", "grouped", "spaced", "reset", "future"]) {
            let visible = true;
            if (id === "grouped") visible = (time > 1 && time < 3) || time > 4;
            if (id === "reset") visible = !(time >= 1 && time < 1.5) && time !== 3.5;
            if (id === "future") visible = time > 3;
            const graph = objects.find((object) => object.id === id);
            assert.equal(graph.visible, visible, `${id} at ${time}`);
            if (visible) {
                assert.ok(Math.abs(graph.paintBounds.width - 8) < 0.01, `${id} width at ${time}`);
                assert.ok(Math.abs(graph.paintBounds.height - 8) < 0.01, `${id} height at ${time}`);
            }
        }
    }
    assert.equal(inspectScript("recut.lua", recutLua).scene.duration, 2);
    // The warp of the pending twin lifts the beam the last tracks lay back flat at y = 0.5 to
    // y = 1.5, 15 pixels to a unit above the center of the frame.
    const liftedBeam = layoutAt(writeScript("pending.lua", pendingLua), 13.5).scene.objects
        .find((object) => object.id === "beam").bounds;
    assert.ok(Math.abs(liftedBeam.y + liftedBeam.height / 2 - 22.5) < 0.5, JSON.stringify(liftedBeam));
    // The texts made from strings lie below the space and the rectangle keeps its parent; placed
    // twice, it ends right of the y axis at -1, its left side 5 + 4 pixels from the axis.
    const graphed = inspectScript("graph.lua", compileScene(graphing));
    assert.deepEqual(graphed.scene.objects.map((object) => [object.type, object.parent]),
        [["rectangle", null], ["space", null], ["text", 2], ["text", 2]]);
    const namedLabels = inspectScript("label-ids.lua", compileScene(labelIds));
    assert.deepEqual(namedLabels.scene.objects.filter((object) => object.type === "text").map((object) => object.id),
        ["axes/label/x/1", "axes/label/x/2", "axes/label/y/1", "axes/label/z/1",
            "axes/label/x/3", "axes/label/y/2", "axes/label/x/4", null, null]);
    const tagged = JSON.parse(cli(["layout", graphed.path, ...fontOptions]).stdout).scene.objects
        .find((object) => object.id === "tag");
    assert.deepEqual(tagged.bounds, { x: 168.375, y: 115.625, width: 16.25, height: 8.75 });
    // A code block lays its tokens out as one block, and the report says so; a plain Group does not.
    const blockPath = writeScript("block.lua", `local scene = vis.scene {width = 320, height = 120}
scene:group {id = "plain"}
scene:code {text = "a = b", language = "plain", id = "snippet"}
return scene
`);
    const blockObjects = JSON.parse(cli(["layout", blockPath, ...fontOptions]).stdout).report.objects;
    const typesetOf = (id) => blockObjects.find((object) => object.id === id)?.typeset;
    assert.equal(typesetOf("snippet"), true);
    assert.equal(typesetOf("plain"), false);
    // The lens shows from its first fade at 1.5 times the frame's scale, twice it halfway through
    // the look, and again 1.5 times after focus() returns it to its opening camera. The overview
    // shows from 0.25 s and keeps pixel sizes as it is not optical. A view that is not yet made
    // or fully faded is left out of the report and draws nothing.
    const inspectedLens = inspectScript("lens.lua", magnifiedLua);
    assert.equal(inspectedLens.scene.duration, 3.75);
    const viewScenes = (time) => layoutAt(inspectedLens.path, time).report.scenes
        .filter((entry) => entry.kind === "view")
        .map((entry) => [entry.path, entry.bounds, entry.scale]);
    const lensBounds = { x: 80, y: 0, width: 80, height: 45 };
    const overviewBounds = { x: 0, y: 45, width: 40, height: 45 };
    assert.deepEqual(viewScenes(0), []);
    assert.deepEqual(viewScenes(0.1), [["root/view:0", lensBounds, { x: 1.5, y: 1.5 }]]);
    assert.deepEqual(viewScenes(1), [
        ["root/view:0", lensBounds, { x: 2, y: 2 }],
        ["root/view:1", overviewBounds, { x: 1, y: 1 }],
    ]);
    assert.deepEqual(viewScenes(3.75), [
        ["root/view:0", lensBounds, { x: 1.5, y: 1.5 }],
        ["root/view:1", overviewBounds, { x: 1, y: 1 }],
    ]);
    assert.ok(!renderFrame(inspectedLens.path, 0).equals(renderFrame(inspectedLens.path, 1)));
    assert.match(refusal("hurried.lua", hurriedLua), /could not fade view: the view already fades until 1\.00 s/);
    assert.equal(inspectScript("turning.lua", turningLua).scene.duration, 2);

    const flattened = inspectScript("flattening.lua", flatteningLua).scene;
    assert.equal(flattened.duration, 2);
    assert.deepEqual(flattened.camera, { mode: "fixed", view: "2d", projection: "orthographic" });
    const listened = inspectScript("listener.lua", listenerScript).scene.objects
        .filter((object) => object.type === "point")
        .map((object) => object.id.split(":").map(Number))
        .sort((first, second) => first[0] - second[0])
        .map(([, value]) => value);
    assert.equal(heard.length, 1 + 5 * 17 + 60);
    assert.deepEqual(listened, heard);
    const merged = inspectScript("merge-heard.lua", mergeHeardScript).scene.objects
        .filter((object) => object.type === "point")
        .map((object) => object.id.split(":").map(Number))
        .sort((first, second) => first[0] - second[0])
        .map(([, value]) => value);
    assert.deepEqual(merged, mergeHeard);
    // The scene merges what the builder merged, close to the Lua twin, whose clock knows the
    // time the fade takes; it differs only in the rounding of the times the builder counts from
    // the parallel.
    const laterFromJs = inspectScript("later-js.lua", laterLua);
    const laterFromLua = inspectScript("later-lua.lua", laterScript);
    assert.equal(laterFromJs.scene.duration, laterFromLua.scene.duration);
    for (const time of [1.2, 1.6, 1.62, 2, 2.5, 3]) {
        const boundsAt = (path) => layoutAt(path, time).scene.objects.find((object) => object.id === "dot").bounds;
        const [fromJs, fromLua] = [boundsAt(laterFromJs.path), boundsAt(laterFromLua.path)];
        for (const key of ["x", "y", "width", "height"]) {
            assert.ok(Math.abs(fromJs[key] - fromLua[key]) < 0.01, `later at ${time}: ${key}`);
        }
    }
    assert.match(refusal("apart.lua", apartLua), /'dot' already animates its size until 3\.00 s/);

    // Halfway through, the shift has carried the bar to the middle and the quarter turn, which
    // travels straight without an arc, has shrunk it to a square 1.25 units wide. Halfway through
    // the morph, the shift still to come holds the blob one unit short of the morph at rest.
    const layersPath = writeScript("layers.lua", layersLua);
    const layered = layoutAt(layersPath, 1).scene.objects.find((object) => object.id === "bar").bounds;
    for (const [value, expected] of [[layered.x, 67.5], [layered.y, 27.5], [layered.width, 25], [layered.height, 25]]) {
        assert.ok(Math.abs(value - expected) < 0.5, `bar bounds ${JSON.stringify(layered)}`);
    }
    const morphing = layoutAt(layersPath, 1.5).scene.objects.find((object) => object.id === "blob").bounds;
    const morphCenter = [morphing.x + morphing.width / 2, morphing.y + morphing.height / 2];
    assert.ok(Math.abs(morphCenter[0] - 100) < 0.5 && Math.abs(morphCenter[1] - 50) < 0.5, JSON.stringify(morphing));
    assert.match(refusal("hasty.lua", hastyLua),
        /'dot' already animates its transform until 2\.00 s; a morph that begins before then must end/);
    const inspectedEmphasis = inspectScript("emphasis.lua", emphasisLua).scene;
    assert.equal(inspectedEmphasis.duration, 10);
    // The shapes of the emphases are named after their target, and the lines of a flash at a
    // point after the effect alone.
    assert.deepEqual(
        inspectedEmphasis.objects.map((object) => object.id),
        [
            "wiggled",
            "wiggled.circumscribe.1.frame",
            "wiggled.circumscribe.1.flash",
            "wiggled.passing_flash.1.flash",
            ...Array.from({ length: 8 }, (_, i) => `wiggled.flash.1.${i + 1}`),
            ...Array.from({ length: 12 }, (_, i) => `flash.1.${i + 1}`),
            "wiggled.ripple.1.ring",
        ],
    );

    // The dot rides the samples from (1, 1) to its image (0, 2) under z -> z^2, then to
    // (0.1, 1); 30 pixels to a unit from the center of the frame.
    const warping = inspectScript("warping.lua", warpingLua);
    for (const [time, expected] of [[0, [270, 105]], [0.5, [255, 90]], [1, [240, 75]], [1.5, [243, 105]]]) {
        const bounds = layoutAt(warping.path, time).scene.objects.find((object) => object.id === "dot").bounds;
        const center = [bounds.x + bounds.width / 2, bounds.y + bounds.height / 2];
        assert.ok(Math.hypot(center[0] - expected[0], center[1] - expected[1]) < 0.5, `dot at ${time} s: ${center}`);
    }
    assert.equal(warping.scene.duration, 2);
    assert.match(refusal("uncovered.lua", uncoveredLua),
        /the warp point \(3\.0, 0\.0, 0\.0\) lies outside its samples, x from -1\.0 to 1\.0 and y from -1\.0 to 1\.0/);

    // The copy keeps the line its source showed halfway through the track, from (-2, -2) to
    // (2, -1), and the warp lifts the line the track leaves, from (-2, -2) to (2, 0), by 1; 15
    // pixels to a unit from the center of the frame. New points for the warped beam fail.
    const copiedPath = writeScript("copied.lua", copiedLua);
    for (const [time, id, expected] of [
        [0.75, "rib2", [80, 67.5]],
        [2, "rib2", [80, 67.5]],
        [0.5, "rib", [80, 67.5]],
        [1, "rib", [80, 60]],
        [2, "rib", [80, 45]],
    ]) {
        const bounds = layoutAt(copiedPath, time).scene.objects.find((object) => object.id === id).bounds;
        const center = [bounds.x + bounds.width / 2, bounds.y + bounds.height / 2];
        assert.ok(Math.hypot(center[0] - expected[0], center[1] - expected[1]) < 0.5, `${id} at ${time} s: ${center}`);
    }
    assert.match(refusal("rewarped.lua", rewarpedLua),
        /could not play animation: a warped shape keeps the outline its warp gave/);

    const inspected = inspectScript("scene.lua", lua).scene;
    assert.equal(inspected.width, 320);
    assert.equal(inspected.height, 180);
    assert.equal(inspected.loop, true);
    assert.deepEqual(
        inspected.objects.map((object) => object.id),
        ["world", "double-arrow", "target"],
    );
    assert.equal(inspected.objects[0].type, "space");
    assert.equal(inspected.objects[0].space, null);
    assert.equal(inspected.objects[1].space, inspected.objects[0].handle);
    assert.equal(inspected.objects[2].space, inspected.objects[0].handle);

    const inspectedSample = inspectScript("cell-voxel.lua", sampleLua).scene;
    assert.deepEqual(inspectedSample.objects.map((object) => object.type), [
        "space", "group", "cell", "cell", "cell", "group", "cell",
    ]);
    const voxelGroup = inspectedSample.objects[1];
    assert.equal(voxelGroup.parent, inspectedSample.objects[0].handle);
    for (const cell of inspectedSample.objects.slice(2, 5)) {
        assert.equal(cell.parent, voxelGroup.handle);
        assert.equal(cell.space, inspectedSample.objects[0].handle);
    }
    const cellGroup = inspectedSample.objects[5];
    assert.equal(cellGroup.parent, inspectedSample.objects[0].handle);
    assert.equal(inspectedSample.objects[6].parent, cellGroup.handle);
    assert.equal(inspectedSample.duration, 1);

    const inspectedFreeform = inspectScript("freeform.lua", freeformLua).scene;
    assert.equal(inspectedFreeform.width, 480);
    assert.equal(inspectedFreeform.height, 270);
    assert.ok(objectCount(inspectedFreeform) >= 5);
    for (const type of ["group", "rectangle", "text", "circle", "connector"]) {
        assert.ok(
            inspectedFreeform.objects.some((object) => object.type === type),
            `freeform scene is missing ${type}`,
        );
    }

    const reservePath = writeScript("reserved.lua", reservedLua);
    const reserveTwin = writeScript("reserved-twin.lua", `
local s=vis.scene{width=320,height=180,camera={height=180}}
s:space{x={-100,100,50},y={-70,70,50},width=1,axis_width=2,ticks=true}
local n=s:number{tabular=true,value=1.5,decimals=2,reserve="-000.00",align={1,0.5},id="n"}
s:matrix{tabular=true,rows={{1,2},{3,4}},reserve={"-000.00","00"},justify="right",id="m"}
s:text{tabular=true,text="x1",reserve="long text",point={0,-50}}
s:play({target=n,value=-3},1,"linear")
return s
`);
    for (const time of [0, 0.25, 0.5, 1]) {
        assert.deepEqual(renderFrame(reservePath, time, fontOptions), renderFrame(reserveTwin, time, fontOptions));
    }
    const proportionalPath = writeScript("proportional.lua",
        reservedLua.replaceAll('["tabular"]=true', '["tabular"]=false'));
    assert.notDeepEqual(renderFrame(reservePath, 0.5, fontOptions), renderFrame(proportionalPath, 0.5, fontOptions));
    assert.equal(layoutAt(reservePath, 0.5, fontOptions).scene.objects.find((object) => object.id === "n").text, "-0.75");

    const live = inspectScript("live-transition.lua", liveLua);
    const liveTwinPath = writeScript("live-transition-twin.lua", `
local cfg = {width=120,height=60,background="#000000",camera={height=4},antialiasing=false}
local root = vis.scene(cfg)
local a, b = vis.scene(cfg), vis.scene(cfg)
local dot = a:circle{center={-1,0},radius=0.2,fill="#ffffff",stroke="#00000000"}
a:shift(dot,{4,0},4,"linear")
local next = b:circle{center={-1,0},radius=0.2,fill="#ffffff",stroke="#00000000"}
b:shift(next,{1,0},0.5,"linear")
root:scene_transition({a,b},{hold={2,1},duration=1,style="slide",curve="linear"})
return root
`);
    for (const time of [1, 2, 2.25, 2.5, 3, 3.5, 4]) {
        assert.deepEqual(renderFrame(live.path, time), renderFrame(liveTwinPath, time), `live transition twin at ${time}`);
    }
    assert.equal(live.scene.duration, 4);
    assert.ok(Math.abs(layoutAt(live.path, 2.25).report.visuals[0].paintBounds.x - 45.75) < 0.01);

    // A quarter into the fade the incoming card shows at a quarter, on its own; halfway into
    // the match the cards pair.
    const styledPath = writeScript("styled.lua", styledLua);
    const fading = layoutAt(styledPath, 0.25).report;
    const stagePaths = fading.scenes.slice(1).map((entry) => entry.path);
    assert.deepEqual(stagePaths, ["root/transition:0", "root/transition:1"]);
    for (const entry of fading.scenes.slice(1)) {
        assert.deepEqual(rounded(entry.bounds), { x: 20, y: 20, width: 160, height: 60 });
    }
    const arriving = fading.visuals.find((visual) => visual.kind === "fade-in");
    assert.ok(arriving && arriving.counterpart === null && Math.abs(arriving.opacity - 64 / 255) < 1e-4);
    assert.ok(layoutAt(styledPath, 1.5).report.visuals.some((visual) => visual.kind === "morph"));

    // A quarter into a slide from the top, the incoming stage has come a quarter of the
    // viewport down and pushed the outgoing one as far.
    const sliding = layoutAt(writeScript("slide.lua", slideLua), 0.25).report.scenes.slice(1);
    assert.deepEqual(
        sliding.map((entry) => rounded(entry.bounds)),
        [
            { x: 20, y: 35, width: 160, height: 60 },
            { x: 20, y: -25, width: 160, height: 60 },
        ],
    );

    // Halfway into a slide that also matches, the cards travel as one morph.
    const pairVisuals = layoutAt(writeScript("pairs.lua", pairLua), 0.5).report.visuals;
    assert.equal(pairVisuals.filter((visual) => visual.kind === "morph").length, 1);

    // Halfway into the zoom, the outgoing stage has grown twice and the incoming one fills the
    // area at twice its first size.
    const zooming = layoutAt(writeScript("zoom.lua", zoomLua), 0.5).report.scenes.slice(1);
    const expected = [
        [20 - 320 / 3, 0, 320, 120],
        [20 + 160 / 3, 30, 80, 30],
    ];
    assert.equal(zooming.length, 2);
    zooming.forEach((entry, index) => {
        const { x, y, width, height } = entry.bounds;
        [x, y, width, height].forEach((value, axis) => assert.ok(Math.abs(value - expected[index][axis]) < 0.01));
    });

    for (const [index, { objectLua, areaLua }] of exactZooms.entries()) {
        const reports = [objectLua, areaLua].map((source, form) =>
            layoutAt(writeScript(`exact-zoom-${index}-${form}.lua`, source), 0.5).report);
        assert.deepEqual(rounded(reports[0].scenes), rounded(reports[1].scenes));
        assert.deepEqual(rounded(reports[0].visuals), rounded(reports[1].visuals));
        assert.deepEqual(rounded(reports[0].paths), rounded(reports[1].paths));
    }

    const { path: viewportPath, scene: inspectedViewport } = inspectScript("viewport.lua", viewportLua);
    assert.equal(inspectedViewport.viewports.length, 1);
    assert.equal(inspectedViewport.viewports[0].scene.viewports.length, 1);
    const viewportBenchmark = cli(["benchmark", viewportPath, "--frames", "1", "--warmup", "0"]);
    assert.equal(JSON.parse(viewportBenchmark.stdout).objects, 5);
    for (const invalidFrames of ["+1", "-1", " 1", "4294967296", "18446744073709551616"]) {
        const invalid = spawnSync(cliPath, ["benchmark", viewportPath, "--frames", invalidFrames], { encoding: "utf8" });
        assert.equal(invalid.status, 2, `Accepted invalid frame count ${JSON.stringify(invalidFrames)}`);
    }

    const maximumDuration = vis.scene({ width: 16, height: 16 });
    maximumDuration.wait(3.4028234663852886e38);
    const maximumDurationPath = writeScript("maximum-duration.lua", compileScene(maximumDuration));
    cli(["benchmark", maximumDurationPath, "--frames", "3", "--warmup", "0"]);

    const textPath = writeScript("text.lua", [
        "local scene = vis.scene { width = 64, height = 32 }",
        "scene:text { text = \"x\" }",
        "scene:text { text = \"y\", font = \"Missing Family\", point = {1, 0} }",
        "return scene",
    ].join("\n"));
    const textPng = join(directory, "text.png");
    // Text in a font that is not loaded is drawn in the default font, with a warning per font.
    const unfonted = cli(["render", textPath, "-o", textPng]);
    assert.ok(existsSync(textPng));
    assert.match(unfonted.stderr, /the font 'Pretendard' is not loaded/);
    assert.match(unfonted.stderr, /the font 'Missing Family' is not loaded, so its text is drawn in the default font; pass its file, such as Missing Family\.ttf, with --font/);
    const misnamed = cli(["layout", textPath, "--font", font]);
    assert.match(misnamed.stderr, /the font 'Missing Family' is not loaded/);
    assert.doesNotMatch(misnamed.stderr, /'Pretendard'/);
    const unreadable = cli(["render", textPath, "--font", join(directory, "none.ttf")], 3);
    assert.match(unreadable.stderr, /could not load the font file '.*none\.ttf'/);
    const unknownOutput = spawnSync(cliPath, ["render", textPath, "-o", join(directory, "text.webm")], {
        encoding: "utf8",
    });
    assert.match(unknownOutput.stderr, /the output must end in \.png, \.gif or \.mp4/);
    for (const failure of [unfonted, misnamed, unreadable, unknownOutput]) {
        assert.doesNotMatch(failure.stderr, /NonSupport|InvalidArguments|InsufficientCondition|IoError/);
    }
    const secondFont = join(directory, "Missing Family.ttf");
    copyFileSync(font, secondFont);
    const bothFonts = ["--font", font, "--font", secondFont];
    cli(["render", textPath, "-o", textPng, ...bothFonts]);
    for (const command of [["layout", textPath], ["benchmark", textPath, "--frames", "1", "--warmup", "0"]]) {
        cli([...command, ...bothFonts]);
    }
    const tooMany = cli(["render", textPath, ...Array(65).fill(["--font", font]).flat()], 2);
    assert.match(tooMany.stderr, /at most 64 --font options/);
    const themeLayout = cli(["layout", resolve(repoRoot, "src/examples/lua/theme_showcase.lua"), "--padding", "4",
        ...fontOptions]);
    const themeReport = JSON.parse(themeLayout.stdout).scene;
    assert.equal(themeReport.viewports.length, 6);
    for (const viewport of themeReport.viewports) {
        const objects = new Map(viewport.scene.objects.map((object) => [object.handle, object]));
        const textCollisions = viewport.scene.intersections.filter((pair) => {
            return objects.get(pair.first)?.type === "text" && objects.get(pair.second)?.type === "text";
        });
        assert.deepEqual(textCollisions, [], "theme-showcase has colliding text roles");
        assert.equal(
            boundsOverlap(viewport.scene, "vector", "point"),
            false,
            "theme-showcase arrowhead overlaps its endpoint marker",
        );
    }
    const galleryLayout = JSON.parse(cli(["layout", resolve(repoRoot, "src/examples/lua/object_gallery.lua"),
        ...fontOptions]).stdout);
    const galleryReport = galleryLayout.scene;
    assert.ok(galleryLayout.report.visuals.length > 0, "object-gallery layout report has no rendered visuals");
    for (const visual of galleryLayout.report.visuals) {
        assert.ok(
            Number.isInteger(visual.object) && visual.object >= 0 && visual.object < galleryLayout.report.objects.length,
            "layout visual does not reference a semantic object",
        );
        assert.ok(
            visual.counterpart === null ||
                Number.isInteger(visual.counterpart) && visual.counterpart >= 0 &&
                visual.counterpart < galleryLayout.report.objects.length,
            "layout visual counterpart is neither null nor a semantic object",
        );
        assert.ok(
            visual.occluder === null ||
                Number.isInteger(visual.occluder) && visual.occluder >= 0 &&
                visual.occluder < galleryLayout.report.scenes.length,
            "layout visual occluder is neither null nor a Scene",
        );
        assert.ok(["stable", "morph", "fade-out", "fade-in"].includes(visual.kind), "unknown layout visual kind");
        assert.ok(Number.isInteger(visual.layer), "layout visual layer is not an integer");
        assert.ok(Number.isFinite(visual.opacity), "layout visual opacity is not finite");
        assert.equal(typeof visual.visible, "boolean");
        assert.equal(typeof visual.clipped, "boolean");
        assert.equal(typeof visual.occluded, "boolean");
    }
    assert.ok(galleryLayout.report.paths.length > 0, "object-gallery layout report has no sampled paths");
    for (const path of galleryLayout.report.paths) {
        assert.ok(
            Number.isInteger(path.visual) && path.visual >= 0 && path.visual < galleryLayout.report.visuals.length,
            "sampled path does not reference a physical visual",
        );
        assert.ok(path.points.length >= 2 && path.points.every((point) => {
            return Number.isFinite(point.x) && Number.isFinite(point.y);
        }), "sampled path has invalid root-pixel vertices");
        assert.ok(Number.isFinite(path.strokeWidth) && path.strokeWidth >= 0);
        assert.equal(typeof path.closed, "boolean");
        assert.equal(typeof path.filled, "boolean");
        assert.equal(typeof path.stroked, "boolean");
        assert.ok(Number.isFinite(path.paintBounds.x) && Number.isFinite(path.paintBounds.y));
        assert.ok(path.paintBounds.width >= 0 && path.paintBounds.height >= 0);
    }
    assert.equal(
        boundsOverlap(galleryReport.viewports[2].scene, "vector", "point"),
        false,
        "object-gallery vector arrowhead overlaps its endpoint marker",
    );
    assert.equal(
        boundsOverlap(galleryReport.viewports[4].scene, "space-vector", "space-point"),
        false,
        "object-gallery Space vector arrowhead overlaps its endpoint marker",
    );

    // A stand-in ffmpeg records its arguments and keeps the soundtrack it is given, so the
    // audio of an MP4 export can be read sample by sample. The tone is 0.1 s at a quarter of
    // full scale.
    const tone = Buffer.alloc(44 + 4800 * 2);
    tone.write("RIFF", 0, "ascii");
    tone.writeUInt32LE(tone.length - 8, 4);
    tone.write("WAVEfmt ", 8, "ascii");
    tone.writeUInt32LE(16, 16);
    tone.writeUInt16LE(1, 20);
    tone.writeUInt16LE(1, 22);
    tone.writeUInt32LE(48000, 24);
    tone.writeUInt32LE(96000, 28);
    tone.writeUInt16LE(2, 32);
    tone.writeUInt16LE(16, 34);
    tone.write("data", 36, "ascii");
    tone.writeUInt32LE(4800 * 2, 40);
    for (let frame = 0; frame < 4800; frame++) tone.writeInt16LE(8192, 44 + frame * 2);
    const tonePath = join(directory, "tone.wav");
    writeFileSync(tonePath, tone);
    const soundPath = writeScript("sound.lua", [
        "local scene = vis.scene { width = 16, height = 16, fps = 10 }",
        "vis.audio.cue(scene, { asset = \"tone.wav\", begin = 0.1, gain = 0.5 })",
        "scene:wait(0.3)",
        "return scene",
    ].join("\n"));
    const bin = join(directory, "bin");
    mkdirSync(bin);
    writeFileSync(join(bin, "ffmpeg"), [
        "#!/bin/sh",
        "printf '%s\\n' \"$@\" > \"$VIS_TEST_ARGUMENTS\"",
        "previous=",
        "for argument in \"$@\"; do",
        "    if [ \"$previous\" = \"-i\" ] && [ \"$argument\" != \"pipe:0\" ]; then",
        "        cp \"$argument\" \"$VIS_TEST_SOUNDTRACK\"",
        "    fi",
        "    previous=$argument",
        "done",
        "cat > /dev/null",
        "",
    ].join("\n"), { mode: 0o755 });
    const argumentsPath = join(directory, "arguments.txt");
    const soundtrackPath = join(directory, "soundtrack.wav");
    const env = {
        ...process.env,
        PATH: `${bin}:${process.env.PATH ?? ""}`,
        VIS_TEST_ARGUMENTS: argumentsPath,
        VIS_TEST_SOUNDTRACK: soundtrackPath,
    };
    const sample = (frame, channel) => readFileSync(soundtrackPath).readInt16LE(44 + frame * 4 + channel * 2);
    // Renders with the stand-in ffmpeg.
    const render = (args, expected = 0) => cli(["render", ...args], expected, { env });

    // --audio hands a scene without cues its file as it is, and needs a readable file and an MP4.
    const plainPath = writeScript("plain.lua",
        "local scene = vis.scene { width = 16, height = 16 }\nscene:wait(0.1)\nreturn scene\n");
    render([plainPath, "-o", join(directory, "plain.mp4"), "--audio", tonePath]);
    const toneArgument = tonePath.replace(/[.*+?^${}()|[\]\\]/g, "\\$&");
    assert.match(readFileSync(argumentsPath, "utf8"), new RegExp(`\\n-i\\n${toneArgument}\\n-map\\n0:v:0\\n-map\\n1:a:0\\n`));
    assert.deepEqual(readFileSync(soundtrackPath), readFileSync(tonePath));
    const narratedGif = render([plainPath, "-o", join(directory, "plain.gif"), "--audio", tonePath], 2);
    assert.match(narratedGif.stderr, /--audio needs an \.mp4 output/);
    const unheard = render([plainPath, "-o", join(directory, "plain.mp4"), "--audio", join(directory, "none.wav")], 3);
    assert.match(unheard.stderr, /could not read the --audio file .*none\.wav/);
    const twice = render([plainPath, "-o", join(directory, "plain.mp4"), "--audio", tonePath, "--audio", tonePath], 2);
    assert.match(twice.stderr, /--audio may be given only once/);

    // A JS scene with cues loads with or without the audio module, here hidden from the script;
    // without it, the scene plays without sound instead of failing on vis.audio.
    for (const [name, prefix] of [["audio", ""], ["unvoiced", "local vis = vis.merge({}, vis)\nvis.audio = nil\n"]]) {
        const loaded = inspectScript(`${name}.lua`, `${prefix}${audioLua}`);
        assert.equal(loaded.scene.duration, 8, name);
    }

    // With the Audio module, an MP4 carries the cues a scene declares.
    if (spawnSync(cliPath, ["inspect", soundPath], { encoding: "utf8" }).status === 0) {
        render([soundPath, "-o", join(directory, "sound.mp4")]);
        assert.match(readFileSync(argumentsPath, "utf8"), /\n-map\n1:a:0\n/);
        const soundtrack = readFileSync(soundtrackPath);
        assert.equal(soundtrack.toString("ascii", 0, 4), "RIFF");
        assert.equal(soundtrack.readUInt16LE(22), 2);
        assert.equal(soundtrack.readUInt32LE(24), 48000);
        assert.equal(soundtrack.readUInt16LE(34), 16);
        // Three frames at 10 fps last 0.3 s; the cue plays its 0.1 s asset from 0.1 s at half gain.
        assert.equal(soundtrack.readUInt32LE(40), 14400 * 4);
        for (const [frame, expected] of [[0, 0], [4799, 0], [4800, 4096], [9599, 4096], [9600, 0], [14399, 0]]) {
            assert.equal(sample(frame, 0), expected, `left sample ${frame}`);
            assert.equal(sample(frame, 1), expected, `right sample ${frame}`);
        }
        assert.deepEqual(readdirSync(directory).filter((name) => name.includes(".vis-")), []);

        // With cues, the --audio file joins the mix from time 0 at full gain.
        render([soundPath, "-o", join(directory, "sound.mp4"), "--audio", tonePath]);
        for (const [frame, expected] of [[0, 8192], [4799, 8192], [4800, 4096], [9600, 0]]) {
            assert.equal(sample(frame, 0), expected, `mixed sample ${frame}`);
        }

        render([soundPath, "-o", join(directory, "sound.gif")]);
        assert.doesNotMatch(readFileSync(argumentsPath, "utf8"), /1:a:0/);

        const missingPath = writeScript("missing-sound.lua", [
            "local scene = vis.scene { width = 16, height = 16 }",
            "vis.audio.cue(scene, { asset = \"missing.wav\" })",
            "scene:wait(0.1)",
            "return scene",
        ].join("\n"));
        const missing = render([missingPath, "-o", join(directory, "missing.mp4")], 3);
        assert.match(missing.stderr, /could not read the audio asset 'missing\.wav' at .*missing\.wav/);
    }
} finally {
    rmSync(directory, { recursive: true, force: true });
}
