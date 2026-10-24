---
title: State Definitions
id: state-definitions
order: 5
parent: interaction
---

# State Definitions

Define named overlays once during loading, then reuse them for interaction. A state contains `origin`, `shift`, `rotation`, `scale`, `opacity` and `progress`.

## Functions

| Lua | JavaScript | Purpose |
| --- | --- | --- |
| `vis.states(scene)` | — | Create the Scene's named-state controller. |
| `states:define(object, name, state)` | — | Define a named Object state during loading. |
| `states:definition_count()` | — | Return the number of named state definitions. |

Use [State Transitions](./state-transitions.md) to apply definitions and [State Events](./state-events.md) to inspect the resulting journal.

## Examples

### vis.states

<!-- api-example: vis.states -->

```viscript
local scene = vis.scene {
    width = 960,
    height = 540,
}

if not vis.states then error("this scene requires -Dmodules=interactive") end
local dot = scene:circle {id = "dot", radius = 0.45, fill = "accent"}
local states = vis.states(scene)
states:define(dot, "right", {shift = {2, 0}})
states:transition(dot, "right", {duration = 1, curve = "linear"})
states:advance(0.5)
scene:text {id = "hint", text = "Named states: halfway to the right", point = {0, 2}, size = 18, fill = "foreground"}

return scene
```

### states.define

<!-- api-example: states.define -->

```viscript
local scene = vis.scene {
    width = 960,
    height = 540,
}

if not vis.states then error("this scene requires -Dmodules=interactive") end
local dot = scene:circle {id = "dot", radius = 0.45, fill = "accent"}
local states = vis.states(scene)
states:define(dot, "right", {shift = {2, 0}})
states:define(dot, "large", {scale = {2, 2}})
states:transition(dot, "large", {duration = 0})
scene:text {id = "hint", text = "The large state doubles the circle", point = {0, 2}, size = 18, fill = "foreground"}

return scene
```

### states.definition_count

<!-- api-example: states.definition_count -->

```viscript
local scene = vis.scene {
    width = 960,
    height = 540,
}

if not vis.states then error("this scene requires -Dmodules=interactive") end
local dot = scene:circle {id = "dot", radius = 0.45, fill = "accent"}
local states = vis.states(scene)
states:define(dot, "right", {shift = {2, 0}})
states:define(dot, "large", {scale = {2, 2}})
scene:text {id = "value", text = "definitions = " .. states:definition_count(), point = {0, 2}, size = 18}

return scene
```
