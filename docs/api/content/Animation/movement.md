---
title: Movement
id: movement
order: 4
parent: animation
---

# Movement

Move, rotate, scale or arrange existing objects. Use descriptor fields when combining movement with other property changes in one `tween` call.

## Transform and arrange

| Lua | JavaScript | Purpose |
| --- | --- | --- |
| `scene:shift(target, by, duration?, curve?)`<br />`scene:shift(target, by, timing)` | `scene.shift(target, by, duration?, curve?)`<br />`scene.shift(target, by, timing)` | Translate by a 2D/3D vector in the parent's space. |
| `scene:transform(target, matrix, duration?, curve?)`<br />`scene:transform(target, matrix, timing)` | `scene.transform(target, matrix, duration?, curve?)`<br />`scene.transform(target, matrix, timing)` | Animate to a 16-number transform matrix. |
| `scene:rotate(targets, radians, options?)` | `scene.rotate(targets, radians, options?)` | Rotate around `about`; options: `axis`, `about`, `shift`, `arc`, `duration`, `curve`, `lag`. |
| `scene:scale(targets, factor, options?)` | `scene.scale(targets, factor, options?)` | Scale uniformly or per axis; options: `about`, `shift`, `arc`, `duration`, `curve`, `lag`. |
| `scene:arrange(items, duration?, curve?, options?)`<br />`scene:arrange(items, options)` | `scene.arrange(items, duration?, curve?, options?)`<br />`scene.arrange(items, options)` | Animate a group's children or sibling list into a row; options: `direction` ([1, 0]), `gap` (0.25), `center`, `duration`, `curve`. |

`about` accepts a point in each target's parent space, a handle, or a bounds anchor: `center`, `left`, `right`, `top`, `bottom`, `top_left`, `top_right`, `bottom_left`, `bottom_right`. Timing follows the [shared rules](./index.md).

## Movement descriptors

| Lua | JavaScript | Purpose |
| --- | --- | --- |
| `shift` | `shift` | Relative movement in the target's parent space. |
| `transform` | `transform` | Absolute 16-number transform matrix. |
| `position` | `position` | Destination of the bounds center in parent space. |
| `arc` | `arc` | Curved travel angle, used with `shift`, `transform`, or `position`. |

Choose only one of `shift`, `transform`, and `position`. `set` accepts the same fields except `arc`. For path following or geometry deformation, see [Path & Field Motion](./path-motion.md).

## Examples

### scene.shift

<!-- api-example: scene.shift -->

```viscript
local scene = vis.scene {
    width = 960,
    height = 540,
}

scene:text {id = "caption", text = "shift", point = {0, 2.2}, size = 20, font = "JetBrainsMono", fill = "muted"}
local subject = scene:rectangle {id = "subject", size = {2.6, 1.5}, stroke = "accent", fill = "accent", width = 3}
scene:shift(subject, {2, 0}, 1.2)

return scene
```

```viscript-js
const scene = vis.scene({
    width: 960,
    height: 540,
});

scene.text({id: "caption", text: "shift", point: [0, 2.2], size: 20, font: "JetBrainsMono", fill: "muted"});
const subject = scene.rectangle({id: "subject", size: [2.6, 1.5], stroke: "accent", fill: "accent", width: 3});
scene.shift(subject, [2, 0], 1.2);

return scene;
```

### scene.transform

<!-- api-example: scene.transform -->

```viscript
local scene = vis.scene {
    width = 960,
    height = 540,
}

scene:text {id = "caption", text = "transform", point = {0, 2.2}, size = 20, font = "JetBrainsMono", fill = "muted"}
local subject = scene:rectangle {id = "subject", size = {2.6, 1.5}, stroke = "accent", fill = "accent", width = 3}
scene:transform(subject, {0,-1,0,1, 1,0,0,0, 0,0,1,0, 0,0,0,1}, 1.2)

return scene
```

```viscript-js
const scene = vis.scene({
    width: 960,
    height: 540,
});

scene.text({id: "caption", text: "transform", point: [0, 2.2], size: 20, font: "JetBrainsMono", fill: "muted"});
const subject = scene.rectangle({id: "subject", size: [2.6, 1.5], stroke: "accent", fill: "accent", width: 3});
scene.transform(subject, [0,-1,0,1, 1,0,0,0, 0,0,1,0, 0,0,0,1], 1.2);

return scene;
```


### scene.rotate

<!-- api-example: scene.rotate -->

```viscript
local scene = vis.scene {
    width = 960,
    height = 540,
}

scene:text {id = "caption", text = "rotate", point = {0, 2.2}, size = 20, font = "JetBrainsMono", fill = "muted"}
local subject = scene:arrow {id = "subject", from = {-2, 0}, to = {2, 0}, stroke = "accent", width = 3, tip = 10}
scene:rotate(subject, math.pi / 2, {about = {0, 0}, duration = 1.2})

return scene
```

```viscript-js
const scene = vis.scene({
    width: 960,
    height: 540,
});

scene.text({id: "caption", text: "rotate", point: [0, 2.2], size: 20, font: "JetBrainsMono", fill: "muted"});
const subject = scene.arrow({id: "subject", from: [-2, 0], to: [2, 0], stroke: "accent", width: 3, tip: 10});
scene.rotate(subject, Math.PI / 2, {about: [0, 0], duration: 1.2});

return scene;
```

### scene.scale

<!-- api-example: scene.scale -->

```viscript
local scene = vis.scene {
    width = 960,
    height = 540,
}

scene:text {id = "caption", text = "scale", point = {0, 2.2}, size = 20, font = "JetBrainsMono", fill = "muted"}
local subject = scene:rectangle {id = "subject", size = {2.6, 1.5}, stroke = "accent", fill = "accent", width = 3}
scene:scale(subject, {1.5, 0.6}, {duration = 1.2})

return scene
```

