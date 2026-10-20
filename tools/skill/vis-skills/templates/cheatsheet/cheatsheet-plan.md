# viscript animated cheatsheet plan

- Audience and prerequisites:
- Output language and notation:
- Delivery format, content-derived dimensions/aspect ratio, FPS, shared-loop duration:
- Single learning goal or capability-map question:
- Two-second takeaway:
- Primary topology: atlas, trace, compare, anatomy, or spine
- Editorial direction: scientific plate, magazine insert, technical flyer, or another justified cut
- User-specified theme, style, or color direction; write `none` when absent:
- Theme decision: name no theme, so that Scenes are `adaptive`, when the previous field is `none`; otherwise list every root and child Scene that receives the chosen theme before object creation:
- Explicit object-style overrides and the exact semantic or measured legibility reason for each; prefer `none`:
- Established brand/example references inspected:

## First-party evidence preservation, when applicable

Prefer mounting the maintained example unchanged. Style and timing are the ordinary adaptation surface; other edits require a concrete integration necessity.

| Figure | Maintained source path | Used unchanged? | Style changes | Timing/loop changes | Other change and exact necessity |
|---|---|---|---|---|---|
| | | | | | |

## Complete-poster contract

- Clean title and optional instructional subtitle:
- Information intentionally excluded from the title:
- `t=0` summary pose:
- Exact-duration summary pose:
- Start/end visual equivalence requirement:
- Static conclusion, legend, or invariant modules:
- Primary figures that must each animate:
- Visible-copy inventory: each Text ID → exact subject fact, API, value, constraint, or conclusion:
- Authoring/review notes explicitly kept off the canvas:

## Content model

- Procedural A → mechanism → checkpoints → B → invariant, or:
- Capability families and why each belongs:
- Input semantics and preconditions:
- Tie, equality, ordering, and boundary rules:
- State representation and implementation model:
- Exact output scope and reconstruction evidence:
- Accuracy and boundary-case verification:
- Entry point and spatial gaze route:
- Column system, unequal spans, figure-caption rhythm, and boundary treatment:
- Monochrome diagram ink and restrained stroke-weight hierarchy:
- Semantic color and identity map; justify every hue beyond the neutral ink family:
- Preserved first-party, data-encoded, API-result, or color-as-subject exceptions:
- Layer and overlap map:

## Layout geometry and exclusion zones

Define these regions before creating any Scene, Viewport, figure, or Text object. Use rendered glyph bounds for text and the full-cycle motion envelope for animated geometry. Coordinates should be stated in delivery pixels or in one declared unit that converts unambiguously to delivery pixels.

Hard layout rules:

- Reserve a poster-header band that is disjoint from every module and Scene/Viewport rectangle.
- Within each module, reserve separate figure-title, evidence, and API/caption bands. A title or caption must not be placed inside the evidence rectangle to save space.
- Treat every Scene/Viewport rectangle as an exclusion zone for poster-level text and neighboring modules. A label may enter it only when it is an intentional annotation owned by that figure.
- Keep each figure's complete animated motion envelope inside its evidence region at every encoded frame, including camera motion, 3D projection, strokes, arrowheads, glows, and shadows.
- Match each child Scene's pixel aspect ratio to its destination Viewport within rounding tolerance. Never stretch X and Y independently.
- Do not use non-uniform scale or shear to fit evidence. Use uniform scale, rotation, translation, a real camera, or reflow the layout. Declare an exception only when distortion is the subject.
- Use a real 3D Scene, Space, and camera for 3D coordinate or camera evidence; do not fake depth by shearing a 2D Group.
- Measure clearance from painted or glyph bounds, not anchors, baselines, object centers, or nominal coordinates.
- When a region cannot satisfy its content and clearance budget, enlarge the canvas, change the grid span, or reflow the module. Do not conceal the conflict with clipping, layering, or unreadably small text.

- Canvas bounds and outer safe inset:
- Poster-header bounds and bottom clearance:
- Grid columns, rows, gutters, and module spans:
- Minimum title-to-evidence gap:
- Minimum evidence-to-API/caption gap:
- Minimum module-to-module gap:
- Minimum scene-internal annotation clearance:
- Theme typography role map for poster title, figure hierarchy, explanations/captions, and code/API; normally `h2`, `h3`, `text`, and `code` respectively:
- Delivery-size readability evidence for each Theme role and every justified local typography override; do not prescribe fixed pixel targets when Theme roles are available:

| Region ID | Owner | Outer bounds | Title band | Evidence Scene/Viewport bounds | API/caption band | Full-cycle motion envelope | Required neighboring clearance |
|---|---|---|---|---|---|---|---|
| | | | | | | | |

