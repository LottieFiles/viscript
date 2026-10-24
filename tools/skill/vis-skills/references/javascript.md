# JavaScript and TypeScript API

Import `vis` and `createVis` from the installed runtime's `client.js`. The builder records a scene definition; a runtime from `createVis` loads and renders it. API declarations beside the runtime describe its options and capabilities.

```js
import {createVis, vis} from "./client.js";

const definition = vis.scene({camera: {height: 7}});
const figure = definition.group({id: "figure"});
const plane = figure.space({x: [-5, 5, 1], y: [-3, 3, 1], numbers: true});
const vector = plane.vector({value: [3, 2], stroke: "accent", width: 2, tip: 7, id: "vector-a"});
vector.label({text: "a = (3, 2)", point: [3.25, 2.2], align: [0, 0.5], role: "code", fill: "accent"});
definition.wait(0.35).fadeIn(figure, {duration: 0.55, curve: "gentle"}).wait(0.65);

const runtime = await createVis();
runtime.font("Pretendard", new Uint8Array(await
    (await fetch("./Pretendard.ttf")).arrayBuffer()), "ttf");
runtime.font("JetBrainsMono", new Uint8Array(await
    (await fetch("./JetBrainsMono.ttf")).arrayBuffer()), "ttf");
runtime.load(definition, "vector-scene.js");
runtime.draw(document.querySelector("canvas"), runtime.duration);
```

Scene size is in pixels, 960×540 when left out, and object positions are world coordinates. Vectors use arrays, ranges use `[min, max, step]`, and matrices are row-major absolute local-to-parent transforms. Follow [scene-contract.md](scene-contract.md) for camera mapping. A Scene that names no theme is `"adaptive"`, the colors and font of the host app, such as VS Code or Obsidian, or Pro White without one; `"pro_white"` and `"pro_black"` are fixed.

## Objects and layout

SceneBuilder and Object handles share factories. Each receiver becomes the new object's parent. See [lua.md](lua.md) for the shared geometry and styling behavior.

| Purpose | Factories |
|---|---|
| Containers | `group`, `space` |
| Geometry | `point`, `line`, `arrow`, `vector`, `circle`, `arc`, `angle`, `rectangle`, `polygon`, `route`, `path`, `curve` |
| Mathematics | `plot`, `area`, `riemann`, `contour`, `vectorField`, `surface` |
| Shape regions | `union`, `intersection`, `difference`, `exclusion` |
| Text | `text`, `number`, `spans`, `code`, `tex`, `matrix`, `table` |
| Media/grid | `picture`, `cell` |
| Relationships | `connector`, `brace` |

Styles include fill, stroke, width, opacity, progress, layer, gradient, dashes, masks, blend, blur, and shadow. `isolate: true` composites a family together. Group fill, stroke, and width pass to child shapes. `object.label(options)` returns a child Text; supply its placement and layer.

Connectors follow sampled endpoint outlines, with bounds as a fallback. `scene.connector(a, b, {padding})` adds distance beyond the outlines. Lines, arrows, and vectors use `trimDistance` to trim both ends, or `[start, finish]` for separate distances. Route markers remain solid over a dashed shaft.

`rectangle({around: handles, padding})` frames objects. `brace({around: term, label: "term"})` returns `[brace, label]`. Arcs use radians and support a filled sector; angle marks can carry text or TeX labels at their bisector.

Handle methods `moveTo`, `nextTo`, and `alignTo` place objects in their direct parent's coordinates. Groups add `arrange` and `arrangeGrid`. `children()` returns authored child handles, and `find(id)` selects an authored ID.

## Axes and fields

Axis/grid styling, ticks, and coordinate labels share the Lua options. Use `space.labels({x: [[value, label]]})`; screen placement supports endpoint labels in rotating 3D views.

```js
const axes = definition.space({x: [-5, 5, 1], y: [-3, 3, 1], ticks: true});
const graph = axes.plot({fn: Math.tan, x_range: [-5, 5, 0.01], y_range: [-3, 3]});
definition.create(graph, {duration: 2, curve: "linear"});
const area = axes.area({fn: (x) => x*x, x_range: [0, 2, 0.05], fill: "#2563eb33"});
```

JavaScript functions are sampled while building. The emitted scene stores points or scalar/vector values, so seeking uses retained geometry. Plot states may redraw through `fn` or `points`, retaining their sampling range and cuts. Matching function grids blend vertically during playback.

`riemann` returns a Group of rectangles; `vectorField` returns arrows in grid order. `surface` accepts a colormap, `{map, range, axis}`, a color callback, or per-point colors. `mesh: [11, 11]` overlays a coarse mesh on a finer surface, and `light` sets world lighting.

`contour` returns one Plot per level. Values run row by row from the bottom left, x varying fastest. The builder traces the bilinear sampled field, so use fine steps for curved contours. Level-value labels use `labels: true` or `{format, size, spacing, padding, upright}`.

