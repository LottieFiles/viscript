---
title: Audio Playback
id: host-audio
order: 6
parent: host
---

# Audio Playback

`host.audio` exists only when the optional audio module is compiled. Buses are `music`, `effect` and `ui`; gain is 0–4 and rate is 0.125–8.

## Functions

| Lua | JavaScript | Purpose |
| --- | --- | --- |
| — | `host.audio.load(name, bytes)` | Register encoded audio bytes under an asset name. |
| — | `host.audio.start()` | Start audio output; call from a browser user gesture. |
| — | `host.audio.play(name, options?)` | Start an independent voice and return its numeric ID; accepts `bus`, `gain`, `rate` and `loop`. |
| — | `host.audio.stop(voice)` | Stop one voice. |
| — | `host.audio.active(voice)` | Report whether a voice remains active. |
| — | `host.audio.masterGain(value)` | Set the master output gain. |
| — | `host.audio.busGain(bus, value)` | Set one bus's gain. |
| — | `host.audio.transport(state)` | Synchronize Scene sound cues with `{time, rate?, playing?, seek?}`; mark discontinuous timeline jumps with `seek`. |

Run an audio example from the button gesture. Each snippet uses a generated WAV. Scene-authored cue construction is documented under [Audio Cues](../Scene/scene-audio.md).

Host snippets use the variables from [Shared Host Setup](./setup.md).

## Examples

### host.audio.load

<!-- api-example: host.audio.load -->

```javascript
const host = await createVis({module});
host.load(source);
if (!host.audio) throw new Error("This example requires the audio module.");
host.audio.load("tone.wav", toneBytes);
write("A generated WAV is registered as tone.wav.");
host.draw(canvas, 0.5);
```

### host.audio.start

<!-- api-example: host.audio.start -->

```javascript
const host = await createVis({module});
host.load(source);
if (!host.audio) throw new Error("This example requires the audio module.");
host.audio.load("tone.wav", toneBytes);
host.audio.start();
write("Audio output started from the Run button gesture.");
host.draw(canvas, 0.5);
```

### host.audio.play

<!-- api-example: host.audio.play -->

```javascript
const host = await createVis({module});
host.load(source);
if (!host.audio) throw new Error("This example requires the audio module.");
host.audio.load("tone.wav", toneBytes);
host.audio.start();
const voice = host.audio.play("tone.wav", {gain: 0.15});
write({voice});
host.draw(canvas, 0.5);
```

### host.audio.stop

<!-- api-example: host.audio.stop -->

```javascript
const host = await createVis({module});
host.load(source);
if (!host.audio) throw new Error("This example requires the audio module.");
host.audio.load("tone.wav", toneBytes);
const voice = host.audio.play("tone.wav", {gain: 0.15, loop: true});
host.audio.stop(voice);
write({active: host.audio.active(voice)});
host.draw(canvas, 0.5);
```

### host.audio.active

<!-- api-example: host.audio.active -->

```javascript
const host = await createVis({module});
host.load(source);
if (!host.audio) throw new Error("This example requires the audio module.");
host.audio.load("tone.wav", toneBytes);
const voice = host.audio.play("tone.wav", {gain: 0.15, loop: true});
write({active: host.audio.active(voice)});
host.draw(canvas, 0.5);
```

### host.audio.masterGain

<!-- api-example: host.audio.masterGain -->

```javascript
const host = await createVis({module});
host.load(source);
if (!host.audio) throw new Error("This example requires the audio module.");
host.audio.load("tone.wav", toneBytes);
host.audio.masterGain(0.2);
host.audio.start();
host.audio.play("tone.wav", {gain: 0.15});
write("Master gain is 0.2.");
host.draw(canvas, 0.5);
```

### host.audio.busGain

<!-- api-example: host.audio.busGain -->

```javascript
const host = await createVis({module});
host.load(source);
if (!host.audio) throw new Error("This example requires the audio module.");
host.audio.load("tone.wav", toneBytes);
host.audio.busGain("effect", 0.2);
host.audio.start();
host.audio.play("tone.wav", {bus: "effect", gain: 0.15});
write("Effect bus gain is 0.2.");
host.draw(canvas, 0.5);
```

### host.audio.transport

<!-- api-example: host.audio.transport -->

```javascript
const host = await createVis({module});
host.load(`local scene = vis.scene {width = 640, height = 360}
local dot = scene:circle {id = "tone", radius = 0.8, fill = "accent"}
vis.audio.cue(scene, {asset = "tone.wav", begin = 0.5, gain = 0.15})
scene:wait(1)
return scene`);
if (!host.audio) throw new Error("This example requires the audio module.");
host.audio.load("tone.wav", toneBytes);
host.audio.start();
host.audio.transport({time: 0.5, playing: true, rate: 1, seek: true});
write("The authored tone cue starts at 0.5 seconds.");
host.draw(canvas, 0.5);
```
