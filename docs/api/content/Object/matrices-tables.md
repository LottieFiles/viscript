---
title: Matrices & Tables
id: matrices-tables
order: 11
parent: object
---

# Matrices & Tables

Typeset rectangular data as a bracketed matrix or a styled table. Rows become Groups for later selection.

| Lua | JavaScript | Purpose |
| --- | --- | --- |
| `scene:matrix(opts)` | `scene.matrix(opts)` | Typeset rectangular `rows` into row Groups with `brackets`; configure `gap`, `justify`, `decimals`, `reserve`, and `tabular`. |
| `scene:table(opts)` | `scene.table(opts)` | Typeset `rows` with optional `header`, `padding`, `justify`, `decimals`, `stripes`, and `rules`. Returns a Group. |

Matrix string entries use inline TeX when available; table strings stay plain text. Select a row, entry, or column with [Group selection methods](../Group/group-layout.md). Selection indices start at **1** in both languages.

## Examples

### scene.matrix

<!-- api-example: scene.matrix -->

```viscript
local scene = vis.scene {
    width = 960,
    height = 540,
}

scene:matrix {id = "matrix", rows = {{1, 2}, {3, 4}}, size = 32, gap = {0.6, 0.35}, fill = "foreground", stroke = "foreground"}
scene:text {id = "caption", text = "A 2 x 2 matrix with square brackets", point = {0, -2.25}, size = 16, fill = "muted"}

return scene
```

```viscript-js
const scene = vis.scene({
    width: 960,
    height: 540,
});

scene.matrix({id: "matrix", rows: [[1, 2], [3, 4]], size: 32, gap: [0.6, 0.35], fill: "foreground", stroke: "foreground"});
scene.text({id: "caption", text: "A 2 x 2 matrix with square brackets", point: [0, -2.25], size: 16, fill: "muted"});

return scene;
```

### scene.table

<!-- api-example: scene.table -->

```viscript
local scene = vis.scene {
    width = 960,
    height = 540,
}

scene:table {id = "data", header = {"item", "value"}, rows = {{"A", 12}, {"B", 28}}, point = {0, 0.3}, size = 22, padding = {0.3, 0.15}, rules = "grid", stroke = "muted", fill = "foreground"}
scene:text {id = "caption", text = "Header and body entries form selectable rows", point = {0, -2.25}, size = 16, fill = "muted"}

return scene
```

```viscript-js
const scene = vis.scene({
    width: 960,
    height: 540,
});

scene.table({id: "data", header: ["item", "value"], rows: [["A", 12], ["B", 28]], point: [0, 0.3], size: 22, padding: [0.3, 0.15], rules: "grid", stroke: "muted", fill: "foreground"});
scene.text({id: "caption", text: "Header and body entries form selectable rows", point: [0, -2.25], size: 16, fill: "muted"});

return scene;
```
