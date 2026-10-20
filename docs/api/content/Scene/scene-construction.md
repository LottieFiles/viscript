---
title: Construction
id: scene-construction
order: 1
parent: scene
---

# Construction

Create the root scene, set its frame and camera, and compile a JavaScript builder to Lua. Return the root scene from the script.

## Create and compile

| Lua | JavaScript | Purpose |
| --- | --- | --- |
| `vis.scene(config)` | `vis.scene(config?)` | Create a scene. Lua accepts an empty table for default configuration. |
| — | `new SceneBuilder(config?)` | Construct the same JavaScript scene builder directly. |
| — | `scene.compile()` | Return the builder's generated Lua source. |

## Frame configuration

`config` fields: `width`, `height` (pixels), `fps`, `background`, `antialiasing`, `loop`, `theme`, `camera`. Width, height and fps are positive integers. `theme` accepts `pro_white`, `pro_black`, `adaptive` (the default), or a custom theme table/object.

## Opening camera

`camera` fields: `mode` (`fixed`, `interactive`), `view` (`2d`, `3d`), `target`, `eye`, `up`, `height`, `projection` (`perspective`, `orthographic`), `fov`, `near`, `far`. Height is the visible world height, 6 when omitted; fov is in radians; clipping requires `0 < near < far`.

Host snippets use the variables from [Shared Host Setup](../Host/setup.md).

## Examples

### vis.scene

<!-- api-example: vis.scene -->

```viscript
local scene = vis.scene {
    width = 960,
    height = 540,
}

scene:circle {id = "circle", radius = 0.8, fill = "accent"}
scene:text {id = "label", text = "960 x 540", point = {0, -1.4}, size = 24}

return scene
```

```viscript-js
const scene = vis.scene({
    width: 960,
    height: 540,
});

scene.circle({id: "circle", radius: 0.8, fill: "accent"});
scene.text({id: "label", text: "960 x 540", point: [0, -1.4], size: 24});

return scene;
```

### SceneBuilder

<!-- api-example: SceneBuilder -->

```javascript
const definition = new SceneBuilder({width: 640, height: 360});
definition.circle({radius: 1, fill: "accent"});
const host = await createVis(definition, "constructor.js", {module});
host.draw(canvas, 0);
```

### scene.compile

<!-- api-example: scene.compile -->

```viscript
local scene = vis.scene {
    width = 960,
    height = 540,
}

scene:text {id = "result", text = "Generated Lua source", size = 28}

return scene
```

```viscript-js
const scene = vis.scene({
    width: 960,
    height: 540,
});

const source = scene.compile();
scene.text({id: "result", text: source.includes("vis.scene") ? "Generated Lua source" : "Empty source", size: 28});

return scene;
```
