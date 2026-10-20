---
title: Plots & Areas
id: plots
order: 5
parent: object
---

# Plots & Areas

Plot samples, fill the area between functions, and approximate an integral with rectangles.

| Lua | JavaScript | Purpose |
| --- | --- | --- |
| `scene:plot(opts)` | `scene.plot(opts)` | Draw `points`, or sample `fn(x)` over `x_range`; `y_range` clips a band, `discontinuities` split it, and `endpoint_radius` marks its active end. |
| `scene:area(opts)` | `scene.area(opts)` | Fill between `fn(x)` and `base` over `x_range`; `base` is a number or another function. |
| `scene:riemann(opts)` | `scene.riemann(opts)` | Build a Group of rectangles from `fn` or `values` over `x_range`; `sample` chooses left/right/midpoint and `negative` colors negative heights. |

Function samplers run during authoring; rendering reads the retained result. For coordinate axes and tick labels, see [Spaces & Sampling](../Group/spaces-sampling.md).

## Examples

### scene.plot

<!-- api-example: scene.plot -->

```viscript
local scene = vis.scene {
    width = 960,
    height = 540,
}

scene:space {x = {-3, 3, 1}, y = {-1.5, 1.5, 0.5}, stroke = "border", axis_x = "muted", axis_y = "muted"}
scene:plot {fn = math.sin, x_range = {-3, 3, 0.05}, stroke = "accent", width = 2}
scene:text {id = "caption", text = "y = sin(x), sampled every 0.05", point = {0, -2.25}, size = 16, fill = "muted"}

return scene
```

```viscript-js
const scene = vis.scene({
    width: 960,
    height: 540,
});

scene.space({x: [-3, 3, 1], y: [-1.5, 1.5, 0.5], stroke: "border", axis_x: "muted", axis_y: "muted"});
scene.plot({fn: Math.sin, x_range: [-3, 3, 0.05], stroke: "accent", width: 2});
scene.text({id: "caption", text: "y = sin(x), sampled every 0.05", point: [0, -2.25], size: 16, fill: "muted"});

return scene;
```

### scene.area

<!-- api-example: scene.area -->

```viscript
local scene = vis.scene {
    width = 960,
    height = 540,
}

scene:area {fn = function(x) return 1 - x*x/4 end, base = 0, x_range = {-2, 2, 0.05}, fill = "#6cb6ff66", stroke = "accent"}
scene:line {from = {-2.5, 0}, to = {2.5, 0}, stroke = "muted"}
scene:text {id = "caption", text = "Fill between y = 1 - x*x/4 and y = 0", point = {0, -2.25}, size = 16, fill = "muted"}

return scene
```

```viscript-js
const scene = vis.scene({
    width: 960,
    height: 540,
});

scene.area({fn: x => 1 - x*x/4, base: 0, x_range: [-2, 2, 0.05], fill: "#6cb6ff66", stroke: "accent"});
scene.line({from: [-2.5, 0], to: [2.5, 0], stroke: "muted"});
scene.text({id: "caption", text: "Fill between y = 1 - x*x/4 and y = 0", point: [0, -2.25], size: 16, fill: "muted"});

return scene;
```

### scene.riemann

<!-- api-example: scene.riemann -->

```viscript
local scene = vis.scene {
    width = 960,
    height = 540,
}

scene:riemann {fn = function(x) return 1 - x*x/4 end, x_range = {-2, 2, 0.5}, sample = "midpoint", fill = "#6cb6ff55", stroke = "accent", width = 1}
scene:plot {fn = function(x) return 1 - x*x/4 end, x_range = {-2, 2, 0.05}, stroke = "foreground"}
scene:text {id = "caption", text = "Eight midpoint rectangles approximate the area", point = {0, -2.25}, size = 16, fill = "muted"}

return scene
```

```viscript-js
const scene = vis.scene({
    width: 960,
    height: 540,
});

scene.riemann({fn: x => 1 - x*x/4, x_range: [-2, 2, 0.5], sample: "midpoint", fill: "#6cb6ff55", stroke: "accent", width: 1});
scene.plot({fn: x => 1 - x*x/4, x_range: [-2, 2, 0.05], stroke: "foreground"});
scene.text({id: "caption", text: "Eight midpoint rectangles approximate the area", point: [0, -2.25], size: 16, fill: "muted"});

return scene;
```

### play · fn

