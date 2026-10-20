# Concept explainer

Use this for any animation that explains how something works. If one labeled still answers the question, make a still. If the reader must compare several regions or stages at once, make a plate ([cheatsheet](profiles/cheatsheet.md) or [pipeline plate](profiles/pipeline-plate.md)); its beats run inside sections and its camera stays still.

Aim for educational cinema, not slides: the viewer understands by watching things move. Shapes, plots, and objects carry the explanation; text only names them ([visible text](scene-contract.md#visible-text)).

## Plan narration first

Write the one question the Scene answers, then the narration you would speak, one sentence per beat (3–7 beats), then what the viewer sees during each sentence. A beat argues with a device: a worked step, a before and after, or the naive approach failing visibly before the fix. The narration stays in the plan, not on screen.

| narration (one sentence) | what moves or transforms | what stays as dimmed context | camera | hold |
|---|---|---|---|---|

The first frame shows the subject itself, not a title. The last beat holds on the result in the picture, with a short answer caption beside it.

## Make the motion explain

- **Concrete before general.** Run one case with real numbers first: work a single step out beside its evidence (the value in, the change, the value out), then let the remaining steps run faster with the same cues and counters, captioned only where a phase changes. Name the rule or formula after the viewer has seen it happen.
- **Show the cause and the stop.** Draw what drives each step (a tangent for a slope, the compared pair for a swap), and give the condition that ends the process (a pass with no swaps, a step too small to matter) its own visible beat.
- **Transform, don't erase.** One object carries the story: it moves, morphs, recolors, or splits into its next state (`morph`, `transform`, tracker bindings). Do not clear the screen and redraw a new version; avoid `fade_out` followed by a new object for the same idea.
- **Continuous relationships.** When one quantity drives others (a point on a curve, a slope, a sweep angle, a weight), make it a `scene:tracker` and `bind` every dependent to it, then `track` the value. The viewer sees the relation hold at every instant instead of a jump between two snapshots.
- **Show, don't print.** Encode quantities as length, area, position, angle, thickness, or color intensity. Show only the few numbers the viewer must read, next to what they measure.
- **Compare side by side.** When the request contrasts cases (a good and a bad setting, before and after, two algorithms), show them at once on shared axes and scale from the same start, and play them in turn or in step. The viewer compares positions, not memories.
- **Attach every cue.** An arrow that shows a move runs from the old position to the new one; a highlight sits on its object; a label travels with its owner or stays with its slot.
- **Guide attention with opacity.** Focus at 1.0, supporting context near 0.4, background structure near 0.15. Move the focus each beat by fading, not by removing.
- **Use the camera.** A plate, or a short clip whose whole subject reads at once, keeps the camera still. Otherwise, in a 2D scene with more than one region, plan a camera column in the beat table and use `scene:focus` (`focus` on a JS builder) at least for these moments: zoom on the part that changes when it is small relative to the frame (`scene:focus(part)`), then pull back to the whole system (`scene:focus(nil)`) to show how it fits. While zoomed, set the labels in the shot as plane text and hide what the frame cuts ([readme-demo motion](profiles/readme-demo.md#motion)). Keep the camera still while the viewer compares positions, and never zoom and orbit in one `look`.
- **One change at a time.** Each beat has one dominant motion; everything else is quiet.
- **One thing, one shape.** Each actor or value appears once and shows its changing state on that one shape. Connect related things with `connector` so lines touch both ends.
- **Pieces keep their identity.** When several pieces move to new places (a dissection, a shuffle, a permutation), number or tint each one, and move them one at a time or along paths that never pile them on top of each other.
- **Familiar conventions.** Standard axis directions, left-to-right time and reading order, the usual textbook picture for the topic.
- **Fill the frame.** At its decisive beat the subject, captions aside, spans at least 60% of the frame's width or height; set captions close beside it, not across a band of empty canvas.

## Keep motion truthful

A visual metaphor must keep the relation it claims: incidence, equal area, projection, and slope come from the same computed data, not just look right. Mark approximations. A discrete step is an event: the marker jumps to its landing in 0.3–0.5 s (straight or arced, never a slide along the curve through values the process never takes) and rests there, its readouts change as it lands, and each landing leaves a mark (a dot joined to the last) bold enough to follow the path.

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

Starting values, then tune at 1×. A requested duration is not a quota.

| role | time |
|---|---|
| supporting reveal, label, cue | 0.35–0.7 s |
| main construction | 0.7–1.4 s |
| transform or tracked change | 1–2.5 s |
| exit | about 3/4 of its entry |
| stagger across a group | 0.5 s in all; order that matters gets its own beats |
| still between a decisive action and its result | 0.3–0.75 s |
| caption (its motion runs meanwhile) or new formula | words ÷ 2.5 s, at least 1.5 s (2 s for a formula) |
| still after a camera move | 0.5–1 s |
| hold after a key result | 1.5–2.5 s |
| final hold | 2–3 s |

No single `wait` over 3 s; in a plate, a section that holds while another plays is being read, so the limit applies to its final hold. Show each distinct step once at this pace and run its repeats about twice as fast. Typical total: 10–30 s, or 5–12 s for a [readme-demo](profiles/readme-demo.md) clip beside prose.

Read in order, the review's contact sheet must carry the core idea without sound or prose. A frame that reads like a slide (mostly text, nothing changed since the last one) needs its claim turned into motion.
