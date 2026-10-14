# Review a scene

Check layout across the timeline with CLI reports, then inspect selected frames for readability and meaning.

## One round

1. `inspect` once for the duration, objects, and named markers.
2. `audit` once over every encoded frame and the exact final time. It writes no
   PNGs; keep its compact summary and the failing times and IDs, not the full JSON.
3. `layout` at the boundary times below, for numeric checks: containment, gaps,
   label ownership, glyph failures.
4. Render and inspect the selected frames below.
5. Fix, then start the next round. Look again only at frames whose content changed.

```sh
viscript inspect scene.lua --font "$VIS_FONT"
viscript audit scene.lua --font "$VIS_FONT" --canvas-inset 4 --text-gap 4 --format text
viscript layout scene.lua --time "$VIS_TIME" --padding 4 --font "$VIS_FONT"
viscript render scene.lua --time "$VIS_TIME" --font "$VIS_FONT" -o temp/review.png
```

Add `--font "$VIS_CODE_FONT"` to each command when the scene sets code. Without
`--format text` the audit prints its full JSON. When text sits in a box, add one
`--contain TEXT_ID RECTANGLE_ID [INSET]` per pair; the container must be a
Rectangle. `--require-containment-ledger` also fails every Text with an ID that
names no box and no `--allow-uncontained TEXT_ID`, which suits a `vis.diagram`
Scene, whose node labels declare their own containment. Add the Diagram or
occlusion policies that apply.

`layout` prints JSON. The entries of `report.collisions` name two indices into
`report.objects`; resolve them to IDs, skip pairs inside one Group whose
`typeset` is true (a code block's tokens, a formula's parts), and keep the IDs
of colliding, `clipped`, or `outside` Text as the result for that time.

Review renders and probes go under `temp/` next to the scene. In a VS Code
workspace keep them outside `.vscode/vis`, which holds only Scenes and their
assets. A Scene for the VS Code Viewer also gets the vis-vscode host review: the
CLI sets `adaptive_vscode` in Pretendard, while the Viewer sets headings and body
text in Source Serif 4 and takes the editor's colors.

A successful command is not proof of correct layout. Check labels against their
owners' own paint bounds, and tell intended contacts from unrelated collisions. A
hidden or not-yet-created object may have null bounds; missing bounds for an object
that should be visible need investigation.

## Boundary times are for numeric checks

Take the start and end of each meaningful authored animation, `t=0`, and the exact
final time from `inspect`, and deduplicate shared times. Record boundaries with
`mark` around an operation rather than guessing durations:

```lua
scene:mark("move/start")
scene:shift(subject, {3, 0}, 1.2)
scene:mark("move/end")
```

For parallel tracks include each branch end; for staggered objects use
`start(i) = start + duration * lag * i` and `end(i) = start(i) + duration`. A
`track`, sampled field, or `advect` call is one operation. Map child Scene and
transition times into root time. Run `layout` at these times and keep a one-line
result per time; do not render an image for each of them.

Add an interior time when the endpoints hide the evidence: an out-and-back effect,
a full turn, a curved path, an overshoot, a temporary highlight, or retyped text.
Use its peak or its closest contact, not a midpoint for every animation.

## Selected frames

Inspect the first settled state, the decisive or densest beat, and the exact final frame. Add reported defect times and use a contact sheet when comparison helps. Render at delivery size and reuse stable filenames in the review directory.

For an independent visual review, provide the source, selected times, and images. Keep findings concise: time, object IDs, problem, and proposed fix.

For JS/WASM, write the Lua from `compileScene` to a file and `audit` it, run
`layoutReport(time, padding)` at the boundary times, and `draw`/`render` only for
the image set. Do not keep copied RGBA buffers or full reports for every frame.

## Report

Report the times checked, the audit result, the images looked at, and what was not
verified. Claim `audit: 0 errors` only when that command ran in this round.
