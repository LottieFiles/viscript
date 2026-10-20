---
title: UI Camera Controls
id: ui-camera
order: 2
parent: interaction
---

# UI Camera Controls

Pan, orbit, zoom, reset and switch views through the same UI Panel. These calls change an interactive camera immediately.

## Functions

| Lua | JavaScript | Purpose |
| --- | --- | --- |
| `panel:camera_move(delta)` | — | Pan the camera by a two-number delta. |
| `panel:camera_orbit(delta)` | — | Orbit the camera by a two-number delta. |
| `panel:camera_zoom(delta)` | — | Zoom the camera by a numeric delta. |
| `panel:camera_reset()` | — | Restore the opening camera. |
| `panel:camera_view(view)` | — | Switch the camera to `"2d"` or `"3d"`. |

Create the Panel with `vis.ui.panel(scene)`. Button camera actions are `move` / `pan`, `orbit`, `zoom`, `reset`, `view2d` and `view3d`. Use [controls](./ui-controls.md) to bind those actions to visible buttons.

## Examples

### panel.camera_move

<!-- api-example: panel.camera_move -->

```viscript interactive
local scene = vis.scene {
    width = 960,
    height = 540,
    camera = {mode = "interactive", view = "2d"},
}

if not vis.ui then error("this scene requires -Dmodules=interactive") end
local panel = vis.ui.panel(scene)
local dot = scene:circle {id = "dot", radius = 0.45, fill = "accent"}
scene:space {x = {-3, 3, 1}, y = {-2, 2, 1}, z = {-2, 2, 1}, stroke = "muted", width = 1}

panel:camera_move({0.2, 0})

return scene
```

### panel.camera_orbit

<!-- api-example: panel.camera_orbit -->

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

panel:camera_orbit({0.4, 0.2})

return scene
```

### panel.camera_zoom

<!-- api-example: panel.camera_zoom -->

```viscript interactive
local scene = vis.scene {
    width = 960,
    height = 540,
    camera = {mode = "interactive", view = "2d"},
}

if not vis.ui then error("this scene requires -Dmodules=interactive") end
local panel = vis.ui.panel(scene)
local dot = scene:circle {id = "dot", radius = 0.45, fill = "accent"}
scene:space {x = {-3, 3, 1}, y = {-2, 2, 1}, z = {-2, 2, 1}, stroke = "muted", width = 1}

panel:camera_zoom(0.5)

return scene
```

### panel.camera_reset

<!-- api-example: panel.camera_reset -->

```viscript interactive
local scene = vis.scene {
    width = 960,
    height = 540,
    camera = {mode = "interactive", view = "2d"},
}

if not vis.ui then error("this scene requires -Dmodules=interactive") end
local panel = vis.ui.panel(scene)
local dot = scene:circle {id = "dot", radius = 0.45, fill = "accent"}
scene:space {x = {-3, 3, 1}, y = {-2, 2, 1}, z = {-2, 2, 1}, stroke = "muted", width = 1}
panel:camera_move({1, 0})
panel:camera_reset()

return scene
```

### panel.camera_view

<!-- api-example: panel.camera_view -->

```viscript interactive
local scene = vis.scene {
    width = 960,
    height = 540,
    camera = {mode = "interactive", view = "2d"},
}

if not vis.ui then error("this scene requires -Dmodules=interactive") end
local panel = vis.ui.panel(scene)
local dot = scene:circle {id = "dot", radius = 0.45, fill = "accent"}
scene:space {x = {-3, 3, 1}, y = {-2, 2, 1}, z = {-2, 2, 1}, stroke = "muted", width = 1}

panel:camera_view("3d")

return scene
```
