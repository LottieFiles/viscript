---
title: Engine Properties
id: host-properties
order: 7
parent: host
---

# Engine Properties

Read the loaded scene's duration, camera, theme and runtime capabilities. These are properties rather than method calls.

## Functions

| Lua | JavaScript | Purpose |
| --- | --- | --- |
| — | `host.duration` | Scene timeline length in seconds. |
| — | `host.loop` | Whether the Scene requests looping. |
| — | `host.cameraMode` | `fixed` or `interactive`. |
| — | `host.cameraView` | `2d` or `3d`. |
| — | `host.adaptiveTheme` | Whether the loaded Scene uses the host theme. |
| — | `host.retainedLua` | Whether the loaded Scene explicitly created `vis.runtime(...)`. |
| — | `host.runtimeTime` | Retained simulation time, or 0 without a retained Runtime. |
| — | `host.audio` | Optional audio adapter. |

Host snippets use the variables from [Shared Host Setup](./setup.md).

## Examples

### host.duration

<!-- api-example: host.duration -->

```javascript
const host = await createVis({module});
host.load(source);
write(host.duration);
host.draw(canvas, 0.5);
```

### host.loop

<!-- api-example: host.loop -->

```javascript
const host = await createVis({module});
host.load(`local scene = vis.scene {width = 640, height = 360, loop = true}
local dot = scene:circle {radius = 1, fill = "accent"}
scene:shift(dot, {2, 0}, 1)
return scene`);
write(host.loop);
host.draw(canvas, 0.5);
```

### host.cameraMode

<!-- api-example: host.cameraMode -->

```javascript
const host = await createVis({module});
host.load(source);
write(host.cameraMode);
host.draw(canvas, 0.5);
```

### host.cameraView

<!-- api-example: host.cameraView -->

```javascript
const host = await createVis({module});
host.load(source);
write(host.cameraView);
host.draw(canvas, 0.5);
```

### host.adaptiveTheme

<!-- api-example: host.adaptiveTheme -->

```javascript
const host = await createVis({module});
host.hostTheme({dark: true, accent: "#e3b341"});
host.load(`local scene = vis.scene {width = 640, height = 360, theme = "adaptive"}
scene:circle {radius = 1, fill = "accent"}
return scene`);
write(host.adaptiveTheme);
host.draw(canvas, 0.5);
```

### host.retainedLua

<!-- api-example: host.retainedLua -->

```javascript
const host = await createVis({module});
host.load(retainedSource);
write(host.retainedLua);
host.draw(canvas, 0.5);
```

### host.runtimeTime

<!-- api-example: host.runtimeTime -->

```javascript
const host = await createVis({module});
host.load(retainedSource);
host.advanceRuntime(0.25);
write(host.runtimeTime);
host.draw(canvas, 0.5);
```

### host.audio

<!-- api-example: host.audio -->

```javascript
const host = await createVis({module});
host.load(source);
write(host.audio ? "Audio adapter available" : "Audio module absent");
host.draw(canvas, 0.5);
```
