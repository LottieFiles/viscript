---
title: Vectors, Matrices & Colors
id: shared-values
order: 5
parent: utilities
---

# Vectors, Matrices & Colors

Vectors, ranges, transforms and colors are data passed to the APIs. The example applies a transform matrix to an object.

| Value | Lua | JavaScript |
| --- | --- | --- |
| 2D vector | `{x, y}` or `{x = x, y = y}` | `[x, y]` or `{x, y}` |
| 3D vector | `{x, y, z}` or `{x = x, y = y, z = z}` | `[x, y, z]` or `{x, y, z}` |
| Range | `{min, max, step}` or named fields | `[min, max, step]` or `{min, max, step}` |
| Transform matrix | A 16-number table, row by row | A 16-number array, row by row |
| Color | Hex string or Theme role | Hex string or Theme role |

Transform matrices multiply column vectors; translation occupies entries 4, 8 and 12 in Lua, or indices 3, 7 and 11 in JavaScript. Matrix constructors and multiplication helpers are not exposed in these bindings; use arrays/tables, Group `matrix`, or Scene `transform`. The `matrix` Object factory creates displayed mathematical entries.

See [Movement](../Animation/movement.md) for transform operations.

## Examples

### play · transform

<!-- api-example: property.transform -->

```viscript
local scene = vis.scene {
    width = 960,
    height = 540,
}

scene:text {id = "caption", text = "play: transform", point = {0, 2.2}, size = 20, font = "JetBrainsMono", fill = "muted"}
local subject = scene:rectangle {id = "subject", size = {2.6, 1.5}, stroke = "accent", fill = "accent", width = 3}
scene:play({target = subject, transform = {0,-1,0,1, 1,0,0,0, 0,0,1,0, 0,0,0,1}}, 1.4)

return scene
```

```viscript-js
const scene = vis.scene({
    width: 960,
    height: 540,
});

scene.text({id: "caption", text: "play: transform", point: [0, 2.2], size: 20, font: "JetBrainsMono", fill: "muted"});
const subject = scene.rectangle({id: "subject", size: [2.6, 1.5], stroke: "accent", fill: "accent", width: 3});
scene.play({target: subject, transform: [0,-1,0,1, 1,0,0,0, 0,0,1,0, 0,0,0,1]}, 1.4);

return scene;
```
