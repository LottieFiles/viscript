---
title: Plot & Field Properties
id: plot-properties
order: 10
parent: animation
---

# Plot & Field Properties

Use these [tween / set descriptor](./state-scheduling.md) fields to update a plot function, its sampling range, or a contour's scalar field.

## Functions and contour values

| Lua | JavaScript | Purpose |
| --- | --- | --- |
| `fn` | `fn` | New plot function or contour scalar field. |
| `values` | `values` | Contour scalar samples, with x varying fastest. |
| `level` | `level` | Contour level, supplied with `fn` or `values`. |

## Sampling and breaks

| Lua | JavaScript | Purpose |
| --- | --- | --- |
| `x_range` | `x_range` | Plot sample range or contour grid columns. |
| `y_range` | `y_range` | Plot band or contour grid rows. |
| `discontinuities` | `discontinuities` | Plot x positions that must not connect. |

To animate explicit plot or path points, use [`points`](./shape-properties.md).

## Examples

### tween · fn

<!-- api-example: property.fn -->

```viscript
local scene = vis.scene {
    width = 960,
    height = 540,
}

scene:text {id = "caption", text = "tween: fn", point = {0, 2.2}, size = 20, font = "JetBrainsMono", fill = "muted"}
local subject = scene:plot {id = "subject", fn = function(x) return math.sin(x) end, x_range = {-3, 3, 0.05}, stroke = "accent", width = 3}
scene:tween({target = subject, fn = function(x) return math.cos(x) end}, 1.4)

return scene
```

```viscript-js
const scene = vis.scene({
    width: 960,
    height: 540,
});

scene.text({id: "caption", text: "tween: fn", point: [0, 2.2], size: 20, font: "JetBrainsMono", fill: "muted"});
const subject = scene.plot({id: "subject", fn: Math.sin, x_range: [-3, 3, 0.05], stroke: "accent", width: 3});
scene.tween({target: subject, fn: Math.cos}, 1.4);

return scene;
```

### tween · values

<!-- api-example: property.values -->

```viscript
local scene = vis.scene {
    width = 960,
    height = 540,
}

scene:text {id = "caption", text = "tween: values", point = {0, 2.2}, size = 20, font = "JetBrainsMono", fill = "muted"}
local contours = scene:contour {values = {8,4,8, 4,0,4, 8,4,8}, levels = {2}, x_range = {-2,2,2}, y_range = {-2,2,2}, stroke = "accent", width = 3}
local subject = contours:children()[1]
scene:tween({target = subject, values = {8,1,8, 7,0,7, 8,1,8}}, 1.4)

return scene
```

```viscript-js
const scene = vis.scene({
    width: 960,
    height: 540,
});

scene.text({id: "caption", text: "tween: values", point: [0, 2.2], size: 20, font: "JetBrainsMono", fill: "muted"});
const contours = scene.contour({values: [8,4,8, 4,0,4, 8,4,8], levels: [2], x_range: [-2,2,2], y_range: [-2,2,2], stroke: "accent", width: 3});
const subject = contours.children()[0];
scene.tween({target: subject, values: [8,1,8, 7,0,7, 8,1,8]}, 1.4);

return scene;
```

### tween · level

<!-- api-example: property.level -->

```viscript
local scene = vis.scene {
    width = 960,
    height = 540,
}

scene:text {id = "caption", text = "tween: level", point = {0, 2.2}, size = 20, font = "JetBrainsMono", fill = "muted"}
local values = {}
for y = -2, 2, 0.125 do
    for x = -2, 2, 0.125 do values[#values + 1] = x*x + y*y end
end
local contours = scene:contour {values = values, levels = {1}, x_range = {-2, 2, 0.125}, y_range = {-2, 2, 0.125}, stroke = "accent", width = 3}
local subject = contours:children()[1]
scene:tween({target = subject, values = values, level = 3}, 1.4)

return scene
```

