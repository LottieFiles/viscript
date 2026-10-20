---
title: Host Input & Simulation
id: host-input
order: 5
parent: host
---

# Host Input & Simulation

Input methods exist when the corresponding modules are compiled. UI controls receive events before unhandled events reach Input. Event coordinates and movement are in logical Scene pixels.

## Functions

| Lua | JavaScript | Purpose |
| --- | --- | --- |
| — | `inputKeyName(key, code?)` | Convert browser key/code strings into a supported Viscript key name; return undefined for unsupported keys. |
| — | `host.cameraInput(action, x?, y?)` | Apply `pan`, `orbit`, `zoom`, `reset`, `view2d` or `view3d`; return false for fixed cameras or unavailable modules. |
| — | `host.input(event)` | Deliver pointer, wheel or keyboard input; return `handled`, `redraw`, `capture`, `release` and optional `sample`/`error`. |
| — | `host.beginInputFrame(time)` | Start a host frame and clear transient key/button edges, pointer movement and wheel deltas. |
| — | `host.releaseInput(time)` | Release all keys and pointer buttons, including after focus loss; return input-result flags. |
| — | `host.advanceRuntime(elapsed)` | Advance retained Lua by elapsed seconds; return `{time, dropped, tick, steps, interpolation, sounds}`, or null without a retained Runtime. |

Input types are `pointerdown`, `pointermove`, `pointerup`, `pointercancel`, `wheel`, `keydown` and `keyup`. Every event requires `time`; optional fields are `x`, `y`, `dx`, `dy`, `wheelX`, `wheelY`, `button`, `buttons`, `modifiers`, `pointerId` and `key`. Shift/Ctrl/Alt/Meta use modifier bits 0–3.

Host snippets use the variables from [Shared Host Setup](./setup.md).

## Examples

### inputKeyName

<!-- api-example: inputKeyName -->

```javascript
const host = await createVis({module});
host.load(source);
write({space: inputKeyName(" ", "Space"), letter: inputKeyName("d", "KeyD")});
host.draw(canvas, 0.5);
```

### host.cameraInput

<!-- api-example: host.cameraInput -->

```javascript
const host = await createVis({module});
host.load(source);
host.cameraInput("zoom", 0, 0.5);
write("Zoom input changes the interactive camera.");
host.draw(canvas, 0.5);
```

### host.input

<!-- api-example: host.input -->

```javascript
const host = await createVis({module});
host.load(retainedSource);
host.beginInputFrame(0);
const result = host.input({type: "pointermove", x: 440, y: 180, pointerId: 0, time: 0});
host.advanceRuntime(1 / 60);
write(result);
host.draw(canvas, 0.5);
```

### host.beginInputFrame

<!-- api-example: host.beginInputFrame -->

```javascript
const host = await createVis({module});
host.load(retainedSource);
host.beginInputFrame(0);
host.input({type: "pointermove", x: 440, y: 180, dx: 20, pointerId: 0, time: 0});
host.advanceRuntime(1 / 60);
write("Input frame begins before events and simulation.");
host.draw(canvas, 0.5);
```

### host.releaseInput

<!-- api-example: host.releaseInput -->

```javascript
const host = await createVis({module});
host.load(retainedSource);
host.beginInputFrame(0);
host.input({type: "pointerdown", x: 440, y: 180, button: 0, buttons: 1, pointerId: 0, time: 0});
write(host.releaseInput(0.1));
host.advanceRuntime(1 / 60);
host.draw(canvas, 0.5);
```

### host.advanceRuntime

<!-- api-example: host.advanceRuntime -->

```javascript
const host = await createVis({module});
host.load(retainedSource);
write(host.advanceRuntime(0.1));
host.draw(canvas, 0.5);
```
