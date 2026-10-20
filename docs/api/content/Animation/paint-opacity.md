---
title: Paint & Opacity
id: paint-opacity
order: 6
parent: animation
---

# Paint & Opacity

`fade_in` and `fade_out` pair opacity changes with optional movement and scaling. `fade` sets a chosen opacity; `stroke` and `fill` animate colors.

## Fade and recolor

| Lua | JavaScript | Purpose |
| --- | --- | --- |
| `scene:fade(target, opacity, duration?, curve?)`<br />`scene:fade(target, opacity, timing)` | `scene.fade(target, opacity, duration?, curve?)`<br />`scene.fade(target, opacity, timing)` | Animate opacity to a value from 0 to 1. |
| `scene:fade_in(target, options?)` | `scene.fadeIn(target, options?)` | Fade into the authored state; options: `shift`, `scale`, `duration`, `curve`. |
| `scene:fade_out(target, options?)` | `scene.fadeOut(target, options?)` | Fade away with optional travel and scaling; options: `shift`, `scale`, `duration`, `curve`. |
| `scene:transition(from, to, duration?, curve?)`<br />`scene:transition(from, to, timing)` | `scene.transition(from, to, duration?, curve?)`<br />`scene.transition(from, to, timing)` | Fade outgoing targets to 0 and incoming targets to 1 together; incoming targets must start at opacity 0. |
| `scene:stroke(target, color, duration?, curve?)`<br />`scene:stroke(target, color, timing)` | `scene.stroke(target, color, duration?, curve?)`<br />`scene.stroke(target, color, timing)` | Animate the stroke color. |
| `scene:fill(target, color, duration?, curve?)`<br />`scene:fill(target, color, timing)` | `scene.fill(target, color, duration?, curve?)`<br />`scene.fill(target, color, timing)` | Animate the fill color. |

Timing follows the [shared rules](./index.md).

## Color and opacity descriptors

| Lua        | JavaScript | Purpose                                                 |
| ---------- | ---------- | ------------------------------------------------------- |
| `opacity`  | `opacity`  | Opacity from 0 to 1.                                    |
| `stroke`   | `stroke`   | Stroke color.                                           |
| `fill`     | `fill`     | Fill color.                                             |
| `gradient` | `gradient` | Gradient end color or a compatible gradient definition. |

Gradient shapes must keep compatible types, stop counts and explicit geometry.

## Stroke descriptors

| Lua | JavaScript | Purpose |
| --- | --- | --- |
| `dash_offset` | `dash_offset` | Stroke dash phase. |
| `tail` | `tail` | Start arrowhead length in screen pixels. |
| `tip` | `tip` | End arrowhead length in screen pixels. |
| `width` | `width` | Stroke width in pixels; inherited widths follow their group. |

## Effects descriptors

| Lua | JavaScript | Purpose |
| --- | --- | --- |
| `blur` | `blur` | Gaussian blur sigma in screen pixels, 0–256. |
| `shadow` | `shadow` | Shadow settings. |

## Examples

### Fade in & out

<!-- api-example: group.fade -->
<!-- api-covers: scene.fade_in scene.fade_out -->

```viscript
local scene = vis.scene {
    width = 960,
    height = 540,
}

local circle = scene:circle {id = "circle", radius = 0.9, fill = "accent"}
scene:fade_in(circle, {duration = 1})
scene:fade_out(circle, {duration = 1})

return scene
```

```viscript-js
const scene = vis.scene({
    width: 960,
    height: 540,
});

const circle = scene.circle({id: "circle", radius: 0.9, fill: "accent"});
scene.fadeIn(circle, {duration: 1});
scene.fadeOut(circle, {duration: 1});

return scene;
```

### scene.fade

<!-- api-example: scene.fade -->

```viscript
local scene = vis.scene {
    width = 960,
    height = 540,
}

scene:text {id = "caption", text = "fade", point = {0, 2.2}, size = 20, font = "JetBrainsMono", fill = "muted"}
local subject = scene:rectangle {id = "subject", size = {2.6, 1.5}, stroke = "accent", fill = "accent", width = 3}
scene:fade(subject, 0.2, 1.2)

return scene
```

