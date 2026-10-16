# viscript WASM runtime

Portable viscript 0.1.0 runtime with the JavaScript builder, Lua authoring, and CPU RGBA rendering. Keep the directory together when copying it into an application.

```js
import {createVis, vis} from "./client.js";
const scene = vis.scene({width: 640, height: 360});
scene.circle({radius: 1, fill: "accent"});
const runtime = await createVis(scene, "scene.js", {renderEngine: "cpu"});
runtime.draw(document.querySelector("canvas"), runtime.duration);
```

Serve the files over HTTP so `vis-wasm.wasm` loads beside the module. In Node, pass its bytes as `wasmBinary`. Register the selected fonts before rendering or measuring text; included families are Pretendard, Source Serif 4, IBM Plex Sans KR, and JetBrainsMono.

The bundle enables TeX, Diagram, UI, Input, retained Lua Runtime, States, and Audio. Use `layoutReport(time, padding)` to inspect sampled bounds, clipping, paths, and collisions. API contracts, ownership, and lifetime rules are documented beside declarations in `client.d.ts` and `bindings.d.ts`.

Destroy the runtime when the host closes. Preserve runtime licenses and font OFL notices when redistributing. `BUILD-MANIFEST.json` records native build provenance, features, file sizes, and SHA-256 checksums.