```viscript-js
const scene = vis.scene({
    width: 960,
    height: 540,
});

scene.text({id: "caption", text: "tween: level", point: [0, 2.2], size: 20, font: "JetBrainsMono", fill: "muted"});
const values = [];
for (let y = -2; y <= 2; y += 0.125) {
    for (let x = -2; x <= 2; x += 0.125) values.push(x*x + y*y);
}
const contours = scene.contour({values, levels: [1], x_range: [-2, 2, 0.125], y_range: [-2, 2, 0.125], stroke: "accent", width: 3});
const subject = contours.children()[0];
scene.tween({target: subject, values, level: 3}, 1.4);

return scene;
```

### tween · x_range

<!-- api-example: property.x_range -->

```viscript
local scene = vis.scene {
    width = 960,
    height = 540,
}

scene:text {id = "caption", text = "tween: x_range", point = {0, 2.2}, size = 20, font = "JetBrainsMono", fill = "muted"}
local subject = scene:plot {id = "subject", fn = function(x) return 0.4*x end, x_range = {-1,1,0.05}, stroke = "accent", width = 3}
scene:tween({target = subject, fn = function(x) return 0.4*x end, x_range = {-3,3,0.05}}, 1.4)

return scene
```

```viscript-js
const scene = vis.scene({
    width: 960,
    height: 540,
});

scene.text({id: "caption", text: "tween: x_range", point: [0, 2.2], size: 20, font: "JetBrainsMono", fill: "muted"});
const subject = scene.plot({id: "subject", fn: (x) => 0.4*x, x_range: [-1,1,0.05], stroke: "accent", width: 3});
scene.tween({target: subject, fn: (x) => 0.4*x, x_range: [-3,3,0.05]}, 1.4);

return scene;
```

### tween · y_range

<!-- api-example: property.y_range -->

```viscript
local scene = vis.scene {
    width = 960,
    height = 540,
}

scene:text {id = "caption", text = "tween: y_range", point = {0, 2.2}, size = 20, font = "JetBrainsMono", fill = "muted"}
local subject = scene:plot {id = "subject", fn = function(x) return x*x - 1 end, x_range = {-2,2,0.05}, y_range = {-0.5,0.5}, stroke = "accent", width = 3}
scene:tween({target = subject, fn = function(x) return x*x - 1 end, y_range = {-1.5,1.5}}, 1.4)

return scene
```

```viscript-js
const scene = vis.scene({
    width: 960,
    height: 540,
});

scene.text({id: "caption", text: "tween: y_range", point: [0, 2.2], size: 20, font: "JetBrainsMono", fill: "muted"});
const subject = scene.plot({id: "subject", fn: (x) => x*x - 1, x_range: [-2,2,0.05], y_range: [-0.5,0.5], stroke: "accent", width: 3});
scene.tween({target: subject, fn: (x) => x*x - 1, y_range: [-1.5,1.5]}, 1.4);

return scene;
```

### tween · discontinuities

<!-- api-example: property.discontinuities -->

```viscript
local scene = vis.scene {
    width = 960,
    height = 540,
}

scene:text {id = "caption", text = "tween: discontinuities", point = {0, 2.2}, size = 20, font = "JetBrainsMono", fill = "muted"}
local subject = scene:plot {id = "subject", fn = function(x) if x < 0 then return -1 else return 1 end end, x_range = {-3,3,1.5}, stroke = "accent", width = 3}
scene:tween({target = subject, fn = function(x) if x < 0 then return -1 else return 1 end end, discontinuities = {0}}, 1.4)

return scene
```

```viscript-js
const scene = vis.scene({
    width: 960,
    height: 540,
});

scene.text({id: "caption", text: "tween: discontinuities", point: [0, 2.2], size: 20, font: "JetBrainsMono", fill: "muted"});
const subject = scene.plot({id: "subject", fn: (x) => x < 0 ? -1 : 1, x_range: [-3,3,1.5], stroke: "accent", width: 3});
scene.tween({target: subject, fn: (x) => x < 0 ? -1 : 1, discontinuities: [0]}, 1.4);

return scene;
```
