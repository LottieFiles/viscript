---
title: Spring & Table Helpers
id: utility-helpers
order: 1
parent: utilities
---

# Spring & Table Helpers

Sample a physical spring or prepare deterministic table data while constructing a scene.

## Functions

| Lua | JavaScript | Purpose |
| --- | --- | --- |
| `vis.spring_response(seconds, options)` | `vis.springResponse(seconds, options)` | Return a physical spring's displacement at non-negative time; requires `mass`, `stiffness`, `damping`, `displacement`, with optional initial `velocity = 0`. |
| `vis.merge(a, b, ...)` | — | Return a shallow merged table; later fields win and nil arguments are skipped. |
| `vis.keys(table)` | — | Return deterministic keys: numbers ascending, then strings in byte order. |

Spring mass and stiffness must be positive, damping non-negative, and every number and result finite. Table helpers are Lua-only.

## Examples

### vis.spring_response

<!-- api-example: vis.spring_response -->

```viscript
local scene = vis.scene {
    width = 960,
    height = 540,
}

scene:space {x = {0, 4, 1}, y = {-1.5, 1.5, 0.5}, stroke = "muted", width = 1}
scene:plot {fn = function(t)
    return vis.spring_response(t, {mass = 1, stiffness = 16, damping = 2, displacement = 1})
end, x_range = {0, 4, 0.02}, stroke = "accent", width = 2}

return scene
```

```viscript-js
const scene = vis.scene({
    width: 960,
    height: 540,
});

scene.space({x: [0, 4, 1], y: [-1.5, 1.5, 0.5], stroke: "muted", width: 1});
scene.plot({fn: (t) => vis.springResponse(t, {mass: 1, stiffness: 16, damping: 2, displacement: 1}),
    x_range: [0, 4, 0.02], stroke: "accent", width: 2});

return scene;
```

### vis.merge

<!-- api-example: vis.merge -->

```viscript
local scene = vis.scene {
    width = 960,
    height = 540,
}

local defaults = {radius = 0.4, fill = "foreground"}
local custom = vis.merge(defaults, {center = {2, 0}, fill = "accent"})
scene:circle(defaults)
scene:circle(custom)
scene:text {id = "value", text = "later fields override defaults", point = {0, 1.6}, size = 24, fill = "accent"}

return scene
```

### vis.keys

<!-- api-example: vis.keys -->

```viscript
local scene = vis.scene {
    width = 960,
    height = 540,
}

local values = {z = 3, a = 1, m = 2}
local keys = vis.keys(values)
scene:text {id = "value", text = table.concat(keys, ", "), point = {0, 0}, size = 24, fill = "accent"}

return scene
```
