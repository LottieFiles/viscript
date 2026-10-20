---
title: Reveal
id: reveal
order: 2
parent: animation
---

# Reveal

`create` and `uncreate` draw and erase the same outline. Filled shapes, borders and ordered text use the related reveal helpers below.

## Draw, erase and write

| Lua | JavaScript | Purpose |
| --- | --- | --- |
| `scene:create(targets, duration?, curve?, lag?, direction?)`<br />`scene:create(targets, options)` | `scene.create(targets, duration?, curve?, lag?, direction?)`<br />`scene.create(targets, options)` | Draw outlines progressively. Options: `duration`, `curve`, `lag`, `direction`. |
| `scene:uncreate(targets, duration?, curve?, lag?, direction?)`<br />`scene:uncreate(targets, options)` | `scene.uncreate(targets, duration?, curve?, lag?, direction?)`<br />`scene.uncreate(targets, options)` | Erase outlines progressively. Options: `duration`, `curve`, `lag`, `direction`. |
| `scene:fill_reveal(targets, duration?, curve?, lag?)`<br />`scene:fill_reveal(targets, options)` | `scene.fillReveal(targets, duration?, curve?, lag?)`<br />`scene.fillReveal(targets, options)` | Reveal filled shapes progressively. Options: `duration`, `curve`, `lag`. |
| `scene:draw_border_then_fill(targets, duration?, curve?, lag?, direction?)`<br />`scene:draw_border_then_fill(targets, options)` | `scene.drawBorderThenFill(targets, duration?, curve?, lag?, direction?)`<br />`scene.drawBorderThenFill(targets, options)` | Draw the border, then bring in the fill. Options: `duration`, `curve`, `lag`, `direction`. |
| `scene:write(targets, duration?, curve?, lag?, direction?)`<br />`scene:write(targets, options)` | `scene.write(targets, duration?, curve?, lag?, direction?)`<br />`scene.write(targets, options)` | Reveal text glyphs or composite pieces in order. Options: `duration`, `curve`, `lag`, `direction`. |

`direction` accepts `forward`, `reverse`, `clockwise`, `counterclockwise`; omitted direction is forward. Timing follows the [shared rules](./index.md).

## Outline progress

| Lua | JavaScript | Purpose |
| --- | --- | --- |
| `progress` | `progress` | Outline drawing progress. |

Use this field in a [play / set descriptor](./state-scheduling.md) to choose the outline progress directly.

## Examples

### Create & uncreate

<!-- api-example: group.reveal -->
<!-- api-covers: scene.create scene.uncreate -->

```viscript
local scene = vis.scene {
    width = 960,
    height = 540,
}

local circle = scene:circle {id = "circle", radius = 1.1, stroke = "accent", fill = "#00000000", width = 3}
scene:create(circle, 1)
scene:uncreate(circle, 1)

return scene
```

```viscript-js
const scene = vis.scene({
    width: 960,
    height: 540,
});

const circle = scene.circle({id: "circle", radius: 1.1, stroke: "accent", fill: "#00000000", width: 3});
scene.create(circle, 1);
scene.uncreate(circle, 1);

return scene;
```

### scene.fill_reveal

<!-- api-example: scene.fill_reveal -->

```viscript
local scene = vis.scene {
    width = 960,
    height = 540,
}

scene:text {id = "caption", text = "fill_reveal", point = {0, 2.2}, size = 20, font = "JetBrainsMono", fill = "muted"}
local subject = scene:rectangle {id = "subject", size = {2.6, 1.5}, stroke = "accent", fill = "accent", width = 3}
scene:fill_reveal(subject, 1.4)

return scene
```

```viscript-js
const scene = vis.scene({
    width: 960,
    height: 540,
});

scene.text({id: "caption", text: "fill_reveal", point: [0, 2.2], size: 20, font: "JetBrainsMono", fill: "muted"});
const subject = scene.rectangle({id: "subject", size: [2.6, 1.5], stroke: "accent", fill: "accent", width: 3});
scene.fillReveal(subject, 1.4);

return scene;
```

### scene.draw_border_then_fill

<!-- api-example: scene.draw_border_then_fill -->

```viscript
local scene = vis.scene {
    width = 960,
    height = 540,
}

scene:text {id = "caption", text = "draw_border_then_fill", point = {0, 2.2}, size = 20, font = "JetBrainsMono", fill = "muted"}
local subject = scene:rectangle {id = "subject", size = {2.6, 1.5}, stroke = "accent", fill = "accent", width = 3}
scene:draw_border_then_fill(subject, 1.4)

return scene
```

```viscript-js
const scene = vis.scene({
    width: 960,
    height: 540,
});

scene.text({id: "caption", text: "draw_border_then_fill", point: [0, 2.2], size: 20, font: "JetBrainsMono", fill: "muted"});
const subject = scene.rectangle({id: "subject", size: [2.6, 1.5], stroke: "accent", fill: "accent", width: 3});
scene.drawBorderThenFill(subject, 1.4);

return scene;
```

### scene.write

<!-- api-example: scene.write -->

```viscript
local scene = vis.scene {
    width = 960,
    height = 540,
}

scene:text {id = "caption", text = "write", point = {0, 2.2}, size = 20, font = "JetBrainsMono", fill = "muted"}
local subject = scene:text {id = "subject", text = "VISCRIPT", size = 42, font = "Pretendard", fill = "accent"}
scene:write(subject, 1.4)

return scene
```

```viscript-js
const scene = vis.scene({
    width: 960,
    height: 540,
});

scene.text({id: "caption", text: "write", point: [0, 2.2], size: 20, font: "JetBrainsMono", fill: "muted"});
const subject = scene.text({id: "subject", text: "VISCRIPT", size: 42, font: "Pretendard", fill: "accent"});
scene.write(subject, 1.4);

return scene;
```

### play · progress

<!-- api-example: property.progress -->

```viscript
local scene = vis.scene {
    width = 960,
    height = 540,
}

scene:text {id = "caption", text = "play: progress", point = {0, 2.2}, size = 20, font = "JetBrainsMono", fill = "muted"}
local subject = scene:circle {id = "subject", radius = 1.1, stroke = "accent", fill = "#00000000", width = 3}
scene:play({target = subject, progress = 0.15}, 1.4)

return scene
```

```viscript-js
const scene = vis.scene({
    width: 960,
    height: 540,
});

scene.text({id: "caption", text: "play: progress", point: [0, 2.2], size: 20, font: "JetBrainsMono", fill: "muted"});
const subject = scene.circle({id: "subject", radius: 1.1, stroke: "accent", fill: "#00000000", width: 3});
scene.play({target: subject, progress: 0.15}, 1.4);

return scene;
```
