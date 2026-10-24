---
title: Morph & Code Edits
id: morph-edits
order: 8
parent: animation
---

# Morph & Code Edits

Morph source geometry into a target, cross-fade families, match related pieces, or animate edits to a code block.

## Transform into a target

| Lua | JavaScript | Purpose |
| --- | --- | --- |
| `scene:morph(source, target, options?)` | `scene.morph(source, target, options?)` | Interpolate geometry into the target, which takes over at completion. Both arguments are handles or equally sized lists. Options: `copy`, `pairs` (one-based contour pairs), `lag`, timing. |
| `scene:fade_transform(source, target, options?)` | `scene.fadeTransform(source, target, options?)` | Cross-fade separate families; remove source visibility at completion. |
| `scene:match_transform(source, target, options?)` | `scene.matchTransform(source, target, options?)` | Carry matching pieces into a new target and fade unmatched pieces. Options: `path`, `arc`, `pairs`, `strings`, `diff`, `shapes`, `fade`, `copy`, `lag`, timing. |

`copy` preserves the source by animating a snapshot. Match `pairs` use source/target handles; `strings` matches equal text or TeX, `diff` matches edited code, and `shapes` matches circles/rectangles. `fade` is the share of duration used by unmatched pieces. `path` and nonzero `arc` are mutually exclusive.

## Edit a code block

| Lua | JavaScript | Purpose |
| --- | --- | --- |
| `scene:edit(block, edits, options?)` | `scene.edit(block, edits, options?)` | Apply code edits in order, animate the diff and return the new block; options: `id`, `arc`, `fade`, `lag`, timing. |

Timing follows the [shared rules](./index.md). For text replacement and typing, see [Text & Values](./text-properties.md).

## Examples

### scene.morph

<!-- api-example: scene.morph -->

```viscript
local scene = vis.scene {
    width = 960,
    height = 540,
}

scene:text {id = "caption", text = "morph", point = {0, 2.2}, size = 20, font = "JetBrainsMono", fill = "muted"}
local subject = scene:polygon {id = "subject", points = {{-1,-1}, {1,-1}, {0,1}}, fill = "accent"}
local next = scene:polygon {id = "next", points = {{0,-1.4}, {1.4,0}, {0,1.4}, {-1.4,0}}, fill = "warning"}
scene:morph(subject, next, {duration = 1.4})

return scene
```

```viscript-js
const scene = vis.scene({
    width: 960,
    height: 540,
});

scene.text({id: "caption", text: "morph", point: [0, 2.2], size: 20, font: "JetBrainsMono", fill: "muted"});
const subject = scene.polygon({id: "subject", points: [[-1,-1], [1,-1], [0,1]], fill: "accent"});
const next = scene.polygon({id: "next", points: [[0,-1.4], [1.4,0], [0,1.4], [-1.4,0]], fill: "warning"});
scene.morph(subject, next, {duration: 1.4});

return scene;
```

### scene.fade_transform

<!-- api-example: scene.fade_transform -->

```viscript
local scene = vis.scene {
    width = 960,
    height = 540,
}

scene:text {id = "caption", text = "fade_transform", point = {0, 2.2}, size = 20, font = "JetBrainsMono", fill = "muted"}
local subject = scene:rectangle {id = "subject", size = {2.6, 1.5}, stroke = "accent", fill = "accent", width = 3}
local next = scene:text {id = "next", text = "rectangle", size = 38, font = "Pretendard", fill = "warning"}
scene:fade_transform(subject, next, {duration = 1.4})

return scene
```

```viscript-js
const scene = vis.scene({
    width: 960,
    height: 540,
});

scene.text({id: "caption", text: "fade_transform", point: [0, 2.2], size: 20, font: "JetBrainsMono", fill: "muted"});
const subject = scene.rectangle({id: "subject", size: [2.6, 1.5], stroke: "accent", fill: "accent", width: 3});
const next = scene.text({id: "next", text: "rectangle", size: 38, font: "Pretendard", fill: "warning"});
scene.fadeTransform(subject, next, {duration: 1.4});

return scene;
```

### scene.match_transform

<!-- api-example: scene.match_transform -->

```viscript
local scene = vis.scene {
    width = 960,
    height = 540,
}

scene:text {id = "caption", text = "match_transform", point = {0, 2.2}, size = 20, font = "JetBrainsMono", fill = "muted"}
local subject = scene:spans {"A + B", id = "subject", point = {0, 0}, size = 38, font = "Pretendard", split = "words"}
local next = scene:spans {"A + B + C", id = "next", point = {0, 0}, size = 38, font = "Pretendard", split = "words"}
scene:match_transform(subject, next, {arc = 1.2, duration = 1.4})

return scene
```

```viscript-js
const scene = vis.scene({
    width: 960,
    height: 540,
});

scene.text({id: "caption", text: "match_transform", point: [0, 2.2], size: 20, font: "JetBrainsMono", fill: "muted"});
const subject = scene.spans(["A + B"], {id: "subject", point: [0, 0], size: 38, font: "Pretendard", split: "words"});
const next = scene.spans(["A + B + C"], {id: "next", point: [0, 0], size: 38, font: "Pretendard", split: "words"});
scene.matchTransform(subject, next, {arc: 1.2, duration: 1.4});

return scene;
```

### scene.edit

<!-- api-example: scene.edit -->

```viscript
local scene = vis.scene {
    width = 960,
    height = 540,
}

scene:text {id = "caption", text = "edit", point = {0, 2.2}, size = 20, font = "JetBrainsMono", fill = "muted"}
local subject = scene:code {id = "subject", text = "const total = 1;", language = "javascript", size = 30, font = "JetBrainsMono"}
local edited = scene:edit(subject, {replace = "total", with = "sum"}, {duration = 1.4})

return scene
```

```viscript-js
const scene = vis.scene({
    width: 960,
    height: 540,
});

scene.text({id: "caption", text: "edit", point: [0, 2.2], size: 20, font: "JetBrainsMono", fill: "muted"});
const subject = scene.code({id: "subject", text: "const total = 1;", language: "javascript", size: 30, font: "JetBrainsMono"});
const edited = scene.edit(subject, {replace: "total", with: "sum"}, {duration: 1.4});

return scene;
```
