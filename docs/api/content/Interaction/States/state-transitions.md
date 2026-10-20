---
title: State Transitions
id: state-transitions
order: 6
parent: state-definitions
---

# State Transitions

Retarget an object's state, advance the controller's independent clock and sample the result. Defaults are 0.2 seconds and the `smooth` curve.

## Functions

| Lua | JavaScript | Purpose |
| --- | --- | --- |
| `states:transition(object, name, duration?, curve?)` | — | Begin a transition to one named state; also accepts `(targets, duration?, curve?)` with `{object, state}` entries. |
| `states:advance(elapsed)` | — | Advance the controller's time and active transitions by elapsed seconds. |
| `states:sample(object)` | — | Return the Object's current state overlay. |
| `states:time()` | — | Return controller time in seconds. |
| `states:active()` | — | Report whether a state transition is still running. |

Create the controller and definitions on [State Definitions](./index.md). In a retained callback, call `states:advance(dt)` explicitly to advance transitions. Mutating methods return the controller.

## Examples

### states.transition

<!-- api-example: states.transition -->

```viscript interactive
local scene = vis.scene {
    width = 960,
    height = 540,
}

if not vis.states then error("this scene requires -Dmodules=interactive") end
local dot = scene:circle {id = "dot", radius = 0.45, fill = "accent"}
local states = vis.states(scene)
states:define(dot, "right", {shift = {2, 0}})
states:transition(dot, "right", 1, "linear")
if not vis.runtime then error("this scene requires -Dmodules=interactive") end
vis.runtime(scene, {update = function(runtime, dt) states:advance(dt) end})
scene:text {id = "hint", text = "The state moves from center to right", point = {0, 2}, size = 18, fill = "foreground"}

return scene
```

### states.advance

<!-- api-example: states.advance -->

```viscript
local scene = vis.scene {
    width = 960,
    height = 540,
}

if not vis.states then error("this scene requires -Dmodules=interactive") end
local dot = scene:circle {id = "dot", radius = 0.45, fill = "accent"}
local states = vis.states(scene)
states:define(dot, "right", {shift = {2, 0}})
states:transition(dot, "right", 1, "linear")
states:advance(0.5)
scene:text {id = "hint", text = "advance(0.5): half of the one-second transition", point = {0, 2}, size = 18, fill = "foreground"}

return scene
```

### states.sample

<!-- api-example: states.sample -->

```viscript
local scene = vis.scene {
    width = 960,
    height = 540,
}

if not vis.states then error("this scene requires -Dmodules=interactive") end
local dot = scene:circle {id = "dot", radius = 0.45, fill = "accent"}
local states = vis.states(scene)
states:define(dot, "right", {shift = {2, 0}})
states:transition(dot, "right", 1, "linear")
states:advance(0.5)
local value = states:sample(dot)
scene:text {id = "value", text = "shift.x = " .. value.shift[1], point = {0, 2}, size = 18}

return scene
```

### states.time

<!-- api-example: states.time -->

```viscript
local scene = vis.scene {
    width = 960,
    height = 540,
}

if not vis.states then error("this scene requires -Dmodules=interactive") end
local dot = scene:circle {id = "dot", radius = 0.45, fill = "accent"}
local states = vis.states(scene)
states:define(dot, "right", {shift = {2, 0}})
states:advance(0.75)
scene:text {id = "value", text = "time = " .. states:time(), point = {0, 2}, size = 18}

return scene
```

### states.active

<!-- api-example: states.active -->

```viscript
local scene = vis.scene {
    width = 960,
    height = 540,
}

if not vis.states then error("this scene requires -Dmodules=interactive") end
local dot = scene:circle {id = "dot", radius = 0.45, fill = "accent"}
local states = vis.states(scene)
states:define(dot, "right", {shift = {2, 0}})
states:transition(dot, "right", 1)
states:advance(0.4)
scene:text {id = "value", text = "active = " .. tostring(states:active()), point = {0, 2}, size = 18}

return scene
```
