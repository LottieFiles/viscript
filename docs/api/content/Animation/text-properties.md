---
title: Text & Values
id: text-properties
order: 11
parent: animation
---

# Text & Values

Change text, animate numbers or update a vector value with [tween / set descriptors](./state-scheduling.md).

## Replace, type, and sweep text

| Lua | JavaScript | Purpose |
| --- | --- | --- |
| `text` | `text` | Fade out the old string, then fade in the replacement. |
| `retype` | `retype` | With `text`, erase to the shared prefix and type the remainder. |
| `sweep` | `sweep` | With `text`, sweep through character positions, mixing both strings. |

`tween` keeps each whole string intact during the default fade. Use `retype = true` / `retype: true` for typing, including from an empty string, or `sweep = true` / `sweep: true` for a stylized character sweep. Both flags require `text` and cannot both be true. `set` replaces text immediately.

An empty source or replacement uses the full duration to fade in or out; identical strings stay visible without fading.

## Numbers and vectors

| Lua | JavaScript | Purpose |
| --- | --- | --- |
| `value` | `value` | Number text value or vector value. |

Text size uses the [`size` descriptor](./shape-properties.md). For animated code changes, see [Morph & Code Edits](./morph-edits.md).

## Examples

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
