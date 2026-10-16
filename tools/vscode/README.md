# viscript Preview

Preview viscript Lua scenes beside their source in Visual Studio Code. The bundled WebAssembly renderer runs locally in a Webview.

## Install

Build the VSIX from the repository and install it:

```sh
cd tools/vscode
npm ci
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
local scene = vis.scene {
    width = 960, height = 540, fps = 30, loop = false,
    theme = "adaptive_vscode",
    camera = {view = "2d", height = 7},
}
local circle = scene:circle {radius = 1, fill = "accent", id = "circle"}
scene:grow_from_center(circle, 0.6)
scene:wait(1)
return scene
```

## Viewer

Compact places the canvas and playback above the Inspector. Wide places the Inspector beside them. Drag the dividers to resize the player, use **Option → Compact Layout** to choose Compact, and **RESET LAYOUT** to restore automatic sizing.

**CANVAS TO FIT WINDOW** scales the scene uniformly. In FIT, the wheel zooms around the pointer and middle-drag pans. **CANVAS TO RAW SIZE** uses 1:1 display pixels. These display changes preserve export dimensions. Canvas Full Screen hides the Inspector; Escape exits, and right-click opens settings.

Playback includes pause, seek, restart, loop, and speed. **Range → Set In / Set Out / Clear** selects a playback interval. Restart and Loop use that interval, and GIF/MP4 export samples it. The range survives a valid reload of the same scene.

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

Interactive-camera scenes support drag to pan in 2D or orbit in 3D, Shift-drag to pan, arrows to pan, `+`/`-` to zoom, `2`/`3` to switch views, and `R` to reset. The viewer routes controls first and releases held input on focus loss.

Relative `asset` and `texture` paths resolve beside the Lua file. The bundled runtime loads SVG and PNG images and WAV audio; JPG, WebP, MP3 and FLAC need a native build. The viewer registers Pretendard, Source Serif 4, IBM Plex Sans KR, and JetBrainsMono. `adaptive_vscode` uses the editor palette and semantic fonts; explicit scene themes and fonts remain authored choices.

## Completion and export

Built-in completion, hover documentation, and signature help work for `vis.scene` files. **viscript: Enable LuaLS Type Definitions** registers generated metadata with Lua Language Server. If the workspace uses `.luarc.json`, add the displayed metadata directory there.

Export from the Command Palette with **viscript: Save Current Viewer as PNG/GIF/MP4**. PNG saves the displayed frame; GIF and MP4 sample the playback range at 24, 30, or 60 fps. Encoding runs locally and reports progress.

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
npm ci
npm run compile
npm test
```

Press `F5` from the extension folder to launch an Extension Development Host. Use `npm run package` to create a VSIX. Runtime checksums and native build provenance are recorded in `runtime/BUILD-MANIFEST.json`.
