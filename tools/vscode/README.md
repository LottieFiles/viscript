# viscript Preview

Preview viscript Lua scenes beside their source in Visual Studio Code. The bundled WebAssembly renderer runs locally in a Webview.

## Install

Build the VSIX from the repository and install it:

```sh
cd tools/vscode
npm ci && npm ci --prefix ../player            # the player the viewer bundles
npm run package                                # vis-preview-<version>.vsix
code --install-extension vis-preview-*.vsix
```

Requires VS Code 1.96 or newer.

## Start

1. Open a Lua file containing `vis.scene`.
2. Click the viscript icon in the editor title bar.
3. Edit the scene; the viewer refreshes the unsaved document.

**viscript: Open Viewer** opens the viewer with the workspace's series catalog. While open, it follows the active Lua tab. Failed reloads keep the last valid scene and report diagnostics.

```lua
local scene = vis.scene {camera = {height = 7}}
local circle = scene:circle {fill = "accent", id = "circle"}
scene:grow_from_center(circle, {duration = 0.6})
scene:wait(1)
return scene
```

## Viewer

Compact places the canvas and playback above the Inspector. Wide places the Inspector beside them. Drag the dividers to resize the player, use **Option → Compact Layout** to choose Compact, and **RESET LAYOUT** to restore automatic sizing.

The canvas fits the player at the scene's aspect ratio; exports keep the scene's own size. A click on the canvas plays or pauses the scene, and a right click opens settings. The viewer plays scenes with the same player as the documentation site and the Obsidian plugin.

Playback includes pause, seek, restart, loop, and speed. **Range → Set In / Set Out / Clear** selects a playback interval. Restart and Loop use that interval, and GIF/MP4 export samples it. An edit of the same file keeps the time, the range, and whether the scene plays; another file starts from the beginning.

The Series button lists groups and numbered episodes. Previous and Next navigate the active group. **Option → Auto-play Next** advances cyclically inside it and takes precedence over Loop. Choosing an episode updates the viewer and Inspector; **SEE** navigates source explicitly.

## Source-linked scenes

Place the manifest and scenes under the workspace:

```text
.vscode/vis/
├── visualizations.json
└── raster/animations/clip-span.lua
```

```json
{
  "version": 2,
  "visualizations": [{
    "source": "src/raster/span.cpp",
    "symbol": "raster::clipSpan",
    "animation": ".vscode/vis/raster/animations/clip-span.lua",
    "series": "raster",
    "title": "Clipping one span",
    "description": "Shows how the right edge clips a span.",
    "links": [{
      "label": "Edge test",
      "source": "src/raster/span.cpp",
      "range": {"startLine": 41, "endLine": 52}
    }]
  }]
}
```

Paths are relative to the workspace. Entries with the same `series` appear together in manifest order. Source definitions and related links receive a Preview CodeLens and hover link. A `line` pins repeated symbol names; a focused `range` gives the Inspector an exact excerpt.

The Inspector shows description Markdown under **NOTES**, followed by source links. A newly loaded scene opens **NOTES** and its first link. Each excerpt has **SEE** for native editor navigation, and **Show Lines** expands its context. Editing a linked file refreshes the excerpt. **Document → Lua** opens the scene; **Document → Source** opens its mapped definition.

A related link can show a Git comparison:

```json
{
  "label": "Ownership moves to the caller",
  "source": "src/raster/span.cpp",
  "range": {"startLine": 41, "endLine": 58},
  "diff": {
    "base": "1111111111111111111111111111111111111111",
    "head": "2222222222222222222222222222222222222222"
  }
}
```

Use full commit SHAs from `git rev-parse`; for a rename, add the previous path as `diff.oldPath`. The viewer verifies source snapshots before showing inserted and removed rows. **SEE** opens VS Code's Diff Editor. Manifest diagnostics identify invalid entries.

## Interaction and assets

The bundled runtime includes Diagram, TeX, UI, Input, retained Lua Runtime, States, and Audio. UI supplies buttons, toggles, sliders, and rendered sampling. Input maps logical pointer/key events to objects and cameras. Retained Lua supplies fixed-step decisions, and States supplies named object states.

```lua
local input = vis.input.controller(scene)
input:key_move {target = player, key = "D", shift = {0.5, 0}, period = 0.12}
```

In interactive-camera scenes, a drag pans in 2D or orbits in 3D, Shift-drag pans, and the wheel zooms; once the canvas has focus, arrows pan, `+`/`-` zoom, `2`/`3` switch views, and `R` resets. **Camera → CONTROL OFF** leaves the camera to the menu's 2D, 3D, and RESET, and the canvas plays and pauses instead.

Scenes that use UI or Input take the canvas's pointer events, and its keys once it has focus (click it first), while their timeline plays. A retained runtime steps while the scene plays and holds while it is paused. Held input is released when the canvas loses focus. Sound starts after the first click or key press in the viewer.

Relative `asset` and `texture` paths resolve beside the Lua file. The bundled runtime loads SVG and PNG images and WAV audio; JPG, WebP, MP3 and FLAC need a native build. The viewer has Pretendard, Source Serif 4, and JetBrainsMono, and registers each for the scenes that set text in it. `adaptive`, the theme of a scene that names no other, takes the editor palette, and the viewer sets its headings and body text in Source Serif 4, with Korean in Pretendard; explicit scene themes and fonts remain authored choices.

## Completion and export

Built-in completion, hover documentation, and signature help work for `vis.scene` files. **viscript: Enable LuaLS Type Definitions** registers generated metadata with Lua Language Server. If the workspace uses `.luarc.json`, add the displayed metadata directory there.

Export from the Command Palette with **viscript: Save Current Viewer as PNG/GIF/MP4**. PNG saves the frame shown at the scene's size times the display's pixel ratio, as far as 3840×2160 pixels allow; GIF and MP4 sample the playback range at 24, 30, or 60 fps. Encoding runs locally and reports progress.

| Command or setting | Purpose |
|---|---|
| viscript: Open Viewer | Open or reveal the viewer |
| viscript: Open Preview to the Side | Preview the active Lua file (editor title icon) |
| viscript: Refresh Preview | Reload the current scene |
| viscript: Open Bundled Runtime Folder | Reveal runtime files, fonts, and licenses |
| viscript: Enable LuaLS Type Definitions | Register generated Lua metadata |
| `visPreview.updateDelay` | Reload debounce in milliseconds; default 280 |

## Development

From the repository root:

```sh
cd tools/vscode
npm ci && npm ci --prefix ../player
npm run compile
npm test
```

Press `F5` from the extension folder to launch an Extension Development Host. Use `npm run package` to create a VSIX. Runtime checksums and native build provenance are recorded in `runtime/BUILD-MANIFEST.json`.
