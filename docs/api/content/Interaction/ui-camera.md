---
title: UI Camera Controls
id: ui-camera
order: 2
parent: interaction
---

# UI Camera Controls

Apply persistent camera adjustments through a UI Panel during scene loading. These adjustments compose with the authored camera timeline. Use button camera bindings or the [host API](../Host/host-input.md) for changes after loading.

## Functions

| Lua | JavaScript | Purpose |
| --- | --- | --- |
| `panel:camera(action, delta?)` | — | Apply a camera action immediately; return the Panel. |

Create the Panel with `vis.ui.panel(scene)`. Camera actions use the same names as [button bindings](./ui-controls.md) and host camera input.

| Action | Delta | Effect |
| --- | --- | --- |
| `"pan"` | Required two-number vector. | Add a pan adjustment. |
| `"orbit"` | Required two-number vector. | Add a 3D orbit adjustment. |
| `"zoom"` | Required number. | Add a zoom adjustment. |
| `"reset"` | Omit. | Clear Panel camera adjustments and its view override. |
| `"view2d"` | Omit. | Override the sampled camera with a 2D view. |
| `"view3d"` | Omit. | Override the sampled camera with a 3D view. |

Deltas must be finite. Vectors accept `{x, y}` or `{x = ..., y = ...}`. A view override lasts until another view action or `panel:camera("reset")`.

## Examples

### panel.camera

<!-- api-example: panel.camera -->

```viscript interactive
local scene = vis.scene {
    width = 960,
    height = 540,
    camera = {mode = "interactive", view = "3d"},
}

if not vis.ui then error("this scene requires -Dmodules=interactive") end
local panel = vis.ui.panel(scene)
local dot = scene:circle {id = "dot", radius = 0.45, fill = "accent"}
scene:space {x = {-3, 3, 1}, y = {-2, 2, 1}, z = {-2, 2, 1}, stroke = "muted", width = 1}

panel:camera("pan", {0.2, 0})
panel:camera("orbit", {0.4, 0.2})
panel:camera("zoom", 0.5)

return scene
```
