---
title: Pictures & Cell Grids
id: pictures-cells
order: 12
parent: object
---

# Pictures & Cell Grids

Display an image or a dense grid of colored cells. Pictures preserve source colors; cells use a base color and patches.

| Lua | JavaScript | Purpose |
| --- | --- | --- |
| `scene:picture(opts)` | `scene.picture(opts)` | Show an image `asset` or inline `pixels` plus `size`; set `center`, world-space `width`, and `filter`. Source colors are preserved. |
| `scene:cell(opts)` | `scene.cell(opts)` | Create a dense grid with `size`, `origin`, `color`, and `patches`; optional `texture`, `source`, `destination`, `mode`, `padding`, and `depth`. |

Space adds callback overloads for sampled cells and voxels; see [Spaces & Sampling](../Group/spaces-sampling.md).

## Examples

### scene.picture

<!-- api-example: scene.picture -->

```viscript
local scene = vis.scene {
    width = 960,
    height = 540,
}

scene:picture {pixels = {"#6cb6ff", "#e6e8ee", "#e6e8ee", "#6cb6ff"}, size = {2, 2}, width = 2.5, filter = "nearest"}
scene:text {id = "caption", text = "Four inline pixels, enlarged with nearest filtering", point = {0, -2.25}, size = 16, fill = "muted"}

return scene
```

```viscript-js
const scene = vis.scene({
    width: 960,
    height: 540,
});

scene.picture({pixels: ["#6cb6ff", "#e6e8ee", "#e6e8ee", "#6cb6ff"], size: [2, 2], width: 2.5, filter: "nearest"});
scene.text({id: "caption", text: "Four inline pixels, enlarged with nearest filtering", point: [0, -2.25], size: 16, fill: "muted"});

return scene;
```

### scene.cell

<!-- api-example: scene.cell -->

```viscript
local scene = vis.scene {
    width = 960,
    height = 540,
}

scene:cell {origin = {-2, -1}, size = {4, 2}, color = "#3a4152", mode = "padd", padding = 0.06, patches = {{region = {1, 0, 2, 1}, color = "accent"}}}
scene:text {id = "caption", text = "A 4 x 2 grid with a two-cell color patch", point = {0, -2.25}, size = 16, fill = "muted"}

return scene
```

```viscript-js
const scene = vis.scene({
    width: 960,
    height: 540,
});

scene.cell({origin: [-2, -1], size: [4, 2], color: "#3a4152", mode: "padd", padding: 0.06, patches: [{region: [1, 0, 2, 1], color: "accent"}]});
scene.text({id: "caption", text: "A 4 x 2 grid with a two-cell color patch", point: [0, -2.25], size: 16, fill: "muted"});

return scene;
```
