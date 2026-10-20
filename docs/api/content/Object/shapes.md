---
title: Shapes
id: shapes
order: 1
parent: object
---

# Shapes

Create dots, circles, rectangles, and closed polygons. Geometry uses world units; point radii use screen pixels.

| Lua | JavaScript | Purpose |
| --- | --- | --- |
| `scene:point(opts)` | `scene.point(opts)` | Draw a dot at `point`; `radius` is in pixels. |
| `scene:circle(opts)` | `scene.circle(opts?)` | Draw a circle using `center` and world-space `radius` (default 1). |
| `scene:rectangle(opts)` | `scene.rectangle(opts?)` | Draw a rectangle using `center`, `size`, and `corner`, or fit it `around` handles with `padding`. |
| `scene:polygon(opts)` | `scene.polygon(opts)` | Close and fill/stroke `points` (at least three). |

Factories are also available on Object, Group, and Space handles. See [Styling & Masks](./object-styling.md) for paint options.

The `tween` examples animate radius, size, corners and polygon points. Their fields are documented in [Shape Properties](../Animation/shape-properties.md).

## Examples

### scene.circle

<!-- api-example: scene.circle -->

```viscript
local scene = vis.scene {
    width = 960,
    height = 540,
}

scene:circle {center = {0, 0}, radius = 1.2, stroke = "accent", width = 2}
scene:text {id = "caption", text = "radius = 1.2 world units", point = {0, -2.25}, size = 16, fill = "muted"}

return scene
```

```viscript-js
const scene = vis.scene({
    width: 960,
    height: 540,
});

scene.circle({center: [0, 0], radius: 1.2, stroke: "accent", width: 2});
scene.text({id: "caption", text: "radius = 1.2 world units", point: [0, -2.25], size: 16, fill: "muted"});

return scene;
```

### scene.point

<!-- api-example: scene.point -->

```viscript
local scene = vis.scene {
    width = 960,
    height = 540,
}

scene:point {point = {0.6, 0.4}, radius = 8, color = "accent"}
scene:text {id = "caption", text = "point = (0.6, 0.4); radius = 8 px", point = {0, -2.25}, size = 16, fill = "muted"}

return scene
```

```viscript-js
const scene = vis.scene({
    width: 960,
    height: 540,
});

scene.point({point: [0.6, 0.4], radius: 8, color: "accent"});
scene.text({id: "caption", text: "point = (0.6, 0.4); radius = 8 px", point: [0, -2.25], size: 16, fill: "muted"});

return scene;
```

### scene.rectangle

<!-- api-example: scene.rectangle -->

```viscript
local scene = vis.scene {
    width = 960,
    height = 540,
}

scene:rectangle {center = {0, 0}, size = {3.2, 1.5}, stroke = "accent", width = 2}
scene:text {id = "caption", text = "size = (3.2, 1.5)", point = {0, -2.25}, size = 16, fill = "muted"}

return scene
```

```viscript-js
const scene = vis.scene({
    width: 960,
    height: 540,
});

scene.rectangle({center: [0, 0], size: [3.2, 1.5], stroke: "accent", width: 2});
scene.text({id: "caption", text: "size = (3.2, 1.5)", point: [0, -2.25], size: 16, fill: "muted"});

return scene;
```

### scene.polygon

<!-- api-example: scene.polygon -->

```viscript
local scene = vis.scene {
    width = 960,
    height = 540,
}

scene:polygon {points = {{-1.5, -1}, {1.5, -1}, {0.8, 1.2}, {-0.6, 0.7}}, stroke = "accent", fill = "#6cb6ff22", width = 2}
scene:text {id = "caption", text = "A closed polygon through four points", point = {0, -2.25}, size = 16, fill = "muted"}

return scene
```

```viscript-js
const scene = vis.scene({
    width: 960,
    height: 540,
});

scene.polygon({points: [[-1.5, -1], [1.5, -1], [0.8, 1.2], [-0.6, 0.7]], stroke: "accent", fill: "#6cb6ff22", width: 2});
scene.text({id: "caption", text: "A closed polygon through four points", point: [0, -2.25], size: 16, fill: "muted"});

return scene;
```

