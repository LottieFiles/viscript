---
title: Retained Runtime
id: retained-runtime
order: 4
parent: interaction
---

# Retained Runtime

The Runtime keeps the Lua VM alive. Its `update(runtime, dt, time, tick)` callback runs at a fixed interval; `time` is simulation time and `tick` starts at 1. Defaults are `fixed_step = 1 / 120` seconds and `max_steps = 8`; valid ranges are `(0, 1]` and `[1, 64]`.

## Functions

| Lua | JavaScript | Purpose |
| --- | --- | --- |
| `vis.runtime(scene, options)` | — | Create a retained Runtime with required `update` callback and optional `fixed_step`/`max_steps`. |
| `runtime:update(object, state)` | — | Replace an Object's retained overlay inside the callback; accepts `origin`, `shift`, `rotation`, `scale`, `opacity`, `progress` and `fill`. |
| `runtime:clear(object?)` | — | Clear one Object's overlay, or all overlays when omitted; callback only. |
| `runtime:sound(asset, options?)` | — | Queue a sound inside the callback; options are `bus`, `gain`, `rate`, `loop`; returns false when the queue is full. |
| `runtime:bind_key(action, key, value?)` | — | Bind a key to a named action before loading finishes; default value is `{1, 0}`. |
| `runtime:bind_pointer(action, button, value?, pointerId?)` | — | Bind pointer button 0–31 to an action; defaults are `{1, 0}` and pointer ID 0. |
| `runtime:action(name)` | — | Read action `previous`, `value`, `delta`, `down`, `pressed` and `released` inside the callback. |
| `runtime:key(key)` | — | Read key `begin`, `end`, `previous`, `down`, `pressed` and `released` inside the callback. |
| `runtime:pointer(pointerId?)` | — | Read `position`, `delta`, `wheel`, `pointer`, `previous_buttons`, `buttons`, `pressed`, `released` and `active`; defaults to pointer 0, returns nil for an unknown pointer; callback only. |

Overlay updates commit together after a successful callback. Opacity multiplies authored opacity. At most 512 Object overlays and 64 sound requests per host advance are retained. Sound buses are `music`, `effect`, `ui`; gain is 0–4 and playback rate is 0.125–8. Input edges, pointer delta and wheel are consumed only by the first catch-up step.

## Examples

### vis.runtime

<!-- api-example: vis.runtime -->

```viscript interactive
local scene = vis.scene {
    width = 960,
    height = 540,
}

if not vis.runtime then error("this scene requires -Dmodules=interactive") end
local dot = scene:circle {id = "dot", radius = 0.45, fill = "accent"}
scene:text {id = "hint", text = "A fixed-step callback moves the circle", point = {0, 2}, size = 18, fill = "foreground"}
local runtime = vis.runtime(scene, {
    fixed_step = 1 / 60,
    update = function(runtime, dt, time, tick)
        runtime:update(dot, {shift = {2 * math.sin(time), 0}})
    end,
})

return scene
```

### runtime.update

<!-- api-example: runtime.update -->

```viscript interactive
local scene = vis.scene {
    width = 960,
    height = 540,
}

if not vis.runtime then error("this scene requires -Dmodules=interactive") end
local dot = scene:circle {id = "dot", radius = 0.45, fill = "accent"}
scene:text {id = "hint", text = "Retained overlays move the existing circle", point = {0, 2}, size = 18, fill = "foreground"}
local runtime = vis.runtime(scene, {
    fixed_step = 1 / 60,
    update = function(runtime, dt, time, tick)
        runtime:update(dot, {shift = {2 * math.sin(time), 0}})
    end,
})

return scene
```

### runtime.clear

<!-- api-example: runtime.clear -->

```viscript interactive
local scene = vis.scene {
    width = 960,
    height = 540,
}

if not vis.runtime then error("this scene requires -Dmodules=interactive") end
local dot = scene:circle {id = "dot", radius = 0.45, fill = "accent"}
scene:text {id = "hint", text = "The overlay clears once every two seconds", point = {0, 2}, size = 18, fill = "foreground"}
local runtime = vis.runtime(scene, {
    fixed_step = 1 / 60,
    update = function(runtime, dt, time, tick)
        if time % 2 < 1 then
            runtime:update(dot, {shift = {2, 0}})
        else
            runtime:clear(dot)
        end
    end,
})

return scene
```

### runtime.sound

In Obsidian, click the scene to focus it, then press **Space**. The example plays the vault's `assets/tone.wav`; use WAV files for browser and Obsidian playback.

<!-- api-example: runtime.sound -->

