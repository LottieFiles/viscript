# Algorithm and code evidence

Use this profile when executable behavior, data structures, or supplied-source relationships must become inspectable evidence. Choose the evidence cut from the reader's question, not an algorithm or pattern name.

## Source truth and trace

Name the implementation or revision, entry point, representative input/event, index and range conventions, equality/tie behavior, mutation order, allocation/aliasing, failure or sentinel behavior, and the question answered. Inspect implementation and call sites when source exists; never substitute a familiar variant.

For an algorithm or data structure, compute a deterministic trace before visual objects. Each record identifies the operation, operands, precondition, state change, result, invariant, and any claimed cost. Replay it and verify the final result.

For runtime collaboration, record source-anchored participants, their wiring (construction, registration, injection), calls and returns (dispatch, callbacks, errors, cancellation, teardown), ownership and lifetime, thread and async boundaries, and the tests or logs that separate observed from possible behavior. Keep types apart from runtime objects, declarations from invocations, ownership from borrowing, and synchronous from asynchronous work; mark unresolved or illustrative relations.

## Visual state and identity

Use stable geometry for structure and separate channels for state. Slots remain slots while values move; nodes keep identity while links or state change. Give a queue cell, stack frame, pointer, range, predecessor edge, task, message, or resource its own identity whenever the explanation depends on it.

For traversals and searches, distinguish discovered/enqueued, currently processed, visited/settled, rejected/stale, predecessor ownership, frontier order, and final reconstruction. Preserve implementation neighbor order and stopping rule. Derive queue contents, distances, colors, labels, and predecessor path from the same trace record; never recolor a graph independently from its queue or table.

Preserve graph direction: derive each route from the canonical edge and trim its ends at the node outlines so the arrowhead stays visible outside the target.

For mutation, distinguish preview, commit, and settled invariant; ghost destinations are temporary evidence, not live values. Track `slot_of[identity]` and move each owner with `position` to its destination slot's coordinates, not a shift derived by hand. A swap moves both owners to exchanged slots at once; never move one value into an occupied slot and imply completion, and never cover a wrong transition with a final-state overlay. At every commit the rendered slot-to-value map equals the trace. Show sequential writes in actual order.

Use the state ledger in code rather than hand-authored distances:

```lua
local function swap_ids(a, b)
  local sa, sb = slot_of[a], slot_of[b]
  scene:tween({
    {target=owners[a], position={slot_x[sb], slot_y}},
    {target=owners[b], position={slot_x[sa], slot_y}},
  }, {duration = swap_time, curve = "ease_in_out", lag = 0})
  slot_of[a], slot_of[b] = sb, sa
end
```

For multi-object collaboration, pair a settled structural view with the one scenario the question needs; a pattern name may annotate verified evidence but never substitutes for it.

## Motion and code anchors

Map events to concrete semantic verbs—compare, read, enqueue, dequeue, visit, settle, swap, write, link, allocate, call, dispatch, block, wake, return, release, or destroy—and animate the named identity. Do not animate every source line or move a generic token without identity.

Compress repetition only after the first distinct case is readable, and never across a branch, boundary case, first divergence, or conclusion-bearing transition. Show minimal source anchors with exact identifiers, indentation, and line order; each highlighted region maps to the state change it controls, and code Text never replaces visible state. Point at source by what it says, not by counted token positions: `block:search("total", {match = "all"})` gives the token handles for `highlight`, `rectangle{around = ...}`, and `indicate`, and `scene:edit(block, {...})` turns a block into its next revision with the same look and place, as a diff. A complexity claim needs counted work, a recurrence or bound, and explicit case assumptions; decorative height or a familiar formula is not proof.

## Show the core problem

When code is the subject, the reader should find the one decisive thing at a glance: the stale value, the wrong bound, the missed branch, the aliased write. Give it the finding's hue in every view (code token, value, mark, edge label, caption) and keep everything else ink. Devices that make it visible:

- a limit ruler at a bound, labeled with the expression that sets it (`count`, `i < n`);
- a scan head or cursor that stops where the code stops;
- dashed outlines for work that never happens: undrawn items, skipped iterations, unreached states;
- a value that keeps its copy, captioned plainly ("never written again"), or an overwritten value struck out at opacity 0.5 ("overwritten");
- paired counts of what exists and what is processed ("items.size() = 5", "count = 3", "3 of 5 drawn").

Draw a fix only when the user asks for it or the change under review is the fix, as a later episode or the final beat: keep the same layout and trace and change only the corrected line and its consequence, so the difference is the only thing that moves. Otherwise name the fix in a caption or, in VS Code, in the notes. When the path spans several functions or passes, use the [pipeline plate](pipeline-plate.md) format.

## Acceptance

- Replaying the trace reproduces every shown state, and every value, counter, label, and color derives from it.
- Identity, range, tie, ownership, and ordering match the named implementation; concurrent work is not drawn as a sequential schedule.
- The settled frame explains the finding without a pattern name or playback, and omitted branches and illustrative relations are disclosed.
