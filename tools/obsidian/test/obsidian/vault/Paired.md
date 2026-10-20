# Paired example

The same API example in Lua and JavaScript:

<!-- api-example: scene.circle -->

```viscript loop
local scene = vis.scene {width = 480, height = 240}
scene:circle {radius = 0.5, fill = "accent"}
scene:wait(4)
return scene
```

```viscript-js loop
const scene = vis.scene({width: 480, height: 240});
scene.circle({radius: 0.75, fill: "accent"});
scene.wait(6);
return scene;
```
