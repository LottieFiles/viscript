# Documentation

The [viscript journal](page/index.html) is the GitHub Pages frontend for these
documents, with executable Lua/JavaScript guides, visual essays, and editable examples.
See [website maintenance](page/README.md) for preview, validation, and deployment.

The [API reference](page/index.html#/api) describes each exposed function. Its
[Markdown source vault](api/content/) opens in Obsidian; see [API authoring](api/README.md).

## Author a scene

- [Scene composition](scene-composition.md): objects, coordinates, timeline, cameras, and transitions
- [Themes](themes.md): presets, semantic colors, and gradients
- [TeX math](tex.md): formulas, parts, and matching
- [Cells and voxels](cell-voxel.md): sampled color fields
- [Lua API](../tools/skill/vis-skills/references/lua.md)
- [JavaScript API](../tools/skill/vis-skills/references/javascript.md)

## Review and run

- [Layout audit](layout-audit.md): clipping, overlap, containment, and fonts
- [States](states.md): named object states on the interaction clock
- [Audio](audio.md): cues, playback, and offline mixing

## Understand and build

- [Architecture](architecture.md): scene sampling and retained rendering
- [Modules](modules.md): capabilities, libraries, and build combinations

The website's guides live as MDX in `docs/page/content/`; see `docs/page/README.md`.
