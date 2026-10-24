# Gain

A bounded fade stays quiet when its scene loops back to the beginning.

<!-- api-example: cue.gain -->

```viscript loop
local scene = vis.scene {width = 480, height = 240}
local bar = scene:rectangle {size = {3, 0.5}, fill = "accent"}
local cue = vis.audio.cue(scene, {asset = "tone.wav", gain = 1, loop = true, ["end"] = 1.5})
cue:gain(0, 0.3, {duration = 1, curve = "linear"})
scene:wait(0.3)
scene:fade(bar, 0, {duration = 1, curve = "linear"})
return scene
```

```viscript-js loop
const scene = vis.scene({width: 480, height: 240});
const bar = scene.rectangle({size: [3, 0.5], fill: "accent"});
const cue = scene.sound({asset: "tone.wav", gain: 1, loop: true, end: 1.5});
cue.gain(0, 0.3, {duration: 1, curve: "linear"});
scene.wait(0.3);
scene.fade(bar, 0, {duration: 1, curve: "linear"});
return scene;
```
