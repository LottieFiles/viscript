---
name: vis-skills
description: "Write viscript scenes in Lua or JS/TS: animations, diagrams, algorithm traces, code pipeline plates, 2D/3D graphics, AI-system explainers, cheatsheets, math shorts. Not for interactive input (vis-interactive), the VS Code extension (vis-vscode), or engine work (vis-animation-dev)."
---

# viscript authoring

Make clear, deterministic visual explanations with the public Lua or JavaScript/TypeScript API in the user's host. Do not depend on source-checkout paths, build products, or C++ APIs.

## Read only what changes the scene

1. Always: [scene-contract.md](references/scene-contract.md).
2. One API: [lua.md](references/lua.md) for `.lua`, or [javascript.md](references/javascript.md) for JS/TS and browser/WASM hosts.
3. Any animation that explains something: [concept-explainer.md](references/concept-explainer.md). Add [equation-derivation.md](references/equation-derivation.md) for symbolic steps and [series-production.md](references/series-production.md) for several Scenes.
4. At most one subject profile: [diagram](references/profiles/diagram.md), [algorithm-and-code](references/profiles/algorithm-and-code.md), [graphics](references/profiles/graphics.md), [ai-systems](references/profiles/ai-systems.md), [math-shortform](references/profiles/math-shortform.md). Add a format profile when the artifact is a plate read all at once: [cheatsheet](references/profiles/cheatsheet.md) for a reference sheet, [pipeline-plate](references/profiles/pipeline-plate.md) for a code path's stages beside per-stage evidence. Use the [readme-demo](references/profiles/readme-demo.md) format for a short looping clip that shows one feature above its README paragraph.
5. Bundled browser distribution only: [wasm-bundle.md](references/wasm-bundle.md).

Templates under `templates/` are working API starting points for each profile. `math-shortform/tracked-tangent.lua` shows the motion style to aim for, and `pipeline-plate/pipeline-plate.lua` shows the figure style. Motion explainers drop template titles unless the user asks for them; plates keep their title and answer line.

## Route elsewhere

- `vis-interactive`: UI/Input, retained runtime, audio, interactive state.
- `vis-vscode`: Preview, manifests, CodeLens/hover, the VS Code host's `adaptive` theme.
- `vis-animation-dev`: engine, bindings, builds, VS Code extension source.

## Author

Decide still, motion, or plate, and one Scene or a series, then write the narration-first beat table (or the plate's ledger) before code. Shapes and motion carry the explanation; text is never the hero, but it is not absent either. Labels, real values, and short captions beside the evidence tell the reader what they are looking at ([visible text](references/scene-contract.md#visible-text)). Draw in the [figure style](references/scene-contract.md#figure-style): ink structure, square corners, and one hue for the finding. Use `tracker` for continuous relationships, and `focus` to zoom on the beat where a small part changes, except in plates, whose camera stays still. Set formulas with `tex` when the host has the tex module, and with `spans` otherwise. Build the settled layout, render it once, then add the timeline.

## Review

Use [review.md](references/review.md). Each round: `inspect` once, `audit` once over every frame (text only, no images), `layout` at each meaningful animation's start/end, `t=0`, and the exact final time, and a small set of selected images covering the subject and reported defects. Resolve `VIS_FONT` to the font the Theme's text roles name: Pretendard for the presets (`assets/wasm/Pretendard.ttf`), or the user's font. For code or the code text role, also resolve `VIS_CODE_FONT` to the bundled monospaced JetBrains Mono NL font (`assets/wasm/JetBrainsMono.ttf`). The VS Code Viewer sets the `adaptive` theme's headings and body text in Source Serif 4, so a Scene for it also gets the vis-vscode host review.

```sh
viscript inspect scene.lua --font "$VIS_FONT" --font "$VIS_CODE_FONT" # duration, objects
viscript audit scene.lua --font "$VIS_FONT" --font "$VIS_CODE_FONT" --canvas-inset 4 --text-gap 4 --format text
viscript layout scene.lua --time T --padding 4 --font "$VIS_FONT" --font "$VIS_CODE_FONT"
viscript render scene.lua --time T --font "$VIS_FONT" --font "$VIS_CODE_FONT" -o temp/review.png
```

Keep compact findings (time, IDs, problem) rather than full reports or per-frame PNGs. Check production-font glyphs, label containment, clipping, and unintended overlap; after a fix, look again only at frames that changed. Deliver source, requested export, reviewed times, and checks run.