```viscript interactive
local scene = vis.scene {
    width = 960,
    height = 540,
}

if not vis.runtime then error("this scene requires -Dmodules=interactive") end
local dot = scene:circle {id = "dot", radius = 0.45, fill = "accent"}
scene:text {id = "hint", text = "Press Space to queue a sound and move the circle", point = {0, 2}, size = 18, fill = "foreground"}
local runtime = vis.runtime(scene, {
    fixed_step = 1 / 60,
    update = function(runtime, dt, time, tick)
        local key = runtime:key("Space")
        if key.pressed then runtime:sound("tone.wav", {gain = 0.25}) end
        runtime:update(dot, {shift = {key.down and 2 or 0, 0}})
    end,
})

return scene
```

### runtime.bind_key

<!-- api-example: runtime.bind_key -->

```viscript interactive
local scene = vis.scene {
    width = 960,
    height = 540,
}

if not vis.runtime then error("this scene requires -Dmodules=interactive") end
local dot = scene:circle {id = "dot", radius = 0.45, fill = "accent"}
scene:text {id = "hint", text = "Hold D: the Move action shifts the circle", point = {0, 2}, size = 18, fill = "foreground"}
local runtime = vis.runtime(scene, {
    fixed_step = 1 / 60,
    update = function(runtime, dt, time, tick)
        local action = runtime:action("Move")
        runtime:update(dot, {shift = {2 * action.value.x, 0}})
    end,
})
runtime:bind_key("Move", "D", {1, 0})

return scene
```

### runtime.bind_pointer

<!-- api-example: runtime.bind_pointer -->

```viscript interactive
local scene = vis.scene {
    width = 960,
    height = 540,
}

if not vis.runtime then error("this scene requires -Dmodules=interactive") end
local dot = scene:circle {id = "dot", radius = 0.45, fill = "accent"}
scene:text {id = "hint", text = "Hold the pointer button: the Press action grows", point = {0, 2}, size = 18, fill = "foreground"}
local runtime = vis.runtime(scene, {
    fixed_step = 1 / 60,
    update = function(runtime, dt, time, tick)
        local action = runtime:action("Press")
        runtime:update(dot, {scale = {1 + action.value.x, 1 + action.value.x}})
    end,
})
runtime:bind_pointer("Press", 0, {1, 0})

return scene
```

### runtime.action

<!-- api-example: runtime.action -->

```viscript interactive
local scene = vis.scene {
    width = 960,
    height = 540,
}

if not vis.runtime then error("this scene requires -Dmodules=interactive") end
local dot = scene:circle {id = "dot", radius = 0.45, fill = "accent"}
scene:text {id = "hint", text = "Hold D to read the action value", point = {0, 2}, size = 18, fill = "foreground"}
local runtime = vis.runtime(scene, {
    fixed_step = 1 / 60,
    update = function(runtime, dt, time, tick)
        local action = runtime:action("Move")
        runtime:update(dot, {shift = {2 * action.value.x, 0}})
    end,
})
runtime:bind_key("Move", "D", {1, 0})

return scene
```

### runtime.key

<!-- api-example: runtime.key -->

```viscript interactive
local scene = vis.scene {
    width = 960,
    height = 540,
}

if not vis.runtime then error("this scene requires -Dmodules=interactive") end
local dot = scene:circle {id = "dot", radius = 0.45, fill = "accent"}
scene:text {id = "hint", text = "Hold Space to read key.down", point = {0, 2}, size = 18, fill = "foreground"}
local runtime = vis.runtime(scene, {
    fixed_step = 1 / 60,
    update = function(runtime, dt, time, tick)
        local key = runtime:key("Space")
        runtime:update(dot, {scale = {key.down and 2 or 1, key.down and 2 or 1}})
    end,
})

return scene
```

### runtime.pointer

<!-- api-example: runtime.pointer -->

```viscript interactive
local scene = vis.scene {
    width = 960,
    height = 540,
}

if not vis.runtime then error("this scene requires -Dmodules=interactive") end
local dot = scene:circle {id = "dot", radius = 0.45, fill = "accent"}
scene:text {id = "hint", text = "Move the pointer to place the circle", point = {0, 2}, size = 18, fill = "foreground"}
local runtime = vis.runtime(scene, {
    fixed_step = 1 / 60,
    update = function(runtime, dt, time, tick)
        local pointer = runtime:pointer(0)
        if pointer and pointer.active then
            runtime:update(dot, {shift = {(pointer.position.x - 480) / 90, (270 - pointer.position.y) / 90}})
        end
    end,
})

return scene
```
