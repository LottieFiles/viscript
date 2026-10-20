---
title: Spaces & Sampling
id: spaces-sampling
order: 2
parent: group-space
---

# Spaces & Sampling

Create coordinate axes, label their ticks, and sample colored cells or voxels. Space exposes Object factories and placement methods; Group-only arrangement and row/entry/column selection are unavailable.

## Create axes

| Lua | JavaScript | Purpose |
| --- | --- | --- |
| `scene:space(opts)` | `scene.space(opts?)` | Create coordinate axes with `x`, `y`, `z` ranges, ticks, and optional numbers; return a Space. |

## Sample grids

| Lua | JavaScript | Purpose |
| --- | --- | --- |
| `space:cell(opts)` | `space.cell(opts)` | Create a static cell grid below the Space; accepts the ordinary `cell` factory options. |
| `space:cell(fn, opts?)` | `space.cell(fn, opts?)` | Sample `fn(x,y,time)` over the Space's x/y ranges into a Group of colored cells. |
| `space:voxel(fn, opts?)` | `space.voxel(fn, opts?)` | Sample `fn(x,y,z,time)` over the Space's x/y/z ranges into a Group of colored voxels. |

## Label ticks

| Lua | JavaScript | Purpose |
| --- | --- | --- |
| `space:labels(opts)` | `space.labels(opts)` | Place labels beside axis ticks and return their handles in x/y/z order. |

Cell/voxel sample options are `mode` (`full` or `padd`), `padding`, `duration`, and `fps`. Callbacks return a hex color and run during authoring, once per sample; their results are retained for seeking.

Label options are `x`, `y`, and `z` lists of `[value, textOrHandle]` pairs, plus `placement`, `gap`, `size`, `color`, and `side`. String labels become Text children; existing handles keep their parents and move beside ticks. `placement: "screen"` keeps direct billboard Text children outward from the projected origin. Place existing handles before the first animation; place screen labels before the Space's first warp.

Space factory options include `x`/`y`/`z` ranges `[min,max,step]`, `matrix`, axis colors (`axis_x`, `axis_y`, `axis_z`), `axis_width`, `ticks`, `tick_length`, `numbers`, `number_mode`, `number_size`, and `number_color`.

## Examples

### scene.space

<!-- api-example: scene.space -->

```viscript
local scene = vis.scene {
    width = 960,
    height = 540,
}

scene:space {x = {-3, 3, 1}, y = {-1.5, 1.5, 0.5}, ticks = true, numbers = true, stroke = "border", axis_x = "foreground", axis_y = "foreground", number_size = 12}
scene:text {id = "caption", text = "Axes, ticks, and coordinate labels", point = {0, -2.25}, size = 16, fill = "muted"}

return scene
```

```viscript-js
const scene = vis.scene({
    width: 960,
    height: 540,
});

scene.space({x: [-3, 3, 1], y: [-1.5, 1.5, 0.5], ticks: true, numbers: true, stroke: "border", axis_x: "foreground", axis_y: "foreground", number_size: 12});
scene.text({id: "caption", text: "Axes, ticks, and coordinate labels", point: [0, -2.25], size: 16, fill: "muted"});

return scene;
```

### space.cell

<!-- api-example: space.cell -->

```viscript
local scene = vis.scene {
    width = 960,
    height = 540,
}

local left = scene:space {x = {-4, -1, 1}, y = {-1, 1, 1}, stroke = "#00000000", axis_width = 0}
left:cell {origin = {-4, -1}, size = {3, 2}, color = "#3a4152", mode = "padd", padding = 0.06, patches = {{region = {1, 0, 1, 2}, color = "accent"}}}
local right = scene:space {x = {1, 4, 0.5}, y = {-1, 1, 0.5}, stroke = "#00000000", axis_width = 0}
right:cell(function(x, y, time) return x + y > 2.5 and "#6cb6ff" or "#3a4152" end, {mode = "padd", padding = 0.06})
scene:text {id = "static", text = "static options", point = {-2.5, 1.5}, size = 16, fill = "foreground"}
scene:text {id = "sampled", text = "callback samples", point = {2.5, 1.5}, size = 16, fill = "foreground"}
scene:text {id = "caption", text = "Both Space.cell overloads create retained grids", point = {0, -2.25}, size = 16, fill = "muted"}

return scene
```

