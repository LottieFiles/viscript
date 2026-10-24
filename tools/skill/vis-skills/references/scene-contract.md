# Scene contract

Rules for every viscript scene. Profiles may narrow a default; they never weaken these.

## Source truth

- Write the data or deterministic trace first (values, steps, events). Derive every displayed coordinate, label, counter, and result from it; format only at display time.
- Two views of one state read the same record and change in the same beat.
- Keep supplied values and special cases. Make illustrative values non-degenerate and inside the claimed regime, and say that they are illustrative in the delivery or the prose, not on the canvas.
- Before final choreography, change one input to an asymmetric value: every dependent must update without editing another literal.

## Defaults

Leave out every option that equals its default; write a default only beside variants that set the same option otherwise, such as one centered label among left-aligned ones.

- A Scene is 960×540 at 30 fps, has `loop = false`, takes the `adaptive` theme, and looks at the origin through a fixed 2D camera 6 units tall: `camera = {height = 8}` names only what differs.
- A `3d` camera is perspective with +y up, and a 2D one orthographic. `look` changes only the fields it names (`scene:look({eye = {-6, 4.5, 8}}, {duration = 2})`), so a 2D camera turned `3d` stays orthographic unless it names `projection = "perspective"`.
- A shape has no fill until given one, so an outline needs only `stroke`. A filled shape without `stroke` still gets an outline in a Theme color, so a fill-only shape sets `width = 0`. A circle sits on the origin with radius 1, and a rectangle on the origin.
- Text sits on the origin, centered (`align = {0.5, 0.5}`), in the `text` role.

## Coordinates and identity

- Scene `width`/`height` are output pixels. Geometry is world units, right-handed, Y-up, relative to the direct parent; two components mean `z=0`.
- A 2D camera shows vertical span `camera.height` (default 6) and horizontal span `camera.height * width / height`, centered on `camera.target`. From a top-left pixel plan: `x = target.x + (px - width/2) * camera.height/height`, `y = target.y + (height/2 - py) * camera.height/height`. Never pass pixels as world positions.
- Text `size` uses points: the em spans `4/3 * size` screen pixels for billboard text, or world units for plane text; ink bounds depend on the font. Stroke `width`, Point radius, and arrow caps are screen pixels.
- A moving token is one Group at its slot with mark and label authored around `{0,0}`; animate the Group, never body and label separately. A Group is paintless; use explicit layer bands for its painted children.
- Give a unique stable `id` to every Text and every object that is queried, connected, matched across states, or reviewed. Reuse the handle; never recreate an ID.
- Use Space only when axes or units teach something; use a child Scene plus Viewport only for an independent camera, clock, or coordinate system.

## Theme, text, color

- Leave `theme` out unless the user picks a style: `adaptive` takes the host app's colors and font (Pro White elsewhere), and Theme roles read on light and dark hosts alike. A chosen theme goes in the config of the root and of every child Scene before visible objects; a Scene whose literal colors suit one background names `pro_white` or `pro_black`.
- Register the selected fonts through the host. Verify meaning-bearing glyphs (`∇`, `±`, `θ`, Korean) in the raster; otherwise spell them plainly.
- Text roles: `h1`, `h2`, `h3`, `text`, `code`. `point` is the anchor and alignment defaults to centered; set `align={0,0.5}` for left-anchored titles and margin labels.
- Theme colors: `background`, `foreground`, `muted`, `accent`, `secondary`, `success`, `warning`, `danger`, `info`, `surface`, `border`, `result`, `focus` (no `object1`). Omitting paint cycles peer colors.
- One identity keeps one color across its mark, label, formula term, and proxies; change them together in one `tween`. `result` is persistent, `focus` transient. Never rely on color alone.
- Text over a filled shape needs strong contrast: when a box gets a strong highlight fill, recolor its text (e.g. `background`) in the same `tween`; otherwise highlight with stroke only.

## Figure style

The default look is a printed technical figure: ink structure, square corners, and color only where the reader must look. A user's style request or a profile's stated exception overrides it.

