---
title: Camera
id: scene-camera
order: 4
parent: scene
---

# Camera

Animate the root camera by moving it, orbiting its target, or fitting objects into the frame. For inset cameras, see [Views](./scene-views.md).

## Camera movement

| Lua | JavaScript | Purpose |
| --- | --- | --- |
| `scene:look(camera, duration?, curve?)`<br />`scene:look(camera, timing)` | `scene.look(camera, duration?, curve?)`<br />`scene.look(camera, timing)` | Animate the scene camera; default duration 1 and curve `smooth`. |
| `scene:orbit(radians, options?)` | `scene.orbit(radians, options?)` | Rotate a 3D camera around its target; options: `axis`, `duration`, `curve`. Default axis is camera up. |
| `scene:focus(targets?, options?)` | `scene.focus(targets?, options?)` | Fit targets with a 2D camera, or restore the opening camera when omitted. Options: `padding` (0.15), `duration`, `curve`. |

`timing` is a table/object with `duration` and `curve`; do not combine it with positional timing arguments.

## Camera fields

`camera` accepts `view`, `eye`, `target`, `up`, `height`, `projection`, `fov`, `near`, `far`. Omitted fields retain their values. `target` accepts a world point, a handle, or handles whose bounds should be centered.

## Examples

### scene.look

<!-- api-example: scene.look -->

```viscript
local scene = vis.scene {
    width = 960,
    height = 540,
}

local ball = scene:circle {id = "ball", center = {-2, 0}, radius = 0.5, fill = "accent"}
local other = scene:rectangle {id = "other", center = {2, 0}, size = {1, 1}, stroke = "foreground"}
scene:look({target = ball, height = 2.5}, 1)
scene:look({target = other, height = 2.5}, 1)

return scene
```

```viscript-js
const scene = vis.scene({
    width: 960,
    height: 540,
});

const ball = scene.circle({id: "ball", center: [-2, 0], radius: 0.5, fill: "accent"});
const other = scene.rectangle({id: "other", center: [2, 0], size: [1, 1], stroke: "foreground"});
scene.look({target: ball, height: 2.5}, 1);
scene.look({target: other, height: 2.5}, 1);

return scene;
```

### scene.orbit

<!-- api-example: scene.orbit -->

```viscript
local scene = vis.scene {
    width = 960,
    height = 540,
    camera = {view = "3d", eye = {6, 4, 7}, target = {0, 0, 0}},
}

local axes = scene:space {x = {-2, 2, 1}, y = {-2, 2, 1}, z = {-2, 2, 1}, stroke = "border"}
axes:vector {value = {1.5, 1, 1}, stroke = "accent", width = 3}
scene:orbit(math.pi, {duration = 2, curve = "linear"})

return scene
```

```viscript-js
const scene = vis.scene({
    width: 960,
    height: 540,
    camera: {view: "3d", eye: [6, 4, 7], target: [0, 0, 0]},
});

const axes = scene.space({x: [-2, 2, 1], y: [-2, 2, 1], z: [-2, 2, 1], stroke: "border"});
axes.vector({value: [1.5, 1, 1], stroke: "accent", width: 3});
scene.orbit(Math.PI, {duration: 2, curve: "linear"});

return scene;
```

### scene.focus

<!-- api-example: scene.focus -->

```viscript
local scene = vis.scene {
    width = 960,
    height = 540,
}

local ball = scene:circle {id = "ball", center = {-2, 0}, radius = 0.5, fill = "accent"}
local other = scene:rectangle {id = "other", center = {2, 0}, size = {1, 1}, stroke = "foreground"}
scene:focus(ball, {padding = 0.5, duration = 1})
scene:focus(nil, {duration = 1})

return scene
```

```viscript-js
const scene = vis.scene({
    width: 960,
    height: 540,
});

const ball = scene.circle({id: "ball", center: [-2, 0], radius: 0.5, fill: "accent"});
const other = scene.rectangle({id: "other", center: [2, 0], size: [1, 1], stroke: "foreground"});
scene.focus(ball, {padding: 0.5, duration: 1});
scene.focus(null, {duration: 1});

return scene;
```
