---
title: Object lifecycle
id: scene-objects
order: 2
parent: scene
---

# Object lifecycle

Find, copy, remove, or measure existing objects. To create geometry and groups, see [Object](../Object/index.md).

## Query and manage objects

| Lua | JavaScript | Purpose |
| --- | --- | --- |
| `scene:find(id)` | `scene.find(id)` | Find an object by its authored id; return `nil` / `null` if absent. |
| `scene:clone(source, options?)` | `scene.clone(source, options?)` | Copy the source family at the cursor without its animation; return the copied handle. Lua also returns a map indexed by original handles. |
| `scene:remove(target)` | `scene.remove(target)` | End the object's visibility at the cursor; earlier frames retain it. |
| `scene:bounds(targets)` | — | Return authored world bounds: `left`, `right`, `bottom`, `top`, `width`, `height`, `center`, `min`, `max`. Accept one handle or a list. |

## Clone options

Clone options: `parent`, `shift`, `id`, and object styling fields. The default parent is the source's parent. Spaces, cells, hidden sources, active morphs/warps and masks crossing the copied family cannot be cloned.

## Examples

### scene.find

<!-- api-example: scene.find -->

```viscript
local scene = vis.scene {
    width = 960,
    height = 540,
}

local ball = scene:circle {id = "ball", center = {-2, 0}, radius = 0.5, fill = "accent"}
local found = scene:find("ball")
scene:indicate(found, {duration = 1})

return scene
```

```viscript-js
const scene = vis.scene({
    width: 960,
    height: 540,
});

const ball = scene.circle({id: "ball", center: [-2, 0], radius: 0.5, fill: "accent"});
const found = scene.find("ball");
scene.indicate(found, {duration: 1});

return scene;
```

### scene.clone

<!-- api-example: scene.clone -->

```viscript
local scene = vis.scene {
    width = 960,
    height = 540,
}

local ball = scene:circle {id = "ball", center = {-2, 0}, radius = 0.5, fill = "accent"}
local copy = scene:clone(ball, {id = "copy", shift = {4, 0}, fill = "success"})
scene:indicate(copy, {duration = 1})

return scene
```

```viscript-js
const scene = vis.scene({
    width: 960,
    height: 540,
});

const ball = scene.circle({id: "ball", center: [-2, 0], radius: 0.5, fill: "accent"});
const copy = scene.clone(ball, {id: "copy", shift: [4, 0], fill: "success"});
scene.indicate(copy, {duration: 1});

return scene;
```

### scene.remove

<!-- api-example: scene.remove -->

```viscript
local scene = vis.scene {
    width = 960,
    height = 540,
}

local ball = scene:circle {id = "ball", center = {-2, 0}, radius = 0.5, fill = "accent"}
local other = scene:rectangle {id = "other", center = {2, 0}, size = {1, 1}, stroke = "foreground"}
scene:wait(0.7)
scene:remove(ball)

return scene
```

```viscript-js
const scene = vis.scene({
    width: 960,
    height: 540,
});

const ball = scene.circle({id: "ball", center: [-2, 0], radius: 0.5, fill: "accent"});
const other = scene.rectangle({id: "other", center: [2, 0], size: [1, 1], stroke: "foreground"});
scene.wait(0.7);
scene.remove(ball);

return scene;
```

### scene.bounds

<!-- api-example: scene.bounds -->

```viscript
local scene = vis.scene {
    width = 960,
    height = 540,
}

local ball = scene:circle {id = "ball", center = {-2, 0}, radius = 0.5, fill = "accent"}
local other = scene:rectangle {id = "other", center = {2, 0}, size = {1, 1}, stroke = "foreground"}
local bounds = scene:bounds({ball, other})
scene:rectangle {id = "bounds", center = bounds.center,
    size = {bounds.width, bounds.height}, stroke = "success", width = 2}
scene:text {id = "width", text = string.format("width = %.1f", bounds.width), point = {0, -1.4}, size = 24}

return scene
```
