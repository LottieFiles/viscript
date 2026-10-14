# Scene contract

Rules for every viscript scene. Profiles may narrow a default; they never weaken these.

## Source truth

- Write the data or deterministic trace first (values, steps, events). Derive every displayed coordinate, label, counter, and result from it; format only at display time.
- Two views of one state read the same record and change in the same beat.
- Keep supplied values and special cases. Mark illustrative values as such; make them non-degenerate and inside the claimed regime.
- Before final choreography, change one input to an asymmetric value: every dependent must update without editing another literal.

## Coordinates and identity

- Scene `width`/`height` are output pixels. Geometry is world units, right-handed, Y-up, relative to the direct parent; two components mean `z=0`.
- A 2D camera shows vertical span `camera.height` (default 8) and horizontal span `camera.height * width / height`, centered on `camera.target`. From a top-left pixel plan: `x = target.x + (px - width/2) * camera.height/height`, `y = target.y + (height/2 - py) * camera.height/height`. Never pass pixels as world positions.
- Text `size` uses points: the em spans `4/3 * size` screen pixels for billboard text, or world units for plane text; ink bounds depend on the font. Stroke `width`, Point radius, and arrow caps are screen pixels.
- A moving token is one Group at its slot with mark and label authored around `{0,0}`; animate the Group, never body and label separately. A Group is paintless; use explicit layer bands for its painted children.
- Give a unique stable `id` to every Text and every object that is queried, connected, matched across states, or reviewed. Reuse the handle; never recreate an ID.
- Use Space only when axes or units teach something; use a child Scene plus Viewport only for an independent camera, clock, or coordinate system.

## Theme, text, color

- Set `theme` in the config of the root and of every child Scene, before visible objects; a child Scene does not inherit it. Use the user's choice, else `adaptive_vscode` for a VS Code host, else `pro_white`.
- Register the selected fonts through the host. Verify meaning-bearing glyphs (`∇`, `±`, `θ`, Korean) in the raster; otherwise spell them plainly.
- Text roles: `h1`, `h2`, `h3`, `text`, `code`. `point` is the anchor and alignment defaults to centered; set `align={0,0.5}` for left-anchored titles and margin labels.
- Theme colors: `background`, `foreground`, `muted`, `accent`, `secondary`, `success`, `warning`, `danger`, `info`, `surface`, `border`, `result`, `focus` (no `object1`). Omitting paint cycles peer colors.
- One identity keeps one color across its mark, label, formula term, and proxies; change them together in one `play`. `result` is persistent, `focus` transient. Never rely on color alone.
- Text over a filled shape needs strong contrast: when a box gets a strong highlight fill, recolor its text (e.g. `background`) in the same `play`; otherwise highlight with stroke only.

## Figure style

The default look is a printed technical figure: ink structure, square corners, and color only where the reader must look. A user's style request or a profile's stated exception overrides it.

- **Square corners.** Leave `corner` at 0 for boxes, cells, slots, panels, and nodes; `vis.diagram` follows the Theme's `roundness`, which is 0 in the pro presets. Round only a shape whose roundness means something (a token, a physical object) or when the user asks.
- **Ink structure.** Frames, axes, slots, nodes, and routes use `foreground`, `muted`, or `border` with no fill, or a `background` fill that masks routes behind a node. No card fills, `surface` panels, shadows, gradients, or glows unless they mark a real region. Give every structural object explicit paint: omitted paint cycles the Theme's object colors.
- **Color budget.** One hue carries the subject or the finding (`danger` for a failure, `result` for an answer, `accent` for the active path); `focus` may appear for one beat. Add a second hue only for a second named state the reader must compare. Color by category only when color is the data or identities must be told apart, and then keep one small palette.
- **Weights.** 1 px for grids, rules, slots, and idle routes; 1.25–1.5 px for outlines and the subject; 2 px for a moving head, cursor, or result curve. Arrow `tip` near 7. A dash marks something implied, missing, constructed, or repeating, never ordinary structure.
- **Type.** Hierarchy comes from role, size, and `muted`, not hue. Keep labels at 11–16 pt; a title uses `h3`, or `h2` on a poster-sized plate. Values, identifiers, and indices use the `code` role.
- **Code fragments.** Use `code {language = "plain"}` in ink and recolor only the decisive tokens (`search`) in the finding's hue; syntax colors compete with the evidence.
- **Context.** Dim context by swapping it to `muted`/`border` or to opacity near 0.4; keep it when it explains an invariant.

