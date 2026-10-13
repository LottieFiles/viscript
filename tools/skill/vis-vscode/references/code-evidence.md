# Code evidence

Read this before writing a code-linked Scene. It decides what the Scene may claim and which source proves it. [vis-skills](../../vis-skills/SKILL.md) then decides how the Scene looks and moves.

## Fix the evidence first

Pin the revision and the reader's question. Read the entry point, the callers and callees on the path, the state owners, the data transformations, the failure or cleanup path, and the tests, traces, benchmarks, docs, or review comments that constrain the claim. Then choose one representative scenario, a concrete input or event, and trace it through the code. A familiar textbook variant never substitutes for the implementation in front of you. The [algorithm-and-code profile](../../vis-skills/references/profiles/algorithm-and-code.md) defines what the trace must record.

Fill in one row per Scene before writing Lua:

| Column | Content |
|---|---|
| question | the one thing this Scene answers |
| scenario | input, configuration, and revision |
| anchors | ordered source ranges, each with its role |
| decisive change | the state transition or handoff that answers the question |
| result | the observable output, invariant, cost, or failure |
| support | the test, trace, or statement behind it; otherwise mark the row inferred |

Drop rows that have no visible causal transition, repeat another row's claim, or depend on a hook the code does not support. One strong Scene beats an artificial series.

## Scope routes

- **Repository.** A reader journey through purpose, entry path, the decisive mechanism, ownership, and one meaningful edge path. Not a directory tour.
- **Symbol or range.** Center that code and follow only the dependencies needed to explain it.
- **Topic.** Pick one concrete executable path, and say in the description that it is one implementation.
- **PR or commit.** Use the exact revisions plus the unchanged context, grouped by behavior rather than by file or hunk. Keep new behavior, intended-equivalent refactoring, test changes, and speculation apart. Then read [diff-authoring.md](diff-authoring.md).

## Choose and order ranges

- Each `links` range is the smallest coherent block that holds the local input or governing condition, the decisive operation, and its immediate result or handoff. Split distant stages into separate links instead of one long range.
- The primary `source`/`symbol` anchors CodeLens but is not listed in the Inspector. When the definition's body is evidence, add it as a ranged link too.
- Order `links` by the scenario's runtime control or data flow, never by file, declaration, alphabet, or patch order. For concurrent work, order by causal handoff and describe the overlap instead of inventing a total order.
- Keep one order and one vocabulary across `links`, Inspector labels, on-Canvas anchors, beats, and the description. If the Scene numbers its sections, start the matching `label` with the same identifier. Never invent an identifier only in the manifest.

## Split Canvas, Inspector, and notes

- **Canvas.** The mechanism, plus only the decisive verified fragment (a condition, call, mutation, return, or signature) with exact identifiers. A fragment may be a strict subset of its link range.
- **Inspector.** The full source, through `links`. File paths, line numbers, hashes, and directory names stay here, never on the Canvas or in the narration.
- **`description`.** The revision, scenario input, scope, caveats, alternate branches, benchmark conditions, why an anchor matters, and which statements are inference.
- **Never evidence.** The Scene's own Lua, helper Lua, exported PNG/GIF/MP4, authoring calls, and any invented file, symbol, range, or notes document.

## Pick the format from the evidence

- **One temporal mechanism dominates.** Make an ordinary animation (`loop = false`).
- **Several regions, states, or invariants must stay visible at once.** Make a plate from the vis-skills [cheatsheet profile](../../vis-skills/references/profiles/cheatsheet.md). Its complete pose must read at `t = 0` and at the loop seam.
- **Several independent claims.** Make a series: one `series` string, one claim per episode, each episode understandable on its own. The Viewer provides navigation.
- **Short-form, only on request.** One verified claim per episode, and a final frame that keeps the answer and its evidence. No phone chrome, subtitles, or social controls. A tall Canvas or a wish for energy is not a short-form request.

Choose the aspect ratio from the evidence geometry and the editor group the Scene will sit in. When a narrow FIT becomes unreadable, cut copy or split the episode before shrinking text.
