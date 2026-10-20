# Runtime audio

Click the scene, then press Space.

```viscript interactive
local scene = vis.scene {width = 480, height = 240}
local dot = scene:circle {radius = 0.5, fill = "accent"}
vis.runtime(scene, {
    fixed_step = 1 / 60,
    update = function(runtime)
        if runtime:key("Space").pressed then runtime:sound("tone.wav", {gain = 0.1}) end
    end,
})
return scene
```
