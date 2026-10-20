---
title: State Events
id: state-events
order: 7
parent: state-definitions
---

# State Events

Inspect and acknowledge the transition journal. Each event records one target's transition; events from a batch share a transaction.

## Functions

| Lua | JavaScript | Purpose |
| --- | --- | --- |
| `states:event_count()` | — | Return the number of recorded transition events. |
| `states:event(index)` | — | Return a 1-based event or nil; fields are `time`, `transaction`, `object`, `state`, `duration` and `curve`. |
| `states:clear_events()` | — | Clear the recorded transition events. |

## Examples

### states.event_count

<!-- api-example: states.event_count -->

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
scene:text {id = "value", text = "events = " .. states:event_count(), point = {0, 2}, size = 18}

return scene
```

### states.event

<!-- api-example: states.event -->

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
local event = states:event(1)
scene:text {id = "value", text = "event.state = " .. event.state, point = {0, 2}, size = 18}

return scene
```

### states.clear_events

<!-- api-example: states.clear_events -->

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
states:clear_events()
scene:text {id = "value", text = "events after clear = " .. states:event_count(), point = {0, 2}, size = 18}

return scene
```
