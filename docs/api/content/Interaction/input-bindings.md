---
title: Pointer & Key Bindings
id: input-bindings
order: 3
parent: interaction
---

# Pointer & Key Bindings

Declare how existing objects follow a pointer or respond to held keys. Pointer regions use logical Scene pixels; movement uses the target's parent space.

## Functions

| Lua | JavaScript | Purpose |
| --- | --- | --- |
| `vis.input.controller(scene)` | — | Create the Scene's declarative Input controller. |
| `input:pointer_follow(options)` | — | Map pointer position in `region` to `target` using required `map_origin`, `map_x` and `map_y`; accepts `target_origin`, `period`, `rotate`, `angle_offset` and `reset_on_leave`. |
| `input:key_move(options)` | — | Shift `target` while `key` is held; requires `shift`, with optional smoothing `period`. |

Input methods return the controller. Keys are `ArrowLeft`, `ArrowRight`, `ArrowUp`, `ArrowDown`, `Space`, `Enter`, `Escape`, `Plus`, `Minus`, `Shift`, `Tab`, letters A–Z and digits 0–9. Letters are case-insensitive.

## Examples

### vis.input.controller

<!-- api-example: vis.input.controller -->

```viscript interactive
local scene = vis.scene {
    width = 960,
    height = 540,
}

if not vis.input then error("this scene requires -Dmodules=interactive") end
local dot = scene:circle {id = "dot", radius = 0.45, fill = "accent"}
scene:text {id = "hint", text = "Move the pointer inside the frame", point = {0, 2}, size = 18, fill = "foreground"}
scene:rectangle {center = {0, -0.25}, size = {8, 3.5}, stroke = "muted", fill = "#00000000"}
local input = vis.input.controller(scene)
input:pointer_follow {target = dot, region = {120, 135, 720, 315},
    map_origin = {-4, 1.5}, map_x = {8, 0}, map_y = {0, -3.5}, period = 0.12}

return scene
```

### input.pointer_follow

<!-- api-example: input.pointer_follow -->

```viscript interactive
local scene = vis.scene {
    width = 960,
    height = 540,
}

if not vis.input then error("this scene requires -Dmodules=interactive") end
local dot = scene:circle {id = "dot", radius = 0.45, fill = "accent"}
scene:text {id = "hint", text = "Move the pointer inside the frame", point = {0, 2}, size = 18, fill = "foreground"}
scene:rectangle {center = {0, -0.25}, size = {8, 3.5}, stroke = "muted", fill = "#00000000"}
local input = vis.input.controller(scene)
input:pointer_follow {target = dot, region = {120, 135, 720, 315},
    map_origin = {-4, 1.5}, map_x = {8, 0}, map_y = {0, -3.5}, period = 0.12}

return scene
```

### input.key_move

<!-- api-example: input.key_move -->

```viscript interactive
local scene = vis.scene {
    width = 960,
    height = 540,
}

if not vis.input then error("this scene requires -Dmodules=interactive") end
local dot = scene:circle {id = "dot", radius = 0.45, fill = "accent"}
scene:text {id = "hint", text = "Focus the canvas, then hold D", point = {0, 2}, size = 18, fill = "foreground"}
local input = vis.input.controller(scene)
input:key_move {target = dot, key = "D", shift = {2, 0}, period = 0.3}

return scene
```
