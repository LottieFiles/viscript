---
title: Path & Field Motion
id: path-motion
order: 5
parent: animation
---

# Path & Field Motion

Follow an outline with `move_along`, integrate a vector field with `advect`, or bend object geometry with `warp`. `advect` is available in Lua.

## Follow, integrate and deform

| Lua | JavaScript | Purpose |
| --- | --- | --- |
| `scene:move_along(targets, path, options?)` | `scene.moveAlong(targets, path, options?)` | Move bounds centers along an outline. Options: `from` (0), `to` (1), `orient` (false), `duration`, `curve`, `lag`. |
| `scene:advect(targets, fn, options?)` | — | Integrate motion through `fn(x, y)` returning a field vector; options: `speed` (1, nonzero), `duration`, `curve` (`linear`). |
| `scene:warp(targets, fnOrSamples, duration?, curve?, options?)`<br />`scene:warp(targets, fnOrSamples, timingAndStep)` | `scene.warp(targets, fn, options)`<br />`scene.warp(targets, fn, duration, curve, options)` | Bend geometry toward a sampled parent-space map. Lua accepts `fn(x, y, z)` or sampled `x_range`, `y_range`, `values`; JS samples `fn(x, y)` using required `x_range`, `y_range`. Options: `step` (0.05), timing. |

Ordinary path outlines are copied when scheduled; connector paths retain live endpoints. Timing follows the [shared rules](./index.md).

## Examples

### scene.move_along

<!-- api-example: scene.move_along -->

```viscript
local scene = vis.scene {
    width = 960,
    height = 540,
}

scene:text {id = "caption", text = "move_along", point = {0, 2.2}, size = 20, font = "JetBrainsMono", fill = "muted"}
local path = scene:arc {id = "path", radius = 1.5, angle = 2 * math.pi, stroke = "muted", width = 1}
local subject = scene:circle {id = "subject", center = {1.5, 0}, radius = 0.15, fill = "accent"}
scene:move_along(subject, path, {duration = 2, curve = "linear"})

return scene
```

```viscript-js
const scene = vis.scene({
    width: 960,
    height: 540,
});

scene.text({id: "caption", text: "move_along", point: [0, 2.2], size: 20, font: "JetBrainsMono", fill: "muted"});
const path = scene.arc({id: "path", radius: 1.5, angle: 2 * Math.PI, stroke: "muted", width: 1});
const subject = scene.circle({id: "subject", center: [1.5, 0], radius: 0.15, fill: "accent"});
scene.moveAlong(subject, path, {duration: 2, curve: "linear"});

return scene;
```

### scene.advect

<!-- api-example: scene.advect -->

```viscript
local scene = vis.scene {
    width = 960,
    height = 540,
}

scene:text {id = "caption", text = "advect", point = {0, 2.2}, size = 20, font = "JetBrainsMono", fill = "muted"}
scene:circle {radius = 1.5, stroke = "muted", fill = "#00000000", width = 1}
local subject = scene:circle {id = "subject", center = {1.5, 0}, radius = 0.15, fill = "accent"}
scene:advect(subject, function(x, y) return -y, x end, {duration = 2, speed = 1})

return scene
```

### scene.warp

<!-- api-example: scene.warp -->

```viscript
local scene = vis.scene {
    width = 960,
    height = 540,
}

scene:text {id = "caption", text = "warp", point = {0, 2.2}, size = 20, font = "JetBrainsMono", fill = "muted"}
local subject = scene:rectangle {id = "subject", size = {2.6, 1.5}, stroke = "accent", fill = "accent", width = 3}
scene:warp(subject, function(x, y) return x, y + 0.5 * x * x end, {duration = 1.2, step = 0.05})

return scene
```

```viscript-js
const scene = vis.scene({
    width: 960,
    height: 540,
});

scene.text({id: "caption", text: "warp", point: [0, 2.2], size: 20, font: "JetBrainsMono", fill: "muted"});
const subject = scene.rectangle({id: "subject", size: [2.6, 1.5], stroke: "accent", fill: "accent", width: 3});
scene.warp(subject, (x, y) => [x, y + 0.5 * x * x], {duration: 1.2, step: 0.05, x_range: [-2, 2, 0.25], y_range: [-2, 2, 0.25]});

return scene;
```
