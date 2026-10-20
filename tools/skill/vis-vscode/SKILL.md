---
name: vis-vscode
description: "Link viscript scenes to product code in the VS Code extension: choose code evidence, write the .vscode/vis manifest (series, ranges, links, verified diffs), and use Preview, CodeLens, hover, Inspector, playback ranges, and export. Not for scene or animation design (vis-skills) or extension source changes (vis-animation-dev)."
---

# viscript VS Code

The extension plays ordinary viscript Lua scenes in a sandboxed Webview beside the code they explain. This skill owns the code side of that pairing: which source is evidence, how `.vscode/vis/visualizations.json` binds it to a Scene, and what the extension does with the binding. How the Scene looks and moves belongs to [vis-skills](../vis-skills/SKILL.md), retained input to [vis-interactive](../vis-interactive/SKILL.md), and edits under `tools/vscode/` to `vis-animation-dev` ([vscode-extension.md](../vis-animation-dev/references/vscode-extension.md)).

## Route

- [code-evidence.md](references/code-evidence.md): turn a repository, symbol, topic, PR, or commit into one scenario, ordered ranges, and episodes.
- [manifest.md](references/manifest.md): fields, paths, series, and how each field reaches CodeLens, hover, the Inspector, and **SEE**.
- [diff-authoring.md](references/diff-authoring.md): revision evidence through verified `diff` links.
- [extension.md](references/extension.md): running the extension, what a Scene gets from the host, Inspector, playback ranges, export, IntelliSense, settings, troubleshooting, and delivery checks.
- Scene code: the vis-skills [scene contract](../vis-skills/references/scene-contract.md), [lua.md](../vis-skills/references/lua.md), and usually the [algorithm-and-code profile](../vis-skills/references/profiles/algorithm-and-code.md). Add the [pipeline-plate profile](../vis-skills/references/profiles/pipeline-plate.md) when the scenario crosses several functions or passes, and [series production](../vis-skills/references/series-production.md) for several Scenes.

## Workflow

1. Read the code before writing the Scene. Fix the revision, one representative scenario, and its ordered evidence ranges. Without genuine product source there is no manifest: deliver plain Lua Preview scenes.
2. Write each Scene with vis-skills. The Viewer fits the Scene's one fixed logical Canvas, 960×540 unless the Scene names another size, to the player, and the Scene leaves the theme out, so it is `adaptive` (unless the user picks a style). The Canvas must explain itself, because exports carry nothing else: a plate states its question and answer, and a motion Scene ends on an answer caption. Never draw editor chrome, file paths, line numbers, or episode navigation on the Canvas.
3. Save the Scene at `.vscode/vis/<series>/animations/<scene>.lua` and add one manifest entry whose `links` follow the scenario's execution order.
4. Run the vis-skills review, then the host review and the VS Code checks in [extension.md](references/extension.md#verify-before-delivery).

## Invariants

- The manifest binds and the Scene explains. Evidence lives in `links` and the Inspector, never in Scene Lua, helper Lua, or exported media.
- The explanation is split three ways, and none of the three is empty. The Canvas shows the mechanism with its question, values, decisive code line, and captions. The Inspector shows the full source. `NOTES`, open whenever a Scene loads, carries the revision, scenario, caveats, and what is inferred.
- Give every animation file exactly one manifest entry. A Lua file named by zero or several entries opens as plain Preview.
- `links` follow runtime order. The Canvas, Inspector, and description use the same order and the same names.
- Nothing opens by itself. CodeLens, the hover card's link, the editor-title icon, and **viscript: Open Viewer** are the only entry points. Inside the Viewer, the source editor moves only on **SEE** or **Document → Source**.