- **Square corners.** Leave `corner` at 0 for boxes, cells, slots, panels, and nodes (`vis.diagram` follows the Theme's `roundness`, 0 in the pro presets); round only a shape whose roundness means something (a token, a physical object) or when the user asks.
- **Ink structure.** Frames, axes, slots, nodes, and routes use `foreground`, `muted`, or `border` with no fill, or a `background` fill that masks routes behind a node. No card fills, `surface` panels, shadows, gradients, or glows unless they mark a real region. Give every structural object explicit paint: omitted paint cycles the Theme's object colors.
- **Color budget.** One hue carries the subject or the finding (`danger` for a failure, `result` for an answer, `accent` for the active path); `focus` may appear for one beat. Add a second hue only for a second named state the reader must compare. A hue means one state: what is active now and what is finished never share it. Color by category only when color is the data or identities must be told apart, and then keep one small palette.
- **Weights.** 1 px for grids, rules, slots, and idle routes; 1.25–1.5 px for outlines and the subject; 2 px for a moving head, cursor, active selection, or result curve. Arrow `tip` near 7. A dash marks something implied, missing, constructed, or repeating, never ordinary structure.
- **Type.** Hierarchy comes from role, size, and `muted`, not hue. Keep labels at 11–16 pt; a title uses `h3`, or `h2` on a poster-sized plate. Values, identifiers, and indices use the `code` role in ink (`fill = "foreground"` or `muted`): the role's own color is a hue, not ink.
- **Code fragments.** Use `code {language = "plain"}` in ink and recolor only the decisive tokens (`search`) in the finding's hue; syntax colors compete with the evidence.
- **Context.** Dim context by swapping it to `muted`/`border` or to opacity near 0.4; keep it when it explains an invariant, and fade it out where new text would land on it.

## Visible text

- Every string teaches: a name, a real value, a code fragment, a formula, or a short caption that states what the reader is looking at ("never written again", "2 of 5 drawn"). No production metadata (loop, theme, size, tool, review status, "illustrative") and no paragraphs. A counter or readout appears at the beat that first uses it.
- Put words beside their evidence: a label next to its mark, a caption beside the figure it explains, the finding in its hue and the reason in `muted`.
- At most one caption is on screen, and it belongs to its beat: it changes or leaves with the beat, never stays as a rule bar or legend. Caption each kind of step the first time it happens, each change of phase (a new pass, the stopping test), and the result; plain repeats run without one. A label that gives a step's reason (a comparison, a condition) leaves as the step's action completes, so no frame shows a claim the picture no longer matches; step labels in one place replace each other (`fade_transform`, or fade out before the next fades in).
- A formula appears once the picture has shown what it names, its terms in the colors of the shapes they describe.
- A motion explainer has no title unless requested; it labels values and ends on a short answer caption beside the result, so a paused or exported final frame explains itself.
- A plate ([cheatsheet](profiles/cheatsheet.md), [pipeline plate](profiles/pipeline-plate.md)) is read as a figure: a title that states the question, one answer line, and a caption per section.
- Beside prose (a note, a README, a docs page), the prose carries the sentences. The Scene keeps names, values, and code, drops the answer caption when the paragraph states the answer, and repeats no sentence of the prose, usage hints included.

## Layout and draw order

- Build the settled layout before timeline calls. Measure text with `text:measure()` in the font its role names, reserve regions for the subject, its labels and captions, and a title when the format has one, and size boxes from contents plus insets. Recompose or split before shrinking text.
- Size the canvas for where it is shown. A host scales the Scene to its column (about 700 px in an Obsidian note), and the labels shrink with it: author near the display width, and split a figure whose labels would show below 11 pt there.
- Use explicit `layer` bands: background < regions < routes/guides < subjects < labels < transient focus. Creation order is not draw order.
- `scene:bounds` is approximate; final containment and gaps come from `viscript audit` and the raster.

## Motion

- Leave one coherent opening group unanimated so `t=0` already shows the subject or question; never show labels before their owners. Reveal later facts in their own beat.
- One beat = one claim and one dominant change of the semantic thing (value, owner, route, trace step, result); decorative dots, sweeps, and pulses are not evidence.
- Continue identities: transform the existing object into its next state rather than removing it and drawing a replacement. Use `morph` for retained outlines and `match_transform` for text or formulas; `scene:clone(source, options)` (JS `definition.clone(source, options)`) snapshots a family at the cursor.
- Keep finished structure as quieter context; hold conclusions. An effect starts on the mark where its cause ends, and a beat's highlight, caption, and readouts change on the same mark.
- Curves: `ease_out` for entrances, `ease_in` for exits, the default `smooth` for moves and reversible change, `linear` for a uniform rate; never `bounce` or `elastic`.
- `loop=false` unless recurrence is requested or a format profile loops (a plate, a readme-demo clip); a loop returns every object, label, and color to its `t=0` state.
- Timelines are deterministic at any sample time: dependents of a changing value use `tracker`/`bind`/`track` (baked at construction on a JS builder), and Cell/Voxel `time` callbacks run at construction.

## Verify

Review in [rounds](../SKILL.md#review-in-rounds), keeping their files outside the project or vault ([review.md](review.md)). Deliver the source, the requested export, the times on the contact sheet, and the checks run.
