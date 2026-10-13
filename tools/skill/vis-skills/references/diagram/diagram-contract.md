# Diagram contract

Use this contract for every viscript diagram. It defines the semantic cut, layout, routing, and review gates without prescribing a particular scene.

## Select the evidence form

Choose the family that makes the reader's question easiest to verify:

| Primary evidence | Suitable family |
| --- | --- |
| Components, ownership, zones, dependencies | architecture or layered structure |
| Values, messages, calls, or control moving between actors | data/control flow, sequence, or swimlane |
| Guards, choices, transitions, and reachable conditions | state flow |
| Parentage, containment, or dependency depth | hierarchy or tree |
| Recurring operational causality | loop |
| Work over time, overlap, milestones, or critical path | timeline or Gantt |
| Entities, keys, and cardinality | ER/schema |
| Comparable measurements | chart with aligned labels or table |

Use a motion scene instead when topology is only background for one changing mathematical or physical mechanism. Use multiple related views when one diagram would mix incompatible reading orders or levels of detail.

## Define semantics before geometry

Record the reader question, evidence boundary, containers, nodes, ports, relation types, labels, states, stable IDs, reading order, and uncertainty. Give visually distinct relations explicit meanings, including direction, request/return, data/control, synchronous/asynchronous, optional/error, ownership, or state transition when relevant.

Apply a removal test to every element and relation. Keep it only when it adds evidence that containment, alignment, order, or a nearby label does not already communicate. Split overview and detail when removal would erase truth but keeping everything would reduce legibility.

## Lay out from measured content

Choose and register the production font before measuring. Size a text-bearing node from its sampled Text bounds plus a declared inset. Size a container only after its semantic members and header reservation are known. A backing shape must express boundary, ownership, clipping, selection, or needed local contrast; do not stretch it to fill decorative space.

Choose one primary reading spine. Align peers, preserve negative space, and reserve clear corridors before drawing routes. Establish node and obstacle bounds first, then place explicit ports where connection meaning or congestion requires them. Keep route labels clear of strokes and node bodies; use a background mask only to solve measured bleed-through.

## Use truthful route objects

- Use `Arrow` for one straight directed segment.
- Use `Route` for an orthogonal or other polyline connection. Its shaft and markers share one progress state.
- Use one closed filled `Path` for a curve or bespoke directed silhouette outside the Route contract.
- Use `Connector` only when its simple bounds-following geometry is truthful. It is not an obstacle router.
- Never overlay a separate marker-bearing object on the same centerline to simulate one edge.

Prefer short, monotone paths and clear corridors. If a route must weave around unrelated content, change the layout or split the view. A route may meet its endpoints but must not disappear behind an unrelated node.

## Preserve hierarchy and identity

Use semantic Groups and explicit paint bands: background, containers, routes, optional masks, nodes, node text, route text, then annotations or focus. Creation order is not a foreground contract.

Choose the Scene Theme before literal color. Let most structure use ordinary ink; reserve color and dash changes for named semantics that remain understandable without color. Keep a stable visual identity synchronized across a mark, its label, and any live animation proxy.

Every visible Text needs a stable ID and exactly one layout status: owned by a named Rectangle with a measured inset, or deliberately uncontained. Titles, edge labels, and annotations are not exempt from the ledger; they are explicitly free text. Audit sampled production-font bounds across every representative frame.

## Review in two passes

Structural review checks semantic truth, containment, direction, route/object choice, crossings, label ownership, source grounding, and the declared reading order. Geometry review checks canvas and container bounds, text containment, route clearance, clipping, and motion envelopes. In a Diagram-enabled native build, keep static IR diagnostics distinct from `viscript audit`'s sampled rendered-geometry issues; correct the source rather than treating either pass as permission to reroute authored straight paths or waypoints.

Raster review then inspects actual target-renderer pixels at delivery size: settled frame, first frame, worst-clearance transitions, state changes, and loop seam when present. Correct the diagnosed geometry or timing, rerender, and repeat. A successful API call, layout command, or vector-bounds check is not visual proof.
