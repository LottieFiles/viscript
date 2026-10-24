---
title: Viewports & transitions
id: scene-viewports
order: 5
parent: scene
---

# Viewports & transitions

Attach completed child scenes in a frame, or sequence stages with transitions. Both operations transfer and seal their child scenes.

## Compose completed scenes

| Lua | JavaScript | Purpose |
| --- | --- | --- |
| `scene:viewport(child, bounds)` | `scene.viewport(child, bounds)` | Mount a completed child scene in a frame rectangle; transfer and seal the child. |
| `scene:scene_transition(stages, options?)` | `scene.sceneTransition(stages, options?)` | Sequence completed scenes with animated transitions; transfer and seal every stage. |

`bounds` uses normalized frame fields `x`, `y`, `width`, `height`.

## Transition options

Transition options: `duration`, `curve`, `hold` (one duration or one per stage), `live`, `viewport`, and either `styles` (one per transition) or a single style configuration. Style fields: `style` (`match`, `fade`, `slide`, `zoom_in`, `zoom_out`), `from` (slide side), `area` (zoom id or normalized rectangle), `area_margin` (0.05), `match`. `live` plays stage timelines; otherwise transitions show their final frames.

## Examples

### scene.viewport

<!-- api-example: scene.viewport -->

```viscript
local scene = vis.scene {
    width = 960,
    height = 540,
}

scene:rectangle {id = "frame", center = {0, 0}, size = {5.3, 3}, stroke = "border"}
local child = vis.scene {width = 480, height = 270, camera = {height = 3}}
local ball = child:circle {id = "child-ball", center = {-1, 0}, radius = 0.4, fill = "accent"}
child:shift(ball, {2, 0}, {duration = 1.5})
scene:viewport(child, {x = 0.25, y = 0.25, width = 0.5, height = 0.5})

return scene
```

```viscript-js
const scene = vis.scene({
    width: 960,
    height: 540,
});

scene.rectangle({id: "frame", center: [0, 0], size: [5.3, 3], stroke: "border"});
const child = vis.scene({width: 480, height: 270, camera: {height: 3}});
const ball = child.circle({id: "child-ball", center: [-1, 0], radius: 0.4, fill: "accent"});
child.shift(ball, [2, 0], {duration: 1.5});
scene.viewport(child, {x: 0.25, y: 0.25, width: 0.5, height: 0.5});

return scene;
```

### scene.scene_transition

<!-- api-example: scene.scene_transition -->

```viscript
local scene = vis.scene {
    width = 960,
    height = 540,
}

local first = vis.scene {width = 960, height = 540}
first:circle {id = "shape", radius = 0.8, fill = "accent"}
local second = vis.scene {width = 960, height = 540}
second:rectangle {id = "shape", size = {1.6, 1.6}, fill = "success"}
scene:scene_transition({first, second}, {style = "slide", from = "right", duration = 1, hold = 0.6})

return scene
```

```viscript-js
const scene = vis.scene({
    width: 960,
    height: 540,
});

const first = vis.scene({width: 960, height: 540});
first.circle({id: "shape", radius: 0.8, fill: "accent"});
const second = vis.scene({width: 960, height: 540});
second.rectangle({id: "shape", size: [1.6, 1.6], fill: "success"});
scene.sceneTransition([first, second], {style: "slide", from: "right", duration: 1, hold: 0.6});

return scene;
```
