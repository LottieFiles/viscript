---
title: Shape Properties
id: shape-properties
order: 9
parent: animation
---

# Shape Properties

These [tween / set descriptor](./state-scheduling.md) fields change an object's own geometry. Coordinates use world units unless the description states pixels.

## Circle, arc and angle

| Lua | JavaScript | Purpose |
| --- | --- | --- |
| `radius` | `radius` | Circle/arc/angle radius in world units, or point radius in pixels. |
| `center` | `center` | Arc center. |
| `start` | `start` | Arc start angle in radians. |
| `angle` | `angle` | Signed Arc sweep in radians, between −2π and 2π. |
| `vertex` | `vertex` | Angle vertex. |

## Size and corners

| Lua | JavaScript | Purpose |
| --- | --- | --- |
| `size` | `size` | Text size, or rectangle width/height. |
| `corner` | `corner` | Rectangle corner radius. |

## Paths, endpoints and vectors

| Lua | JavaScript | Purpose |
| --- | --- | --- |
| `points` | `points` | Polygon, route, path or plot points; non-plot point counts remain fixed. |
| `from` | `from` | Line, arrow, curve or angle start/ray endpoint. |
| `to` | `to` | Line, arrow, curve or angle end/ray endpoint. |
| `origin` | `origin` | Vector origin. |
| `control1` | `control1` | First cubic curve control point. |
| `control2` | `control2` | Second cubic curve control point. |
| `trim_distance` | `trimDistance` | Local endpoint trim distance: one value or start/end pair. |

For plot samples and fields, see [Plot & Field Properties](./plot-properties.md). Animate vector magnitude/direction with the [`value` field](./text-properties.md).

## Examples

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

### tween · center

<!-- api-example: property.center -->

```viscript
local scene = vis.scene {
    width = 960,
    height = 540,
}

scene:text {id = "caption", text = "tween: center", point = {0, 2.2}, size = 20, font = "JetBrainsMono", fill = "muted"}
local subject = scene:arc {id = "subject", radius = 1.2, start = 0, angle = math.pi, stroke = "accent", width = 4}
scene:tween({target = subject, center = {1.2, -0.7}}, 1.4)

return scene
```

```viscript-js
const scene = vis.scene({
    width: 960,
    height: 540,
});

scene.text({id: "caption", text: "tween: center", point: [0, 2.2], size: 20, font: "JetBrainsMono", fill: "muted"});
const subject = scene.arc({id: "subject", radius: 1.2, start: 0, angle: Math.PI, stroke: "accent", width: 4});
scene.tween({target: subject, center: [1.2, -0.7]}, 1.4);

return scene;
```

### tween · start

<!-- api-example: property.start -->

```viscript
local scene = vis.scene {
    width = 960,
    height = 540,
}

scene:text {id = "caption", text = "tween: start", point = {0, 2.2}, size = 20, font = "JetBrainsMono", fill = "muted"}
local subject = scene:arc {id = "subject", radius = 1.2, start = 0, angle = math.pi, stroke = "accent", width = 4}
scene:tween({target = subject, start = math.pi}, 1.4)

return scene
```

```viscript-js
const scene = vis.scene({
    width: 960,
    height: 540,
});

scene.text({id: "caption", text: "tween: start", point: [0, 2.2], size: 20, font: "JetBrainsMono", fill: "muted"});
const subject = scene.arc({id: "subject", radius: 1.2, start: 0, angle: Math.PI, stroke: "accent", width: 4});
scene.tween({target: subject, start: Math.PI}, 1.4);

return scene;
```

### tween · angle

<!-- api-example: property.angle -->

```viscript
local scene = vis.scene {
    width = 960,
    height = 540,
}

scene:text {id = "caption", text = "tween: angle", point = {0, 2.2}, size = 20, font = "JetBrainsMono", fill = "muted"}
local subject = scene:arc {id = "subject", radius = 1.2, start = 0, angle = math.pi, stroke = "accent", width = 4}
scene:tween({target = subject, angle = 1.8 * math.pi}, 1.4)

return scene
```

