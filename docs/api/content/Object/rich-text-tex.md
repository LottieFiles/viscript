---
title: Rich Text & TeX
id: rich-text-tex
order: 9
parent: object
---

# Rich Text & TeX

Typeset styled text parts and formulas, then select formula parts for individual styling or animation.

## Typeset

| Lua | JavaScript | Purpose |
| --- | --- | --- |
| `scene:spans(partsAndOpts)` | `scene.spans(parts, opts?)` | Typeset strings, styled text parts, and TeX parts into a Group; configure `split`, `wrap`, `leading`, and `baseline`. Lua combines numbered parts and named options in one table. |
| `scene:tex(opts)` | `scene.tex(opts)` | Typeset `tex` as a string or styled parts; configure text placement, `display`, and `baseline`. Returns a formula Group; requires the TeX module. |

## Select formula parts

| Lua | JavaScript | Purpose |
| --- | --- | --- |
| `group:part(tex, nth?)` | `group.part(tex, nth?)` | Find the `nth` formula part or glyph/shape whose TeX matches, ignoring blanks; default 1. On a part, search its own pieces. Requires the TeX module and a formula/part receiver. |

TeX requires the TeX module. Formula roots and explicit part Groups expose [Group methods](../Group/group-layout.md); individual glyph/shape results expose [Object methods](./object-placement.md).

## Examples

### scene.spans

<!-- api-example: scene.spans -->

```viscript
local scene = vis.scene {
    width = 960,
    height = 540,
}

scene:spans {"One ", {text = "styled", fill = "accent"}, " baseline", id = "sentence", point = {0, 0}, size = 28, fill = "foreground"}
scene:text {id = "caption", text = "Styled parts share a text baseline", point = {0, -2.25}, size = 16, fill = "muted"}

return scene
```

```viscript-js
const scene = vis.scene({
    width: 960,
    height: 540,
});

scene.spans(["One ", {text: "styled", fill: "accent"}, " baseline"], {id: "sentence", point: [0, 0], size: 28, fill: "foreground"});
scene.text({id: "caption", text: "Styled parts share a text baseline", point: [0, -2.25], size: 16, fill: "muted"});

return scene;
```

### scene.tex

<!-- api-example: scene.tex -->

```viscript
local scene = vis.scene {
    width = 960,
    height = 540,
}

scene:tex {id = "formula", tex = {"a^2", "+", "b^2", "=", "c^2"}, point = {0, 0}, size = 32, fill = "foreground"}
scene:text {id = "caption", text = "TeX becomes selectable formula parts", point = {0, -2.25}, size = 16, fill = "muted"}

return scene
```

```viscript-js
const scene = vis.scene({
    width: 960,
    height: 540,
});

scene.tex({id: "formula", tex: ["a^2", "+", "b^2", "=", "c^2"], point: [0, 0], size: 32, fill: "foreground"});
scene.text({id: "caption", text: "TeX becomes selectable formula parts", point: [0, -2.25], size: 16, fill: "muted"});

return scene;
```

### group.part

<!-- api-example: group.part -->

```viscript
local scene = vis.scene {
    width = 960,
    height = 540,
}

local formula = scene:tex {id = "formula", tex = {"a^2", "+", "b^2", "=", "c^2"}, size = 32, fill = "foreground"}
local result = formula:part("c^2")
scene:set {target = result, fill = "accent"}
scene:text {id = "caption", text = "Select one formula part by its TeX", point = {0, -2.25}, size = 16, fill = "muted"}

return scene
```

```viscript-js
const scene = vis.scene({
    width: 960,
    height: 540,
});

const formula = scene.tex({id: "formula", tex: ["a^2", "+", "b^2", "=", "c^2"], size: 32, fill: "foreground"});
const result = formula.part("c^2");
scene.set({target: result, fill: "accent"});
scene.text({id: "caption", text: "Select one formula part by its TeX", point: [0, -2.25], size: 16, fill: "muted"});

return scene;
```
