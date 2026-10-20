# Pipeline plate format

Use this profile when the reader needs a code path's stages and the evidence for each stage in one view: why a value goes wrong between load, update, and draw; how a request crosses layers; what each pass of a renderer contributes. The stage spine runs down the left and one section per stage, or per group of stages, explains it on the right. This is a format profile: combine it with the subject profile that owns the truth, usually [algorithm-and-code](algorithm-and-code.md). One temporal mechanism is an ordinary animation, and several independent claims are a series.

When both a plate and an animation fit, look at what the answer names. If it names a stage (where a value is fixed, lost, or ignored), make the plate, because the reader must see every stage at once. If every step happens in one place and only the order of states explains it, make an animation. A plate needs a wide display: in an Obsidian note, about 700 px wide, give each stage its own short clip ([readme-demo](readme-demo.md)) instead.

Start from [pipeline-plate.lua](../../templates/pipeline-plate/pipeline-plate.lua).

## Fill the ledger before geometry

| stage | spine name | how often it runs | what crosses to the next stage | section |
|---|---|---|---|---|

- **Question.** The title, phrased as the reader would ask it.
- **Answer.** One line naming the mechanism that answers it.
- **Core problem.** The one value, line, or state that answers the question. It takes the finding's hue everywhere it appears: edge label, code token, ruler, missing outline, caption.
- **Spine names.** The call as the reader writes it: the Lua call (`root:at`, `scene:wait`) when the subject is a Lua API, the C++ function when it is engine internals. A stage that is not code, such as the screen or playback, takes a plain noun.
- **Trace.** Deterministic data that derives every row, counter, edge label, and caption. Assert the claim on it (for example, that some items stay undrawn).

Group stages into one section when one mechanism spans them, such as a loop and the pixels it writes. Each section shows its evidence rows, the decisive code fragment (one line with exact identifiers), and a caption only where the picture needs words.

## Layout

- **Pixel plan.** Author in pixels through `P(x, y)` with a camera whose `height` equals the canvas height. Fix the width (1280 suits an editor column) and derive the height: header + section heights + gutters + margin. Measure text before placing it (`local w, h = label:measure()`); see [lua.md](../lua.md#text-and-code) for text that appears later. When content does not fit, grow the canvas or split the plate; never shrink text.
- **Spine.** One node per stage in runtime order, top to bottom: a rectangle with square corners, the name in the `code` role, and a `muted` second line for frequency or role ("once, on load", "every frame"). Straight arrows join the nodes, each labeled with the value that crosses it. When two values cross, stack the second under the first; more belong in the section. A dashed `muted` route marks repetition.
- **Sections.** One band per section, level with its nodes: each node's center lies inside its band, and in a group each node sits level with its own row. Separate bands with hairline rules, never boxes or fills. Row labels go in `muted` at the band's left edge, and captions go beside the evidence they explain.
- **Header.** The title (`h3`) and the answer line (`muted`) at top left, and nothing else.
- **Margins.** Keep equal outer margins (48 px at 1280 wide), measured to the outermost ink, repeat routes and edge labels included. No text runs into the right margin.
- **Theme.** Choose it as the [scene contract](../scene-contract.md#theme-text-color) says, and set it on the root and on every section Scene: a child Scene does not inherit it.

## Section mechanics

- Make each section a child Scene whose camera targets its band's center in root coordinates, so `P()` holds inside it, and mount it with `viewport` at the band's normalized bounds. The aspect then matches exactly.
- Every section is complete at `t = 0`: its summary pose already shows the finding. Its animation leaves that pose and returns to it before the loop ends.
- Animate the stage's verb (copy, grow, scan, compare, skip, write) on things with identity: a token Group that moves, a cursor over a time axis, a scan head over slots, a limit ruler at a bound, dashed outlines for what never arrives, an overwritten value struck out at opacity 0.5.
- The figure style in the [scene contract](../scene-contract.md#figure-style) applies throughout: ink structure, square corners, one hue for the finding, `focus` only for one beat.

## Timing

Both modes default to `loop = true`, overriding the core one-shot default, because a plate stays on screen while the reader inspects it; the seam must be pixel-identical. Play it once only when the user asks.

- **`loops` (default).** Each section loops on its own clock. Starts stagger along the reading order (about 0.8 s, then 0.6 s per section). The shared loop is the longest section plus a hold, rounded up to a whole second, and every section is padded to it. Use this when the sections are mechanisms the reader can inspect in any order.
- **`sequence`.** Sections play one after another from top to bottom, and the spine marks the active stages with a transient `focus` stroke. The others hold their summary pose. Use this when one value is handed down the stages and the order is the explanation. A section that holds while another plays is being read, not paused, so the 3 s limit in [pace](../concept-explainer.md#pace) applies only to the final hold before the seam.
- Pad every section and the root to the shared length with `scene:seek(total)`. `wait(d)` advances from the cursor, and `at` puts the cursor back where it was, so after any `at` call `wait(total - scene:duration())` ends short of `total`.
- The camera never moves; the reader inspects the whole plate at once.
- A flowing dashed route moves by whole dash periods per loop, so the seam is invisible.
- Assert that every section's duration equals the root's before mounting it.

## Text

The title is the question and the answer line is the mechanism. Edge labels carry values, row labels name the data structures, and each section carries one decisive code line and at most one caption (the finding in its hue, the reason in `muted`). File paths, line numbers, and revisions stay off the Canvas; in VS Code they belong to the Inspector and the notes.

## Review

In addition to the core review, check:

- `t = 0`: the complete plate, with every section in its summary pose;
- each section's peak;
- the exact final time and both sides of the seam;
- light and dark renders: for a VS Code Scene, [host-review.mjs](../../../vis-vscode/scripts/host-review.mjs) renders both editor palettes in the Viewer's fonts; elsewhere, render a `pro_black` copy;
- the audit with one `--contain node:<id>:name node:<id>:body 8` per node Text, the second line included. The container must be a Rectangle. A plate names its pairs; `--require-containment-ledger` suits a `vis.diagram` Scene, whose node labels declare their own containment;
- one asymmetric change to the trace, which every row, ruler, label, and caption must follow.
