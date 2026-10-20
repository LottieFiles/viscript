---
title: Emphasis
id: emphasis
order: 7
parent: animation
---

# Emphasis

Use a temporary size, color, frame, flash or ring to draw attention to an object. `highlight` selects children to keep bright.

## Emphasize and select

| Lua | JavaScript | Purpose |
| --- | --- | --- |
| `scene:indicate(targets, options?)` | `scene.indicate(targets, options?)` | Briefly enlarge and recolor targets, then restore them; options: `color`, `scale` (1.2), `duration`, `curve`. |
| `scene:wiggle(target, options?)` | `scene.wiggle(target, options?)` | Enlarge and rock the object, then restore it; options: `scale` (1.1), `angle` (0.0628), `swings` (6), `duration` (2), `curve`. |
| `scene:circumscribe(target, options?)` | `scene.circumscribe(target, options?)` | Draw a temporary frame with a moving bright section; options: `shape` (`rectangle`, `ellipse`), `color`, `width` (3), `padding` (0.1), `window` (0.3), timing. |
| `scene:passing_flash(target, options?)` | `scene.passingFlash(target, options?)` | Run a temporary bright section along an outline; options: `color`, `width` (6), `window` (0.3), timing. |
| `scene:flash(targetOrPoint, options?)` | `scene.flash(targetOrPoint, options?)` | Burst temporary lines from an object or world point; options: `color`, `width` (3), `padding` (0.1), `size` (0.2), `lines` (12, range 1–64), timing. |
| `scene:ripple(target, options?)` | `scene.ripple(target, options?)` | Expand and fade a temporary ring; options: `color`, `width` (3), `padding` (0), `size` (0.4), `duration`, `curve` (`snappy`). |
| `scene:highlight(target, first, last?, options?)`<br />`scene:highlight(target, selection, options?)`<br />`scene:highlight(target, options?)` | `scene.highlight(target, first, last?, options?)`<br />`scene.highlight(target, selection, options?)`<br />`scene.highlight(target, options?)` | Keep selected children/subfamilies bright and dim the rest, or restore all. Child indices start at 1; options: `dim` (0.25), `duration` (0.5), `curve`. |

Emphasis colors default to the theme's `focus` color. A `window` is the visible share of an outline, greater than 0 and at most 1. Timing follows the [shared rules](./index.md).

## Examples

### scene.indicate

<!-- api-example: scene.indicate -->

```viscript
local scene = vis.scene {
    width = 960,
    height = 540,
}

scene:text {id = "caption", text = "indicate", point = {0, 2.2}, size = 20, font = "JetBrainsMono", fill = "muted"}
local subject = scene:rectangle {id = "subject", size = {2.6, 1.5}, stroke = "accent", fill = "accent", width = 3}
scene:indicate(subject, {color = "warning", scale = 1.25, duration = 1.2})

return scene
```

```viscript-js
const scene = vis.scene({
    width: 960,
    height: 540,
});

scene.text({id: "caption", text: "indicate", point: [0, 2.2], size: 20, font: "JetBrainsMono", fill: "muted"});
const subject = scene.rectangle({id: "subject", size: [2.6, 1.5], stroke: "accent", fill: "accent", width: 3});
scene.indicate(subject, {color: "warning", scale: 1.25, duration: 1.2});

return scene;
```

### scene.wiggle

<!-- api-example: scene.wiggle -->

```viscript
local scene = vis.scene {
    width = 960,
    height = 540,
}

scene:text {id = "caption", text = "wiggle", point = {0, 2.2}, size = 20, font = "JetBrainsMono", fill = "muted"}
local subject = scene:rectangle {id = "subject", size = {2.6, 1.5}, stroke = "accent", fill = "accent", width = 3}
scene:wiggle(subject, {angle = 0.2, swings = 3, duration = 1.4})

return scene
```

```viscript-js
const scene = vis.scene({
    width: 960,
    height: 540,
});

scene.text({id: "caption", text: "wiggle", point: [0, 2.2], size: 20, font: "JetBrainsMono", fill: "muted"});
const subject = scene.rectangle({id: "subject", size: [2.6, 1.5], stroke: "accent", fill: "accent", width: 3});
scene.wiggle(subject, {angle: 0.2, swings: 3, duration: 1.4});

return scene;
```

### scene.circumscribe

<!-- api-example: scene.circumscribe -->

```viscript
local scene = vis.scene {
    width = 960,
    height = 540,
}

scene:text {id = "caption", text = "circumscribe", point = {0, 2.2}, size = 20, font = "JetBrainsMono", fill = "muted"}
local subject = scene:rectangle {id = "subject", size = {2.6, 1.5}, stroke = "accent", fill = "accent", width = 3}
scene:circumscribe(subject, {color = "warning", padding = 0.15, duration = 1.4})

return scene
```

