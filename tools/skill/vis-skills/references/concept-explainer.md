# Concept explainer

Use this for any animation that explains how something works. If one labeled still answers the question, make a still. If the reader must compare several regions or stages at once, make a plate ([cheatsheet](profiles/cheatsheet.md) or [pipeline plate](profiles/pipeline-plate.md)); its beats run inside sections and its camera stays still.

Aim for educational cinema, not slides: the viewer understands by watching things move, and the motion itself is the explanation. Text is never the hero of the screen; shapes, images, plots, and objects are.

## Plan narration first

Write the narration you would speak, one sentence per beat (3–7 beats), then decide what the viewer sees during each sentence. The narration stays in the plan, not on screen.

| narration (one sentence) | what moves or transforms | what stays as dimmed context | camera | hold |
|---|---|---|---|---|

The first frame shows the subject itself, not a title. The last beat holds on the result in the picture, with a short answer caption beside it.

## Make the motion explain

- **Geometry before formulas.** Show the concrete case moving first; name it with a formula or label only after the viewer has seen it.
- **Transform, don't erase.** One object carries the story: it moves, morphs, recolors, or splits into its next state (`morph`, `transform`, tracker bindings). Do not clear the screen and redraw a new version; avoid `fade_out` followed by a new object for the same idea.
- **Continuous relationships.** When one quantity drives others (a point on a curve, a slope, a sweep angle, a weight), make it a `scene:tracker` and `bind` every dependent to it, then `track` the value. The viewer sees the relation hold at every instant instead of a jump between two snapshots.
- **Show, don't print.** Encode quantities as length, area, position, angle, thickness, or color intensity. Show only the few numbers the viewer must read, next to what they measure.
- **Guide attention with opacity.** Focus at 1.0, supporting context near 0.4, background structure near 0.15. Move the focus each beat by fading, not by removing.
- **Use the camera.** A plate, or a short clip whose whole subject reads at once, keeps the camera still. Otherwise, in a 2D scene with more than one region, plan a camera column in the beat table and use `scene:focus` (`focus` on a JS builder) at least for these moments: zoom on the part that changes when it is small relative to the frame (`scene:focus(part)`), then pull back to the whole system (`scene:focus(nil)`) to show how it fits. While zoomed, set the labels in the shot as plane text and hide what the frame cuts ([readme-demo motion](profiles/readme-demo.md#motion)). Keep the camera still while the viewer compares positions.
- **One change at a time.** Each beat has one dominant motion; everything else is quiet.
- **One thing, one shape.** Each actor or value appears once and shows its changing state on that one shape. Connect related things with `connector` so lines touch both ends.
- **Familiar conventions.** Standard axis directions, left-to-right time and reading order, the usual textbook picture for the topic.
- **Fill the frame.** Size the subject to most of the frame in every beat; place comparisons side by side on shared axes and scale.

## Text, sparingly

- No title or heading unless the user asks for one; plates follow their format profile.
- No persistent caption bar or stacked notes. Label objects briefly (a word, a symbol, a short value) next to what they name.
- A formula appears when it names something already shown, and its terms share the colors of the shapes they describe.
- Where meaning would otherwise be lost, show one short phrase near the action for that beat, then fade it. The final hold keeps one answer caption beside the result (a value, a comparison such as `65 > 50`, or a few words), so a paused or exported frame explains itself.
- A sequence of step labels in one place replaces itself (`fade_transform` or fade out before the next fades in); never let two step labels sit side by side unless spaced as a list.

## Keep motion truthful

A visual metaphor must keep the relation it claims: incidence, equal area, projection, and slope come from the same computed data, not just look right. Mark approximations and illustrative values.

| Intent | Operation |
|---|---|
| establish existence | `create`, `write`, `fade_in` |
| same identity, compatible geometry | `morph` |
| one object becomes its successor | `morph` |
| new representation, same meaning | `fade_transform` |
| a dependent follows a changing value | `tracker` + `bind` + `track` |
| draw attention without changing state | `indicate`, opacity layering, a temporary `focus` color |
| reveal a sequence | an array with deliberate `lag` |
| overview, zoom, emphasis | `scene:focus` (2D), camera `look` |

## Pace

Starting ranges, then tune at 1×: supporting reveal 0.35–0.7 s; main construction 0.7–1.4 s; transform or tracked change 1–2.5 s; hold after a key result 1.5–2.5 s; final hold 2–3 s. No single `wait` over 3 s; in a plate, a section that holds while another plays is being read, so the limit applies to its final hold. Typical total: 15–40 s, or 5–12 s for a [readme-demo](profiles/readme-demo.md) clip beside prose. A requested duration is not a quota.

## Review the movie

Watch at 1× with sound off: the silent motion alone must carry the core idea. If a frame reads like a slide (mostly text, nothing moving for long), turn its claim into motion.
