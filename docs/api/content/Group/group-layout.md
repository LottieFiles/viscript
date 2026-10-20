---
title: Group Layout & Selection
id: group-layout
order: 1
parent: group-space
---

# Group Layout & Selection

Group objects to share a transform/style, arrange their children, and select rows or entries. Layout methods run before the first timeline animation.

## Create a Group

| Lua | JavaScript | Purpose |
| --- | --- | --- |
| `scene:group(opts)` | `scene.group(opts?)` | Create a transform/style container; `matrix` sets its transform and `typeset` treats descendant text as one layout block. |

## Arrange children

| Lua | JavaScript | Purpose |
| --- | --- | --- |
| `group:arrange(direction?, gap?)` | `group.arrange(direction?, gap?)` | Lay out direct children in `direction` (default `[1,0,0]`) with `gap` (default 0.25). |
| `group:arrange_grid(columns, columnGap?, rowGap?)` | `group.arrangeGrid(columns, columnGap?, rowGap?)` | Lay out children in rows of `columns`, top to bottom; each gap defaults to 0.25. |

## Select children

| Lua | JavaScript | Purpose |
| --- | --- | --- |
| `group:row(index)` | `group.row(index)` | Return the indexed direct child Group, skipping non-Group children. |
| `group:entry(row, column)` | `group.entry(row, column)` | Return the indexed direct child of a row. |
| `group:column(index)` | `group.column(index)` | Return that column's child from each row that contains it, in row order. |

Selection indices count from **1** in both languages; a row is a Group directly below the receiver. Group, formula Group, and Space inherit the factories and methods in [Object](../Object/index.md).

For code tokens use [Code Blocks & Search](../Object/code-blocks.md); for formula parts use [Rich Text & TeX](../Object/rich-text-tex.md).

## Examples

### scene.group

<!-- api-example: scene.group -->

```viscript
local scene = vis.scene {
    width = 960,
    height = 540,
}

local group = scene:group {id = "pair"}
group:circle {center = {-0.7, 0}, radius = 0.5, stroke = "accent", width = 2}
group:rectangle {center = {0.7, 0}, size = {0.8, 1}, stroke = "foreground"}
scene:shift(group, {1, 0}, 1)
scene:text {id = "caption", text = "Children share one transform", point = {0, -2.25}, size = 16, fill = "muted"}

return scene
```

```viscript-js
const scene = vis.scene({
    width: 960,
    height: 540,
});

const group = scene.group({id: "pair"});
group.circle({center: [-0.7, 0], radius: 0.5, stroke: "accent", width: 2});
group.rectangle({center: [0.7, 0], size: [0.8, 1], stroke: "foreground"});
scene.shift(group, [1, 0], 1);
scene.text({id: "caption", text: "Children share one transform", point: [0, -2.25], size: 16, fill: "muted"});

return scene;
```

### group.arrange

<!-- api-example: group.arrange -->

```viscript
local scene = vis.scene {
    width = 960,
    height = 540,
}

local group = scene:group {}
for i = 1, 3 do group:circle {radius = 0.4 + i * 0.12, stroke = i == 2 and "accent" or "foreground", width = 2} end
group:arrange({1, 0}, 0.35)
group:move_to({0, 0})
scene:text {id = "caption", text = "Different bounds, equal 0.35-unit gaps", point = {0, -2.25}, size = 16, fill = "muted"}

return scene
```

```viscript-js
const scene = vis.scene({
    width: 960,
    height: 540,
});

const group = scene.group();
for (let i = 1; i <= 3; i++) group.circle({radius: 0.4 + i * 0.12, stroke: i === 2 ? "accent" : "foreground", width: 2});
group.arrange([1, 0], 0.35);
group.moveTo([0, 0]);
scene.text({id: "caption", text: "Different bounds, equal 0.35-unit gaps", point: [0, -2.25], size: 16, fill: "muted"});

return scene;
```

### group.arrange_grid

<!-- api-example: group.arrange_grid -->

```viscript
local scene = vis.scene {
    width = 960,
    height = 540,
}

local group = scene:group {}
for i = 1, 6 do group:rectangle {size = {0.8, 0.6}, stroke = i == 4 and "accent" or "foreground", width = 2} end
group:arrange_grid(3, 0.4, 0.3)
group:move_to({0, 0})
scene:text {id = "caption", text = "Three columns; rows fill from top to bottom", point = {0, -2.25}, size = 16, fill = "muted"}

return scene
```

