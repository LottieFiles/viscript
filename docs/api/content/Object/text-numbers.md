---
title: Text & Numbers
id: text-numbers
order: 8
parent: object
---

# Text & Numbers

Create plain text, formatted numeric values, and object labels. Text defaults to billboard orientation; plane text uses world-space sizes.

## Create text

| Lua | JavaScript | Purpose |
| --- | --- | --- |
| `scene:text(opts)` | `scene.text(opts)` | Draw `text` at `point`; configure `size`, `font`, `role`, `align`, `orientation`, `slant`, `measure`, `reserve`, `tabular`, and `baseline`. |
| `scene:number(opts)` | `scene.number(opts?)` | Draw animatable `value`; format with `decimals`, `sign`, `separator`, `prefix`, and `suffix`, plus text placement/font options. |
| `object:label(opts)` | `object.label(opts)` | Add a Text child using text options; its bounds count as the object's semantic label. |

## Measure text

| Lua | JavaScript | Purpose |
| --- | --- | --- |
| `object:measure()` | — | Return a Text object's width and height as two Lua values in world units; billboard measurement uses the authored camera. |

A label is a child of its receiver and contributes to its semantic bounds. Use [Rich Text & TeX](./rich-text-tex.md) for mixed styles or formulas.

The `tween` examples fade between whole strings, sweep characters, retype a string or count a numeric value. See [Text & Values](../Animation/text-properties.md) for those animation fields.

## Examples

### scene.text

<!-- api-example: scene.text -->

```viscript
local scene = vis.scene {
    width = 960,
    height = 540,
}

scene:text {id = "message", text = "Hello, VISCRIPT", point = {0, 0}, size = 30, fill = "foreground"}
scene:text {id = "caption", text = "Text anchored at (0, 0), size = 30 pt", point = {0, -2.25}, size = 16, fill = "muted"}

return scene
```

```viscript-js
const scene = vis.scene({
    width: 960,
    height: 540,
});

scene.text({id: "message", text: "Hello, VISCRIPT", point: [0, 0], size: 30, fill: "foreground"});
scene.text({id: "caption", text: "Text anchored at (0, 0), size = 30 pt", point: [0, -2.25], size: 16, fill: "muted"});

return scene;
```

### scene.number

<!-- api-example: scene.number -->

```viscript
local scene = vis.scene {
    width = 960,
    height = 540,
}

local value = scene:number {id = "value", value = 0, decimals = 2, suffix = " m", size = 40, fill = "accent"}
scene:tween({target = value, value = 12.5}, {duration = 1.2, curve = "linear"})
scene:text {id = "caption", text = "The numeric value animates; formatting stays fixed", point = {0, -2.25}, size = 16, fill = "muted"}

return scene
```

```viscript-js
const scene = vis.scene({
    width: 960,
    height: 540,
});

const value = scene.number({id: "value", value: 0, decimals: 2, suffix: " m", size: 40, fill: "accent"});
scene.tween({target: value, value: 12.5}, {duration: 1.2, curve: "linear"});
scene.text({id: "caption", text: "The numeric value animates; formatting stays fixed", point: [0, -2.25], size: 16, fill: "muted"});

return scene;
```

### object.label

<!-- api-example: object.label -->

```viscript
local scene = vis.scene {
    width = 960,
    height = 540,
}

local circle = scene:circle {radius = 0.8, stroke = "accent", width = 2}
circle:label {id = "label", text = "child label", point = {0, 1.2}, size = 18, fill = "foreground"}
scene:shift(circle, {1, 0}, {duration = 1})
scene:text {id = "caption", text = "The label moves with its parent", point = {0, -2.25}, size = 16, fill = "muted"}

return scene
```

```viscript-js
const scene = vis.scene({
    width: 960,
    height: 540,
});

const circle = scene.circle({radius: 0.8, stroke: "accent", width: 2});
circle.label({id: "label", text: "child label", point: [0, 1.2], size: 18, fill: "foreground"});
scene.shift(circle, [1, 0], {duration: 1});
scene.text({id: "caption", text: "The label moves with its parent", point: [0, -2.25], size: 16, fill: "muted"});

return scene;
```

### object.measure

<!-- api-example: object.measure -->