```viscript-js
const scene = vis.scene({
    width: 960,
    height: 540,
});

scene.text({id: "caption", text: "fade", point: [0, 2.2], size: 20, font: "JetBrainsMono", fill: "muted"});
const subject = scene.rectangle({id: "subject", size: [2.6, 1.5], stroke: "accent", fill: "accent", width: 3});
scene.fade(subject, 0.2, 1.2);

return scene;
```

### scene.transition

<!-- api-example: scene.transition -->

```viscript
local scene = vis.scene {
    width = 960,
    height = 540,
}

scene:text {id = "caption", text = "transition", point = {0, 2.2}, size = 20, font = "JetBrainsMono", fill = "muted"}
local subject = scene:rectangle {id = "subject", size = {2.6, 1.5}, stroke = "accent", fill = "accent", width = 3}
local next = scene:circle {id = "next", radius = 1, fill = "warning", opacity = 0}
scene:transition(subject, next, 1.4)

return scene
```

```viscript-js
const scene = vis.scene({
    width: 960,
    height: 540,
});

scene.text({id: "caption", text: "transition", point: [0, 2.2], size: 20, font: "JetBrainsMono", fill: "muted"});
const subject = scene.rectangle({id: "subject", size: [2.6, 1.5], stroke: "accent", fill: "accent", width: 3});
const next = scene.circle({id: "next", radius: 1, fill: "warning", opacity: 0});
scene.transition(subject, next, 1.4);

return scene;
```

### scene.stroke

<!-- api-example: scene.stroke -->

```viscript
local scene = vis.scene {
    width = 960,
    height = 540,
}

scene:text {id = "caption", text = "stroke", point = {0, 2.2}, size = 20, font = "JetBrainsMono", fill = "muted"}
local subject = scene:circle {id = "subject", radius = 1.1, stroke = "accent", fill = "#00000000", width = 3}
scene:stroke(subject, "warning", 1.2)

return scene
```

```viscript-js
const scene = vis.scene({
    width: 960,
    height: 540,
});

scene.text({id: "caption", text: "stroke", point: [0, 2.2], size: 20, font: "JetBrainsMono", fill: "muted"});
const subject = scene.circle({id: "subject", radius: 1.1, stroke: "accent", fill: "#00000000", width: 3});
scene.stroke(subject, "warning", 1.2);

return scene;
```

### scene.fill

<!-- api-example: scene.fill -->

```viscript
local scene = vis.scene {
    width = 960,
    height = 540,
}

scene:text {id = "caption", text = "fill", point = {0, 2.2}, size = 20, font = "JetBrainsMono", fill = "muted"}
local subject = scene:rectangle {id = "subject", size = {2.6, 1.5}, stroke = "accent", fill = "accent", width = 3}
scene:fill(subject, "warning", 1.2)

return scene
```

```viscript-js
const scene = vis.scene({
    width: 960,
    height: 540,
});

scene.text({id: "caption", text: "fill", point: [0, 2.2], size: 20, font: "JetBrainsMono", fill: "muted"});
const subject = scene.rectangle({id: "subject", size: [2.6, 1.5], stroke: "accent", fill: "accent", width: 3});
scene.fill(subject, "warning", 1.2);

return scene;
```

### tween · opacity

<!-- api-example: property.opacity -->

```viscript
local scene = vis.scene {
    width = 960,
    height = 540,
}

scene:text {id = "caption", text = "tween: opacity", point = {0, 2.2}, size = 20, font = "JetBrainsMono", fill = "muted"}
local subject = scene:rectangle {id = "subject", size = {2.6, 1.5}, stroke = "accent", fill = "accent", width = 3}
scene:tween({target = subject, opacity = 0.15}, 1.4)

return scene
```

```viscript-js
const scene = vis.scene({
    width: 960,
    height: 540,
});

scene.text({id: "caption", text: "tween: opacity", point: [0, 2.2], size: 20, font: "JetBrainsMono", fill: "muted"});
const subject = scene.rectangle({id: "subject", size: [2.6, 1.5], stroke: "accent", fill: "accent", width: 3});
scene.tween({target: subject, opacity: 0.15}, 1.4);

return scene;
```

### tween · stroke

<!-- api-example: property.stroke -->

```viscript
local scene = vis.scene {
    width = 960,
    height = 540,
}

scene:text {id = "caption", text = "tween: stroke", point = {0, 2.2}, size = 20, font = "JetBrainsMono", fill = "muted"}
local subject = scene:circle {id = "subject", radius = 1.1, stroke = "accent", fill = "#00000000", width = 3}
scene:tween({target = subject, stroke = "warning"}, 1.4)

return scene
```

