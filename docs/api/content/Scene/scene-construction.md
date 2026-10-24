---
title: Construction
id: scene-construction
order: 1
parent: scene
---

# Construction

Create the root scene and set its frame and camera. Return the root scene from the script. JavaScript uses `vis.scene()` as its construction entry point; host tools can use [Scene Compilation](../Utilities/scene-compilation.md) when they need Lua source.

## Create a scene

| Lua | JavaScript | Purpose |
| --- | --- | --- |
| `vis.scene(config)` | `vis.scene(config?)` | Create a scene. Lua accepts an empty table for default configuration. |

## Frame configuration

`config` fields: `width`, `height` (pixels), `fps`, `background`, `antialiasing`, `loop`, `theme`, `camera`. Width, height and fps are positive integers. `theme` accepts `pro_white`, `pro_black`, `adaptive` (the default), or a custom theme table/object.

## Opening camera

`camera` fields: `mode` (`fixed`, `interactive`), `view` (`2d`, `3d`), `target`, `eye`, `up`, `height`, `projection` (`perspective`, `orthographic`), `fov`, `near`, `far`. Height is the visible world height, 6 when omitted; fov is in radians; clipping requires `0 < near < far`.

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