```viscript-js
const scene = vis.scene({
    width: 960,
    height: 540,
});

scene.text({id: "caption", text: "circumscribe", point: [0, 2.2], size: 20, font: "JetBrainsMono", fill: "muted"});
const subject = scene.rectangle({id: "subject", size: [2.6, 1.5], stroke: "accent", fill: "accent", width: 3});
scene.circumscribe(subject, {color: "warning", padding: 0.15, duration: 1.4});

return scene;
```

### scene.passing_flash

<!-- api-example: scene.passing_flash -->

```viscript
local scene = vis.scene {
    width = 960,
    height = 540,
}

scene:text {id = "caption", text = "passing_flash", point = {0, 2.2}, size = 20, font = "JetBrainsMono", fill = "muted"}
local subject = scene:curve {id = "subject", from = {-2, -0.6}, control1 = {-1, 1.5}, control2 = {1, -1.5}, to = {2, 0.6}, stroke = "accent", width = 4}
scene:passing_flash(subject, {color = "warning", window = 0.25, duration = 1.4})

return scene
```

```viscript-js
const scene = vis.scene({
    width: 960,
    height: 540,
});

scene.text({id: "caption", text: "passing_flash", point: [0, 2.2], size: 20, font: "JetBrainsMono", fill: "muted"});
const subject = scene.curve({id: "subject", from: [-2, -0.6], control1: [-1, 1.5], control2: [1, -1.5], to: [2, 0.6], stroke: "accent", width: 4});
scene.passingFlash(subject, {color: "warning", window: 0.25, duration: 1.4});

return scene;
```

### scene.flash

<!-- api-example: scene.flash -->

```viscript
local scene = vis.scene {
    width = 960,
    height = 540,
}

scene:text {id = "caption", text = "flash", point = {0, 2.2}, size = 20, font = "JetBrainsMono", fill = "muted"}
local subject = scene:circle {id = "subject", radius = 1.1, stroke = "accent", fill = "#00000000", width = 3}
scene:flash(subject, {color = "warning", size = 0.3, duration = 1.2})

return scene
```

```viscript-js
const scene = vis.scene({
    width: 960,
    height: 540,
});

scene.text({id: "caption", text: "flash", point: [0, 2.2], size: 20, font: "JetBrainsMono", fill: "muted"});
const subject = scene.circle({id: "subject", radius: 1.1, stroke: "accent", fill: "#00000000", width: 3});
scene.flash(subject, {color: "warning", size: 0.3, duration: 1.2});

return scene;
```

### scene.ripple

<!-- api-example: scene.ripple -->

```viscript
local scene = vis.scene {
    width = 960,
    height = 540,
}

scene:text {id = "caption", text = "ripple", point = {0, 2.2}, size = 20, font = "JetBrainsMono", fill = "muted"}
local subject = scene:circle {id = "subject", radius = 1.1, stroke = "accent", fill = "#00000000", width = 3}
scene:ripple(subject, {color = "warning", size = 0.7, duration = 1.2})

return scene
```

```viscript-js
const scene = vis.scene({
    width: 960,
    height: 540,
});

scene.text({id: "caption", text: "ripple", point: [0, 2.2], size: 20, font: "JetBrainsMono", fill: "muted"});
const subject = scene.circle({id: "subject", radius: 1.1, stroke: "accent", fill: "#00000000", width: 3});
scene.ripple(subject, {color: "warning", size: 0.7, duration: 1.2});

return scene;
```

### scene.highlight

<!-- api-example: scene.highlight -->

```viscript
local scene = vis.scene {
    width = 960,
    height = 540,
}

scene:text {id = "caption", text = "highlight", point = {0, 2.2}, size = 20, font = "JetBrainsMono", fill = "muted"}
local subject = scene:spans {"first second third", id = "subject", point = {0, 0}, size = 30, font = "Pretendard", split = "words"}
scene:highlight(subject, 2, 2, {dim = 0.15, duration = 1.2})

return scene
```

```viscript-js
const scene = vis.scene({
    width: 960,
    height: 540,
});

scene.text({id: "caption", text: "highlight", point: [0, 2.2], size: 20, font: "JetBrainsMono", fill: "muted"});
const subject = scene.spans(["first second third"], {id: "subject", point: [0, 0], size: 30, font: "Pretendard", split: "words"});
scene.highlight(subject, 2, 2, {dim: 0.15, duration: 1.2});

return scene;
```
