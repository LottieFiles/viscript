---
title: State & Scheduling
id: state-scheduling
order: 1
parent: animation
---

# State & Scheduling

`tween` animates descriptor fields; `set` applies those fields immediately. Use `save` and `restore` together to return an object family to an earlier state. See [Animation](./index.md) for shared timing rules.

## Animate, apply and restore

| Lua | JavaScript | Purpose |
| --- | --- | --- |
| `scene:tween(specs, duration?, curve?, lag?)`<br />`scene:tween(specs, options)` | `scene.tween(specs, duration?, curve?, lag?)`<br />`scene.tween(specs, options)` | Animate one descriptor or a list; options: `duration`, `curve`, `lag`. |
| `scene:set(specs)` | `scene.set(specs)` | Apply descriptor values immediately: to the initial state before playback starts, or at the cursor afterwards. |
| `scene:save(targets)` | `scene.save(targets)` | Push the current state of each target family onto its own saved-state stack. |
| `scene:restore(targets, duration?, curve?, lag?)`<br />`scene:restore(targets, options)` | `scene.restore(targets, duration?, curve?, lag?)`<br />`scene.restore(targets, options)` | Pop and restore saved family states; default duration 0 applies immediately. Options: `duration`, `curve`, `lag`. |

Restore covers transforms, opacity, colors, gradients, sizes, layers, text and number values. It does not undo removal, warps, or switching between plain and counting text.

## Draw order

| Lua | JavaScript | Purpose |
| --- | --- | --- |
| `layer` | `layer` | Change draw order; the family uses the higher layer while animating. |

Each `tween` / `set` descriptor contains `target` and at least one property. Lua uses a table; JavaScript uses an object. Array/list forms animate distinct targets together. Property pages cover [movement](./movement.md), [paint](./paint-opacity.md), [shape geometry](./shape-properties.md), [plot data](./plot-properties.md) and [text](./text-properties.md).

## Examples

### scene.tween

<!-- api-example: scene.tween -->

```viscript
local scene = vis.scene {
    width = 960,
    height = 540,
}

scene:text {id = "caption", text = "tween", point = {0, 2.2}, size = 20, font = "JetBrainsMono", fill = "muted"}
local subject = scene:rectangle {id = "subject", size = {2.6, 1.5}, stroke = "accent", fill = "accent", width = 3}
scene:tween({target = subject, shift = {1.5, 0}, fill = "warning"}, 1.2)

return scene
```

```viscript-js
const scene = vis.scene({
    width: 960,
    height: 540,
});

scene.text({id: "caption", text: "tween", point: [0, 2.2], size: 20, font: "JetBrainsMono", fill: "muted"});
const subject = scene.rectangle({id: "subject", size: [2.6, 1.5], stroke: "accent", fill: "accent", width: 3});
scene.tween({target: subject, shift: [1.5, 0], fill: "warning"}, 1.2);

return scene;
```

### scene.set

<!-- api-example: scene.set -->

```viscript
local scene = vis.scene {
    width = 960,
    height = 540,
}

scene:text {id = "caption", text = "set", point = {0, 2.2}, size = 20, font = "JetBrainsMono", fill = "muted"}
local subject = scene:rectangle {id = "subject", size = {2.6, 1.5}, stroke = "accent", fill = "accent", width = 3}
scene:set {target = subject, position = {1.5, 0}, fill = "warning"}

return scene
```

```viscript-js
const scene = vis.scene({
    width: 960,
    height: 540,
});

scene.text({id: "caption", text: "set", point: [0, 2.2], size: 20, font: "JetBrainsMono", fill: "muted"});
const subject = scene.rectangle({id: "subject", size: [2.6, 1.5], stroke: "accent", fill: "accent", width: 3});
scene.set({target: subject, position: [1.5, 0], fill: "warning"});

return scene;
```

### Save & restore

<!-- api-example: group.save-restore -->
<!-- api-covers: scene.save scene.restore -->

```viscript
local scene = vis.scene {
    width = 960,
    height = 540,
}

local circle = scene:circle {id = "circle", center = {-2, 0}, radius = 0.65, fill = "accent"}
scene:save(circle)
scene:shift(circle, {4, 0}, 1)
scene:restore(circle, {duration = 1})

return scene
```

```viscript-js
const scene = vis.scene({
    width: 960,
    height: 540,
});

const circle = scene.circle({id: "circle", center: [-2, 0], radius: 0.65, fill: "accent"});
scene.save(circle);
scene.shift(circle, [4, 0], 1);
scene.restore(circle, {duration: 1});

return scene;
```

### tween · layer

<!-- api-example: property.layer -->

```viscript
local scene = vis.scene {
    width = 960,
    height = 540,
}

scene:text {id = "caption", text = "tween: layer", point = {0, 2.2}, size = 20, font = "JetBrainsMono", fill = "muted"}
local subject = scene:rectangle {id = "subject", center = {-0.5,0}, size = {2,2}, fill = "accent", layer = 1}
scene:circle {center = {0.6,0}, radius = 1.1, fill = "warning", layer = 2}
scene:wait(0.4) -- Show the initial stacking order before it switches.
scene:tween({target = subject, layer = 3}, 1.4)

return scene
```

```viscript-js
const scene = vis.scene({
    width: 960,
    height: 540,
});

scene.text({id: "caption", text: "tween: layer", point: [0, 2.2], size: 20, font: "JetBrainsMono", fill: "muted"});
const subject = scene.rectangle({id: "subject", center: [-0.5,0], size: [2,2], fill: "accent", layer: 1});
scene.circle({center: [0.6,0], radius: 1.1, fill: "warning", layer: 2});
scene.wait(0.4); // Show the initial stacking order before it switches.
scene.tween({target: subject, layer: 3}, 1.4);

return scene;
```
