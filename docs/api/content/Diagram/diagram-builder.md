---
title: Builder
id: diagram-builder
order: 1
parent: diagram
---

# Builder

Declare nodes, edges and zones, then build once to attach an ordinary object tree to the scene. The optional diagram module is Lua-only.

## Declare and build

| Lua | JavaScript | Purpose |
| --- | --- | --- |
| `vis.diagram(scene, config)` | — | Create a diagram owned by `scene`; `config` is required and may be empty. |
| `diagram:node(opts)` | — | Declare a node with required `id`; returns a semantic node handle. |
| `diagram:connect(opts)` | — | Declare an edge with required `id`, `from`, and `to` node handles; returns an edge handle. |
| `diagram:zone(opts)` | — | Declare a zone with required `id` and a nonempty `members` list of node handles; returns a zone handle. |
| `diagram:build()` | — | Lay out and attach the object tree to the Scene; return a Built handle. Call once, after declarations. |

## Layout configuration

`config` accepts `id`, `layout`, `direction`, `origin`, `node_size`, `rank_gap`, `node_gap`, `time_unit`, `zone_padding`, `route_width`, `arrow_length`, `arrow_width`, `corner`, and `layers`. Layouts are `ranked`, `manual`, `grid`, and `timeline`; direction is `lr` / `left_to_right` or `tb` / `top_to_bottom`. `layers` names `zones`, `routes`, `nodes`, and `annotations`.

## Node options

Node options are `id`, `label`, `detail`, `kind`, `rank`, `row`, `column`, `start`, `span`, `position`, and `size`. Kinds are `default`, `state`, `decision`, `terminal`, `entity`, `cell`, `task`, and `evidence`. Grid indices are zero-based; manual layout uses `position`, and timeline layout uses `row`, `start`, and positive `span`.

## Edge options

Edge options are `id`, `label`, `from`, `to`, `from_port`, `to_port`, `route`, `kind`, `waypoints`, `flow`, and `flow_offset`. Ports are `auto`, `left`, `right`, `top`, and `bottom`; routes are `auto`, `straight`, and `orthogonal`; kinds are `directed`, `relation`, `return`, `error`, and `optional`. `flow` enables dashes for animating `dash_offset`.

## Zone options

Zone options are `id`, `label`, `members`, and `full_width`. Semantic handles must belong to the same diagram. Declarations stop after a successful build.

## Examples

### vis.diagram

<!-- api-example: vis.diagram -->

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
diagram:build()

return scene
```

### diagram.node

<!-- api-example: diagram.node -->

```viscript
local scene = vis.scene {
    width = 960,
    height = 540,
}

local diagram = vis.diagram(scene, {id = "single"})
local node = diagram:node {id = "task", label = "Task", detail = "one semantic node"}
diagram:build()

return scene
```

### diagram.connect

<!-- api-example: diagram.connect -->

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
scene:set {target = built:edge(edge), stroke = "accent"}

return scene
```

### diagram.zone

<!-- api-example: diagram.zone -->

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
scene:set {target = built:zone_body(zone), stroke = "accent"}
scene:set {target = built:zone_label(zone), fill = "accent"}

return scene
```

### diagram.build

<!-- api-example: diagram.build -->

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
diagram:build()

return scene
```
