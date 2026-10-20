---
name: vis-skills
description: "Write viscript scenes in Lua or JS/TS: animations, diagrams, algorithm traces, code pipeline plates, 2D/3D graphics, AI-system explainers, cheatsheets, math shorts. Not for interactive input (vis-interactive), the VS Code extension (vis-vscode), or engine work (vis-animation-dev)."
---

# viscript authoring

Make clear, deterministic visual explanations with the public Lua or JavaScript/TypeScript API in the user's host, never with source-checkout paths, build products, or C++ APIs.

## Read what the scene needs in one step

Read these together, in one shell command or one batch of parallel file reads:

1. Always [scene-contract.md](references/scene-contract.md), plus [lua.md](references/lua.md) for `.lua` ([lua-more.md](references/lua-more.md) for tables, matrices, masks, fields, surfaces, `warp`, inset views, springs, Cell/Voxel, and child Scenes) or [javascript.md](references/javascript.md) for JS/TS and browser/WASM hosts.
2. An animation that explains: [concept-explainer.md](references/concept-explainer.md). Add [equation-derivation.md](references/equation-derivation.md) for symbolic steps and [series-production.md](references/series-production.md) for several Scenes.
3. At most one subject profile: [diagram](references/profiles/diagram.md), [algorithm-and-code](references/profiles/algorithm-and-code.md), [graphics](references/profiles/graphics.md), [ai-systems](references/profiles/ai-systems.md), [math-shortform](references/profiles/math-shortform.md). A plate read all at once adds a format profile: [cheatsheet](references/profiles/cheatsheet.md) for a reference sheet, [pipeline-plate](references/profiles/pipeline-plate.md) for a code path's stages beside per-stage evidence. A short clip above its paragraph in a README, a docs page, or a note takes the [readme-demo](references/profiles/readme-demo.md) format.
4. Only when needed: [review.md](references/review.md) for `inspect` and `layout` probes and for VS Code or JS/WASM hosts, and [wasm-bundle.md](references/wasm-bundle.md) for the bundled browser distribution.

Templates under `templates/` are working API starting points for each profile. `math-shortform/tracked-tangent.lua` shows the motion style to aim for, `readme-demo/command-to-result.lua` a short clip, and `pipeline-plate/pipeline-plate.lua` the figure style at the density only a plate needs. Motion explainers drop template titles unless the user asks for them; plates keep their title and answer line.

## Route elsewhere

- `vis-interactive`: UI/Input, retained runtime, audio, interactive state.
- `vis-vscode`: Preview, manifests, CodeLens/hover, the VS Code host's `adaptive` theme.
- [vis-obsidian](../vis-obsidian/SKILL.md): the bundled Obsidian plugin, scene-file embeds, fonts, and player source toggles.
- `vis-animation-dev`: engine, bindings, builds, VS Code extension source.

## Work in this order

1. **Plan.** Decide still, motion, or plate, and one Scene or a series. Write the beat table from the concept explainer (or the plate's ledger) before code.
2. **Data and layout.** Compute the trace, measure text, and place every object at rest.
3. **Timeline.** Animate the beats: one change each, a `scene:mark` at each beat's peak for the review, `tracker` for continuous relationships, and `focus` to zoom on a small part that changes, except in plates, whose camera stays still. Set formulas with `tex` when the host has the tex module, and with `spans` otherwise.
4. **Review** in rounds (below), the first in the command that writes the Scene.
5. **Deliver** the source, the requested export, the times on the sheet, the audit result, and what was not verified; claim `audit: 0 errors` only when that command ran in the last round.

Keep each Scene simple: one idea, the few objects that show it, and short labels beside their evidence ([visible text](references/scene-contract.md#visible-text)), drawn in the [figure style](references/scene-contract.md#figure-style). Cut every box, arrow, word, and beat the idea does not use. A topic with several parts gets one short Scene per part rather than one figure of the whole system, and a flow whose path depends on a type or mode is one diagram in which choosing a type highlights its path ([vis-interactive](../vis-interactive/SKILL.md)). Beside prose (a note, a README, a docs page) the prose explains and the Scene shows; no sentence appears in both.

## Review in rounds

A round is one shell command and one look: the audit, then a contact sheet of the marked beats. End the command that writes the Scene with the first round. Fix what a round shows and rerun it; after two fix rounds, deliver and name what is still wrong. The scene contract's asymmetric-input check runs inside a round's command, on a copy in `$REVIEW_DIR`. Every flag a round needs is below, so skip `viscript --help`.

```sh
viscript audit scene.lua --font "$VIS_FONT" --font "$VIS_CODE_FONT" --canvas-inset 4 --text-gap 4 --format text;
viscript render scene.lua --font "$VIS_FONT" --font "$VIS_CODE_FONT" --times 0,compare,swap,end -o "$REVIEW_DIR/sheet.png"
```

1. **Times.** `--times` takes up to 12 times: seconds, `scene:mark` names, or `end`, the exact duration. Mark each beat's peak or end as you author it (`scene:mark("swap")`, a name that starts with a letter), and list `0`, those marks (the decisive and the densest frames), and `end`.
2. **Audit** samples every frame for text clearance, Text/Text spacing, declared containment, IDs, visibility, and glyph coverage. Keep the summary line and the failing times and IDs. For a label that must stay in its box, add `--contain TEXT_ID RECTANGLE_ID [INSET]`; the container is a Rectangle. A `vis.diagram` Scene adds `--require-containment-ledger`, since its node labels declare their own boxes.
3. **Contact sheet.** The frames sit three to a row, in order. Look at it once. Name the focal evidence in each frame, as a first-time viewer would within 3 seconds, then check what the audit cannot see: shapes over labels or over each other, a subject that is small, cut off, or lost in empty space, a label hugging the frame edge, cues detached from what they point at, a label whose claim the picture no longer shows, one color with two meanings, and whether the frames alone tell the story in order.

## Fonts and files

`VIS_FONT` is the font the Theme's text roles name: Pretendard for the presets (`assets/wasm/Pretendard.ttf`), or the user's font. `VIS_CODE_FONT` is the bundled monospaced JetBrains Mono NL (`assets/wasm/JetBrainsMono.ttf`); pass both to every CLI command. `REVIEW_DIR` is a scratch directory outside the user's project or vault; renders and probes go there, and only the Scene files are delivered.
