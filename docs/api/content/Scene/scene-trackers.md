---
title: Trackers
id: scene-trackers
order: 7
parent: scene
---

# Trackers

Trackers drive sampled object properties. Callbacks return a state table/object and must be pure: they cannot author scenes. `shift` is relative to the object's position when bound; `transform` is absolute.

## Values and bindings

| Lua | JavaScript | Purpose |
| --- | --- | --- |
| `scene:tracker(value)` | `scene.tracker(value)` | Return a tracker with an initial finite number. |
| `tracker.get()` | `tracker.get()` | Return the value reached at this point in the script. |
| `scene:bind(target, trackers, fn)` | `scene.bind(target, trackers, fn)` | Bind one object to one tracker or a list; apply the initial state and return the object. |
| `scene:track(tracker, value, options)`<br />`scene:track(moves, options)` | `scene.track(tracker, value, options)`<br />`scene.track(moves, options)` | Move one tracker, or a list of tracker/value pairs, while baking bound states at scene fps. |

`track` requires an options table/object with `duration` and `curve`; omitted fields default to 1 second and `smooth`.

Bindings accept the [animation descriptor properties](../Animation/index.md) except `position`, `arc`, `text`, `retype`, `sweep`, and `layer`. One object has one binding; different trackers can overlap on shared objects, but one tracker cannot run overlapping tracks.

## Examples

### scene.tracker

<!-- api-example: scene.tracker -->

```viscript
local scene = vis.scene {
    width = 960,
    height = 540,
}

local ball = scene:circle {id = "ball", center = {-2, 0}, radius = 0.5, fill = "accent"}
local tracker = scene:tracker(0)
scene:bind(ball, tracker, function(value) return {shift = {value, 0}} end)
scene:track(tracker, 4, {duration = 1.5})

return scene
```

```viscript-js
const scene = vis.scene({
    width: 960,
    height: 540,
});

const ball = scene.circle({id: "ball", center: [-2, 0], radius: 0.5, fill: "accent"});
const tracker = scene.tracker(0);
scene.bind(ball, tracker, (value) => ({shift: [value, 0]}));
scene.track(tracker, 4, {duration: 1.5});

return scene;
```

### tracker.get

<!-- api-example: tracker.get -->

```viscript
local scene = vis.scene {
    width = 960,
    height = 540,
}

local tracker = scene:tracker(2.5)
local value = tracker.get()
scene:number {id = "value", value = value, decimals = 1, size = 48, fill = "accent"}

return scene
```

```viscript-js
const scene = vis.scene({
    width: 960,
    height: 540,
});

const tracker = scene.tracker(2.5);
const value = tracker.get();
scene.number({id: "value", value, decimals: 1, size: 48, fill: "accent"});

return scene;
```

### scene.bind

<!-- api-example: scene.bind -->

```viscript
local scene = vis.scene {
    width = 960,
    height = 540,
}

local ball = scene:circle {id = "ball", center = {-2, 0}, radius = 0.5, fill = "accent"}
local phase = scene:tracker(0)
scene:bind(ball, phase, function(value)
    return {shift = {2 + 1.5 * math.cos(value), 1.5 * math.sin(value)}}
end)
scene:track(phase, 2 * math.pi, {duration = 2, curve = "linear"})

return scene
```

```viscript-js
const scene = vis.scene({
    width: 960,
    height: 540,
});

const ball = scene.circle({id: "ball", center: [-2, 0], radius: 0.5, fill: "accent"});
const phase = scene.tracker(0);
scene.bind(ball, phase, (value) => ({shift: [2 + 1.5 * Math.cos(value), 1.5 * Math.sin(value)]}));
scene.track(phase, 2 * Math.PI, {duration: 2, curve: "linear"});

return scene;
```

### scene.track

<!-- api-example: scene.track -->

```viscript
local scene = vis.scene {
    width = 960,
    height = 540,
}

local ball = scene:circle {id = "ball", center = {-2, 0}, radius = 0.5, fill = "accent"}
local tracker = scene:tracker(0)
scene:bind(ball, tracker, function(value) return {shift = {value, 0}} end)
scene:track(tracker, 4, {duration = 1.5, curve = "linear"})

return scene
```

```viscript-js
const scene = vis.scene({
    width: 960,
    height: 540,
});

const ball = scene.circle({id: "ball", center: [-2, 0], radius: 0.5, fill: "accent"});
const tracker = scene.tracker(0);
scene.bind(ball, tracker, (value) => ({shift: [value, 0]}));
scene.track(tracker, 4, {duration: 1.5, curve: "linear"});

return scene;
```