```js
const curves = axes.contour({
    fn: (x, y) => x*x + y*y, levels: [1, 2],
    x_range: [-2, 2, 0.05], y_range: [-2, 2, 0.05], colors: "viridis", labels: true,
});
const radius = definition.tracker(1);
definition.bind(curves.children()[0], radius,
    (r) => ({fn: (x, y) => x*x/(r*r) + y*y}));
definition.track(radius, 2, {duration: 1, curve: "linear"});
```

## Text and code

Text size is in points: the em spans `4/3 * size` screen pixels for billboard text and world units for plane text. Theme roles choose fonts and semantic colors. A font can be a comma list, such as `font: "Source Serif 4, Pretendard"`: each character takes the first registered font that has it, then the bundled Noto Sans Math symbols or the default font. `measure: true` gives text a layout box; `baseline: true` pins the first baseline at `point`. `reserve: "-0.00"` keeps a stable box, and `tabular: true` gives digits, minus, and spaces equal advance cells.

`number` animates `value` and formats decimals, sign, separator, prefix, and suffix. `matrix` and `table` return row/entry groups accessible through one-based `row`, `entry`, and `column` methods. Matrix string entries are inline TeX when the tex module is present (plain text otherwise); numbers stay countable and table strings stay text.

`spans(parts, options)` combines strings and styled parts on shared baselines. Parts can set `rise` or contain `{tex}`. `split`, `wrap`, and `leading` control readable text flow.

```js
const equation = definition.tex({tex: ["a^2", "+", "b^2", "=", {text: "c^2", id: "hyp"}], id: "eq"});
definition.indicate(equation.part("c^2"));
const sentence = definition.spans(["The area is ", {tex: "\\pi r^2"}, "."], {role: "text"});
```

TeX returns a TexHandle with `part(tex, occurrence)` selection. Matching formulas pairs pieces by TeX; split terms into parts to move them together. `display: false` selects inline style.

`code` returns line groups containing token handles, named `<id>/<line>/<token>`. `search` accepts a string or RegExp; options select match occurrence, lines, and whole tokens. `edit` applies replacement/insertion/removal or append/prepend operations and animates a code diff:

```js
const listing = definition.code({text: "local total = 0\nreturn total", language: "lua", id: "listing"});
definition.highlight(listing, listing.search(/\btotal\b/, {match: "all"}));
const nextListing = definition.edit(listing, {replace: "total", with: "sum", match: "all"});
```

## Timeline

The builder uses camelCase versions of the Lua timeline calls. Most calls advance the authoring cursor; `at` authors at an absolute time or mark and then restores it. `parallel` starts branches together.

Animation options share `TimingOptions` (`duration`, `curve`), extended by `AnimationOptions` with `lag` for staggered calls. Effect-specific options build on these types:

```js
const timing = {duration: 0.6, curve: "ease_in_out"};
definition.shift(vector, [1, 0], timing).rotate(vector, Math.PI / 2, timing);
definition.create([vector, label], {...timing, lag: 0.15});
```

Pass timing and operation-specific fields in one options object. Omit it to use the operation's defaults; `track` and `warp` require an options object. `wait(seconds)` advances the cursor by a numeric duration.

| Purpose | Calls |
|---|---|
| Draw | `create`, `uncreate`, `fillReveal`, `drawBorderThenFill`, `write` |
| Fade/grow | `fadeIn`, `fadeOut`, `growFromCenter`, `growFromEdge`, `shrinkToCenter` |
| Emphasis | `indicate`, `wiggle`, `circumscribe`, `passingFlash`, `flash`, `ripple` |
| Movement/style | `shift`, `rotate`, `scale`, `transform`, `moveAlong`, `fade`, `stroke`, `fill` |
| Replacement | `morph`, `fadeTransform`, `matchTransform`, `transition` |
| State | `tween`, `set`, `save`, `restore`, `clone`, `warp`, `remove` |

`tween` accepts property descriptors with a target; `set` applies them immediately. `shift` is relative and `transform` is absolute; `position` moves the center of the target's bounds to a point in its parent's space, from wherever earlier clips left it. A descriptor takes only one of the three, and a bound state cannot use `position`. `arc` bends movement and `lag` offsets successive targets. `save`/`restore` keep target state stacks.

Playing `text` fades out the whole old string, then fades in the whole replacement. `retype: true` erases to the shared prefix before typing the new suffix; use it when typing from an empty string too. `sweep: true` opts into a character sweep that mixes both strings. Both flags require `text` and cannot both be true. Numeric `value` still interpolates continuously.

```js
definition.tween({target: vector, shift: [1, 0], stroke: "success"}, {duration: 1, curve: "smooth"});
definition.tween({target: vector, position: [0, 1], arc: Math.PI / 2}, {duration: 1});
definition.parallel(
    (scene) => scene.shift(vector, [-1, 0], {duration: 1}),
    (scene) => scene.fade(vector, 0.5, {duration: 1}),
);
```