```viscript-js
const scene = vis.scene({
    width: 960,
    height: 540,
});

scene.text({id: "caption", text: "tween: stroke", point: [0, 2.2], size: 20, font: "JetBrainsMono", fill: "muted"});
const subject = scene.circle({id: "subject", radius: 1.1, stroke: "accent", fill: "#00000000", width: 3});
scene.tween({target: subject, stroke: "warning"}, 1.4);

return scene;
```

### tween · fill

<!-- api-example: property.fill -->

```viscript
local scene = vis.scene {
    width = 960,
    height = 540,
}

scene:text {id = "caption", text = "tween: fill", point = {0, 2.2}, size = 20, font = "JetBrainsMono", fill = "muted"}
local subject = scene:rectangle {id = "subject", size = {2.6, 1.5}, stroke = "accent", fill = "accent", width = 3}
scene:tween({target = subject, fill = "warning"}, 1.4)

return scene
```

```viscript-js
const scene = vis.scene({
    width: 960,
    height: 540,
});

scene.text({id: "caption", text: "tween: fill", point: [0, 2.2], size: 20, font: "JetBrainsMono", fill: "muted"});
const subject = scene.rectangle({id: "subject", size: [2.6, 1.5], stroke: "accent", fill: "accent", width: 3});
scene.tween({target: subject, fill: "warning"}, 1.4);

return scene;
```

### tween · gradient

<!-- api-example: property.gradient -->

```viscript
local scene = vis.scene {
    width = 960,
    height = 540,
}

scene:text {id = "caption", text = "tween: gradient", point = {0, 2.2}, size = 20, font = "JetBrainsMono", fill = "muted"}
local subject = scene:rectangle {id = "subject", size = {3.5, 1.8}, fill = "#ffffff", gradient = {type = "linear", from = {-1.75, 0}, to = {1.75, 0}, stops = {"#6cb6ff", "#7ee787"}}}
scene:tween({target = subject, gradient = {type = "linear", from = {-1.75, 0}, to = {1.75, 0}, stops = {"#ff7b72", "#e3b341"}}}, 1.4)

return scene
```

```viscript-js
const scene = vis.scene({
    width: 960,
    height: 540,
});

scene.text({id: "caption", text: "tween: gradient", point: [0, 2.2], size: 20, font: "JetBrainsMono", fill: "muted"});
const subject = scene.rectangle({id: "subject", size: [3.5, 1.8], fill: "#ffffff", gradient: {type: "linear", from: [-1.75, 0], to: [1.75, 0], stops: ["#6cb6ff", "#7ee787"]}});
scene.tween({target: subject, gradient: {type: "linear", from: [-1.75, 0], to: [1.75, 0], stops: ["#ff7b72", "#e3b341"]}}, 1.4);

return scene;
```

### tween · dash_offset

<!-- api-example: property.dash_offset -->

```viscript
local scene = vis.scene {
    width = 960,
    height = 540,
}

scene:text {id = "caption", text = "tween: dash_offset", point = {0, 2.2}, size = 20, font = "JetBrainsMono", fill = "muted"}
local subject = scene:circle {id = "subject", radius = 1.2, stroke = "accent", width = 4, dash = {18, 12}}
scene:tween({target = subject, dash_offset = 120}, 1.4)

return scene
```

```viscript-js
const scene = vis.scene({
    width: 960,
    height: 540,
});

scene.text({id: "caption", text: "tween: dash_offset", point: [0, 2.2], size: 20, font: "JetBrainsMono", fill: "muted"});
const subject = scene.circle({id: "subject", radius: 1.2, stroke: "accent", width: 4, dash: [18, 12]});
scene.tween({target: subject, dash_offset: 120}, 1.4);

return scene;
```

### tween · tail

<!-- api-example: property.tail -->

```viscript
local scene = vis.scene {
    width = 960,
    height = 540,
}

scene:text {id = "caption", text = "tween: tail", point = {0, 2.2}, size = 20, font = "JetBrainsMono", fill = "muted"}
local subject = scene:arrow {id = "subject", from = {-2, 0}, to = {2, 0}, stroke = "accent", width = 3, tip = 10}
scene:tween({target = subject, tail = 28}, 1.4)

return scene
```

