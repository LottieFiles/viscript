---
title: Built groups
id: diagram-groups
order: 2
parent: diagram
---

# Built groups

Use the Built handle returned by [the builder](./diagram-builder.md) to select whole diagram layers. Animate these ordinary Groups with [Animation](../Animation/index.md).

## Whole diagram and layers

| Lua | JavaScript | Purpose |
| --- | --- | --- |
| `built:root()` | — | Return the whole diagram's root Group. |
| `built:zones()` | — | Return the Group containing all zone objects. |
| `built:routes()` | — | Return the Group containing all edge routes. |
| `built:nodes()` | — | Return the Group containing all node objects. |
| `built:annotations()` | — | Return the Group containing diagram annotations, including edge labels. |

## Examples

### built.root

<!-- api-example: built.root -->

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
local selected = built:root()
scene:set {target = selected, stroke = "accent"}

return scene
```

### built.zones

<!-- api-example: built.zones -->

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
local selected = built:zones()
scene:set {target = selected, stroke = "accent"}

return scene
```

### built.routes

<!-- api-example: built.routes -->

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
local selected = built:routes()
scene:set {target = selected, stroke = "accent"}

return scene
```

### built.nodes

<!-- api-example: built.nodes -->

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
local selected = built:nodes()
scene:set {target = selected, stroke = "accent"}

return scene
```

### built.annotations

<!-- api-example: built.annotations -->

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
local selected = built:annotations()
scene:set {target = selected, fill = "accent"}

return scene
```
