---
title: Code Blocks & Search
id: code-blocks
order: 10
parent: object
---

# Code Blocks & Search

Create syntax-colored code and find its token handles. Code blocks contain line Groups; each line contains tokens.

## Create code

| Lua | JavaScript | Purpose |
| --- | --- | --- |
| `scene:code(opts)` | `scene.code(opts)` | Tokenize `text` using `language` into line Groups containing token handles; configure `colors`, `leading` or `pitch`, and text placement. |

## Select tokens

| Lua | JavaScript | Purpose |
| --- | --- | --- |
| `group:search(text, opts?)` | `group.search(textOrRegExp, opts?)` | Return code-block token handles touched by matches. Only Groups created by `code` support search. |

Search options are `match` (`first`, `last`, `all`, or a positive index), `lines` (one line or `[first,last]`), and `whole` (require whole-token boundaries). The default selects the first match. A partial-token match returns the whole token; JavaScript also accepts a `RegExp`. Only Groups created by `code` support search.

The same example area also shows animated [highlighting](../Animation/emphasis.md) and [code edits](../Animation/morph-edits.md).

## Examples

### scene.code

<!-- api-example: scene.code -->

```viscript
local scene = vis.scene {
    width = 960,
    height = 540,
}

scene:code {id = "listing", text = "local radius = 2\nreturn radius * radius", language = "lua", point = {-1.8, 0.6}, align = {0, 0.5}, size = 22}
scene:text {id = "caption", text = "Lines contain independently selectable tokens", point = {0, -2.25}, size = 16, fill = "muted"}

return scene
```

```viscript-js
const scene = vis.scene({
    width: 960,
    height: 540,
});

scene.code({id: "listing", text: "local radius = 2\nreturn radius * radius", language: "lua", point: [-1.8, 0.6], align: [0, 0.5], size: 22});
scene.text({id: "caption", text: "Lines contain independently selectable tokens", point: [0, -2.25], size: 16, fill: "muted"});

return scene;
```

### group.search

<!-- api-example: group.search -->

```viscript
local scene = vis.scene {
    width = 960,
    height = 540,
}

local code = scene:code {id = "listing", text = "local total = 2\nreturn total + 1", language = "plain", size = 24, point = {-1.8, 0.6}, align = {0, 0.5}, color = "foreground"}
local matches = code:search("total", {match = "all", whole = true})
for _, token in ipairs(matches) do scene:set {target = token, fill = "accent"} end
scene:text {id = "caption", text = "Search selects both complete total tokens", point = {0, -2.25}, size = 16, fill = "muted"}

return scene
```

```viscript-js
const scene = vis.scene({
    width: 960,
    height: 540,
});

const code = scene.code({id: "listing", text: "local total = 2\nreturn total + 1", language: "plain", size: 24, point: [-1.8, 0.6], align: [0, 0.5], color: "foreground"});
const matches = code.search("total", {match: "all", whole: true});
for (const token of matches) scene.set({target: token, fill: "accent"});
scene.text({id: "caption", text: "Search selects both complete total tokens", point: [0, -2.25], size: 16, fill: "muted"});

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
