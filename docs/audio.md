# Audio

`vis-audio` plays immediate sounds and synchronizes authored cues with Scene time.
The Player owns loaded audio and playback voices. A Scene-owned Soundscape stores
cue names, timing, and gain animation. The same Player can mix cues offline for export.

## Build

```sh
meson setup build-audio -Dmodules=audio
meson compile -C build-audio
meson test -C build-audio --print-errorlogs
```

The module installs `vis_audio.h`, `libvis-audio.a`, and `vis-audio.pc`.
It uses miniaudio as a private dependency and includes its MIT-0 license.
Audio-enabled WASM hosts expose `scene.audio`.

## Cues and transport

Cues use absolute Scene seconds. `end=0` follows the asset's natural end; a looping
cue continues until its configured end or transport stop. Gain clips sample on the
same Scene clock. `scene:wait` establishes the animation duration.

```lua
local scene = vis.scene {width = 960, height = 540}
local music = vis.audio.cue(scene, {
    asset = "music.mp3", bus = "music", begin = 0, ["end"] = 18,
    gain = 0.7, loop = true,
})
music:gain(0.25, 14, 2, "smooth")

vis.audio.cue(scene, {asset = "hit.wav", bus = "effect", begin = 2.4})
scene:wait(18)
return scene
```

Buses are `music`, `effect`, and `ui`. Gain is linear amplitude: zero is silent and
one preserves the source level. Master and bus gains multiply cue or voice gain.

The host loads asset bytes, binds the Soundscape, and supplies playback state:

```cpp
#include "vis_audio.h"

using namespace vis;
auto scene = Scene::gen();
auto soundscape = audio::Soundscape::gen(scene);
audio::CueConfig music;
music.asset = "music.mp3";
music.bus = audio::Bus::Music;
music.begin = 0.0f;
music.end = 18.0f;
music.gain = 0.7f;
music.loop = true;
audio::Cue cue;
soundscape->cue(music, cue);
soundscape->gain(cue, 0.25f, 14.0f, 2.0f, AnimCurve::preset(AnimCurvePreset::Smooth));

auto player = audio::Player::gen();
player->load("music.mp3", musicBytes, musicSize);
player->bind(soundscape);
player->start();

audio::Transport transport;
transport.time = animationTime;
transport.rate = playbackRate;
transport.playing = playing;
transport.seek = scrubbedOrJumped;
player->sync(transport);
```

Synchronize on play, pause, seek, rate change, and loop wrap. Set `seek=true` for a
jump. Resuming within a cue starts at its corresponding asset offset. Playback rate
also changes pitch.

## Immediate sounds

Immediate voices suit control feedback and simulation events. Each play creates an
independent voice; seeking Scene time leaves these voices at their playback positions.

```cpp
audio::Playback click;
click.asset = "click.wav";
click.bus = audio::Bus::Ui;
click.gain = 0.8f;
audio::Voice voice;
player->play(click, &voice);
player->gain(0.75f);
player->gain(audio::Bus::Music, 0.6f);
player->stop(voice);
```

Retained Lua callbacks emit descriptions for the host to play:

```lua
if fire.released then
    ctx:sound("laser.wav", {bus = "effect", gain = 0.7, rate = 0.94})
end
```

```js
const step = scene.advanceRuntime(elapsed);
for (const sound of step?.sounds ?? []) {
    scene.audio?.play(sound.asset, sound);
}
```

The Runtime owns event production; the Audio host owns assets and playback. The
preview connects these paths and synchronizes authored cues with its transport.

## JavaScript and WASM

The builder declares cues with `sound`:

```js
const definition = vis.scene({width: 960, height: 540});
const music = definition.sound({
    asset: "music.mp3", bus: "music", begin: 0, end: 18,
    gain: 0.7, loop: true,
});
music.gain(0.25, 14, 2, "smooth");
definition.wait(18);
```

Register assets under the names used by cues. Start audio from a user activation
handler, then provide the animation transport:

```js
const music = new Uint8Array(await (await fetch("music.mp3")).arrayBuffer());
scene.audio.load("music.mp3", music);
startButton.addEventListener("click", () => {
    scene.audio.start();
    scene.audio.transport({time: timeline.current, rate: timeline.rate,
                           playing: true, seek: true});
});
```

| Runtime method | Purpose |
|---|---|
| `load(name, bytes)` | Register encoded audio |
| `start()` | Activate the playback backend |
| `play(name, options)` | Start a voice with bus, gain, rate, and loop options |
| `stop(voice)`, `active(voice)` | Control or query a voice |
| `masterGain(gain)`, `busGain(bus, gain)` | Set mixer gain |
| `transport({time, rate, playing, seek})` | Synchronize authored cues |

Use `scene.audio` to detect the capability before invoking these methods.

## Offline mixing and export

`Player::mix` writes interleaved stereo float samples. Sample frame `n` corresponds
to Scene time `n / rate`; cue timing and gain follow the authored Soundscape.
`PlayerConfig::device=false` creates a mixer for offline use.

```cpp
constexpr uint32_t rate = 48000u;
constexpr uint32_t block = 4800u;
static float samples[block * 2u];
for (uint64_t first = 0u; first < total; first += block) {
    auto frames = static_cast<uint32_t>(total - first < block ? total - first : block);
    auto result = player->mix(rate, first, samples, frames);
    // Write the interleaved left/right samples to the export stream.
}
```

An Audio-enabled CLI mixes cues at 48 kHz, writes a temporary WAV, and passes it to
the MP4 saver. Relative cue assets resolve from the Lua script's directory.

```sh
build-audio/viscript render scene.lua -o scene.mp4
build-audio/viscript render scene.lua -o scene.mp4 --audio narration.mp3
```

Narration joins the cue mix from Scene time zero. For a scene without cues, FFmpeg
reads the narration file directly. See [Modules](modules.md) for exporter selection.
