---
title: Arcs, Angles & Braces
id: arcs-angles-braces
order: 2
parent: object
---

# Arcs, Angles & Braces

Mark a circular sweep, an angle, or the span of an object. Arc angles use radians.

| Lua | JavaScript | Purpose |
| --- | --- | --- |
| `scene:arc(opts)` | `scene.arc(opts?)` | Draw an arc using `center`, `radius`, `start`, and signed `angle` in radians; `sector` fills a pie. |
| `scene:angle(opts)` | `scene.angle(opts?)` | Mark the angle from `from` to `to` about `vertex`; configure `radius`, `other`, `right`, and an optional `label`. |
| `scene:brace(opts)` | `scene.brace(opts)` | Span `around` handles or `from`/`to` with a brace; configure `direction`, `offset`, `depth`, `label`, and `gap`. Returns brace and optional label as two Lua values / a JS pair. |

`brace` returns its brace and optional label as separate handles; animate or style each handle independently.

## Examples

### scene.arc

<!-- api-example: scene.arc -->

```viscript
local scene = vis.scene {
    width = 960,
    height = 540,
}

scene:arc {radius = 1.4, start = 0, angle = math.pi * 1.5, stroke = "accent", width = 2}
scene:text {id = "caption", text = "A counterclockwise sweep of 3 pi / 2", point = {0, -2.25}, size = 16, fill = "muted"}

return scene
```

```viscript-js
const scene = vis.scene({
    width: 960,
    height: 540,
});

scene.arc({radius: 1.4, start: 0, angle: Math.PI * 1.5, stroke: "accent", width: 2});
scene.text({id: "caption", text: "A counterclockwise sweep of 3 pi / 2", point: [0, -2.25], size: 16, fill: "muted"});

return scene;
```

### scene.angle

<!-- api-example: scene.angle -->

```viscript
local scene = vis.scene {
    width = 960,
    height = 540,
}

local tip = {1, math.sqrt(3)}
scene:line {from = {0, 0}, to = {2, 0}, stroke = "muted"}
scene:line {from = {0, 0}, to = tip, stroke = "muted"}
scene:angle {vertex = {0, 0}, from = {2, 0}, to = tip, radius = 0.7, label = "60 deg", stroke = "accent", width = 2}
scene:text {id = "caption", text = "Mark the angle between two rays", point = {0, -2.25}, size = 16, fill = "muted"}

return scene
```

```viscript-js
const scene = vis.scene({
    width: 960,
    height: 540,
});

const tip = [1, Math.sqrt(3)];
scene.line({from: [0, 0], to: [2, 0], stroke: "muted"});
scene.line({from: [0, 0], to: tip, stroke: "muted"});
scene.angle({vertex: [0, 0], from: [2, 0], to: tip, radius: 0.7, label: "60 deg", stroke: "accent", width: 2});
scene.text({id: "caption", text: "Mark the angle between two rays", point: [0, -2.25], size: 16, fill: "muted"});

return scene;
```

### scene.brace

<!-- api-example: scene.brace -->

```viscript
local scene = vis.scene {
    width = 960,
    height = 540,
}

local box = scene:rectangle {size = {3, 1}, stroke = "foreground"}
local brace, label = scene:brace {around = box, direction = "down", label = "3 units", stroke = "accent", offset = 0.3}
scene:text {id = "caption", text = "The brace and label are separate handles", point = {0, -2.25}, size = 16, fill = "muted"}

return scene
```

```viscript-js
const scene = vis.scene({
    width: 960,
    height: 540,
});

const box = scene.rectangle({size: [3, 1], stroke: "foreground"});
const [brace, label] = scene.brace({around: box, direction: "down", label: "3 units", stroke: "accent", offset: 0.3});
scene.text({id: "caption", text: "The brace and label are separate handles", point: [0, -2.25], size: 16, fill: "muted"});

return scene;
```
