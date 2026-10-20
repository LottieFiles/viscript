---
title: Placement & Parenting
id: object-placement
order: 13
parent: object
---

# Placement & Parenting

Place an object relative to a point or another object, then inspect its children. Group and Space inherit these methods. Layout changes run before the first timeline animation.

## Place objects

| Lua | JavaScript | Purpose |
| --- | --- | --- |
| `object:move_to(point)` | `object.moveTo(point)` | Place the object's bounds center at `point`. |
| `object:next_to(target, direction, gap?)` | `object.nextTo(target, direction, gap?)` | Place beside `target` in `direction`; default gap is 0.25 world units. |
| `object:align_to(target, direction)` | `object.alignTo(target, direction)` | Align the edge facing `direction` with the matching target edge. |

## Inspect children

| Lua | JavaScript | Purpose |
| --- | --- | --- |
| `object:children()` | `object.children()` | Return direct child handles in creation order. |

Calling a factory on an object makes the new object its child. Use [Group Layout & Selection](../Group/group-layout.md) to arrange several children together, or [Text & Numbers](./text-numbers.md) to add a semantic label.

## Examples

### object.move_to

<!-- api-example: object.move_to -->

```viscript
local scene = vis.scene {
    width = 960,
    height = 540,
}

scene:point {point = {1.5, 0.5}, radius = 4, color = "foreground"}
local circle = scene:circle {center = {-2, 0}, radius = 0.7, stroke = "accent", width = 2}
circle:move_to({1.5, 0.5})
scene:text {id = "caption", text = "Place the bounds center at (1.5, 0.5)", point = {0, -2.25}, size = 16, fill = "muted"}

return scene
```

```viscript-js
const scene = vis.scene({
    width: 960,
    height: 540,
});

scene.point({point: [1.5, 0.5], radius: 4, color: "foreground"});
const circle = scene.circle({center: [-2, 0], radius: 0.7, stroke: "accent", width: 2});
circle.moveTo([1.5, 0.5]);
scene.text({id: "caption", text: "Place the bounds center at (1.5, 0.5)", point: [0, -2.25], size: 16, fill: "muted"});

return scene;
```

### object.next_to

<!-- api-example: object.next_to -->

```viscript
local scene = vis.scene {
    width = 960,
    height = 540,
}

local box = scene:rectangle {center = {-1, 0}, size = {1.5, 1.5}, stroke = "foreground"}
local circle = scene:circle {radius = 0.7, stroke = "accent", width = 2}
circle:next_to(box, {1, 0}, 0.4)
scene:text {id = "caption", text = "Place beside the rectangle with a 0.4-unit gap", point = {0, -2.25}, size = 16, fill = "muted"}

return scene
```

```viscript-js
const scene = vis.scene({
    width: 960,
    height: 540,
});

const box = scene.rectangle({center: [-1, 0], size: [1.5, 1.5], stroke: "foreground"});
const circle = scene.circle({radius: 0.7, stroke: "accent", width: 2});
circle.nextTo(box, [1, 0], 0.4);
scene.text({id: "caption", text: "Place beside the rectangle with a 0.4-unit gap", point: [0, -2.25], size: 16, fill: "muted"});

return scene;
```

### object.align_to

<!-- api-example: object.align_to -->

```viscript
local scene = vis.scene {
    width = 960,
    height = 540,
}

local a = scene:rectangle {center = {-1.3, 0.2}, size = {1.2, 2}, stroke = "foreground"}
local b = scene:rectangle {center = {1.3, 0}, size = {1.2, 0.8}, stroke = "accent", width = 2}
b:align_to(a, {0, 1})
scene:line {from = {-2.3, 1.2}, to = {2.3, 1.2}, stroke = "muted", dash = {4, 4}}
scene:text {id = "caption", text = "Align the upper edges", point = {0, -2.25}, size = 16, fill = "muted"}

return scene
```

```viscript-js
const scene = vis.scene({
    width: 960,
    height: 540,
});

const a = scene.rectangle({center: [-1.3, 0.2], size: [1.2, 2], stroke: "foreground"});
const b = scene.rectangle({center: [1.3, 0], size: [1.2, 0.8], stroke: "accent", width: 2});
b.alignTo(a, [0, 1]);
scene.line({from: [-2.3, 1.2], to: [2.3, 1.2], stroke: "muted", dash: [4, 4]});
scene.text({id: "caption", text: "Align the upper edges", point: [0, -2.25], size: 16, fill: "muted"});

return scene;
```

### object.children

<!-- api-example: object.children -->

```viscript
local scene = vis.scene {
    width = 960,
    height = 540,
}

local group = scene:group {}
group:circle {center = {-1, 0}, radius = 0.6, stroke = "foreground"}
group:circle {center = {1, 0}, radius = 0.6, stroke = "foreground"}
local children = group:children()
scene:set {target = children[2], stroke = "accent", width = 3}
scene:text {id = "count", text = #children .. " direct children", point = {0, 1.3}, size = 18, fill = "foreground"}
scene:text {id = "caption", text = "Creation order selects the second child", point = {0, -2.25}, size = 16, fill = "muted"}

return scene
```

```viscript-js
const scene = vis.scene({
    width: 960,
    height: 540,
});

const group = scene.group();
group.circle({center: [-1, 0], radius: 0.6, stroke: "foreground"});
group.circle({center: [1, 0], radius: 0.6, stroke: "foreground"});
const children = group.children();
scene.set({target: children[1], stroke: "accent", width: 3});
scene.text({id: "count", text: children.length + " direct children", point: [0, 1.3], size: 18, fill: "foreground"});
scene.text({id: "caption", text: "Creation order selects the second child", point: [0, -2.25], size: 16, fill: "muted"});

return scene;
```
