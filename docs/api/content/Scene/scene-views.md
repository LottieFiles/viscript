---
title: Views
id: scene-views
order: 6
parent: scene
---

# Views

Show the same scene through an additional camera, then animate that view independently. A view shares the objects and timeline of its scene.

## Create a view

| Lua | JavaScript | Purpose |
| --- | --- | --- |
| `scene:view(bounds, options?)` | `scene.view(bounds, options?)` | Return a second view of this scene through its own camera; up to 8 views per scene. |

`bounds` uses normalized frame fields `x`, `y`, `width`, `height`. A view's options are `camera`, `optical` (true), `background`, `opacity` (0–1), `layer`, `hide` (one handle or up to 256 handles), `follow` (a handle whose drawn center the camera follows).

## Animate a view

Use the handle returned by `scene:view` / `scene.view`.

| Lua | JavaScript | Purpose |
| --- | --- | --- |
| `view:look(camera, options?)` | `view.look(camera, options?)` | Animate this view's camera with the scene camera's arguments. |
| `view:orbit(radians, options?)` | `view.orbit(radians, options?)` | Orbit this view's 3D camera; options: `axis`, `duration`, `curve`. |
| `view:focus(targets?, options?)` | `view.focus(targets?, options?)` | Fit targets inside this view, or restore its opening camera; options: `padding`, `duration`, `curve`. |
| `view:fade(opacity, options?)` | `view.fade(opacity, options?)` | Fade the whole view to opacity 0–1; default duration 1 and curve `smooth`. |

## Examples

### scene.view

<!-- api-example: scene.view -->

```viscript
local scene = vis.scene {
    width = 960,
    height = 540,
}

local ball = scene:circle {id = "ball", center = {-2, 0}, radius = 0.5, fill = "accent"}
local other = scene:rectangle {id = "other", center = {2, 0}, size = {1, 1}, stroke = "foreground"}
local view = scene:view({x = 0.55, y = 0.05, width = 0.4, height = 0.4}, {
    camera = {target = {-2, 0}, height = 2.5}, background = "surface", layer = 20,
})
scene:shift(ball, {4, 0}, {duration = 1.5})

return scene
```

```viscript-js
const scene = vis.scene({
    width: 960,
    height: 540,
});

const ball = scene.circle({id: "ball", center: [-2, 0], radius: 0.5, fill: "accent"});
const other = scene.rectangle({id: "other", center: [2, 0], size: [1, 1], stroke: "foreground"});
const view = scene.view({x: 0.55, y: 0.05, width: 0.4, height: 0.4}, {
    camera: {target: [-2, 0], height: 2.5}, background: "surface", layer: 20,
});
scene.shift(ball, [4, 0], {duration: 1.5});

return scene;
```

### view.look

<!-- api-example: view.look -->

```viscript
local scene = vis.scene {
    width = 960,
    height = 540,
}

local ball = scene:circle {id = "ball", center = {-2, 0}, radius = 0.5, fill = "accent"}
local other = scene:rectangle {id = "other", center = {2, 0}, size = {1, 1}, stroke = "foreground"}
local view = scene:view({x = 0.55, y = 0.05, width = 0.4, height = 0.4}, {
    camera = {target = {-2, 0}, height = 2.5}, background = "surface", layer = 20,
})
view:look({target = other}, {duration = 1.5})

return scene
```

```viscript-js
const scene = vis.scene({
    width: 960,
    height: 540,
});

const ball = scene.circle({id: "ball", center: [-2, 0], radius: 0.5, fill: "accent"});
const other = scene.rectangle({id: "other", center: [2, 0], size: [1, 1], stroke: "foreground"});
const view = scene.view({x: 0.55, y: 0.05, width: 0.4, height: 0.4}, {
    camera: {target: [-2, 0], height: 2.5}, background: "surface", layer: 20,
});
view.look({target: other}, {duration: 1.5});

return scene;
```

### view.orbit

<!-- api-example: view.orbit -->

```viscript
local scene = vis.scene {
    width = 960,
    height = 540,
    camera = {view = "3d", eye = {6, 4, 7}, target = {0, 0, 0}},
}

local axes = scene:space {x = {-2, 2, 1}, y = {-2, 2, 1}, z = {-2, 2, 1}, stroke = "border"}
axes:vector {value = {1.5, 1, 1}, stroke = "accent", width = 3}
local view = scene:view({x = 0.55, y = 0.05, width = 0.4, height = 0.5}, {
    background = "surface", layer = 20,
    camera = {view = "3d", eye = {6, 4, 7}, target = {0, 0, 0}},
})
view:orbit(math.pi, {duration = 2, curve = "linear"})

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
const view = scene.view({x: 0.55, y: 0.05, width: 0.4, height: 0.5}, {
    background: "surface", layer: 20,
    camera: {view: "3d", eye: [6, 4, 7], target: [0, 0, 0]},
});
view.orbit(Math.PI, {duration: 2, curve: "linear"});

return scene;
```

### view.focus

<!-- api-example: view.focus -->

```viscript
local scene = vis.scene {
    width = 960,
    height = 540,
}

local ball = scene:circle {id = "ball", center = {-2, 0}, radius = 0.5, fill = "accent"}
local other = scene:rectangle {id = "other", center = {2, 0}, size = {1, 1}, stroke = "foreground"}
local view = scene:view({x = 0.55, y = 0.05, width = 0.4, height = 0.4}, {
    camera = {target = {-2, 0}, height = 2.5}, background = "surface", layer = 20,
})
view:focus(other, {padding = 0.5, duration = 1})
view:focus(nil, {duration = 1})

return scene
```

```viscript-js
const scene = vis.scene({
    width: 960,
    height: 540,
});

const ball = scene.circle({id: "ball", center: [-2, 0], radius: 0.5, fill: "accent"});
const other = scene.rectangle({id: "other", center: [2, 0], size: [1, 1], stroke: "foreground"});
const view = scene.view({x: 0.55, y: 0.05, width: 0.4, height: 0.4}, {
    camera: {target: [-2, 0], height: 2.5}, background: "surface", layer: 20,
});
view.focus(other, {padding: 0.5, duration: 1});
view.focus(null, {duration: 1});

return scene;
```

### view.fade

<!-- api-example: view.fade -->

```viscript
local scene = vis.scene {
    width = 960,
    height = 540,
}

local ball = scene:circle {id = "ball", center = {-2, 0}, radius = 0.5, fill = "accent"}
local other = scene:rectangle {id = "other", center = {2, 0}, size = {1, 1}, stroke = "foreground"}
local view = scene:view({x = 0.55, y = 0.05, width = 0.4, height = 0.4}, {
    camera = {target = {-2, 0}, height = 2.5}, background = "surface", layer = 20,
})
view:fade(0, {duration = 1})
view:fade(1, {duration = 1})

return scene
```

```viscript-js
const scene = vis.scene({
    width: 960,
    height: 540,
});

const ball = scene.circle({id: "ball", center: [-2, 0], radius: 0.5, fill: "accent"});
const other = scene.rectangle({id: "other", center: [2, 0], size: [1, 1], stroke: "foreground"});
const view = scene.view({x: 0.55, y: 0.05, width: 0.4, height: 0.4}, {
    camera: {target: [-2, 0], height: 2.5}, background: "surface", layer: 20,
});
view.fade(0, {duration: 1});
view.fade(1, {duration: 1});

return scene;
```
