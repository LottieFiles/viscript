---
title: Lines & Paths
id: lines-paths
order: 3
parent: object
---

# Lines & Paths

Draw straight segments, vectors, routed lines, Bézier curves, and connections between object outlines.

## Segments & vectors

| Lua | JavaScript | Purpose |
| --- | --- | --- |
| `scene:line(opts)` | `scene.line(opts)` | Draw the segment `from` → `to`; `trim_distance` in Lua / `trimDistance` in JS trims its ends in local units. |
| `scene:arrow(opts)` | `scene.arrow(opts)` | Draw a segment with `tail` and `tip` arrowheads in pixels; accepts the same endpoint trim as `line`. |
| `scene:vector(opts)` | `scene.vector(opts)` | Draw the vector `value` starting at `origin`; accepts arrowheads and endpoint trim. |

## Paths & connections

| Lua | JavaScript | Purpose |
| --- | --- | --- |
| `scene:route(opts)` | `scene.route(opts)` | Join `points` with straight segments; `tail` and `tip` set arrowhead lengths (defaults 0 and 16 pixels). |
| `scene:path(opts)` | `scene.path(opts)` | Draw `commands` (`move`, `line`, `quadratic`, `cubic`, `close`); `samples` sets curve sampling. Closed contours can form holes. |
| `scene:curve(opts)` | `scene.curve(opts)` | Draw a cubic Bézier from `from` through `control1`/`control2` to `to`; optional `samples`. |
| `scene:connector(opts)` | `scene.connector(from, to, opts?)` | Join object outlines, falling back to bounds; optional `padding`, `tail`, and `tip`. Lua puts handle endpoints in `opts.from`/`opts.to`. |

Endpoint trim uses world units. Stroke width and arrowhead lengths use screen pixels. A closed `path` can contain holes and serve as a [boolean region](./boolean-regions.md).

## Examples

### scene.line

<!-- api-example: scene.line -->

```viscript
local scene = vis.scene {
    width = 960,
    height = 540,
}

scene:line {from = {-2, -0.5}, to = {2, 0.7}, stroke = "accent", width = 2}
scene:text {id = "caption", text = "A segment between two endpoints", point = {0, -2.25}, size = 16, fill = "muted"}

return scene
```

```viscript-js
const scene = vis.scene({
    width: 960,
    height: 540,
});

scene.line({from: [-2, -0.5], to: [2, 0.7], stroke: "accent", width: 2});
scene.text({id: "caption", text: "A segment between two endpoints", point: [0, -2.25], size: 16, fill: "muted"});

return scene;
```

### scene.arrow

<!-- api-example: scene.arrow -->

```viscript
local scene = vis.scene {
    width = 960,
    height = 540,
}

scene:arrow {from = {-2, -0.5}, to = {2, 0.7}, tip = 12, stroke = "accent", width = 2}
scene:text {id = "caption", text = "A segment with a 12 px arrowhead", point = {0, -2.25}, size = 16, fill = "muted"}

return scene
```

```viscript-js
const scene = vis.scene({
    width: 960,
    height: 540,
});

scene.arrow({from: [-2, -0.5], to: [2, 0.7], tip: 12, stroke: "accent", width: 2});
scene.text({id: "caption", text: "A segment with a 12 px arrowhead", point: [0, -2.25], size: 16, fill: "muted"});

return scene;
```

### scene.vector

<!-- api-example: scene.vector -->

```viscript
local scene = vis.scene {
    width = 960,
    height = 540,
}

scene:point {point = {-1.5, -0.5}, radius = 4, color = "foreground"}
scene:vector {origin = {-1.5, -0.5}, value = {3, 1.5}, tip = 10, stroke = "accent", width = 2}
scene:text {id = "caption", text = "Vector (3, 1.5) from its origin", point = {0, -2.25}, size = 16, fill = "muted"}

return scene
```

```viscript-js
const scene = vis.scene({
    width: 960,
    height: 540,
});

scene.point({point: [-1.5, -0.5], radius: 4, color: "foreground"});
scene.vector({origin: [-1.5, -0.5], value: [3, 1.5], tip: 10, stroke: "accent", width: 2});
scene.text({id: "caption", text: "Vector (3, 1.5) from its origin", point: [0, -2.25], size: 16, fill: "muted"});

return scene;
```

