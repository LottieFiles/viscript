# SVG and template migration

Migrate an SVG according to how the reader must use it, not according to its file extension. Display a complete SVG as one `Picture`, or rebuild meaningful internal parts as native objects.

## Choose the migration level

| Source content | Preferred viscript form |
|---|---|
| Logo, icon, or decorative illustration that remains atomic | one `Picture` Object |
| Diagram node, lane, container, label, or route whose meaning must remain addressable | native Group, shape, Text, and stable IDs |
| Straight marker-ended SVG line | one `Arrow` |
| Orthogonal or polyline marker-ended SVG route | one `Route` containing the ordered centerline and endpoint markers |
| Curved or bespoke marker-ended SVG route | one closed filled `Path` containing shaft, bends, and head |
| CSS design token | Scene Theme role or automatic object cycle |
| SVG group ID or role metadata | semantic Group and stable Object IDs |
| Route-following SVG clone or particle | one stable route plus a separate deterministic marker |

Prefer native reconstruction for topology, code-linked figures, Theme adaptation, semantic color identity, per-object motion, and layout validation. Keep a complete `Picture` only when its internal parts are intentionally atomic.

## Do not copy presentation coordinates blindly

Extract and verify the source's semantic inventory before authoring geometry:

```text
containers: ownership or scope
nodes: role, label, detail, stable ID
edges: source, target, relation, direction, preferred ports
motion: stage, order, settled endpoint
```

Then place node and container bounds for the actual viscript Canvas, reserve label and route corridors, and author new coordinates. HTML box metrics, CSS transforms, SVG viewBox scaling, marker units, and DOM text measurement are not viscript layout contracts.

## Converter scope

For a requested converter, define the supported SVG elements in its API comments and verify examples before expanding coverage. Lower marker-ended lines to an integrated Arrow, Route, or filled Path so shaft and markers animate as one identity.

## Licensing and attribution

Reimplementing a visual idea or topology does not require copying its implementation. Prefer an independent viscript-native reconstruction. When substantial source code, path data, icons, palettes, fonts, or templates are copied, preserve the applicable copyright notice and license in the delivered package. Do not infer that a repository license covers separately licensed fonts or assets.

## Review

Inspect the original source and the viscript result at their real delivery sizes. Verify semantic equivalence, route direction, node ownership, text, arrow seams, clipping, Theme contrast, and the complete settled endpoint. Pixel identity is not the goal unless the user explicitly requests a faithful redraw; truthful structure and native editability are.
