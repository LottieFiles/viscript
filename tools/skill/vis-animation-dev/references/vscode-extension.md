# Developing the VS Code extension

Use this guide for extension source changes. Read [vis-vscode](../../vis-vscode/SKILL.md) for the viewer's user workflow.

## Source map

| Source | Responsibility |
|---|---|
| `src/extension.ts` | Activation, commands, providers, diagnostics |
| `src/languageProvider.ts` | Lua completion, hover, signature help |
| `src/visualizationProvider.ts` | Manifest loading, CodeLens, source context |
| `src/visualizationManifest.ts` | Workspace manifest parsing and validation |
| `src/symbolLocator.ts` | Document symbols and text fallback |
| `src/previewTarget.ts` | Preview command target resolution |
| `src/previewPanel.ts` | Webview lifecycle, scene payloads, assets, navigation, export |
| `src/gitSnapshots.ts`, `src/unifiedDiff.ts` | Verified Git snapshots and focused diffs |
| `media/main.js`, `media/main.css` | Viewer behavior and styling |
| `../player` (bundled into `media/player/`) | Scene playback: compiling, drawing, input, the camera, sound, frame capture |
| `media/time.mjs`, `media/playback-range.mjs` | Playback time and range model |
| `media/series-step.mjs` | Group navigation |
| `media/source-inspector.mjs`, `media/markdown.mjs` | Source rows and escaped notes |
| `media/viewer-layout.mjs` | Responsive layout |
| `webview/export-plan.mjs`, `webview/savers.js` | Export sampling, frame timing and encoding |
| `schemas/visualizations.schema.json` | Manifest editor validation |

Paths above are relative to `tools/vscode/`.

## Data flow

The provider loads `.vscode/vis/visualizations.json`, resolves a definition or use site, and opens its scene through `PreviewPanel`. Related links resolve range, line/column, then symbol context. The viewer receives escaped excerpts and display metadata; the host retains source locations for native navigation.

The same panel handles active-Lua following, named series, source excerpts, and Git diffs. Keep one player and Inspector across Compact and Wide layouts. Source excerpts update independently of scene recompilation. **SEE** and **Document → Source** drive native editor navigation.

Use source and manifest API comments for validation, location precedence, snapshot requirements, and message ownership.

## Change workflow

- Update parser, schema, tests, and [manifest reference](../../vis-vscode/references/manifest.md) together for manifest changes.
- Update matching host/Webview message handlers together. Validate host-owned indices before resolving source context.
- Preserve escaped source text, command-link trust, and the Content Security Policy.
- Use VS Code semantic tokens for chrome, menus, focus, code, and diffs. Rebuild adaptive scenes when the editor palette changes; retain authored fixed themes.
- Reuse existing DOM and state when changing responsive layout. Preserve excerpt and Inspector scroll positions through refreshes.
- Keep playback ranges in `playback-range.mjs`. Playback, Restart, Loop, and export read the same range.
- Canvas input, the camera, sound and frame capture belong to the shared player in `tools/player`, which the documentation site and the Obsidian plugin use too. Change them there, with its unit and browser tests; the viewer sets the player's options (`camera`, `rate`, `end`) and each scene's `interactive`, and follows its events.
- Update README and [extension reference](../../vis-vscode/references/extension.md) when a visible workflow changes.

The portable runtime uses CPU RGBA rendering. Synchronize its complete artifacts and manifests when rebuilding, and synchronize generated Lua metadata after API schema changes. Native renderer selection and ownership rules belong beside the engine API declarations.

## Verify

Run from `tools/vscode/`, after `npm ci --prefix ../player` once, since the build bundles the player:

```sh
npm run check
npm test
npx vsce ls
npm run package
```

`npm test` compiles the host/Webview, verifies API metadata, exercises manifest and viewer utilities, export planning, and runtime seeking. Add focused cases for the behavior changed. Inspect responsive modes, range boundaries, series navigation, keyboard focus, source links, and reload recovery in an Extension Development Host when UI behavior changes.

Package inspection should include compiled providers, schema, media, runtime, fonts, and license notices. Remove stale `out/` files after deleting TypeScript sources before packaging.
