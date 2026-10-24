---
title: Styling & Masks
id: object-styling
order: 14
parent: object
---

# Styling & Masks

Factories accept the styles their object type supports. Apply paint options at construction, and use `mask` to attach or remove a mask.

## Masks

| Lua | JavaScript | Purpose |
| --- | --- | --- |
| `object:mask(target, mode?)` | `object.mask(target, mode?)` | Apply a mask using `alpha`, `inverse_alpha`, `luma`, or `inverse_luma`; Lua `nil` / JS `null` removes it. |

## Shared styles

Supported style fields are `id`, `color`, `stroke`, `fill`, `gradient`, `width`, `radius`, `dash`, `dash_offset`, `opacity`, `progress`, `layer`, `isolate`, `mask`, `blend`, `blur`, and `shadow`.

`gradient` accepts a flag, end color, or linear/radial/conic configuration. `shadow` accepts `color`, `offset`, and `blur`. Stroke widths, dashes, arrowheads, blur, and shadow offsets use screen pixels. Geometry uses world units; a point's radius uses pixels.

Groups inherit `stroke`, `fill`, and `width` into descendants; they do not accept `color`, `gradient`, `radius`, or dashes. Text/formulas use fills instead of shape outlines. Pictures preserve source colors; cells use their own base color and patches. See [Styling](https://lottiefiles.github.io/viscript/#/docs/styling) for paint options.

## Examples

### object.mask

<!-- api-example: object.mask -->

```viscript
local scene = vis.scene {
    width = 960,
    height = 540,
}

local content = scene:rectangle {size = {3.6, 2.2}, fill = "accent", stroke = "#00000000"}
local mask = scene:circle {radius = 1, fill = "foreground", stroke = "#00000000"}
content:mask(mask, "alpha")
scene:text {id = "caption", text = "Only the circle's alpha reveals the rectangle", point = {0, -2.25}, size = 16, fill = "muted"}
scene:shift(mask, {1.3, 0}, {duration = 1.2})

return scene
```

```viscript-js
const scene = vis.scene({
    width: 960,
    height: 540,
});

const content = scene.rectangle({size: [3.6, 2.2], fill: "accent", stroke: "#00000000"});
const mask = scene.circle({radius: 1, fill: "foreground", stroke: "#00000000"});
content.mask(mask, "alpha");
scene.text({id: "caption", text: "Only the circle's alpha reveals the rectangle", point: [0, -2.25], size: 16, fill: "muted"});
scene.shift(mask, [1.3, 0], {duration: 1.2});

return scene;
```
