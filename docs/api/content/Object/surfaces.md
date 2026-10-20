---
title: 3D Surfaces
id: surfaces
order: 7
parent: object
---

# 3D Surfaces

Draw a sampled 3D surface with a filled face, wire mesh, or both. Supply a rectangular point grid and its column/row count.

| Lua | JavaScript | Purpose |
| --- | --- | --- |
| `scene:surface(opts)` | `scene.surface(opts)` | Draw a 3D grid of `points` with `size` `[columns, rows]`; choose solid/mesh mode, `mesh` density, `shading`, `light`, and `colors`. |

Use a 3D camera to view depth. Surface `mesh` controls displayed mesh density; `shading`, `light`, and `colors` control its appearance.

## Examples

### scene.surface

<!-- api-example: scene.surface -->

```viscript
local scene = vis.scene {
    width = 960,
    height = 540,
    camera = {view = "3d", height = 7, eye = {6, 5, 8}, target = {0, 0, 0}},
}

local points = {}
for j = 0, 12 do
    for i = 0, 12 do
        local x, y = -1.5 + i/4, -1.5 + j/4
        points[#points + 1] = {x, y, 0.4 * (x*x - y*y)}
    end
end
scene:surface {points = points, size = {13, 13}, mode = "solid_mesh", mesh = {5, 5}, fill = "accent", stroke = "foreground", width = 1, shading = true}
scene:text {id = "caption", text = "A 13 x 13 saddle surface with a coarse mesh", point = {0, -3.2}, size = 16, fill = "muted"}

return scene
```

```viscript-js
const scene = vis.scene({
    width: 960,
    height: 540,
    camera: {view: "3d", height: 7, eye: [6, 5, 8], target: [0, 0, 0]},
});

const points = [];
for (let j = 0; j <= 12; j++) for (let i = 0; i <= 12; i++) {
    const x = -1.5 + i/4, y = -1.5 + j/4;
    points.push([x, y, 0.4 * (x*x - y*y)]);
}
scene.surface({points, size: [13, 13], mode: "solid_mesh", mesh: [5, 5], fill: "accent", stroke: "foreground", width: 1, shading: true});
scene.text({id: "caption", text: "A 13 x 13 saddle surface with a coarse mesh", point: [0, -3.2], size: 16, fill: "muted"});

return scene;
```
