---
title: Boolean Regions
id: boolean-regions
order: 4
parent: object
---

# Boolean Regions

Combine closed shapes into a new closed path. Each operation takes `of` with 2–64 circles, rectangles, polygons, or closed paths.

| Lua | JavaScript | Purpose |
| --- | --- | --- |
| `scene:union(opts)` | `scene.union(opts)` | Combine closed shapes in `of` into the region covered by any input. |
| `scene:intersection(opts)` | `scene.intersection(opts)` | Keep the region covered by every shape in `of`. |
| `scene:difference(opts)` | `scene.difference(opts)` | Subtract all later shapes in `of` from its first shape. |
| `scene:exclusion(opts)` | `scene.exclusion(opts)` | Keep regions covered by an odd number of shapes in `of`. |

## Examples

### scene.union

<!-- api-example: scene.union -->

```viscript
local scene = vis.scene {
    width = 960,
    height = 540,
}

local a = scene:circle {center = {-0.55, 0}, radius = 1.2, stroke = "border"}
local b = scene:circle {center = {0.55, 0}, radius = 1.2, stroke = "border"}
scene:union {of = {a, b}, fill = "#6cb6ff88", stroke = "accent", width = 2}
scene:text {id = "caption", text = "Keep either circle", point = {0, -2.25}, size = 16, fill = "muted"}

return scene
```

```viscript-js
const scene = vis.scene({
    width: 960,
    height: 540,
});

const a = scene.circle({center: [-0.55, 0], radius: 1.2, stroke: "border"});
const b = scene.circle({center: [0.55, 0], radius: 1.2, stroke: "border"});
scene.union({of: [a, b], fill: "#6cb6ff88", stroke: "accent", width: 2});
scene.text({id: "caption", text: "Keep either circle", point: [0, -2.25], size: 16, fill: "muted"});

return scene;
```

### scene.intersection

<!-- api-example: scene.intersection -->

```viscript
local scene = vis.scene {
    width = 960,
    height = 540,
}

local a = scene:circle {center = {-0.55, 0}, radius = 1.2, stroke = "border"}
local b = scene:circle {center = {0.55, 0}, radius = 1.2, stroke = "border"}
scene:intersection {of = {a, b}, fill = "#6cb6ff88", stroke = "accent", width = 2}
scene:text {id = "caption", text = "Keep their overlap", point = {0, -2.25}, size = 16, fill = "muted"}

return scene
```

```viscript-js
const scene = vis.scene({
    width: 960,
    height: 540,
});

const a = scene.circle({center: [-0.55, 0], radius: 1.2, stroke: "border"});
const b = scene.circle({center: [0.55, 0], radius: 1.2, stroke: "border"});
scene.intersection({of: [a, b], fill: "#6cb6ff88", stroke: "accent", width: 2});
scene.text({id: "caption", text: "Keep their overlap", point: [0, -2.25], size: 16, fill: "muted"});

return scene;
```

### scene.difference

<!-- api-example: scene.difference -->

```viscript
local scene = vis.scene {
    width = 960,
    height = 540,
}

local a = scene:circle {center = {-0.55, 0}, radius = 1.2, stroke = "border"}
local b = scene:circle {center = {0.55, 0}, radius = 1.2, stroke = "border"}
scene:difference {of = {a, b}, fill = "#6cb6ff88", stroke = "accent", width = 2}
scene:text {id = "caption", text = "Subtract the right circle from the left", point = {0, -2.25}, size = 16, fill = "muted"}

return scene
```

```viscript-js
const scene = vis.scene({
    width: 960,
    height: 540,
});

const a = scene.circle({center: [-0.55, 0], radius: 1.2, stroke: "border"});
const b = scene.circle({center: [0.55, 0], radius: 1.2, stroke: "border"});
scene.difference({of: [a, b], fill: "#6cb6ff88", stroke: "accent", width: 2});
scene.text({id: "caption", text: "Subtract the right circle from the left", point: [0, -2.25], size: 16, fill: "muted"});

return scene;
```

### scene.exclusion

<!-- api-example: scene.exclusion -->

```viscript
local scene = vis.scene {
    width = 960,
    height = 540,
}

local a = scene:circle {center = {-0.55, 0}, radius = 1.2, stroke = "border"}
local b = scene:circle {center = {0.55, 0}, radius = 1.2, stroke = "border"}
scene:exclusion {of = {a, b}, fill = "#6cb6ff88", stroke = "accent", width = 2}
scene:text {id = "caption", text = "Keep everything except their overlap", point = {0, -2.25}, size = 16, fill = "muted"}

return scene
```

```viscript-js
const scene = vis.scene({
    width: 960,
    height: 540,
});

const a = scene.circle({center: [-0.55, 0], radius: 1.2, stroke: "border"});
const b = scene.circle({center: [0.55, 0], radius: 1.2, stroke: "border"});
scene.exclusion({of: [a, b], fill: "#6cb6ff88", stroke: "accent", width: 2});
scene.text({id: "caption", text: "Keep everything except their overlap", point: [0, -2.25], size: 16, fill: "muted"});

return scene;
```
