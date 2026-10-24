---
title: Scene Compilation
id: scene-compilation
order: 2
parent: utilities
---

# Scene Compilation

Host and editor tools can compile a JavaScript builder to Lua, or evaluate trusted JavaScript into a builder before loading it. Scene scripts create builders with `vis.scene()` and return them directly.

## Functions

| Lua | JavaScript | Purpose |
| --- | --- | --- |
| — | `compileScene(scene)` | Compile a JavaScript `SceneBuilder` to Lua source. |
| — | `evaluateScene(source, name?)` | Evaluate trusted JavaScript source that returns an owning root `SceneBuilder`; default name is `scene.js`. |

These tooling exports are available from `bindings.js` and `client.js`. Hosts can pass a builder directly to `createVis` or `host.load`; explicit compilation is needed when a tool wants Lua source. `evaluateScene` runs JavaScript in the host environment; use trusted authoring code.

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
