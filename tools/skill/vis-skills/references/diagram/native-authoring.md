# Native Lua Diagram authoring

`vis.diagram(scene, config)` authors semantic nodes, edges, zones, ports, and waypoints, then lowers them into ordinary scene objects. Use ranked, manual, grid, or timeline layout to fit the evidence.

```text
semantic inventory → layout and routing → Object tree → timeline animation
```

Configure the Scene Theme, create the inventory, call `build()`, retain its handles, and measure the generated layout before adding annotations. JavaScript scenes use ordinary Object handles; Lua and C++ expose the semantic builder.

The generated root retains versioned Diagram IR. In a native Diagram build, `viscript audit` reports semantic IDs, relations, placements, bindings, and rendered clearance. Join records by `(scenePath, diagram id, entity kind, entity id)`.

## Layout and routing

- Ranked layout arranges peers around authored position pins.
- Manual layout uses authored node positions.
- Grid layout uses row/column cells.
- Timeline layout uses task row, start, and span.

Edges identify their relation, endpoints, ports, route style, and optional waypoints. Automatic and orthogonal routing use node geometry to select clear corridors. Explicit straight routes and waypoints retain the authored path; audit reports conflicts for the author to resolve.

Directed edges lower to one object containing shaft and markers: Arrow for straight edges, Route for dashed orthogonal edges, and filled Path for undashed orthogonal edges. `flow` animates a route's dash. Zones derive their bounds from member nodes.

Lowered objects follow the Scene Theme. Corners use its `roundness`, which is 0 (square) in the pro presets, unless the config sets `corner` explicitly. Every node kind gets a `background` fill and an ink outline, and zones get a hairline outline with no fill. Edges stay gray except `error` (danger) and flowing `directed` edges (accent). Node kinds are therefore told apart by their labels, not by hue. To mark the node or edge the reader must find, recolor `built:node_body(node)` or `built:edge(edge)` with `set` or `play`.

Read API comments for builder ownership, accepted fields, validation, and search budgets.

## Review

Run the Diagram-enabled `viscript audit` with the delivery font. Generated containment checks cover node label/detail text and zone labels. Declare intentional ownership exceptions explicitly in the audit policy.

At delivery sample times, inspect rendered route/node, route/label, route/route, and route/canvas clearance. Use `--diagram-route-gap` and `--canvas-inset` for the requested clearance. Check the worst frames at original size, then repair layout or authored routes and rerun the relevant samples.
