---
title: Audio cues
id: scene-audio
order: 8
parent: scene
---

# Audio cues

Lua audio requires the optional audio module. JavaScript sound calls compile to guarded Lua and remain silent when that module is unavailable. Cue times are absolute scene seconds and do not advance the cursor.

In Obsidian, click **Play** to hear the cue. The examples use `assets/tone.wav` (a short tone) and `assets/gain.wav` (a steady tone for the gain fade). WAV files are resolved like links from the note. Browser and Obsidian playback require WAV audio.

## Create and control a cue

| Lua | JavaScript | Purpose |
| --- | --- | --- |
| `vis.audio.cue(scene, options)` | `scene.sound(options)` | Return an audio cue for `asset`; options: `bus` (`music`, `effect`, `ui`), `begin`, `end`, `gain`, `loop`. |
| `cue:gain(value, begin, duration?, curve?)`<br />`cue:gain(value, begin, timing)` | `cue.gain(value, begin, duration?, curve?)`<br />`cue.gain(value, begin, timing)` | Animate cue gain (0–4) from absolute `begin`; default duration 1 and curve `smooth`. |

## Examples

### vis.audio.cue

<!-- api-example: vis.audio.cue -->

```viscript
local scene = vis.scene {
    width = 960,
    height = 540,
}

local pulse = scene:circle {id = "pulse", radius = 0.65, fill = "accent"}
if vis.audio then
    vis.audio.cue(scene, {asset = "tone.wav", begin = 0.3, gain = 0.3})
end
scene:wait(0.3)
scene:indicate(pulse, {duration = 0.6, scale = 1.5})

return scene
```

```viscript-js
const scene = vis.scene({
    width: 960,
    height: 540,
});

const pulse = scene.circle({id: "pulse", radius: 0.65, fill: "accent"});
scene.sound({asset: "tone.wav", begin: 0.3, gain: 0.3});
scene.wait(0.3);
scene.indicate(pulse, {duration: 0.6, scale: 1.5});

return scene;
```

### cue.gain

<!-- api-example: cue.gain -->

```viscript
local scene = vis.scene {
    width = 960,
    height = 540,
}

local bar = scene:rectangle {id = "gain", size = {4, 0.6}, fill = "accent"}
scene:text {id = "label", text = "gain: 1 to 0", point = {0, -1}, size = 24}
if vis.audio then
    local cue = vis.audio.cue(scene, {asset = "gain.wav", gain = 1, ["end"] = 1.5})
    cue:gain(0, 0.3, 1, "linear")
end
scene:wait(0.3)
scene:fade(bar, 0, 1, "linear")

return scene
```

```viscript-js
const scene = vis.scene({
    width: 960,
    height: 540,
});

const bar = scene.rectangle({id: "gain", size: [4, 0.6], fill: "accent"});
scene.text({id: "label", text: "gain: 1 to 0", point: [0, -1], size: 24});
const cue = scene.sound({asset: "gain.wav", gain: 1, end: 1.5});
cue.gain(0, 0.3, 1, "linear");
scene.wait(0.3);
scene.fade(bar, 0, 1, "linear");

return scene;
```
