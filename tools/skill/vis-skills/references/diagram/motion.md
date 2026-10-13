# Diagram motion

The shared scene contract governs timing, pacing, focus, camera, endpoints, and general loop safety. This profile adds only diagram-specific invariants. Use motion when it proves construction order, causality, propagation, evaluation, or a real state/topology change; otherwise keep the diagram stable for inspection.

## Choose one evidence mode

| Mode | Required endpoint |
|---|---|
| Construction | Complete, inspectable diagram; normally one-shot |
| Causal trace | Complete stable topology plus the settled result |
| State transition | Stable node identities with the final state readable |
| Operational loop | Identical start/end topology and a truthfully recurring state |
| Comparison | Both resolved states visible together or at an explicit final frame |

If construction and operation both matter, complete and hold the construction before one concise operational trace. Do not hide relationships that are required to interpret the trace.

## Directed-route invariant

A directed route remains one Object throughout motion.

- A straight directed edge is one `Arrow`. Use its directional Create when construction direction is evidence.
- An orthogonal directed edge is one `Route` when the routed shaft and endpoint markers must share progress. Use a closed filled `Path` only for bespoke directed geometry that Route cannot express.
- Reveal a route label with or after its route, never before the relation it names.
- Once established, keep the route stable. Do not redraw it for every event or stack a Line, Plot, Path, or second Arrow over it.

For continuous active transport, dash the same Arrow or Route and animate its `dash_offset` linearly. The target must have a non-empty dash pattern. The end offset must differ from the start by exactly one complete dash period: the sum of every dash and gap length, or `9` for the native `{5, 4}` pattern. This preserves phase across a loop seam. The solid head and optional tail belong to the same Object but do not consume dash phase.

For one discrete arrival, animate a deliberately named payload only when the payload itself is evidence; otherwise change the truthful destination state, result, counter, field, or queue slot. Do not use an anonymous moving Point as generic activity.

## Stable topology and state

During an ordinary trace, do not move nodes, resize containers, reroute corridors, or change camera framing. Spatial memory is the reader's index. A request, response, error, or state write should receive distinct treatment only when their distinction supports the claim. Advance authored stages deterministically, and let arrival trigger the smallest truthful state change. When a route, affected node or field, and explanatory Text must change as one semantic identity, recolor them together in one `play` array.

Animate topology only when reconfiguration, failover, migration, regrouping, or a before/after layout is the subject. Show and hold the complete before state; preserve stable IDs and unchanged nodes; replace or move only changed structures; reroute affected edges as one semantic transition; then show and hold the complete after state.

## Operational loop invariant

Loop only a route phase, sweep, or genuinely cyclic authored state—not the construction sequence. Across the seam, topology, labels, node identities, and every noncyclic semantic value must match. For dashed flow, the start and end samples must have the same pattern phase. Reject a loop that requires a connector-free flash, random callback state, or an untruthful reset of a one-way result; use one-shot playback instead.
