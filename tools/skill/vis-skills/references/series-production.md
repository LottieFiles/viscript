# Series production

Use several Scenes only when the request contains several independently useful questions or claims. Split before shrinking text, accelerating explanation, or accumulating unrelated evidence in one frame.

## Plan the sequence

Write a series ledger before scene code:

| Field | Requirement |
|---|---|
| episode | stable ID, title, and one question |
| carry-in | prerequisite meaning retained from earlier episodes |
| model | canonical state or declared variant |
| opening | meaningful standalone first frame |
| decisive beat | the motion that proves this episode's claim |
| answer | readable final still and handoff |
| evidence | authoritative source or derivation |
| duration | beat-derived target, not padded runtime |

Every episode must stand alone while sharing aspect ratio, Theme, typography, camera convention, pacing bands, and semantic color meanings. Its episode-ledger row does not replace the 3–7 beat ledger required for that Scene. Carry concepts and composition between episodes, and namespace internal IDs when useful. Do not erase final evidence merely to manufacture an inter-episode transition—the viewer owns navigation.

Reuse one canonical model when episodes explain the same example. Declare any changed input as a variant. Do not present an illustrative balanced tree as proof of an average-case claim, or a chosen trace as a distributional result; derive or source statistical conclusions separately.

## Source-linked VS Code series

For a requested editor-hosted series, also read `../../vis-vscode/references/code-evidence.md` and `../../vis-vscode/references/manifest.md`.

- Put the v2 manifest at `.vscode/vis/visualizations.json` and each scene directly at `.vscode/vis/<series>/animations/<scene>.lua`.
- Keep manifest `links` in execution or causal order and map each animation to the evidence it actually explains.
- Never invent a product source file, symbol, range, or notes document to satisfy the manifest. If no genuine source exists, omit both the manifest and its reserved `.vscode/vis/<series>/` tree; deliver ordinary Lua Preview scenes in a neutral project folder, or use only a user-provided conceptual document that is truly authoritative and in scope.
- Let the VS Code viewer provide series selection and FIT; do not duplicate editor chrome or navigation inside a Scene.

## Review as episodes and as a whole

Apply a [review round](review.md) to every episode: one audit over every frame with its Text ownership ledger, layout checks at meaningful animation starts/ends, `t=0`, the exact final time, and the necessary interior peaks, and the episode's small image set. Retain compact per-episode findings; one passing episode or a series montage does not cover another episode. Then watch the complete order at 1×. Confirm that openings restore enough context, semantic colors never drift, text and focal objects keep comparable scale, no episode repeats another's conclusion, and the handoffs form one argument. In VS Code, also verify manifest diagnostics, FIT, CodeLens and **SEE** evidence, and episode selection.
