# Audio

The cue and the player share the timeline, in both languages.

<!-- api-example: vis.audio.cue -->

```viscript loop
local scene = vis.scene {width = 480, height = 240}
scene:circle {radius = 0.5, fill = "accent"}
vis.audio.cue(scene, {asset = "tone.wav", bus = "music", gain = 0.1, loop = true, ["end"] = 4})
scene:wait(4)
return scene
```

```viscript-js loop
const scene = vis.scene({width: 480, height: 240});
scene.circle({radius: 0.5, fill: "accent"});
scene.sound({asset: "tone.wav", bus: "music", gain: 0.1, loop: true, end: 4});
scene.wait(5);
return scene;
```
