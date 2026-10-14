# Algorithm and code evidence

Use this profile when executable behavior, data structures, or supplied-source relationships must become inspectable evidence. Choose the evidence cut from the reader's question, not an algorithm or pattern name.

## Source truth and trace

Name the implementation or revision, entry point, representative input/event, index and range conventions, equality/tie behavior, mutation order, allocation/aliasing, failure or sentinel behavior, and the question answered. Inspect implementation and call sites when source exists; never substitute a familiar variant.

For an algorithm or data structure, compute a deterministic trace before visual objects. Each record identifies the operation, operands, precondition, state change, result, invariant, and any claimed cost. Replay it and verify the final result.

For runtime collaboration, record source-anchored stable participants; construction, registration, injection, factory/registry wiring; calls, dispatch, delegation, callbacks, returns, errors, cancellation, and teardown; reads/writes, ownership, borrowing, and lifetime; thread, task, queue, lock, and async boundaries; and tests, traces, or logs distinguishing possible from observed behavior. Keep type relations separate from runtime-object relations, declarations from invocations, possible from observed targets, ownership from borrowing/sharing, synchronous from asynchronous work, and total from partial order. Mark unresolved or illustrative relations.

## Visual state and identity

Use stable geometry for structure and separate channels for state. Slots remain slots while values move; nodes keep identity while links or state change. Give a queue cell, stack frame, pointer, range, predecessor edge, task, message, or resource its own identity whenever the explanation depends on it.

For traversals and searches, distinguish discovered/enqueued, currently processed, visited/settled, rejected/stale, predecessor ownership, frontier order, and final reconstruction. Preserve implementation neighbor order and stopping rule. Derive queue contents, distances, colors, labels, and predecessor path from the same trace record; never recolor a graph independently from its queue or table.

Preserve graph directionality. For a directed input, derive each visible route from the canonical edge and shorten its endpoints from node geometry so the arrowhead remains visible outside the target body; a center-to-center shaft hidden by nodes is not directional evidence.

For mutation, distinguish preview, commit, and settled invariant. Ghost destinations are temporary evidence, not live values or pointers. Track `slot_of[identity]`; derive each target or shift from the actual source and destination slot coordinates, then update the record after commit. A swap moves both value owners to exchanged slots or replaces the state atomically; never move one value into an occupied slot and imply completion. At every settled commit, verify the rendered slot-to-value map equals the trace with exactly one live value per occupied slot. Never cover a wrong transition with a reconstructed final-state overlay. Show sequential writes in actual order.

Use the state ledger in code rather than hand-authored distances:

```lua
local function swap_ids(a, b)
  local sa, sb = slot_of[a], slot_of[b]
  scene:play({
    {target=owners[a], shift={slot_x[sb]-slot_x[sa], 0, 0}},
    {target=owners[b], shift={slot_x[sa]-slot_x[sb], 0, 0}},
  }, swap_time, "ease_in_out", 0)
  slot_of[a], slot_of[b] = sb, sa
end
```

For multi-object collaboration, pair a settled structural view with only the representative scenario needed by the question. Show participants, members, relations, and source fragments that decide it. A pattern name may annotate verified evidence but never substitutes for it.

## Motion and code anchors

Map events to concrete semantic verbs—compare, read, enqueue, dequeue, visit, settle, swap, write, link, allocate, call, dispatch, block, wake, return, release, or destroy—and animate the named identity. Do not animate every source line or move a generic token without identity.

Keep excluded data as quiet context when it explains an invariant. Compress repetition only after the first distinct case is readable and only from the complete trace; never cross a branch, boundary case, first divergence, or conclusion-bearing transition. Display minimal source anchors with exact identifiers, indentation, line order, and control flow. Each highlighted region maps to its controlled state change; code Text does not replace visible state. Point at source by what it says, not by counted token positions: `block:search("total", {match = "all"})` gives the token handles for `highlight`, `rectangle{around = ...}`, and `indicate`, and `scene:edit(block, {...})` turns a block into its next revision with the same look and place, as a diff. A complexity claim needs counted work, a recurrence or bound, and explicit case assumptions; decorative height or a familiar formula is not proof.

## Show the core problem

When code is the subject, the reader should find the one decisive thing at a glance: the stale value, the wrong bound, the missed branch, the aliased write. Give it the finding's hue in every view (code token, value, mark, edge label, caption) and keep everything else ink. Devices that make it visible:

- a limit ruler at a bound, labeled with the expression that sets it (`count`, `i < n`);
- a scan head or cursor that stops where the code stops;
- dashed outlines for work that never happens: undrawn items, skipped iterations, unreached states;
- a value that keeps its copy, captioned plainly ("never written again"), or an overwritten value struck out at opacity 0.5 ("overwritten");
- paired counts of what exists and what is processed ("items.size() = 5", "count = 3", "3 of 5 drawn").

Draw a fix only when the user asks for it or the change under review is the fix, as a later episode or the final beat: keep the same layout and trace and change only the corrected line and its consequence, so the difference is the only thing that moves. Otherwise name the fix in a caption or, in VS Code, in the notes. When the path spans several functions or passes, use the [pipeline plate](pipeline-plate.md) format.

## Acceptance

- Replaying the canonical trace reproduces every shown state and final result.
- Every value, counter, queue, path, label, color, and duplicate view derives from the trace or ledger.
- Identity, range, tie, ownership, and ordering match the named implementation.
- Concurrent work is not fabricated as a sequential schedule.
- The settled frame explains the invariant, behavior, or finding without a pattern name or playback.
- Omitted branches, unverified targets, and illustrative relations are disclosed.