| Exclusion pair | Must remain disjoint or minimum gap | Frames/times to audit | Resolution when space is insufficient |
|---|---|---|---|
| Poster title/subtitle ↔ all module and Scene/Viewport bounds | | animation boundaries + required interior peaks | enlarge header/canvas or move the grid |
| Figure title ↔ its evidence Scene/Viewport | | animation boundaries + required interior peaks | enlarge title band or move evidence |
| Evidence motion envelope ↔ API call/caption | | animation boundaries + required interior peaks | enlarge evidence/caption gap or reflow |
| Neighboring module envelopes ↔ each other | | animation boundaries + required interior peaks | widen gutter or change spans |
| Scene-owned annotation ↔ animated geometry | | animation boundaries + required interior peaks | move annotation or reserve an internal label zone |

| Child Scene / Viewport | Child pixel size + aspect | Destination pixel size + aspect | Uniform-scale result | Pass or reflow |
|---|---|---|---|---|
| | | | | |

| Transform owner | Basis-vector lengths | Pairwise basis dot products | Declared semantic exception | Pass or replace |
|---|---|---|---|---|
| | | | | |

## Code or pseudocode anchor, when applicable

- Notation choice and why: verified executable code or language-neutral pseudocode
- Complete overview fragment and stable line numbers:
- Long-listing split boundaries and continuation labels:
- Verification method: run, type-check, compile, public-API inspection, or independent trace recomputation

| Line or range | Exact operation | Destination figure | Shared term/color identity | Highlight peak |
|---|---|---|---|---|
| | | | | |

## Module loops

| Module | Question + operative verb | Visible operands | Named state encoded by every motion | Mechanism and observable result | Literal conclusion/caption | Summary pose | Local period + phase | Return/seam check |
|---|---|---|---|---|---|---|---|---|
| | | | | | | | | |

## Concurrent-motion matrix

| Time | Modules in motion | Highest-salience peak | Why concurrent changes remain readable | Review frame |
|---|---|---|---|---|
| 0.0 | summary poses | none | complete plate | start |
| exact duration | restored summary poses | none | seam alignment | final/seam |

- Primary figure count `N` and ordinary-frame concurrency floor `ceil(N / 2)`:
- Per-figure first-half and second-half motion proof:
- Intended maximum-concurrency frame:

## viscript implementation

- Language/runtime and how viscript is loaded:
- `loop=true` confirmation:
- Scene/Group/Space/Viewport ownership:
- Root editorial plate → child Scene/Viewports and child loop periods:
- Exact font asset/family and registration:
- Text ID → canvas/panel → minimum inset:
- Text/geometry non-intersection pair → minimum pixel gap:
- Static region bounds and full-cycle motion-envelope audit method:
- Whole-Scene layout-report command/runtime, padding, and `scenePath + id` lookup:
- Collision allowlist with exact semantic reason for each pair:
- Parent/descendant containment surfaces and required inset; non-container parent exceptions:
- Scene/Viewport clipping and overflow policy:
- Child Scene → destination Viewport aspect-ratio audit:
- Transform matrix audit for unequal basis lengths and non-orthogonal basis vectors:
- Real 3D Scene/Space/camera evidence, when claimed:
- Persistent objects, local proxies, and consumed handles:
- Start/final/seam comparison method:
- Review and export commands:

## Completion evidence

- Content proof:
- `t=0` complete-poster review:
- Per-module peak and return review:
- Per-module local pixel-difference evidence:
- Representative concurrent-motion review:
- Full audit summary and boundary-time text and containment checks:
- Whole-Scene report audit: cross-Viewport collisions, clipped/outside families, parent-child containment, and worst-frame separation/overflow:
- Two-cycle delivery-speed review:
- Exact-final and seam review:
- Looping export artifact:
- Subject-only copy review: no loop, layout, theme, tooling, or audit meta text visible:
- Default-visual review: absent a user style request, all Scenes name no theme and so are `adaptive`, object styles remain Theme-resolved by default, and diagrams use one neutral ink family:
- Accent review: every extra hue names a necessary subject state or documented semantic exception; hierarchy otherwise uses restrained adjacent stroke weights:
- Code/pseudocode review: readable at delivery size, complete mappings, unobscured glyphs, correct behavior, preserved flow across fragments:
- Mechanism-evidence review: every title verb is enacted by its figure and every transient accent names a subject state:
- First-party preservation review: source mechanism, topology, geometry/data, sampler, camera/projection, and state transitions unchanged except documented necessities:
- Semantic-motion review: every animated target names its subject state, rule, and observation; no attention-only animation remains:
- Geometry-preservation review: no Viewport stretch, non-uniform layout scale, undeclared shear, or sheared 2D substitute for 3D evidence:
- Delivery-speed pacing review: measured rather than rapid, with readable results and settled holds:
- Text-to-geometry collision review at selected boundaries and closest-contact peaks:
- Header, figure-title, evidence, API/caption, and neighboring-module exclusion-zone review at selected boundaries and relevant peaks:
