# Using the extension

## Run

From `tools/vscode/`, run `npm install` and `npm run compile`, press `F5`, and open the target workspace in the Extension Development Host. For an installable package, run `npm run package` and install the VSIX. The extension activates for Lua, C, and C++ files and for workspaces that contain the manifest.

## Open and follow

- The editor-title icon opens the active Lua file when it contains `vis.scene {` or `vis.scene(`.
- **viscript: Open Viewer** reveals the Viewer. If none is open, it opens an empty one (`NO SCENE`) with the workspace series listed.
- CodeLens **Preview "<title>" animation** and the hover card's Preview link open a mapped Scene.
- While the Viewer is open, it follows the active Lua tab and reloads the unsaved document after `visPreview.updateDelay` (280 ms). A failed reload keeps the last valid frame and reports Lua diagnostics.
- Moving the cursor, opening a file, and hovering never open the Viewer.

## What a Scene gets from the host

These are the only delivery differences from vis-skills:

- **Canvas.** One fixed logical Canvas. FIT scales it uniformly without cropping or stretching, and resizing never reruns Lua. Display zoom never changes export size.
- **Theme.** `theme = "adaptive_vscode"` follows the workbench theme and rebuilds when the theme changes. Fixed and custom themes stay Scene-owned.
- **Fonts.** Pretendard, monospaced JetBrains Mono NL (`JetBrainsMono`, used by code), and Source Serif 4 (used by the `adaptive_vscode` heading and body roles). Those roles name the list `Source Serif 4, Pretendard`, so Korean letters, which Source Serif 4 lacks, come from Pretendard, as in the Viewer's CSS. The CLI has no host, so it sets `adaptive_vscode` in Pretendard with Pro White colors; the host review below measures what the Viewer shows.
- **Assets.** `asset = "..."` and `texture = "..."` load relative to the Lua file.
- **Runtime.** The bundled CPU WASM runtime includes Diagram authoring, UI, Input, the retained Lua Runtime, States, and Audio.
- **Chrome.** The Viewer already shows the manifest title, notes, transport, Related source, and series controls. Do not draw these controls, IDE chrome, file paths, or episode navigation on the Canvas. Exports contain only the Canvas, so the Canvas still carries its own question, values, and captions ([code-evidence.md](code-evidence.md#split-canvas-inspector-and-notes)).

## Inspector and navigation

- `NOTES` (the `description`) comes first, then one entry per `links` item in order. A newly loaded Scene opens `NOTES` and its first link. Reloading the same Scene, or a Theme change, keeps whatever the reader opened or closed. Any number of entries can stay open.
- Each source entry shows its path, focused range, an excerpt, and **SEE**. **SEE** reveals the exact range, with a collapsed cursor, in the editor group to the right of the Viewer. On a diff entry, **SEE** opens the native Diff Editor.
- `SHOW 5 LINES ABOVE` and `SHOW 5 LINES BELOW` extend an excerpt, and **Show Lines** changes the step to 5, 10, or 20.
- Editing a linked file refreshes open excerpts without recompiling the Scene. A range past the end of the file, a missing file, or an unverified diff shows as unavailable.
- In the settings menu, **Document → Lua** opens the Scene file and **Document → Source** opens the mapped definition.

## CodeLens and hover

A mapped definition gets CodeLens **Preview "<title>" animation**, and every link with `symbol`, `line`, or `range` gets **Preview related "<title>" animation**. Hovering the same place, or a use site that resolves to it through VS Code's definition provider, shows a text card. The card has a Preview link, plus *Definition* and one link per `links` label that jump to that source. The hover never renders the Scene.

## Series

The Series button between Previous and Next opens a two-level list: groups, then numbered episodes (`#01`, `#02`, …). In Wide, the list docks below Playback. Choosing an episode changes only the Viewer and Inspector, never the source editor. Previous and Next stay inside the group and stop at its ends. **Option → Auto-play Next** is off by default; when on, it overrides Loop and wraps from the last episode to the first, within the same group.

## Playback range and export

**RANGE → SET IN / SET OUT / CLEAR** sets an interval in the Viewer, not in the Scene. The seekbar highlights it, Restart seeks to its start, Loop wraps inside it, and playback without Loop stops at its end. The range survives a reload of the same Scene and clears when another Scene loads. The Scene's own timeline is unchanged.

Export from the Command Palette with **viscript: Save Current Viewer as PNG/GIF/MP4**. PNG saves the current frame. GIF and MP4 sample only the active range, at 24, 30, or 60 fps.

## Layout

Compact stacks the Canvas and Playback above the Inspector. Wide starts at 600 px and returns to Compact below 567 px, and **Option → Compact Layout** forces Compact. The sashes resize the player width and the Canvas height, and **RESET LAYOUT** restores automatic sizing. In FIT, the wheel zooms around the pointer and middle-drag pans; **CANVAS TO RAW SIZE** shows the Canvas at 1:1. Canvas Full Screen hides the Inspector, and Escape exits it. Right-clicking the Canvas opens the settings menu.

## Interactive Scenes

Author UI, Input, States, Runtime, and Audio with [vis-interactive](../../vis-interactive/SKILL.md). The Preview offers each Canvas pointer event to UI first and then to Input; only unhandled events fall through to camera and FIT gestures. With `camera = {mode = "interactive"}`:

- dragging pans in 2D or orbits in 3D, and Shift-drag pans;
- the arrow keys pan and `+`/`-` zoom;
- `2` and `3` switch views, and `R` resets.

Keep the first FIT frame complete for readers who never interact.

## IntelliSense

API completion, hover documentation, and signature help work in the Scene that the Viewer follows and in any Lua file that contains `vis.scene`. **viscript: Enable LuaLS Type Definitions** copies the generated `vis.d.lua` into extension storage and adds it to `Lua.workspace.library`. A workspace `.luarc.json` overrides that setting, so add the path there instead when one exists.

## Settings and commands

| Item | Purpose |
|---|---|
| `visPreview.updateDelay` | the reload delay after a Lua edit: default 280 ms. |
| viscript: Open Viewer | reveal or open the Viewer with the series list |
| viscript: Refresh Preview | reload the current Scene |
| viscript: Save Current Viewer as PNG/GIF/MP4 | export the loaded Scene |
| viscript: Open Bundled Runtime Folder | reveal the packaged WASM, fonts, and licenses |
| viscript: Enable LuaLS Type Definitions | register the generated Lua types with LuaLS |

## Troubleshooting

- **No CodeLens.** Fix the manifest diagnostics, check the path case and the `symbol` or `line`, turn on Editor › Code Lens, then reload the window.
- **The Scene opens without notes or links.** The file is not named by exactly one entry, or its path differs from the `animation` string.
- **The excerpt shows the wrong block.** Give the link a `range`, or a `line` for repeated names.
- **`DIFF UNAVAILABLE`.** See [diff-authoring.md](diff-authoring.md).
- **No suggestions.** Confirm the file is Lua and contains `vis.scene`, then reload after `npm run compile`.

## Verify before delivery

Run the vis-skills review (`inspect`, `render`, `audit`) on every Scene. Then run the host review, which renders a Scene as the Viewer does: the same WASM runtime and fonts, with the runtime's light and dark host colors. They approximate the default Light and Dark themes; a workbench theme's own colors can differ.

```sh
node <skill>/scripts/host-review.mjs .vscode/vis/<series>/animations/<scene>.lua --time 0 --time T --time end
```

`<skill>` is this skill's directory. The script prints one line per palette and time, and exits with 1 when a Text overlaps another, comes closer to it than `--padding` (4 px), or is clipped. Add `--contain TEXT BOX [INSET]` for Text that sits in a box: a `*` in TEXT matches any run of characters and fills the `*` in BOX, so `--contain 'node:*:name' 'node:*:body' 8` covers every node of a plate. The PNGs go to the system temp directory unless `--out` names another directory outside `.vscode/vis`; look at them, because the script checks text, not meaning. It loads the vis-skills bundle; `--runtime` points it at another copy, such as the folder **viscript: Open Bundled Runtime Folder** reveals.

Then check in VS Code:

- the manifest shows no diagnostics, and every CodeLens sits on its definition;
- each Inspector excerpt shows the intended block, and **SEE** lands on it;
- the first frame, the decisive beat, and the final frame read at a narrow Compact width, at Wide, and in Canvas Full Screen;
- in a series, Previous, Next, and the Series list follow the intended order.

Without a VS Code window, the host review still covers the Canvas. For the manifest, check its keys against [manifest.md](manifest.md), confirm that every `animation` exists, and print each linked range from the working tree to confirm it holds the intended block. Report the manifest diagnostics, CodeLens placement, **SEE**, and the Compact, Wide, and Full Screen widths as not verified.