```js
definition.tween({target: caption, text: "Ready to play"}, {duration: 1});
definition.tween({target: caption, text: "Ready to export", retype: true}, {duration: 1});
definition.tween({target: caption, text: "EXPORTED", sweep: true}, {duration: 1});
```

`morph` resamples outlines to turn source into target; contour `pairs` override automatic matching. `{copy: true}` preserves the source. `clone` snapshots a family at the current time. `fadeTransform` cross-fades into a target attached at the current cursor without clips and removes the source from the timeline on completion. `transition` fades outgoing handles out and incoming handles in without removing them; create every incoming handle with `opacity: 0`. `matchTransform` carries matching text, TeX, and optional shapes into a target attached at the current cursor without clips. Its source must be at rest unless `copy: true` snapshots it; arc or path detours and independent unmatched fades control the handoff.

`warp` samples a plane mapping over explicit ranges, with `step` controlling initial subdivision. The scene retains the resulting geometry:

```js
definition.warp(vector, (x, y) => [x + 0.2*y*y, y], {duration: 1, curve: "smooth", x_range: [-5, 5, 0.1], y_range: [-3, 3, 0.1], step: 0.05});
```

Curves include named presets and `vis.animCurve.preset`, `cubicBezier`, and `reverse`. `vis.random(seed, seed2)` produces repeatable samples matching Lua's seeded generator.

## Trackers and cameras

`tracker(value)` creates a number; `bind(target, trackerOrArray, fn)` maps its values to object states. `track` bakes frame samples into retained clips. Keep callbacks pure. Different trackers can overlap on one bound object.

```js
const phase = definition.tracker(0);
definition.bind(vector, phase, (v) => ({value: [Math.cos(v), Math.sin(v)]}));
definition.track(phase, 2 * Math.PI, {duration: 3, curve: "linear"});
```

For discrete reveals and counters, put every related state in the same `at` event:

```js
samples.forEach((point, index) => {
    const count = index + 1;
    definition.at(start + count / rate, (scene) => scene.set([
        {target: point, opacity: 1},
        {target: total, value: count},
        {target: hits, value: inside[count]},
    ]));
});
```

`vis.springResponse(seconds, {mass, stiffness, damping, displacement, velocity})` samples a physical spring. Bind it to an elapsed-seconds tracker and track with `"linear"` after the release hold.

`look` aims at points or handles. `focus` frames authored bounds in a 2D view with fractional padding; use `look` to frame a 3D view. `focus()` returns to the opening camera from either view. `orbit` circles a 3D target. `view(bounds, options)` draws the same scene through another camera; the returned handle exposes look, focus, orbit, and fade.

## Fonts, assets, and lifecycle

Register font and image bytes with stable application names; a font name cannot contain a comma, and `missingFonts()` lists the fonts a loaded scene still needs. A runtime started without a scene takes them before it loads the authored definition:

```js
const runtime = await createVis();
runtime.font("Scene Sans", fontBytes, "ttf");
runtime.asset("diagram", imageBytes, "png");
runtime.load(definition, "scene.js");
```

Serve browser runtime files over HTTP. In Node, pass `.wasm` bytes as `wasmBinary`.

```js
runtime.render(time);             // a copy of the pixels
runtime.draw(canvas, time);
runtime.bounds("subject", time);
runtime.intersects("title", "plot", time, 4);
runtime.layoutReport(time, 4);
runtime.resize(width, height);
runtime.load(nextDefinition, "next.js");
runtime.load(luaSource, "next.lua");
runtime.destroy();
```

Destroy the runtime at teardown. Failed reload preserves the last valid scene.

`layoutReport` uses root pixels across nested Scenes. Resolve objects by `scenePath + id`; visual records refer back to object indices, and paths refer to their owning visuals. Inspect visible, unoccluded paint at boundary times and run CLI `audit` on `compileScene(definition)` output. Follow the [review round](../SKILL.md#review-in-rounds) for frame and image selection.

## Scene composition and modules

`space.cell(callback, options)` and `space.voxel(callback, options)` bake sampled fields during authoring. `viewport(child, bounds)` mounts a child builder; `sceneTransition(stages, options)` plays a sequence with local stage times. Finish authoring children and stages first: successful attachment seals and transfers them, so they cannot be edited or reused in another mount.

```js
definition.sceneTransition([overview, detail, summary], {
    duration: 0.8,
    styles: [{style: "zoom_in", area: "chart", area_margin: 0},
             {style: "slide", from: "bottom", match: true}],
});
```

Styles include match, fade, slide, zoom_in, and zoom_out. `hold` controls fully shown time; `live: false` shows completed stages. A zoom `area` is an object ID or normalized rectangle; `area_margin` sets object padding.

Use Lua or C++ for the Diagram semantic builder. See [vis-interactive](../../vis-interactive/SKILL.md) for UI, Input, action maps, retained runtime, audio, and simulation loops.
