---
title: Built parts
id: diagram-parts
order: 3
parent: diagram
---

# Built parts

Select individual diagram parts using the semantic handles returned by [the builder](./diagram-builder.md). Each call returns an ordinary Object or Group for styling and animation.

## Nodes

| Lua | JavaScript | Purpose |
| --- | --- | --- |
| `built:node(node)` | — | Return the node's complete Group. |
| `built:node_body(node)` | — | Return the node's body shape. |
| `built:node_label(node)` | — | Return the node's label. |
| `built:node_detail(node)` | — | Return the node's detail text, or `nil` if absent. |

## Edges

| Lua | JavaScript | Purpose |
| --- | --- | --- |
| `built:edge(edge)` | — | Return the edge's route object. |
| `built:edge_label(edge)` | — | Return the edge's label, or `nil` if absent. |

## Zones

| Lua | JavaScript | Purpose |
| --- | --- | --- |
| `built:zone(zone)` | — | Return the zone's complete Group. |
| `built:zone_body(zone)` | — | Return the zone's background/border shape. |
| `built:zone_label(zone)` | — | Return the zone's label. |

## Examples

### built.node

<!-- api-example: built.node -->

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
local selected = built:node(output)
scene:set {target = selected, stroke = "accent"}

return scene
```

### built.node_body

<!-- api-example: built.node_body -->

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
local selected = built:node_body(output)
scene:set {target = selected, stroke = "accent"}

return scene
```

### built.node_label

<!-- api-example: built.node_label -->

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
local selected = built:node_label(output)
scene:set {target = selected, fill = "accent"}

return scene
```

### built.node_detail

<!-- api-example: built.node_detail -->

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
local selected = built:node_detail(output)
scene:set {target = selected, fill = "accent"}

return scene
```

### built.edge

<!-- api-example: built.edge -->

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
local selected = built:edge(edge)
scene:set {target = selected, stroke = "accent"}

return scene
```

### built.edge_label

<!-- api-example: built.edge_label -->

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
local selected = built:edge_label(edge)
scene:set {target = selected, fill = "accent"}

return scene
```

### built.zone

<!-- api-example: built.zone -->

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
local selected = built:zone(zone)
scene:set {target = selected, stroke = "accent"}

return scene
```

### built.zone_body

<!-- api-example: built.zone_body -->

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
local selected = built:zone_body(zone)
scene:set {target = selected, stroke = "accent"}

return scene
```

### built.zone_label

<!-- api-example: built.zone_label -->

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
local selected = built:zone_label(zone)
scene:set {target = selected, fill = "accent"}

return scene
```