```viscript-js
const scene = vis.scene({
    width: 960,
    height: 540,
});

const left = scene.space({x: [-4, -1, 1], y: [-1, 1, 1], stroke: "#00000000", axis_width: 0});
left.cell({origin: [-4, -1], size: [3, 2], color: "#3a4152", mode: "padd", padding: 0.06, patches: [{region: [1, 0, 1, 2], color: "accent"}]});
const right = scene.space({x: [1, 4, 0.5], y: [-1, 1, 0.5], stroke: "#00000000", axis_width: 0});
right.cell((x, y, time) => x + y > 2.5 ? "#6cb6ff" : "#3a4152", {mode: "padd", padding: 0.06});
scene.text({id: "static", text: "static options", point: [-2.5, 1.5], size: 16, fill: "foreground"});
scene.text({id: "sampled", text: "callback samples", point: [2.5, 1.5], size: 16, fill: "foreground"});
scene.text({id: "caption", text: "Both Space.cell overloads create retained grids", point: [0, -2.25], size: 16, fill: "muted"});

return scene;
```

### space.voxel

<!-- api-example: space.voxel -->

```viscript
local scene = vis.scene {
    width = 960,
    height = 540,
    camera = {view = "3d", height = 7, eye = {6, 5, 8}, target = {0, 0, 0}},
}

local space = scene:space {x = {-1, 1, 0.5}, y = {-1, 1, 0.5}, z = {-1, 1, 0.5}, stroke = "#00000000", axis_width = 0}
space:voxel(function(x, y, z, time)
    return x*x + y*y + z*z < 1.3 and "#6cb6ff" or "#00000000"
end, {mode = "padd", padding = 0.08})
scene:text {id = "caption", text = "Sample a sphere into colored voxels", point = {0, -3.2}, size = 16, fill = "muted"}

return scene
```

```viscript-js
const scene = vis.scene({
    width: 960,
    height: 540,
    camera: {view: "3d", height: 7, eye: [6, 5, 8], target: [0, 0, 0]},
});

const space = scene.space({x: [-1, 1, 0.5], y: [-1, 1, 0.5], z: [-1, 1, 0.5], stroke: "#00000000", axis_width: 0});
space.voxel((x, y, z, time) => x*x + y*y + z*z < 1.3 ? "#6cb6ff" : "#00000000", {mode: "padd", padding: 0.08});
scene.text({id: "caption", text: "Sample a sphere into colored voxels", point: [0, -3.2], size: 16, fill: "muted"});

return scene;
```

### space.labels

<!-- api-example: space.labels -->

```viscript
local scene = vis.scene {
    width = 960,
    height = 540,
}

local space = scene:space {x = {-3, 3, 1}, y = {-1.5, 1.5, 0.5}, stroke = "border", ticks = true, axis_x = "foreground", axis_y = "foreground"}
local labels = space:labels {x = {{-2, "-2"}, {2, "+2"}}, y = {{1, "one"}}, size = 18, color = "accent", gap = 8}
scene:text {id = "caption", text = "Custom labels beside selected axis ticks", point = {0, -2.25}, size = 16, fill = "muted"}

return scene
```

```viscript-js
const scene = vis.scene({
    width: 960,
    height: 540,
});

const space = scene.space({x: [-3, 3, 1], y: [-1.5, 1.5, 0.5], stroke: "border", ticks: true, axis_x: "foreground", axis_y: "foreground"});
const labels = space.labels({x: [[-2, "-2"], [2, "+2"]], y: [[1, "one"]], size: 18, color: "accent", gap: 8});
scene.text({id: "caption", text: "Custom labels beside selected axis ticks", point: [0, -2.25], size: 16, fill: "muted"});

return scene;
```
