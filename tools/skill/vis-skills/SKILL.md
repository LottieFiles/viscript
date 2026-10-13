---
name: vis-skills
description: "Write viscript scenes in Lua or JS/TS: animations, diagrams, algorithm traces, 2D/3D graphics, AI-system explainers, cheatsheets, math shorts. Not for games/input (vis-game), the VS Code extension (vis-vscode), or engine work (vis-animation-dev)."
---

# viscript authoring

Make clear, deterministic visual explanations with the public Lua or JavaScript/TypeScript API in the user's host. Do not depend on source-checkout paths, build products, or C++ APIs.

## Read only what changes the scene

1. Always: [scene-contract.md](references/scene-contract.md).
2. One API: [lua.md](references/lua.md) for `.lua`, or [javascript.md](references/javascript.md) for JS/TS and browser/WASM hosts.
3. Any animation that explains something: [concept-explainer.md](references/concept-explainer.md). Add [equation-derivation.md](references/equation-derivation.md) for symbolic steps and [series-production.md](references/series-production.md) for several Scenes.
4. At most one matching profile: [diagram](references/profiles/diagram.md), [algorithm-and-code](references/profiles/algorithm-and-code.md), [graphics](references/profiles/graphics.md), [ai-systems](references/profiles/ai-systems.md), [cheatsheet](references/profiles/cheatsheet.md), [math-shortform](references/profiles/math-shortform.md).
5. Bundled browser distribution only: [wasm-bundle.md](references/wasm-bundle.md).

Templates under `templates/` are working API starting points for each profile; `math-shortform/tracked-tangent.lua` shows the motion style to aim for. Drop template titles and subtitles unless the user asks for them.

## Route elsewhere

- `vis-game`: UI/Input, retained runtime, audio, game state.
- `vis-vscode`: Preview, manifests, CodeLens/hover, the `adaptive_vscode` host theme.
- `vis-animation-dev`: engine, bindings, builds, VS Code extension source.

## Author

Decide still vs. motion and one Scene vs. a series, then write the narration-first beat table from the concept explainer before code. Text is never the hero: no title unless requested, and let motion carry the explanation. Use `tracker` for continuous relationships and `focus` to zoom on the beat where a small part changes. Set formulas with `tex` when the host has the tex module, and with `spans` otherwise. Build the settled layout, render it once, then add the timeline.

## Review

Use [review.md](references/review.md). Each round: `inspect` once, `audit` once over every frame (text only, no images), `layout` at each meaningful animation's start/end, `t=0`, and the exact final time, and a small set of selected images covering the subject and reported defects. Resolve `VIS_FONT` to the delivery font (CLI-only fallback: `assets/wasm/Pretendard.ttf`). For code or the code text role, also resolve `VIS_CODE_FONT` to the bundled monospaced JetBrains Mono NL font (CLI-only fallback: `assets/wasm/JetBrainsMono.ttf`).

```sh
viscript inspect scene.lua --font "$VIS_FONT" --font "$VIS_CODE_FONT" # duration, objects
viscript audit scene.lua --font "$VIS_FONT" --font "$VIS_CODE_FONT" --canvas-inset 4 --text-gap 4
viscript layout scene.lua --time T --padding 4 --font "$VIS_FONT" --font "$VIS_CODE_FONT"
viscript render scene.lua --time T --font "$VIS_FONT" --font "$VIS_CODE_FONT" -o temp/review.png
```

Keep compact findings (time, IDs, problem) rather than full reports or per-frame PNGs. Check production-font glyphs, label containment, clipping, and unintended overlap; after a fix, look again only at frames that changed. Deliver source, requested export, reviewed times, and checks run.
