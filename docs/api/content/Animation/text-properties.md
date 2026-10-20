---
title: Text & Values
id: text-properties
order: 11
parent: animation
---

# Text & Values

Change text, animate numbers or update a vector value with [play / set descriptors](./state-scheduling.md).

## Replace and type text

| Lua | JavaScript | Purpose |
| --- | --- | --- |
| `text` | `text` | Replacement string for text or labels. |
| `retype` | `retype` | With `text`, erase to the shared prefix and type the remainder. |

Plain text animation changes characters and length progressively; `retype` chooses erase-and-type behavior.

## Numbers and vectors

| Lua | JavaScript | Purpose |
| --- | --- | --- |
| `value` | `value` | Number text value or vector value. |

Text size uses the [`size` descriptor](./shape-properties.md). For animated code changes, see [Morph & Code Edits](./morph-edits.md).

## Examples

### play · text

<!-- api-example: property.text -->

```viscript
local scene = vis.scene {
    width = 960,
    height = 540,
}

scene:text {id = "caption", text = "play: text", point = {0, 2.2}, size = 20, font = "JetBrainsMono", fill = "muted"}
local subject = scene:text {id = "subject", text = "VISCRIPT", size = 42, font = "Pretendard", fill = "accent"}
scene:play({target = subject, text = "Hello, API"}, 1.4)

return scene
```

```viscript-js
const scene = vis.scene({
    width: 960,
    height: 540,
});

scene.text({id: "caption", text: "play: text", point: [0, 2.2], size: 20, font: "JetBrainsMono", fill: "muted"});
const subject = scene.text({id: "subject", text: "VISCRIPT", size: 42, font: "Pretendard", fill: "accent"});
scene.play({target: subject, text: "Hello, API"}, 1.4);

return scene;
```

### play · retype

<!-- api-example: property.retype -->

```viscript
local scene = vis.scene {
    width = 960,
    height = 540,
}

scene:text {id = "caption", text = "play: retype", point = {0, 2.2}, size = 20, font = "JetBrainsMono", fill = "muted"}
local subject = scene:text {id = "subject", text = "vis.scene", size = 38, font = "JetBrainsMono", fill = "accent"}
scene:play({target = subject, text = "vis.script", retype = true}, 1.4)

return scene
```

```viscript-js
const scene = vis.scene({
    width: 960,
    height: 540,
});

scene.text({id: "caption", text: "play: retype", point: [0, 2.2], size: 20, font: "JetBrainsMono", fill: "muted"});
const subject = scene.text({id: "subject", text: "vis.scene", size: 38, font: "JetBrainsMono", fill: "accent"});
scene.play({target: subject, text: "vis.script", retype: true}, 1.4);

return scene;
```

### play · value

<!-- api-example: property.value -->

```viscript
local scene = vis.scene {
    width = 960,
    height = 540,
}

scene:text {id = "caption", text = "play: value", point = {0, 2.2}, size = 20, font = "JetBrainsMono", fill = "muted"}
local subject = scene:vector {id = "subject", value = {2, 1}, origin = {-1, -0.5}, stroke = "accent", width = 3, tip = 10}
scene:play({target = subject, value = {-1, 2}}, 1.4)

return scene
```

```viscript-js
const scene = vis.scene({
    width: 960,
    height: 540,
});

scene.text({id: "caption", text: "play: value", point: [0, 2.2], size: 20, font: "JetBrainsMono", fill: "muted"});
const subject = scene.vector({id: "subject", value: [2, 1], origin: [-1, -0.5], stroke: "accent", width: 3, tip: 10});
scene.play({target: subject, value: [-1, 2]}, 1.4);

return scene;
```