```viscript-js
const scene = vis.scene({
    width: 960,
    height: 540,
});

scene.text({id: "caption", text: "tween: angle", point: [0, 2.2], size: 20, font: "JetBrainsMono", fill: "muted"});
const subject = scene.arc({id: "subject", radius: 1.2, start: 0, angle: Math.PI, stroke: "accent", width: 4});
scene.tween({target: subject, angle: 1.8 * Math.PI}, 1.4);

return scene;
```

### tween · vertex

<!-- api-example: property.vertex -->

```viscript
local scene = vis.scene {
    width = 960,
    height = 540,
}

scene:text {id = "caption", text = "tween: vertex", point = {0, 2.2}, size = 20, font = "JetBrainsMono", fill = "muted"}
local subject = scene:angle {id = "subject", vertex = {-1, -0.5}, from = {2, -0.5}, to = {1, 1}, radius = 0.7, stroke = "accent", width = 4}
scene:tween({target = subject, vertex = {0, -1.2}}, 1.4)

return scene
```

```viscript-js
const scene = vis.scene({
    width: 960,
    height: 540,
});

scene.text({id: "caption", text: "tween: vertex", point: [0, 2.2], size: 20, font: "JetBrainsMono", fill: "muted"});
const subject = scene.angle({id: "subject", vertex: [-1, -0.5], from: [2, -0.5], to: [1, 1], radius: 0.7, stroke: "accent", width: 4});
scene.tween({target: subject, vertex: [0, -1.2]}, 1.4);

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

### tween · trim_distance

<!-- api-example: property.trim_distance -->

```viscript
local scene = vis.scene {
    width = 960,
    height = 540,
}

scene:text {id = "caption", text = "tween: trim_distance", point = {0, 2.2}, size = 20, font = "JetBrainsMono", fill = "muted"}
local subject = scene:arrow {id = "subject", from = {-2, 0}, to = {2, 0}, stroke = "accent", width = 3, tip = 10}
scene:tween({target = subject, trim_distance = {0.8, 0.8}}, 1.4)

return scene
```

```viscript-js
const scene = vis.scene({
    width: 960,
    height: 540,
});

scene.text({id: "caption", text: "tween: trim_distance", point: [0, 2.2], size: 20, font: "JetBrainsMono", fill: "muted"});
const subject = scene.arrow({id: "subject", from: [-2, 0], to: [2, 0], stroke: "accent", width: 3, tip: 10});
scene.tween({target: subject, trimDistance: [0.8, 0.8]}, 1.4);

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

### tween · from

<!-- api-example: property.from -->

```viscript
local scene = vis.scene {
    width = 960,
    height = 540,
}

scene:text {id = "caption", text = "tween: from", point = {0, 2.2}, size = 20, font = "JetBrainsMono", fill = "muted"}
local subject = scene:arrow {id = "subject", from = {-2, 0}, to = {2, 0}, stroke = "accent", width = 3, tip = 10}
scene:tween({target = subject, from = {-1, -1.4}}, 1.4)

return scene
```

```viscript-js
const scene = vis.scene({
    width: 960,
    height: 540,
});

scene.text({id: "caption", text: "tween: from", point: [0, 2.2], size: 20, font: "JetBrainsMono", fill: "muted"});
const subject = scene.arrow({id: "subject", from: [-2, 0], to: [2, 0], stroke: "accent", width: 3, tip: 10});
scene.tween({target: subject, from: [-1, -1.4]}, 1.4);

return scene;
```

### tween · to

<!-- api-example: property.to -->

