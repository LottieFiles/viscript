# Manifest

`.vscode/vis/visualizations.json` binds product source to Scenes. The extension reads one manifest per workspace folder and reloads it whenever the file is created, changed, or deleted. A bundled JSON schema gives completion and flags unknown keys. Parser errors appear as `viscript Visualizations` diagnostics on the manifest, and only the invalid entries are dropped.

## Layout and paths

```text
<workspace folder>/.vscode/vis/
├── visualizations.json
└── <series>/animations/<scene>.lua
```

Every path is relative to the workspace folder and uses `/`. A path may not start with `/` or a drive letter or contain an empty, `.`, or `..` segment; a leading `./` is dropped. `animation` must be exactly `.vscode/vis/<series>/animations/<name>.lua`: a direct child, in the directory named by `series`. `series` is a single directory name. `asset = "..."` and `texture = "..."` in the Scene resolve next to the Scene file.

## Entry

```json
{
  "version": 2,
  "visualizations": [{
    "source": "src/raster/span.cpp",
    "symbol": "raster::clipSpan",
    "animation": ".vscode/vis/raster/animations/clip-span.lua",
    "series": "raster",
    "title": "Clipping one span",
    "description": "Traces `clipSpan` for a span that crosses the right edge.",
    "links": [
      {"label": "Edge test", "source": "src/raster/span.cpp", "range": {"startLine": 41, "endLine": 52}},
      {"label": "Write the clipped span", "source": "src/raster/write.cpp", "symbol": "raster::writeSpan"}
    ]
  }]
}
```

| Field | Required | Drives |
|---|---|---|
| `version` | yes, `2` | — |
| `source`, `symbol` | yes | CodeLens at the definition, the hover card's *Definition* link, **Document → Source**, and use-site hover matching. The definition is not listed in the Inspector. |
| `line` | no | a one-based line that pins `symbol` when the name repeats or document symbols are unavailable |
| `animation`, `series` | yes | the Scene path and its group |
| `title` | no | Viewer title, CodeLens and hover text, and episode label; defaults to `symbol` |
| `description` | no | the text under `NOTES`, in a small Markdown subset: paragraphs, `-` and `1.` lists of one-line items, `#` to `####` headings, `>` quotes, `---`, fenced code, inline code, `**bold**`, `*emphasis*`, and `[text](https://…)` links. Anything else, HTML included, shows as typed. The default names the symbol and its file path, so write one. |
| `links` | no | Inspector entries in array order. A link with `symbol`, `line`, or `range` also gets its own CodeLens and hover in its file. |

Each link requires `label` and `source` and may add `symbol`, `line`, `column`, `range`, and `diff`. A `range` has one-based inclusive `startLine` and `endLine`, plus optional one-based `startColumn` and exclusive `endColumn`, which must appear together. A `diff` requires a `range`; see [diff-authoring.md](diff-authoring.md). No other keys exist.

## How a location resolves

Every location resolves in the same order: an explicit `range`, then `line` (with `column`), then the document-symbol range for `symbol`, then a text search for the symbol's last name segment. In the text search, a `function name` line or a `name(` line wins. The resolved location decides what the reader sees:

| Given | Inspector excerpt | CodeLens and hover anchor |
|---|---|---|
| `range` | exactly that block | the block |
| `line` | that one line | the symbol name on that line |
| `symbol` only | the whole symbol when a language server reports it, otherwise the matched line; up to 240 lines | the symbol name |
| none of them | the file from line 1, up to 240 lines | none |

Give evidence links a `range`. Use `symbol` alone only when the whole function is the point and the file's language server provides document symbols, and use `line` to pin the primary definition. Symbol matching compares the last segment after `::`, `.`, or `:` and takes the first match in the file, so a repeated name needs `line`. A range past the end of the file shows as unavailable instead of a guess. Recheck ranges whenever the source changes.

## Matching

- **Scene → entry.** Opening or following a Lua file restores its title, notes, links, and series only when exactly one entry names it.
- **Definition → entry.** CodeLens and hover appear at `source`/`symbol` and at each link location, in that file.
- **Use site → entry.** Hovering a call asks VS Code's definition provider, and the entry whose `source` or link files contain the definition wins. Without definition data, the bare name must match exactly one entry.
- **Series.** Entries that share a `series` form one group in manifest order, and groups appear in order of first appearance. **viscript: Open Viewer** lists the series of the active editor's workspace folder, or of the first folder.

## Check

- The manifest shows no diagnostics, and every `animation` file exists. A missing file reports `viscript animation not found`.
- Each CodeLens sits on the intended definition, and each Inspector excerpt shows the intended block.
- Episode titles read as a sequence in the Series list.
