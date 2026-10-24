---
title: Animation Curves
id: animation-curves
order: 3
parent: utilities
---

# Animation Curves

Both languages accept a preset string or a table/object with exactly one of `preset` and `bezier`, plus optional `strength` and `reverse`. Lua creates these values directly; JavaScript also provides factories.

## Functions

| Lua | JavaScript | Purpose |
| --- | --- | --- |
| — | `vis.animCurve.preset(name, strength?)` | Create a preset curve; strength defaults to 1. |
| — | `vis.animCurve.cubicBezier(x1, y1, x2, y2, strength?)` | Create a timing Bézier curve; x controls are 0–1 and y controls are −2–2. |
| — | `vis.animCurve.reverse(curve)` | Toggle time reversal on a curve value. |

Presets are `linear`, `smooth`, `ease_in`, `ease_out`, `ease_in_out`, `gentle`, `snappy`, `back`, `bounce` and `elastic`. Strength is 0–2. Equivalent custom curves are `{bezier = {x1, y1, x2, y2}, strength = 1}` in Lua and `{bezier: [x1, y1, x2, y2], strength: 1}` in JavaScript.

## Examples

### vis.animCurve.preset

<!-- api-example: vis.animCurve.preset -->

```viscript
local scene = vis.scene {
    width = 960,
    height = 540,
}

local dot = scene:circle {id = "dot", center = {-3, 0}, radius = 0.4, fill = "accent"}
scene:line {from = {-3, -0.7}, to = {3, -0.7}, stroke = "muted", width = 1}
scene:shift(dot, {6, 0}, {duration = 2, curve = {preset = "bounce", strength = 1}})

return scene
```

```viscript-js
const scene = vis.scene({
    width: 960,
    height: 540,
});

const dot = scene.circle({id: "dot", center: [-3, 0], radius: 0.4, fill: "accent"});
scene.line({from: [-3, -0.7], to: [3, -0.7], stroke: "muted", width: 1});
scene.shift(dot, [6, 0], {duration: 2, curve: vis.animCurve.preset("bounce", 1)});

return scene;
```

### vis.animCurve.cubicBezier

<!-- api-example: vis.animCurve.cubicBezier -->

```viscript
local scene = vis.scene {
    width = 960,
    height = 540,
}

local dot = scene:circle {id = "dot", center = {-3, 0}, radius = 0.4, fill = "accent"}
scene:line {from = {-3, -0.7}, to = {3, -0.7}, stroke = "muted", width = 1}
scene:shift(dot, {6, 0}, {duration = 2, curve = {bezier = {0.2, 0, 0.8, 1}, strength = 1}})

return scene
```

```viscript-js
const scene = vis.scene({
    width: 960,
    height: 540,
});

const dot = scene.circle({id: "dot", center: [-3, 0], radius: 0.4, fill: "accent"});
scene.line({from: [-3, -0.7], to: [3, -0.7], stroke: "muted", width: 1});
scene.shift(dot, [6, 0], {duration: 2, curve: vis.animCurve.cubicBezier(0.2, 0, 0.8, 1)});

return scene;
```

### vis.animCurve.reverse

<!-- api-example: vis.animCurve.reverse -->

```viscript
local scene = vis.scene {
    width = 960,
    height = 540,
}

local dot = scene:circle {id = "dot", center = {-3, 0}, radius = 0.4, fill = "accent"}
scene:line {from = {-3, -0.7}, to = {3, -0.7}, stroke = "muted", width = 1}
scene:shift(dot, {6, 0}, {duration = 2, curve = {preset = "ease_in", strength = 1, reverse = true}})

return scene
```

```viscript-js
const scene = vis.scene({
    width: 960,
    height: 540,
});

const dot = scene.circle({id: "dot", center: [-3, 0], radius: 0.4, fill: "accent"});
scene.line({from: [-3, -0.7], to: [3, -0.7], stroke: "muted", width: 1});
scene.shift(dot, [6, 0], {duration: 2, curve: vis.animCurve.reverse("ease_in")});

return scene;
```