```viscript
local scene = vis.scene {
    width = 960,
    height = 540,
}

scene:text {id = "caption", text = "tween: to", point = {0, 2.2}, size = 20, font = "JetBrainsMono", fill = "muted"}
local subject = scene:arrow {id = "subject", from = {-2, 0}, to = {2, 0}, stroke = "accent", width = 3, tip = 10}
scene:tween({target = subject, to = {1, 1.4}}, 1.4)

return scene
```

```viscript-js
const scene = vis.scene({
    width: 960,
    height: 540,
});

scene.text({id: "caption", text: "tween: to", point: [0, 2.2], size: 20, font: "JetBrainsMono", fill: "muted"});
const subject = scene.arrow({id: "subject", from: [-2, 0], to: [2, 0], stroke: "accent", width: 3, tip: 10});
scene.tween({target: subject, to: [1, 1.4]}, 1.4);

return scene;
```

### tween · origin

<!-- api-example: property.origin -->

```viscript
local scene = vis.scene {
    width = 960,
    height = 540,
}

scene:text {id = "caption", text = "tween: origin", point = {0, 2.2}, size = 20, font = "JetBrainsMono", fill = "muted"}
local subject = scene:vector {id = "subject", value = {2, 1}, origin = {-1, -0.5}, stroke = "accent", width = 3, tip = 10}
scene:tween({target = subject, origin = {0, -1.5}}, 1.4)

return scene
```

```viscript-js
const scene = vis.scene({
    width: 960,
    height: 540,
});

scene.text({id: "caption", text: "tween: origin", point: [0, 2.2], size: 20, font: "JetBrainsMono", fill: "muted"});
const subject = scene.vector({id: "subject", value: [2, 1], origin: [-1, -0.5], stroke: "accent", width: 3, tip: 10});
scene.tween({target: subject, origin: [0, -1.5]}, 1.4);

return scene;
```

### tween · control1

<!-- api-example: property.control1 -->

```viscript
local scene = vis.scene {
    width = 960,
    height = 540,
}

scene:text {id = "caption", text = "tween: control1", point = {0, 2.2}, size = 20, font = "JetBrainsMono", fill = "muted"}
local subject = scene:curve {id = "subject", from = {-2, -0.6}, control1 = {-1, 1.5}, control2 = {1, -1.5}, to = {2, 0.6}, stroke = "accent", width = 4}
scene:tween({target = subject, control1 = {-1, -1.5}}, 1.4)

return scene
```

```viscript-js
const scene = vis.scene({
    width: 960,
    height: 540,
});

scene.text({id: "caption", text: "tween: control1", point: [0, 2.2], size: 20, font: "JetBrainsMono", fill: "muted"});
const subject = scene.curve({id: "subject", from: [-2, -0.6], control1: [-1, 1.5], control2: [1, -1.5], to: [2, 0.6], stroke: "accent", width: 4});
scene.tween({target: subject, control1: [-1, -1.5]}, 1.4);

return scene;
```

### tween · control2

<!-- api-example: property.control2 -->

```viscript
local scene = vis.scene {
    width = 960,
    height = 540,
}

scene:text {id = "caption", text = "tween: control2", point = {0, 2.2}, size = 20, font = "JetBrainsMono", fill = "muted"}
local subject = scene:curve {id = "subject", from = {-2, -0.6}, control1 = {-1, 1.5}, control2 = {1, -1.5}, to = {2, 0.6}, stroke = "accent", width = 4}
scene:tween({target = subject, control2 = {1, 1.5}}, 1.4)

return scene
```

```viscript-js
const scene = vis.scene({
    width: 960,
    height: 540,
});

scene.text({id: "caption", text: "tween: control2", point: [0, 2.2], size: 20, font: "JetBrainsMono", fill: "muted"});
const subject = scene.curve({id: "subject", from: [-2, -0.6], control1: [-1, 1.5], control2: [1, -1.5], to: [2, 0.6], stroke: "accent", width: 4});
scene.tween({target: subject, control2: [1, 1.5]}, 1.4);

return scene;
```