```viscript-js
const scene = vis.scene({
    width: 960,
    height: 540,
});

scene.text({id: "caption", text: "tween: tail", point: [0, 2.2], size: 20, font: "JetBrainsMono", fill: "muted"});
const subject = scene.arrow({id: "subject", from: [-2, 0], to: [2, 0], stroke: "accent", width: 3, tip: 10});
scene.tween({target: subject, tail: 28}, 1.4);

return scene;
```

### tween · tip

<!-- api-example: property.tip -->

```viscript
local scene = vis.scene {
    width = 960,
    height = 540,
}

scene:text {id = "caption", text = "tween: tip", point = {0, 2.2}, size = 20, font = "JetBrainsMono", fill = "muted"}
local subject = scene:arrow {id = "subject", from = {-2, 0}, to = {2, 0}, stroke = "accent", width = 3, tip = 10}
scene:tween({target = subject, tip = 32}, 1.4)

return scene
```

```viscript-js
const scene = vis.scene({
    width: 960,
    height: 540,
});

scene.text({id: "caption", text: "tween: tip", point: [0, 2.2], size: 20, font: "JetBrainsMono", fill: "muted"});
const subject = scene.arrow({id: "subject", from: [-2, 0], to: [2, 0], stroke: "accent", width: 3, tip: 10});
scene.tween({target: subject, tip: 32}, 1.4);

return scene;
```

### tween · width

<!-- api-example: property.width -->

```viscript
local scene = vis.scene {
    width = 960,
    height = 540,
}

scene:text {id = "caption", text = "tween: width", point = {0, 2.2}, size = 20, font = "JetBrainsMono", fill = "muted"}
local subject = scene:circle {id = "subject", radius = 1.1, stroke = "accent", fill = "#00000000", width = 3}
scene:tween({target = subject, width = 14}, 1.4)

return scene
```

```viscript-js
const scene = vis.scene({
    width: 960,
    height: 540,
});

scene.text({id: "caption", text: "tween: width", point: [0, 2.2], size: 20, font: "JetBrainsMono", fill: "muted"});
const subject = scene.circle({id: "subject", radius: 1.1, stroke: "accent", fill: "#00000000", width: 3});
scene.tween({target: subject, width: 14}, 1.4);

return scene;
```

### tween · blur

<!-- api-example: property.blur -->

```viscript
local scene = vis.scene {
    width = 960,
    height = 540,
}

scene:text {id = "caption", text = "tween: blur", point = {0, 2.2}, size = 20, font = "JetBrainsMono", fill = "muted"}
local subject = scene:rectangle {id = "subject", size = {2.6, 1.5}, stroke = "accent", fill = "accent", width = 3}
scene:tween({target = subject, blur = 12}, 1.4)

return scene
```

```viscript-js
const scene = vis.scene({
    width: 960,
    height: 540,
});

scene.text({id: "caption", text: "tween: blur", point: [0, 2.2], size: 20, font: "JetBrainsMono", fill: "muted"});
const subject = scene.rectangle({id: "subject", size: [2.6, 1.5], stroke: "accent", fill: "accent", width: 3});
scene.tween({target: subject, blur: 12}, 1.4);

return scene;
```

### tween · shadow

<!-- api-example: property.shadow -->

```viscript
local scene = vis.scene {
    width = 960,
    height = 540,
}

scene:text {id = "caption", text = "tween: shadow", point = {0, 2.2}, size = 20, font = "JetBrainsMono", fill = "muted"}
local subject = scene:rectangle {id = "subject", size = {2.6, 1.5}, stroke = "accent", fill = "accent", width = 3}
scene:tween({target = subject, shadow = {color = "#6cb6ffcc", offset = {24, -24}, blur = 16}}, 1.4)

return scene
```

```viscript-js
const scene = vis.scene({
    width: 960,
    height: 540,
});

scene.text({id: "caption", text: "tween: shadow", point: [0, 2.2], size: 20, font: "JetBrainsMono", fill: "muted"});
const subject = scene.rectangle({id: "subject", size: [2.6, 1.5], stroke: "accent", fill: "accent", width: 3});
scene.tween({target: subject, shadow: {color: "#6cb6ffcc", offset: [24, -24], blur: 16}}, 1.4);

return scene;
```
