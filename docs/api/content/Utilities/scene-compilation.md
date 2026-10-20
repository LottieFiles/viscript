---
title: Scene Compilation
id: scene-compilation
order: 2
parent: utilities
---

# Scene Compilation

Compile a JavaScript builder to Lua, or evaluate trusted editor JavaScript into a builder before loading it.

## Functions

| Lua | JavaScript | Purpose |
| --- | --- | --- |
| — | `compileScene(scene)` | Compile a JavaScript `SceneBuilder` to Lua source. |
| — | `evaluateScene(source, name?)` | Evaluate trusted JavaScript source that returns an owning root `SceneBuilder`; default name is `scene.js`. |

These exports are available from `bindings.js` and `client.js`. `evaluateScene` runs JavaScript in the host environment; use trusted authoring code. A builder also exposes [scene.compile](../Scene/scene-construction.md).

Host snippets use the variables from [Shared Host Setup](../Host/setup.md).

## Examples

### compileScene

<!-- api-example: compileScene -->

```javascript
const lua = compileScene(source);
const host = await createVis({module});
host.load(lua, "compiled.lua");
host.draw(canvas, 0.5);
write(lua);
```

### evaluateScene

<!-- api-example: evaluateScene -->

```javascript
const definition = evaluateScene('const scene = vis.scene({width: 640, height: 360}); scene.circle({radius: 1, fill: "accent"}); return scene;');
const host = await createVis(definition, "evaluated.js", {module});
host.draw(canvas, 0);
```
