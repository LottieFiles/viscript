---
title: Built parts
id: diagram-parts
order: 3
parent: diagram
---

# Built parts

Select generated objects using semantic handles from the same diagram. These selectors return ordinary scene objects for styling and animation after `build()`.

| Lua | JavaScript | Purpose |
| --- | --- | --- |
| `built:object(handle)` | — | Return a node or zone's generated Group, or an edge's route object. |
| `built:body(handle)` | — | Return a node or zone's body shape. Edge handles are rejected; use `object(edge)` for the route. |
| `built:label(handle)` | — | Return a node, edge, or zone's label. Return `nil` for an edge without a label. |
| `built:detail(node)` | — | Return a node's detail text, or `nil` if absent. Only node handles are accepted. |

A handle from another diagram is rejected. Use [Built groups](./diagram-groups.md) to select an entire layer.

## Examples

### built.object

<!-- api-example: built.object -->

```viscript
local scene = vis.scene {
    width = 960,
    height = 540,
}

local diagram = vis.diagram(scene, {
    id = "flow", layout = "manual", node_size = {2.3, 1},
})
local input = diagram:node {id = "input", label = "Input", detail = "source", position = {-2, 0}}
local output = diagram:node {id = "output", label = "Output", detail = "result", position = {2, 0}}
local edge = diagram:connect {id = "process", from = input, to = output, label = "process"}
local zone = diagram:zone {id = "pipeline", label = "Pipeline", members = {input, output}}
local built = diagram:build()
local selected = built:object(output)
scene:set {target = selected, stroke = "accent"}

return scene
```

### built.body

<!-- api-example: built.body -->

```viscript
local scene = vis.scene {
    width = 960,
    height = 540,
}

local diagram = vis.diagram(scene, {
    id = "flow", layout = "manual", node_size = {2.3, 1},
})
local input = diagram:node {id = "input", label = "Input", detail = "source", position = {-2, 0}}
local output = diagram:node {id = "output", label = "Output", detail = "result", position = {2, 0}}
local edge = diagram:connect {id = "process", from = input, to = output, label = "process"}
local zone = diagram:zone {id = "pipeline", label = "Pipeline", members = {input, output}}
local built = diagram:build()
local selected = built:body(output)
scene:set {target = selected, stroke = "accent"}

return scene
```

### built.label

<!-- api-example: built.label -->

```viscript
local scene = vis.scene {
    width = 960,
    height = 540,
}

local diagram = vis.diagram(scene, {
    id = "flow", layout = "manual", node_size = {2.3, 1},
})
local input = diagram:node {id = "input", label = "Input", detail = "source", position = {-2, 0}}
local output = diagram:node {id = "output", label = "Output", detail = "result", position = {2, 0}}
local edge = diagram:connect {id = "process", from = input, to = output, label = "process"}
local zone = diagram:zone {id = "pipeline", label = "Pipeline", members = {input, output}}
local built = diagram:build()
local selected = built:label(output)
scene:set {target = selected, fill = "accent"}

return scene
```

### built.detail

<!-- api-example: built.detail -->

```viscript
local scene = vis.scene {
    width = 960,
    height = 540,
}

local diagram = vis.diagram(scene, {
    id = "flow", layout = "manual", node_size = {2.3, 1},
})
local input = diagram:node {id = "input", label = "Input", detail = "source", position = {-2, 0}}
local output = diagram:node {id = "output", label = "Output", detail = "result", position = {2, 0}}
local edge = diagram:connect {id = "process", from = input, to = output, label = "process"}
local zone = diagram:zone {id = "pipeline", label = "Pipeline", members = {input, output}}
local built = diagram:build()
local selected = built:detail(output)
scene:set {target = selected, fill = "accent"}

return scene
```
