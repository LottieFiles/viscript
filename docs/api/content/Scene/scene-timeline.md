---
title: Timeline
id: scene-timeline
order: 3
parent: scene
---

# Timeline

Control the authoring cursor in seconds. Markers name times; callbacks place work at a chosen time or run tracks together.

## Cursor and scheduling

| Lua | JavaScript | Purpose |
| --- | --- | --- |
| `scene:wait(duration?)` | `scene.wait(duration?)` | Advance the cursor, leaving the frame unchanged; default 1, zero allowed. |
| `scene:mark(name)` | `scene.mark(name)` | Name the current cursor time. |
| `scene:at(timeOrMarker, fn)` | `scene.at(timeOrMarker, fn)` | Author the callback at an absolute time or marker, then restore the cursor. The callback receives the scene. |
| `scene:parallel(fn, ...)`<br />`scene:parallel(functions)` | `scene.parallel(...functions)`<br />`scene.parallel(functions)` | Start each callback together; continue after the longest track. |
| `scene:time(marker?)` | — | Return the cursor time, or the named marker's time. |
| `scene:seek(timeOrMarker)` | — | Move the authoring cursor directly. |
| `scene:duration()` | — | Return the latest clip, cursor or child viewport end time. |

## Examples

### scene.wait

<!-- api-example: scene.wait -->

```viscript
local scene = vis.scene {
    width = 960,
    height = 540,
}

local ball = scene:circle {id = "ball", center = {-2, 0}, radius = 0.5, fill = "accent"}
scene:wait(1)
scene:shift(ball, {4, 0}, 0.8)

return scene
```

```viscript-js
const scene = vis.scene({
    width: 960,
    height: 540,
});

const ball = scene.circle({id: "ball", center: [-2, 0], radius: 0.5, fill: "accent"});
scene.wait(1);
scene.shift(ball, [4, 0], 0.8);

return scene;
```

### scene.mark

<!-- api-example: scene.mark -->

```viscript
local scene = vis.scene {
    width = 960,
    height = 540,
}

local ball = scene:circle {id = "ball", center = {-2, 0}, radius = 0.5, fill = "accent"}
scene:wait(0.5)
scene:mark("move")
scene:at("move", function(s) s:shift(ball, {4, 0}, 1) end)

return scene
```

```viscript-js
const scene = vis.scene({
    width: 960,
    height: 540,
});

const ball = scene.circle({id: "ball", center: [-2, 0], radius: 0.5, fill: "accent"});
scene.wait(0.5);
scene.mark("move");
scene.at("move", (s) => s.shift(ball, [4, 0], 1));

return scene;
```

### scene.at

<!-- api-example: scene.at -->

```viscript
local scene = vis.scene {
    width = 960,
    height = 540,
}

local ball = scene:circle {id = "ball", center = {-2, 0}, radius = 0.5, fill = "accent"}
local other = scene:rectangle {id = "other", center = {2, 0}, size = {1, 1}, stroke = "foreground"}
scene:at(0.4, function(s) s:shift(ball, {4, 0}, 1) end)
scene:at(0.9, function(s) s:shift(other, {-4, 0}, 1) end)

return scene
```

```viscript-js
const scene = vis.scene({
    width: 960,
    height: 540,
});

const ball = scene.circle({id: "ball", center: [-2, 0], radius: 0.5, fill: "accent"});
const other = scene.rectangle({id: "other", center: [2, 0], size: [1, 1], stroke: "foreground"});
scene.at(0.4, (s) => s.shift(ball, [4, 0], 1));
scene.at(0.9, (s) => s.shift(other, [-4, 0], 1));

return scene;
```

### scene.parallel

<!-- api-example: scene.parallel -->

```viscript
local scene = vis.scene {
    width = 960,
    height = 540,
}

local ball = scene:circle {id = "ball", center = {-2, 0}, radius = 0.5, fill = "accent"}
local other = scene:rectangle {id = "other", center = {2, 0}, size = {1, 1}, stroke = "foreground"}
scene:parallel(
    function(s) s:shift(ball, {0, 1.5}, 1) end,
    function(s) s:shift(other, {0, -1.5}, 1.5) end
)

return scene
```

```viscript-js
const scene = vis.scene({
    width: 960,
    height: 540,
});

const ball = scene.circle({id: "ball", center: [-2, 0], radius: 0.5, fill: "accent"});
const other = scene.rectangle({id: "other", center: [2, 0], size: [1, 1], stroke: "foreground"});
scene.parallel(
    (s) => s.shift(ball, [0, 1.5], 1),
    (s) => s.shift(other, [0, -1.5], 1.5),
);

return scene;
```

### scene.time

<!-- api-example: scene.time -->

```viscript
local scene = vis.scene {
    width = 960,
    height = 540,
}

local ball = scene:circle {id = "ball", center = {-2, 0}, radius = 0.5, fill = "accent"}
scene:shift(ball, {4, 0}, 1)
local time = scene:time()
scene:text {id = "time", text = string.format("cursor = %.1f s", time), point = {0, -1.5}, size = 24}

return scene
```

### scene.seek

<!-- api-example: scene.seek -->

```viscript
local scene = vis.scene {
    width = 960,
    height = 540,
}

local ball = scene:circle {id = "ball", center = {-2, 0}, radius = 0.5, fill = "accent"}
local other = scene:rectangle {id = "other", center = {2, 0}, size = {1, 1}, stroke = "foreground"}
scene:shift(ball, {4, 0}, 1)
scene:seek(0)
scene:shift(other, {-4, 0}, 1)

return scene
```

### scene.duration

<!-- api-example: scene.duration -->

```viscript
local scene = vis.scene {
    width = 960,
    height = 540,
}

local ball = scene:circle {id = "ball", center = {-2, 0}, radius = 0.5, fill = "accent"}
scene:shift(ball, {4, 0}, 1.2)
local duration = scene:duration()
scene:text {id = "duration", text = string.format("duration = %.1f s", duration), point = {0, -1.5}, size = 24}

return scene
```