```viscript-js
const scene = vis.scene({
    width: 960,
    height: 540,
});

scene.text({id: "caption", text: "scale", point: [0, 2.2], size: 20, font: "JetBrainsMono", fill: "muted"});
const subject = scene.rectangle({id: "subject", size: [2.6, 1.5], stroke: "accent", fill: "accent", width: 3});
scene.scale(subject, [1.5, 0.6], {duration: 1.2});

return scene;
```

### scene.arrange

<!-- api-example: scene.arrange -->

```viscript
local scene = vis.scene {
    width = 960,
    height = 540,
}

scene:text {id = "caption", text = "arrange", point = {0, 2.2}, size = 20, font = "JetBrainsMono", fill = "muted"}
local items = {}
for i = 1, 3 do
    items[i] = scene:rectangle {id = "item" .. i, center = {i - 2, (i % 2) * 1.2 - 0.6}, size = {0.8, 0.8}, fill = "accent"}
end
scene:arrange(items, {gap = 0.5, center = {0, 0}, duration = 1.2})

return scene
```

```viscript-js
const scene = vis.scene({
    width: 960,
    height: 540,
});

scene.text({id: "caption", text: "arrange", point: [0, 2.2], size: 20, font: "JetBrainsMono", fill: "muted"});
const items = Array.from({length: 3}, (_, n) => {
    const i = n + 1;
    return scene.rectangle({id: "item" + i, center: [i - 2, (i % 2) * 1.2 - 0.6], size: [0.8, 0.8], fill: "accent"});
});
scene.arrange(items, {gap: 0.5, center: [0, 0], duration: 1.2});

return scene;
```

### tween · shift

<!-- api-example: property.shift -->

```viscript
local scene = vis.scene {
    width = 960,
    height = 540,
}

scene:text {id = "caption", text = "tween: shift", point = {0, 2.2}, size = 20, font = "JetBrainsMono", fill = "muted"}
local subject = scene:rectangle {id = "subject", size = {2.6, 1.5}, stroke = "accent", fill = "accent", width = 3}
scene:tween({target = subject, shift = {2, 0}}, 1.4)

return scene
```

```viscript-js
const scene = vis.scene({
    width: 960,
    height: 540,
});

scene.text({id: "caption", text: "tween: shift", point: [0, 2.2], size: 20, font: "JetBrainsMono", fill: "muted"});
const subject = scene.rectangle({id: "subject", size: [2.6, 1.5], stroke: "accent", fill: "accent", width: 3});
scene.tween({target: subject, shift: [2, 0]}, 1.4);

return scene;
```

### tween · transform

<!-- api-example: property.transform -->

```viscript
local scene = vis.scene {
    width = 960,
    height = 540,
}

scene:text {id = "caption", text = "tween: transform", point = {0, 2.2}, size = 20, font = "JetBrainsMono", fill = "muted"}
local subject = scene:rectangle {id = "subject", size = {2.6, 1.5}, stroke = "accent", fill = "accent", width = 3}
scene:tween({target = subject, transform = {0,-1,0,1, 1,0,0,0, 0,0,1,0, 0,0,0,1}}, 1.4)

return scene
```

```viscript-js
const scene = vis.scene({
    width: 960,
    height: 540,
});

scene.text({id: "caption", text: "tween: transform", point: [0, 2.2], size: 20, font: "JetBrainsMono", fill: "muted"});
const subject = scene.rectangle({id: "subject", size: [2.6, 1.5], stroke: "accent", fill: "accent", width: 3});
scene.tween({target: subject, transform: [0,-1,0,1, 1,0,0,0, 0,0,1,0, 0,0,0,1]}, 1.4);

return scene;
```

### tween · position

<!-- api-example: property.position -->

```viscript
local scene = vis.scene {
    width = 960,
    height = 540,
}

scene:text {id = "caption", text = "tween: position", point = {0, 2.2}, size = 20, font = "JetBrainsMono", fill = "muted"}
local subject = scene:rectangle {id = "subject", size = {2.6, 1.5}, stroke = "accent", fill = "accent", width = 3}
scene:tween({target = subject, position = {2, 0}}, 1.4)

return scene
```

```viscript-js
const scene = vis.scene({
    width: 960,
    height: 540,
});

scene.text({id: "caption", text: "tween: position", point: [0, 2.2], size: 20, font: "JetBrainsMono", fill: "muted"});
const subject = scene.rectangle({id: "subject", size: [2.6, 1.5], stroke: "accent", fill: "accent", width: 3});
scene.tween({target: subject, position: [2, 0]}, 1.4);

return scene;
```

### tween · arc

<!-- api-example: property.arc -->

```viscript
local scene = vis.scene {
    width = 960,
    height = 540,
}

scene:text {id = "caption", text = "tween: arc", point = {0, 2.2}, size = 20, font = "JetBrainsMono", fill = "muted"}
local subject = scene:circle {id = "subject", center = {-2, 0}, radius = 0.3, fill = "accent"}
scene:tween({target = subject, position = {2, 0}, arc = math.pi / 2}, 1.4)

return scene
```

```viscript-js
const scene = vis.scene({
    width: 960,
    height: 540,
});

scene.text({id: "caption", text: "tween: arc", point: [0, 2.2], size: 20, font: "JetBrainsMono", fill: "muted"});
const subject = scene.circle({id: "subject", center: [-2, 0], radius: 0.3, fill: "accent"});
scene.tween({target: subject, position: [2, 0], arc: Math.PI / 2}, 1.4);

return scene;
```