<!-- api-example: property.fn -->

```viscript
local scene = vis.scene {
    width = 960,
    height = 540,
}

scene:text {id = "caption", text = "play: fn", point = {0, 2.2}, size = 20, font = "JetBrainsMono", fill = "muted"}
local subject = scene:plot {id = "subject", fn = function(x) return math.sin(x) end, x_range = {-3, 3, 0.05}, stroke = "accent", width = 3}
scene:play({target = subject, fn = function(x) return math.cos(x) end}, 1.4)

return scene
```

```viscript-js
const scene = vis.scene({
    width: 960,
    height: 540,
});

scene.text({id: "caption", text: "play: fn", point: [0, 2.2], size: 20, font: "JetBrainsMono", fill: "muted"});
const subject = scene.plot({id: "subject", fn: Math.sin, x_range: [-3, 3, 0.05], stroke: "accent", width: 3});
scene.play({target: subject, fn: Math.cos}, 1.4);

return scene;
```

### play · x_range

<!-- api-example: property.x_range -->

```viscript
local scene = vis.scene {
    width = 960,
    height = 540,
}

scene:text {id = "caption", text = "play: x_range", point = {0, 2.2}, size = 20, font = "JetBrainsMono", fill = "muted"}
local subject = scene:plot {id = "subject", fn = function(x) return 0.4*x end, x_range = {-1,1,0.05}, stroke = "accent", width = 3}
scene:play({target = subject, fn = function(x) return 0.4*x end, x_range = {-3,3,0.05}}, 1.4)

return scene
```

```viscript-js
const scene = vis.scene({
    width: 960,
    height: 540,
});

scene.text({id: "caption", text: "play: x_range", point: [0, 2.2], size: 20, font: "JetBrainsMono", fill: "muted"});
const subject = scene.plot({id: "subject", fn: (x) => 0.4*x, x_range: [-1,1,0.05], stroke: "accent", width: 3});
scene.play({target: subject, fn: (x) => 0.4*x, x_range: [-3,3,0.05]}, 1.4);

return scene;
```

### play · y_range

<!-- api-example: property.y_range -->

```viscript
local scene = vis.scene {
    width = 960,
    height = 540,
}

scene:text {id = "caption", text = "play: y_range", point = {0, 2.2}, size = 20, font = "JetBrainsMono", fill = "muted"}
local subject = scene:plot {id = "subject", fn = function(x) return x*x - 1 end, x_range = {-2,2,0.05}, y_range = {-0.5,0.5}, stroke = "accent", width = 3}
scene:play({target = subject, fn = function(x) return x*x - 1 end, y_range = {-1.5,1.5}}, 1.4)

return scene
```

```viscript-js
const scene = vis.scene({
    width: 960,
    height: 540,
});

scene.text({id: "caption", text: "play: y_range", point: [0, 2.2], size: 20, font: "JetBrainsMono", fill: "muted"});
const subject = scene.plot({id: "subject", fn: (x) => x*x - 1, x_range: [-2,2,0.05], y_range: [-0.5,0.5], stroke: "accent", width: 3});
scene.play({target: subject, fn: (x) => x*x - 1, y_range: [-1.5,1.5]}, 1.4);

return scene;
```

### play · discontinuities

<!-- api-example: property.discontinuities -->

```viscript
local scene = vis.scene {
    width = 960,
    height = 540,
}

scene:text {id = "caption", text = "play: discontinuities", point = {0, 2.2}, size = 20, font = "JetBrainsMono", fill = "muted"}
local subject = scene:plot {id = "subject", fn = function(x) if x < 0 then return -1 else return 1 end end, x_range = {-3,3,1.5}, stroke = "accent", width = 3}
scene:play({target = subject, fn = function(x) if x < 0 then return -1 else return 1 end end, discontinuities = {0}}, 1.4)

return scene
```

```viscript-js
const scene = vis.scene({
    width: 960,
    height: 540,
});

scene.text({id: "caption", text: "play: discontinuities", point: [0, 2.2], size: 20, font: "JetBrainsMono", fill: "muted"});
const subject = scene.plot({id: "subject", fn: (x) => x < 0 ? -1 : 1, x_range: [-3,3,1.5], stroke: "accent", width: 3});
scene.play({target: subject, fn: (x) => x < 0 ? -1 : 1, discontinuities: [0]}, 1.4);

return scene;
```
