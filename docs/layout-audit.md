# Layout audit

`viscript audit` checks a Scene across its export timeline and writes a layout report.
It samples every encoded frame plus the exact final time using the registered fonts
and selected output profile.

```sh
build/viscript audit scene.lua \
  --font src/examples/assets/Pretendard.ttf \
  --text-gap 4 --canvas-inset 4 --panel-inset 12 --diagram-route-gap 2
```

The default output is `vis.layout-audit/v1` JSON. Add `--format text` for a readable
report, or `--width`, `--height`, and `--fps` to set the export profile.

## Checks

The command inventories root, Viewport, and transition Scenes, then checks:

- Text canvas clearance and Text/Text spacing;
- declared Text-to-Rectangle containment;
- Object IDs, authored Text visibility, and font glyph coverage;
- full cover by opaque mounted Scene backgrounds;
- Viewport proportions;
- Diagram placements and sampled route clearance when Diagram is enabled.

Text within a `typeset` Group is treated as one block, so words, code tokens,
matrix entries, and formula pieces are checked against surrounding text. Matching
and replacement transitions carry their intended handoff through sampled reports.

## Containment and allowances

Stable IDs connect policy to authored content. Declare a panel relationship with
`--contain TEXT PANEL [INSET]`, or use the containment ledger to account for each
Text's role:

```sh
build/viscript audit scene.lua \
  --font src/examples/assets/Pretendard.ttf \
  --require-containment-ledger \
  --contain node-label node-body 12 \
  --allow-uncontained title
```

Containment compares the Text's paint with the Rectangle's own paint in root pixels.
The optional inset overrides `--panel-inset` for that relationship.

Declare intentional overlaps with an exact pair of Scene paths and IDs:

```sh
build/viscript audit scene.lua \
  --font src/examples/assets/Pretendard.ttf \
  --allow-text-overlap root title root/viewport:0 title
```

`--allow-occluded-scene PATH` records an intentional composition cover.
`--allow-viewport-stretch` records intentional nonuniform mounting. The report
includes each policy entry's matched and used state.

## Diagram inspection

A generated diagram root retains a `vis.diagram.layout/v2` receipt. The audit
reads its semantic nodes, edges, zones, settings, placements, and Object bindings.
Generated label-to-body relationships provide containment policy; explicit entries
replace them.

Static checks report node, zone, endpoint, and route issues in Diagram coordinates.
The sampled pass joins receipt bindings to renderer paths in root pixels, checking
routes against node bodies, labels, other routes, and the active clip.
`--diagram-route-gap` sets route clearance.

The physical issue codes are:

- `diagram_sampled_route_node_collision`;
- `diagram_sampled_route_label_collision`;
- `diagram_sampled_route_route_collision`;
- `diagram_sampled_route_canvas_overflow`.

## Report

The JSON records the output profile, frame/sample counts, thresholds, policy,
aggregated issues, Diagram receipts, and final `valid` value. Repeated issues carry
first, last, and worst sample times, measured clearance, required clearance, bounds,
and suggested separation.

Scene paths identify composition occurrences, such as `root/viewport:0`.
Object records identify authored elements. Visual records identify sampled paint
and transition counterparts. Path records hold projected vertices and stroke/fill
metadata. Own paint bounds and descendant-inclusive family bounds are separate.

`visible` describes surviving paint; `occluded` identifies complete coverage by an
opaque mounted Scene. Glyph issues include the missing character count and first
missing character. Diagram issues include semantic IDs and static or sampled
provenance. `summary.failingGeometrySamples` counts sampled failures.

| Exit status | Meaning |
|---|---|
| `0` | Passed |
| `1` | Layout issues found; report written |
| `2` | Usage, loading, or policy syntax failed |
| `3` | Renderer or font loading failed |
| `4` | Timeline, inventory, layout, or allocation failed |

Use the aggregated worst times to select frames for visual review. See
[Scene composition](scene-composition.md) for layout and sampled bounds.
