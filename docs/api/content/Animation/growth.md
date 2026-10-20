---
title: Growth
id: growth
order: 3
parent: animation
---

# Growth

Grow a shape from its center or an edge, then shrink it back into its center. These helpers share the [standard duration and curve](./index.md).

## Grow and shrink

| Lua | JavaScript | Purpose |
| --- | --- | --- |
| `scene:grow_from_center(target, duration?, curve?)`<br />`scene:grow_from_center(target, timing)` | `scene.growFromCenter(target, duration?, curve?)`<br />`scene.growFromCenter(target, timing)` | Grow the object from its bounds center. |
| `scene:grow_from_edge(target, edge, duration?, curve?)`<br />`scene:grow_from_edge(target, edge, timing)` | `scene.growFromEdge(target, edge, duration?, curve?)`<br />`scene.growFromEdge(target, edge, timing)` | Grow from `left`, `right`, `bottom`, or `top`. |
| `scene:shrink_to_center(target, duration?, curve?)`<br />`scene:shrink_to_center(target, timing)` | `scene.shrinkToCenter(target, duration?, curve?)`<br />`scene.shrinkToCenter(target, timing)` | Shrink the object into its bounds center. |

## Examples

### Grow & shrink

<!-- api-example: group.growth -->
<!-- api-covers: scene.grow_from_center scene.shrink_to_center -->

```viscript
local scene = vis.scene {
    width = 960,
    height = 540,
}

local circle = scene:circle {id = "circle", radius = 0.9, fill = "accent"}
scene:grow_from_center(circle, 1)
scene:shrink_to_center(circle, 1)

return scene
```

```viscript-js
const scene = vis.scene({
    width: 960,
    height: 540,
});

const circle = scene.circle({id: "circle", radius: 0.9, fill: "accent"});
scene.growFromCenter(circle, 1);
scene.shrinkToCenter(circle, 1);

return scene;
```

### scene.grow_from_edge

<!-- api-example: scene.grow_from_edge -->

```viscript
local scene = vis.scene {
    width = 960,
    height = 540,
}

scene:text {id = "caption", text = "grow_from_edge", point = {0, 2.2}, size = 20, font = "JetBrainsMono", fill = "muted"}
local subject = scene:rectangle {id = "subject", size = {2.6, 1.5}, stroke = "accent", fill = "accent", width = 3}
scene:grow_from_edge(subject, "left", 1.2)

return scene
```

```viscript-js
const scene = vis.scene({
    width: 960,
    height: 540,
});

scene.text({id: "caption", text: "grow_from_edge", point: [0, 2.2], size: 20, font: "JetBrainsMono", fill: "muted"});
const subject = scene.rectangle({id: "subject", size: [2.6, 1.5], stroke: "accent", fill: "accent", width: 3});
scene.growFromEdge(subject, "left", 1.2);

return scene;
```