### tween · radius

<!-- api-example: property.radius -->

```viscript
local scene = vis.scene {
    width = 960,
    height = 540,
}

scene:text {id = "caption", text = "tween: radius", point = {0, 2.2}, size = 20, font = "JetBrainsMono", fill = "muted"}
local subject = scene:circle {id = "subject", radius = 1.1, stroke = "accent", fill = "#00000000", width = 3}
scene:tween({target = subject, radius = 1.8}, 1.4)

return scene
```

```viscript-js
const scene = vis.scene({
    width: 960,
    height: 540,
});

scene.text({id: "caption", text: "tween: radius", point: [0, 2.2], size: 20, font: "JetBrainsMono", fill: "muted"});
const subject = scene.circle({id: "subject", radius: 1.1, stroke: "accent", fill: "#00000000", width: 3});
scene.tween({target: subject, radius: 1.8}, 1.4);

return scene;
```

### tween · size

<!-- api-example: property.size -->

```viscript
local scene = vis.scene {
    width = 960,
    height = 540,
}

scene:text {id = "caption", text = "tween: size", point = {0, 2.2}, size = 20, font = "JetBrainsMono", fill = "muted"}
local subject = scene:rectangle {id = "subject", size = {2.6, 1.5}, stroke = "accent", fill = "accent", width = 3}
scene:tween({target = subject, size = {4, 0.6}}, 1.4)

return scene
```

```viscript-js
const scene = vis.scene({
    width: 960,
    height: 540,
});

scene.text({id: "caption", text: "tween: size", point: [0, 2.2], size: 20, font: "JetBrainsMono", fill: "muted"});
const subject = scene.rectangle({id: "subject", size: [2.6, 1.5], stroke: "accent", fill: "accent", width: 3});
scene.tween({target: subject, size: [4, 0.6]}, 1.4);

return scene;
```

### tween · corner

<!-- api-example: property.corner -->

```viscript
local scene = vis.scene {
    width = 960,
    height = 540,
}

scene:text {id = "caption", text = "tween: corner", point = {0, 2.2}, size = 20, font = "JetBrainsMono", fill = "muted"}
local subject = scene:rectangle {id = "subject", size = {2.6, 1.5}, stroke = "accent", fill = "accent", width = 3}
scene:tween({target = subject, corner = 0.7}, 1.4)

return scene
```

```viscript-js
const scene = vis.scene({
    width: 960,
    height: 540,
});

scene.text({id: "caption", text: "tween: corner", point: [0, 2.2], size: 20, font: "JetBrainsMono", fill: "muted"});
const subject = scene.rectangle({id: "subject", size: [2.6, 1.5], stroke: "accent", fill: "accent", width: 3});
scene.tween({target: subject, corner: 0.7}, 1.4);

return scene;
```

### tween · points

<!-- api-example: property.points -->

```viscript
local scene = vis.scene {
    width = 960,
    height = 540,
}

scene:text {id = "caption", text = "tween: points", point = {0, 2.2}, size = 20, font = "JetBrainsMono", fill = "muted"}
local subject = scene:polygon {id = "subject", points = {{-1,-1}, {1,-1}, {0,1}}, fill = "accent"}
scene:tween({target = subject, points = {{-2,-0.6}, {2,-0.6}, {1,1.4}}}, 1.4)

return scene
```

```viscript-js
const scene = vis.scene({
    width: 960,
    height: 540,
});

scene.text({id: "caption", text: "tween: points", point: [0, 2.2], size: 20, font: "JetBrainsMono", fill: "muted"});
const subject = scene.polygon({id: "subject", points: [[-1,-1], [1,-1], [0,1]], fill: "accent"});
scene.tween({target: subject, points: [[-2,-0.6], [2,-0.6], [1,1.4]]}, 1.4);

return scene;
```
