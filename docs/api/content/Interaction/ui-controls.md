---
title: Buttons, Sliders & Sampling
id: ui-controls
order: 1
parent: interaction
---

# Buttons, Sliders & Sampling

Controls use existing Scene Objects as visuals. A `region` is a logical Scene-pixel rectangle with `x`, `y`, `width` and `height`, given as named fields or four numbers. Transform tables accept `shift`, `scale`, `rotation`, `opacity` and `progress`.

## Functions

| Lua | JavaScript | Purpose |
| --- | --- | --- |
| `vis.ui.panel(scene)` | — | Create the Scene's UI Panel. |
| `panel:button(options)` | — | Add a button that transforms `target` or performs `camera`; accepts `visual`, `hover_visual`, `pressed_visual`, `region`, `delta`, `transform` and `duration`. |
| `panel:toggle_button(options)` | — | Toggle `target` between required `off` and `on` transforms; accepts `visual`, `region`, hover/pressed visuals, initial `value` and `duration`. |
| `panel:slider(options)` | — | Interpolate `target` between `from` and `to`; accepts `visual`, `region`, `axis`, initial normalized `value`, or multiple `bindings`. |
| `panel:sample_area(options)` | — | Sample isolated target-family RGBA inside `region`; optionally move `marker` through `marker_origin`/`marker_x`/`marker_y` and color `swatch`. |

Panel methods return the Panel. Buttons can transform objects or trigger camera actions; sliders use a horizontal or vertical axis. Click or drag the example canvas to try each control.

Button examples apply changes immediately. A nonzero duration requires the host to advance scene time.

## Examples

### vis.ui.panel

<!-- api-example: vis.ui.panel -->

```viscript interactive
local scene = vis.scene {
    width = 960,
    height = 540,
}

if not vis.ui then error("this scene requires -Dmodules=interactive") end
local panel = vis.ui.panel(scene)
local dot = scene:circle {id = "dot", radius = 0.45, fill = "accent"}
scene:text {id = "hint", text = "A panel connects existing objects to controls", point = {0, 2}, size = 18, fill = "foreground"}
local face = scene:rectangle {id = "button", center = {0, -1.5}, size = {2.4, 0.65}, stroke = "foreground", fill = "background"}
scene:text {id = "button-label", text = "Click here", point = {0, -1.5}, size = 16, fill = "foreground"}
panel:button {visual = face, region = {372, 376, 216, 58}, target = dot, transform = {shift = {2, 0, 0}}}

return scene
```

### panel.button

<!-- api-example: panel.button -->

```viscript interactive
local scene = vis.scene {
    width = 960,
    height = 540,
}

if not vis.ui then error("this scene requires -Dmodules=interactive") end
local panel = vis.ui.panel(scene)
local dot = scene:circle {id = "dot", radius = 0.45, fill = "accent"}
scene:text {id = "hint", text = "Click the button to move the circle", point = {0, 2}, size = 18, fill = "foreground"}
local face = scene:rectangle {id = "button", center = {0, -1.5}, size = {2.4, 0.65}, stroke = "foreground", fill = "background"}
scene:text {id = "button-label", text = "Click here", point = {0, -1.5}, size = 16, fill = "foreground"}
panel:button {visual = face, region = {372, 376, 216, 58}, target = dot, transform = {shift = {2, 0, 0}}}

return scene
```

### panel.toggle_button

<!-- api-example: panel.toggle_button -->

```viscript interactive
local scene = vis.scene {
    width = 960,
    height = 540,
}

if not vis.ui then error("this scene requires -Dmodules=interactive") end
local panel = vis.ui.panel(scene)
local dot = scene:circle {id = "dot", radius = 0.45, fill = "accent"}
scene:text {id = "hint", text = "Click again to switch off", point = {0, 2}, size = 18, fill = "foreground"}
local face = scene:rectangle {id = "button", center = {0, -1.5}, size = {2.4, 0.65}, stroke = "foreground", fill = "background"}
scene:text {id = "button-label", text = "Click here", point = {0, -1.5}, size = 16, fill = "foreground"}
panel:toggle_button {visual = face, region = {372, 376, 216, 58}, target = dot,
    off = {shift = {-2, 0, 0}}, on = {shift = {2, 0, 0}}}

return scene
```

### panel.slider

<!-- api-example: panel.slider -->

```viscript interactive
local scene = vis.scene {
    width = 960,
    height = 540,
}

if not vis.ui then error("this scene requires -Dmodules=interactive") end
local panel = vis.ui.panel(scene)
local dot = scene:circle {id = "dot", radius = 0.45, fill = "accent"}
scene:text {id = "hint", text = "Drag the handle to place the circle", point = {0, 2}, size = 18, fill = "foreground"}
scene:line {from = {-3, -1.5}, to = {3, -1.5}, stroke = "muted", width = 2}
local handle = scene:circle {id = "handle", center = {0, -1.5}, radius = 0.16, fill = "foreground"}
panel:slider {visual = handle, region = {210, 385, 540, 40}, value = 0.5, bindings = {
    {target = dot, from = {shift = {-3, 0, 0}}, to = {shift = {3, 0, 0}}},
    {target = handle, from = {shift = {-3, 0, 0}}, to = {shift = {3, 0, 0}}},
}}

return scene
```

### panel.sample_area

<!-- api-example: panel.sample_area -->

```viscript interactive
local scene = vis.scene {
    width = 960,
    height = 540,
}

if not vis.ui then error("this scene requires -Dmodules=interactive") end
local panel = vis.ui.panel(scene)
scene:text {id = "hint", text = "Click the painted squares to sample their color", point = {0, 2}, size = 18, fill = "foreground"}
local a = scene:rectangle {id = "sample-a", center = {-1, 0}, size = {1.8, 1.8}, fill = "accent", stroke = "#00000000"}
local b = scene:rectangle {id = "sample-b", center = {1, 0}, size = {1.8, 1.8}, fill = "foreground", stroke = "#00000000"}
local swatch = scene:rectangle {id = "swatch", center = {3.4, 0}, size = {0.7, 0.7}, fill = "muted"}
panel:sample_area {region = {290, 170, 380, 200}, targets = {a, b}, swatch = swatch}

return scene
```
