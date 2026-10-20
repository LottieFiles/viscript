---
title: Vector Fields & Contours
id: vector-fields
order: 6
parent: object
---

# Vector Fields & Contours

Visualize a vector field with arrows or streamlines, or a scalar field with level curves.

| Lua | JavaScript | Purpose |
| --- | --- | --- |
| `scene:vector_field(opts)` | `scene.vectorField(opts)` | Build a Group of arrows from `fn(x,y)` or `values` on `x_range`/`y_range`; set `length` or `scale`, `anchor`, `colors`, and `magnitude`. |
| `scene:stream_lines(opts)` | — | Integrate `fn(x,y)` from `seeds` or an `x_range`/`y_range` grid into a Group of streamlines; set `time`, `dt`, `direction`, `colors`, and `magnitude`. |
| `scene:contour(opts)` | `scene.contour(opts)` | Build one curve per `levels` entry from scalar `fn(x,y)` or grid `values`; configure `x_range`, `y_range`, `colors`, and `labels`. Returns a Group. |

These factories return Groups. Samplers run during authoring; rendering reads the retained result. `stream_lines` is available only in Lua.

## Examples

### scene.vector_field

<!-- api-example: scene.vector_field -->

```viscript
local scene = vis.scene {
    width = 960,
    height = 540,
}

scene:vector_field {fn = function(x, y) return -y, x end, x_range = {-2, 2, 0.5}, y_range = {-1.5, 1.5, 0.5}, length = 0.4, stroke = "accent", width = 1.5, tip = 6}
scene:text {id = "caption", text = "Vector field F(x, y) = (-y, x)", point = {0, -2.25}, size = 16, fill = "muted"}

return scene
```

```viscript-js
const scene = vis.scene({
    width: 960,
    height: 540,
});

scene.vectorField({fn: (x, y) => [-y, x], x_range: [-2, 2, 0.5], y_range: [-1.5, 1.5, 0.5], length: 0.4, stroke: "accent", width: 1.5, tip: 6});
scene.text({id: "caption", text: "Vector field F(x, y) = (-y, x)", point: [0, -2.25], size: 16, fill: "muted"});

return scene;
```

### scene.stream_lines

<!-- api-example: scene.stream_lines -->

```viscript
local scene = vis.scene {
    width = 960,
    height = 540,
}

scene:stream_lines {fn = function(x, y) return -y, x end, seeds = {{0.5, 0}, {1, 0}, {1.5, 0}}, time = 5, dt = 0.05, direction = "forward", stroke = "accent", width = 2}
scene:text {id = "caption", text = "Three trajectories in F(x, y) = (-y, x)", point = {0, -2.25}, size = 16, fill = "muted"}

return scene
```

### scene.contour

<!-- api-example: scene.contour -->

```viscript
local scene = vis.scene {
    width = 960,
    height = 540,
}

local values = {}
for j = 0, 32 do
    for i = 0, 32 do
        local x, y = -2 + i/8, -2 + j/8
        values[#values + 1] = x*x + y*y
    end
end
scene:contour {values = values, levels = {0.5, 1.5, 3}, x_range = {-2, 2, 0.125}, y_range = {-2, 2, 0.125}, stroke = "accent", width = 2}
scene:text {id = "caption", text = "Level sets x*x + y*y = 0.5, 1.5, 3", point = {0, -2.25}, size = 16, fill = "muted"}

return scene
```

```viscript-js
const scene = vis.scene({
    width: 960,
    height: 540,
});

const values = [];
for (let j = 0; j <= 32; j++) for (let i = 0; i <= 32; i++) {
    const x = -2 + i/8, y = -2 + j/8;
    values.push(x*x + y*y);
}
scene.contour({values, levels: [0.5, 1.5, 3], x_range: [-2, 2, 0.125], y_range: [-2, 2, 0.125], stroke: "accent", width: 2});
scene.text({id: "caption", text: "Level sets x*x + y*y = 0.5, 1.5, 3", point: [0, -2.25], size: 16, fill: "muted"});

return scene;
```
