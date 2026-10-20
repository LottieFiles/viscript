# Separate examples

Adjacent scenes without an API example marker stay separate:

```viscript
local scene = vis.scene {width = 480, height = 240}
scene:circle {radius = 0.5, fill = "accent"}
scene:wait(1)
return scene
```

```viscript-js
const scene = vis.scene({width: 480, height: 240});
scene.circle({radius: 0.75, fill: "accent"});
scene.wait(1);
return scene;
```

Another example after its own explanation:

```viscript
local scene = vis.scene {width = 480, height = 240}
scene:rectangle {size = {2, 1}, fill = "accent"}
scene:wait(1)
return scene
```