```viscript-js
const scene = vis.scene({
    width: 960,
    height: 540,
});

const group = scene.group();
for (let i = 1; i <= 6; i++) group.rectangle({size: [0.8, 0.6], stroke: i === 4 ? "accent" : "foreground", width: 2});
group.arrangeGrid(3, 0.4, 0.3);
group.moveTo([0, 0]);
scene.text({id: "caption", text: "Three columns; rows fill from top to bottom", point: [0, -2.25], size: 16, fill: "muted"});

return scene;
```

### group.row

<!-- api-example: group.row -->

```viscript
local scene = vis.scene {
    width = 960,
    height = 540,
}

local matrix = scene:matrix {rows = {{1, 2, 3}, {4, 5, 6}}, size = 32, fill = "foreground", stroke = "foreground"}
local row = matrix:row(2)
for _, entry in ipairs(row:children()) do scene:set {target = entry, fill = "accent"} end
scene:text {id = "caption", text = "row(2) selects the second row", point = {0, -2.25}, size = 16, fill = "muted"}

return scene
```

```viscript-js
const scene = vis.scene({
    width: 960,
    height: 540,
});

const matrix = scene.matrix({rows: [[1, 2, 3], [4, 5, 6]], size: 32, fill: "foreground", stroke: "foreground"});
const row = matrix.row(2);
for (const entry of row.children()) scene.set({target: entry, fill: "accent"});
scene.text({id: "caption", text: "row(2) selects the second row", point: [0, -2.25], size: 16, fill: "muted"});

return scene;
```

### group.entry

<!-- api-example: group.entry -->

```viscript
local scene = vis.scene {
    width = 960,
    height = 540,
}

local matrix = scene:matrix {rows = {{1, 2, 3}, {4, 5, 6}}, size = 32, fill = "foreground", stroke = "foreground"}
local entry = matrix:entry(2, 3)
scene:set {target = entry, fill = "accent"}
scene:text {id = "caption", text = "entry(2, 3) selects the value 6", point = {0, -2.25}, size = 16, fill = "muted"}

return scene
```

```viscript-js
const scene = vis.scene({
    width: 960,
    height: 540,
});

const matrix = scene.matrix({rows: [[1, 2, 3], [4, 5, 6]], size: 32, fill: "foreground", stroke: "foreground"});
const entry = matrix.entry(2, 3);
scene.set({target: entry, fill: "accent"});
scene.text({id: "caption", text: "entry(2, 3) selects the value 6", point: [0, -2.25], size: 16, fill: "muted"});

return scene;
```

### group.column

<!-- api-example: group.column -->

```viscript
local scene = vis.scene {
    width = 960,
    height = 540,
}

local matrix = scene:matrix {rows = {{1, 2, 3}, {4, 5, 6}}, size = 32, fill = "foreground", stroke = "foreground"}
local column = matrix:column(2)
for _, entry in ipairs(column) do scene:set {target = entry, fill = "accent"} end
scene:text {id = "caption", text = "column(2) returns values 2 and 5", point = {0, -2.25}, size = 16, fill = "muted"}

return scene
```

```viscript-js
const scene = vis.scene({
    width: 960,
    height: 540,
});

const matrix = scene.matrix({rows: [[1, 2, 3], [4, 5, 6]], size: 32, fill: "foreground", stroke: "foreground"});
const column = matrix.column(2);
for (const entry of column) scene.set({target: entry, fill: "accent"});
scene.text({id: "caption", text: "column(2) returns values 2 and 5", point: [0, -2.25], size: 16, fill: "muted"});

return scene;
```

### scene.arrange

<!-- api-example: scene.arrange -->

```viscript
local scene = vis.scene {
    width = 960,
    height = 540,
}

scene:text {id = "caption", text = "arrange", point = {0, 2.2}, size = 20, font = "JetBrainsMono", fill = "muted"}
local items = {}
for i = 1, 3 do
    items[i] = scene:rectangle {id = "item" .. i, center = {i - 2, (i % 2) * 1.2 - 0.6}, size = {0.8, 0.8}, fill = "accent"}
end
scene:arrange(items, {gap = 0.5, center = {0, 0}, duration = 1.2})

return scene
```

```viscript-js
const scene = vis.scene({
    width: 960,
    height: 540,
});

scene.text({id: "caption", text: "arrange", point: [0, 2.2], size: 20, font: "JetBrainsMono", fill: "muted"});
const items = Array.from({length: 3}, (_, n) => {
    const i = n + 1;
    return scene.rectangle({id: "item" + i, center: [i - 2, (i % 2) * 1.2 - 0.6], size: [0.8, 0.8], fill: "accent"});
});
scene.arrange(items, {gap: 0.5, center: [0, 0], duration: 1.2});

return scene;
```
