---
title: Engines & Loading
id: host-loading
order: 1
parent: host
---

# Engines & Loading

Create an engine, share a WebAssembly instance and load or replace a scene. These JavaScript calls run in the embedding application; `host` is the engine returned by `await createVis(...)`.

## Functions

| Lua | JavaScript | Purpose |
| --- | --- | --- |
| — | `createVisModule(options?)` | Load a WebAssembly instance; pass it as `{module}` to share one instance among engines. |
| — | `createVis(options?)` | Create an empty engine so fonts, assets and theme can be registered before loading. |
| — | `createVis(source, name?, options?)` | Create an engine and load Lua source or a `SceneBuilder`. |
| — | `host.load(source, name?)` | Replace the Scene and return its duration; failed loads retain the previous Scene. |
| — | `host.destroy()` | Release the engine; repeated calls are harmless. |

Import host functions from the runtime's `client.js`. To evaluate editor JavaScript before loading it, see [Scene Compilation](../Utilities/scene-compilation.md).

Host snippets use the variables from [Shared Host Setup](./setup.md).

## Examples

### createVisModule

<!-- api-example: createVisModule -->

```javascript
const shared = await createVisModule();
const host = await createVis({module: shared});
host.load(source);
host.draw(canvas, 0.5);
write("One module can host multiple engines.");
```

### createVis

<!-- api-example: createVis -->

```javascript
const host = await createVis({module});
host.load(source);
write("Created an engine, then loaded the scene.");
host.draw(canvas, 0.5);
```

### createVis.source

<!-- api-example: createVis.source -->

```javascript
const host = await createVis(source, "example.js", {module});
host.draw(canvas, 0.5);
write(host.duration);
```

### host.load

<!-- api-example: host.load -->

```javascript
const host = await createVis({module});
host.load(source);
const duration = host.load(source, "replacement.js");
write({duration});
host.draw(canvas, 0.5);
```

### host.destroy

<!-- api-example: host.destroy -->

```javascript
const host = await createVis({module});
host.load(source);
host.draw(canvas, 0.5);
host.destroy();
write("Engine released; the canvas keeps its last pixels.");
```