## Visible text

- Every string teaches: a name, a real value, a code fragment, a formula, or a short caption that states what the reader is looking at ("never written again", "2 of 5 drawn"). No production metadata (loop, theme, size, tool, review status) and no paragraphs.
- Put words beside their evidence: a label next to its mark, a caption beside the figure it explains, the finding in its hue and the reason in `muted`.
- A motion explainer has no title unless requested; it labels values and ends on a short answer caption beside the result, so a paused or exported final frame explains itself.
- A plate ([cheatsheet](profiles/cheatsheet.md), [pipeline plate](profiles/pipeline-plate.md)) is read as a figure: a title that states the question, one answer line, and a caption per section.

## Layout and draw order

- Build the settled layout before timeline calls. Measure text with `text:measure()` in the font its role names (a Text that appears later starts at `opacity = 0`, so it can be measured and placed first), reserve regions for the subject, its labels and captions, and a title when the format has one, and size boxes from contents plus insets. Recompose or split before shrinking text.
- Render the skeleton at delivery size before animating; reject empty, off-camera, mirrored, or wrong-scale frames.
- Use explicit `layer` bands: background < regions < routes/guides < subjects < labels < transient focus. Creation order is not draw order.
- `Object::bounds()` is approximate; final containment and gaps come from `viscript layout`/`audit` and the raster.

## Motion

- Leave one coherent opening group unanimated so `t=0` already shows the subject or question; never show labels before their owners. Reveal later facts in their own beat.
- One beat = one claim and one dominant change. Animate the semantic thing (value, owner, route, trace step, result); decorative dots, sweeps, and pulses are not evidence. Camera moves frame the evidence (overview, zoom, emphasis); a plate keeps its camera still because the reader compares everything at once.
- Continue identities: transform the existing object into its next state rather than removing it and drawing a replacement.
- Keep finished structure as quieter context; hold conclusions. Linear for uniform rate, ease-out for construction, ease-in-out for reversible change.
- `loop=false` unless recurrence is requested or a plate profile loops; a loop must return every object, label, and color to its `t=0` state.
- Use `morph` for retained outlines and `match_transform` for text or formulas. Use `scene:clone(source, options)` or JS `definition.clone(source, options)` to snapshot a family at the cursor. Read API comments for ownership and geometry contracts.
- Timelines are deterministic at any sample time. For dependents of a changing value use `scene:tracker`/`bind`/`track`, or `tracker`/`bind`/`track` on a JS builder (baked at construction). Cell/Voxel `time` callbacks run at construction.

## Verify and deliver

Use [review.md](review.md) for each review round: one `viscript audit` over every frame, `viscript layout` at each meaningful animation's start/end, `t=0`, and the exact final time from `viscript inspect`, plus the interior peaks or closest-contact times that expose a transient effect, and a small image set. Missing glyphs, unexpected null bounds for a visible object, overflow, unintended overlap, and clipping fail even when commands succeed. Use the same selected times for JS/TS runtime reports. For composed or Diagram scenes preserve the relevant audit policies from `viscript audit --help`.

Look at the selected original-size frames: name the focal evidence and the tightest clearance in each, fix, and re-render using the same filenames. Use existing normal-speed host playback when needed; do not create an extra video for review. Keep probes and review renders under `temp/` next to the scene, or outside `.vscode/vis` in a VS Code workspace, and retain concise findings instead of full per-frame reports. Deliver source, requested export, reviewed times, and checks run.