### scene.route

<!-- api-example: scene.route -->

```viscript
local scene = vis.scene {
    width = 960,
    height = 540,
}

scene:route {points = {{-2, -0.8}, {-0.5, -0.8}, {-0.5, 0.8}, {2, 0.8}}, stroke = "accent", width = 2, tip = 10}
scene:text {id = "caption", text = "An open route through its waypoints", point = {0, -2.25}, size = 16, fill = "muted"}

return scene
```

```viscript-js
const scene = vis.scene({
    width: 960,
    height: 540,
});

scene.route({points: [[-2, -0.8], [-0.5, -0.8], [-0.5, 0.8], [2, 0.8]], stroke: "accent", width: 2, tip: 10});
scene.text({id: "caption", text: "An open route through its waypoints", point: [0, -2.25], size: 16, fill: "muted"});

return scene;
```

### scene.path

<!-- api-example: scene.path -->

```viscript
local scene = vis.scene {
    width = 960,
    height = 540,
}

scene:path {commands = {{type = "move", to = {-2, -0.8}}, {type = "quadratic", control = {0, 2.5}, to = {2, -0.8}}, {type = "line", to = {-2, -0.8}}, {type = "close"}}, stroke = "accent", fill = "#6cb6ff22", width = 2}
scene:text {id = "caption", text = "Move, quadratic, line, and close commands", point = {0, -2.25}, size = 16, fill = "muted"}

return scene
```

```viscript-js
const scene = vis.scene({
    width: 960,
    height: 540,
});

scene.path({commands: [{type: "move", to: [-2, -0.8]}, {type: "quadratic", control: [0, 2.5], to: [2, -0.8]}, {type: "line", to: [-2, -0.8]}, {type: "close"}], stroke: "accent", fill: "#6cb6ff22", width: 2});
scene.text({id: "caption", text: "Move, quadratic, line, and close commands", point: [0, -2.25], size: 16, fill: "muted"});

return scene;
```

### scene.curve

<!-- api-example: scene.curve -->

```viscript
local scene = vis.scene {
    width = 960,
    height = 540,
}

scene:curve {from = {-2, -0.7}, control1 = {-1, 2}, control2 = {1, -2}, to = {2, 0.7}, stroke = "accent", width = 2}
scene:text {id = "caption", text = "A cubic Bezier with two control points", point = {0, -2.25}, size = 16, fill = "muted"}

return scene
```

```viscript-js
const scene = vis.scene({
    width: 960,
    height: 540,
});

scene.curve({from: [-2, -0.7], control1: [-1, 2], control2: [1, -2], to: [2, 0.7], stroke: "accent", width: 2});
scene.text({id: "caption", text: "A cubic Bezier with two control points", point: [0, -2.25], size: 16, fill: "muted"});

return scene;
```

### scene.connector

<!-- api-example: scene.connector -->

```viscript
local scene = vis.scene {
    width = 960,
    height = 540,
}

local a = scene:circle {center = {-2, 0}, radius = 0.6, stroke = "foreground"}
local b = scene:rectangle {center = {2, 0.6}, size = {1.2, 1}, stroke = "foreground"}
scene:connector {from = a, to = b, padding = 0.12, stroke = "accent", width = 2, tip = 10}
scene:text {id = "caption", text = "Connector follows the moving object's outline", point = {0, -2.25}, size = 16, fill = "muted"}
scene:shift(b, {-1, 1}, {duration = 1.2})

return scene
```

```viscript-js
const scene = vis.scene({
    width: 960,
    height: 540,
});

const a = scene.circle({center: [-2, 0], radius: 0.6, stroke: "foreground"});
const b = scene.rectangle({center: [2, 0.6], size: [1.2, 1], stroke: "foreground"});
scene.connector(a, b, {padding: 0.12, stroke: "accent", width: 2, tip: 10});
scene.text({id: "caption", text: "Connector follows the moving object's outline", point: [0, -2.25], size: 16, fill: "muted"});
scene.shift(b, [-1, 1], {duration: 1.2});

return scene;
```