```viscript
local scene = vis.scene {
    width = 960,
    height = 540,
}

local text = scene:text {id = "measured", text = "Measure me", point = {0, 0.3}, size = 32, fill = "foreground"}
local width, height = text:measure()
scene:rectangle {center = {0, 0.3}, size = {width, height}, stroke = "accent", width = 1.5}
scene:text {id = "dimensions", text = string.format("%.2f x %.2f world units", width, height), point = {0, -0.8}, size = 16, fill = "accent"}
scene:text {id = "caption", text = "measure() returns text width and height", point = {0, -2.25}, size = 16, fill = "muted"}

return scene
```

### tween · text

<!-- api-example: property.text -->

```viscript
local scene = vis.scene {
    width = 960,
    height = 540,
}

scene:text {id = "caption", text = "tween: text", point = {0, 2.2}, size = 20, font = "JetBrainsMono", fill = "muted"}
local subject = scene:text {id = "subject", text = "VISCRIPT", size = 42, font = "Pretendard", fill = "accent"}
scene:tween({target = subject, text = "Hello, API"}, {duration = 1.4})

return scene
```

```viscript-js
const scene = vis.scene({
    width: 960,
    height: 540,
});

scene.text({id: "caption", text: "tween: text", point: [0, 2.2], size: 20, font: "JetBrainsMono", fill: "muted"});
const subject = scene.text({id: "subject", text: "VISCRIPT", size: 42, font: "Pretendard", fill: "accent"});
scene.tween({target: subject, text: "Hello, API"}, {duration: 1.4});

return scene;
```

### tween · sweep

<!-- api-example: property.sweep -->

```viscript
local scene = vis.scene {
    width = 960,
    height = 540,
}

scene:text {id = "caption", text = "tween: sweep", point = {0, 2.2}, size = 20, font = "JetBrainsMono", fill = "muted"}
local subject = scene:text {id = "subject", text = "LOADING", size = 42, font = "JetBrainsMono", fill = "accent"}
scene:tween({target = subject, text = "SUCCESS", sweep = true}, {duration = 1.4, curve = "linear"})

return scene
```

```viscript-js
const scene = vis.scene({
    width: 960,
    height: 540,
});

scene.text({id: "caption", text: "tween: sweep", point: [0, 2.2], size: 20, font: "JetBrainsMono", fill: "muted"});
const subject = scene.text({id: "subject", text: "LOADING", size: 42, font: "JetBrainsMono", fill: "accent"});
scene.tween({target: subject, text: "SUCCESS", sweep: true}, {duration: 1.4, curve: "linear"});

return scene;
```

### tween · retype

<!-- api-example: property.retype -->

```viscript
local scene = vis.scene {
    width = 960,
    height = 540,
}

scene:text {id = "caption", text = "tween: retype", point = {0, 2.2}, size = 20, font = "JetBrainsMono", fill = "muted"}
local subject = scene:text {id = "subject", text = "vis.scene", size = 38, font = "JetBrainsMono", fill = "accent"}
scene:tween({target = subject, text = "vis.script", retype = true}, {duration = 1.4})

return scene
```

```viscript-js
const scene = vis.scene({
    width: 960,
    height: 540,
});

scene.text({id: "caption", text: "tween: retype", point: [0, 2.2], size: 20, font: "JetBrainsMono", fill: "muted"});
const subject = scene.text({id: "subject", text: "vis.scene", size: 38, font: "JetBrainsMono", fill: "accent"});
scene.tween({target: subject, text: "vis.script", retype: true}, {duration: 1.4});

return scene;
```

### tween · value

<!-- api-example: property.value -->

```viscript
local scene = vis.scene {
    width = 960,
    height = 540,
}

scene:text {id = "caption", text = "tween: value", point = {0, 2.2}, size = 20, font = "JetBrainsMono", fill = "muted"}
local subject = scene:vector {id = "subject", value = {2, 1}, origin = {-1, -0.5}, stroke = "accent", width = 3, tip = 10}
scene:tween({target = subject, value = {-1, 2}}, {duration = 1.4})

return scene
```

```viscript-js
const scene = vis.scene({
    width: 960,
    height: 540,
});

scene.text({id: "caption", text: "tween: value", point: [0, 2.2], size: 20, font: "JetBrainsMono", fill: "muted"});
const subject = scene.vector({id: "subject", value: [2, 1], origin: [-1, -0.5], stroke: "accent", width: 3, tip: 10});
scene.tween({target: subject, value: [-1, 2]}, {duration: 1.4});

return scene;
```
