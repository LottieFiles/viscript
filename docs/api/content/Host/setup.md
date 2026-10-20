---
title: Shared Host Setup
id: host-setup
order: 0
parent: host
---

# Shared Host Setup

Host examples run in a web application, outside a scene. Place the following setup before a snippet, with the runtime and assets at the displayed paths. The Quartz reference supplies these variables automatically when you run a host example, and gives each engine the site's colours with `hostTheme` before its first scene, so a scene that names no theme follows the site's light and dark modes. On your own page such a scene draws in Pro White until you call `host.hostTheme`.

```html
<canvas width="640" height="360"></canvas>
<pre id="output"></pre>
<script type="module" src="./example.js"></script>
```

## example.js

```javascript
import {createVis, createVisModule, vis, SceneBuilder, compileScene,
    evaluateScene, inputKeyName} from "./runtime/client.js";
const canvas = document.querySelector("canvas");
const module = await createVisModule();
// load accepts a SceneBuilder or Lua source. Evaluate JavaScript source with evaluateScene().
const source = vis.scene({width: 640, height: 360, camera: {mode: "interactive"}});
const dot = source.circle({id: "dot", center: [-1, 0], radius: 0.6, fill: "accent"});
source.rectangle({id: "box", center: [1.3, 0], size: [2, 2], stroke: "foreground", fill: "#00000000"});
source.shift(dot, [1.5, 0], 1);
const fontBytes = new Uint8Array(await (await fetch("./runtime/PretendardStd-Regular.ttf")).arrayBuffer());
const imageBytes = new Uint8Array(await (await fetch("./assets/earth.png")).arrayBuffer());
const toneBytes = makeToneBytes(); // a generated 150 ms WAV
const write = (value) => {
    document.querySelector("#output").textContent = typeof value === "string"
        ? value : JSON.stringify(value, null, 2);
};

function makeToneBytes() {
  const rate = 8000, samples = 1200, bytes = new Uint8Array(44 + samples * 2), view = new DataView(bytes.buffer);
  const word = (offset, value) => [...value].forEach((letter, i) => bytes[offset + i] = letter.charCodeAt(0));
  word(0, "RIFF"); view.setUint32(4, bytes.length - 8, true); word(8, "WAVEfmt ");
  view.setUint32(16, 16, true); view.setUint16(20, 1, true); view.setUint16(22, 1, true);
  view.setUint32(24, rate, true); view.setUint32(28, rate * 2, true); view.setUint16(32, 2, true); view.setUint16(34, 16, true);
  word(36, "data"); view.setUint32(40, samples * 2, true);
  for (let i = 0; i < samples; i++) view.setInt16(44 + i * 2, Math.sin(i / rate * 440 * Math.PI * 2) * 5000 * (1 - i / samples), true);
  return bytes;
}
```

## Retained input scene

Input and runtime examples also use this Lua source as `retainedSource`. Define it before the selected host snippet.

```javascript
const retainedSource = `local scene = vis.scene {width = 640, height = 360}
local dot = scene:circle {id = "dot", radius = 0.5, fill = "accent"}
vis.runtime(scene, {fixed_step = 1 / 60, update = function(runtime, dt, time)
    local pointer = runtime:pointer(0)
    local x = pointer and pointer.active and (pointer.position.x - 320) / 60 or math.sin(time)
    runtime:update(dot, {shift = {x, 0}})
end})
return scene`;
```


The snippets draw to `canvas` and report results through `write(value)`. Audio examples must run after a user gesture. Release each engine with `host.destroy()` when its work is complete.
