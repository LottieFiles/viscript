# viscript website

The website uses a shared HTML shell (`index.html`, `app.js`) and MDX guides.
The public build includes Home, Docs and Examples. An explicit local build adds
the API reference, compiled from Obsidian-compatible Markdown by Quartz:

| Route | Source |
|---|---|
| Home `#/` | `content/index.mdx` |
| Docs `#/docs/<id>` | `content/docs/NN-<id>.mdx` (frontmatter: `title`, `group`, `parent`) |
| API `#/api/<id>` (local build only) | `../api/content/**/*.md` → Quartz HTML fragments |
| Examples `#/examples/<id>` | `content/examples.mdx` + `scenes/collections.js` |

A ` ```lua live ` fence becomes an editable player. MDX pages may use the
components in `components.js`: `Scene`, `Hero`, `Feature`, `BubbleSortDemo`, `SkillsDemo`, `Gallery`.

```sh
npm ci --prefix docs/page && npm ci --prefix tools/player
npm run build --prefix docs/page  # public MDX + shared player -> build/, excluding API
npm run check --prefix docs/page  # render every scene and validate interactive playback ranges
node scripts/check-page-api.mjs  # check the API catalog against Lua registrations and JS declarations
node --test scripts/page-resources.test.mjs  # the fonts and images the page offers
```

To open the API locally, start the server explicitly:

```sh
npm ci --prefix docs/api
npm start --prefix docs/page  # build with API, then serve at http://127.0.0.1:8000
```

Stop the server when finished. `npm run build:local --prefix docs/page` (or
`node scripts/build-page.mjs --with-api`) rebuilds the local API without starting
a server. The default build removes previous `build/api/` output and exports an
empty API catalog, so a later public build cannot retain local API articles.

Scenes play through the shared player in [`tools/player`](../../tools/player): one render
worker draws every figure on the page, and `player.js` adds the editor, controls, language
switch and light/dark colours on top. `resources.js` names the font the page registers under
each name scenes set text in, and the images it offers. As in ThorVG's WebCanvas, the worker
asks for a font when a scene first sets text in it; text in a font the page does not have is
drawn in the engine's default font. "Pretendard" is Pretendard Std until a scene has letters
only the full Pretendard has. The runtime and the first figures' fonts are preloaded by
`index.html` on routes that open on a figure.

For GitHub Pages, set **Settings → Pages → Build and deployment → Source** to
**GitHub Actions**. The [Documentation workflow](../../.github/workflows/pages.yml)
builds the public MDX and deploys `docs/page` on matching pushes to `viscript`, or
when manually run on that branch. It does not install Quartz or publish API
articles. Pull requests run the public build and scene checks.

`jsx-runtime.js` turns compiled MDX straight into DOM nodes; there is no framework.
In the local build, API navigation follows each Markdown page's `parent`
metadata as a collapsible page tree.
[`../api/content`](../api/content) is the API's source vault: ordinary `.md` files,
relative Markdown links, and complete `viscript` / `viscript-js` scene fences. Open
that folder directly in Obsidian. See [API authoring](../api/README.md) for the format.
Quartz generates only article fragments and a manifest under `build/api/`; the
same site header, routes, sidebar, theme and shared animation player display them.
Quartz also emits `graph.json` from the Markdown links and page hierarchy. The
API overview shows the full graph; each page shows its neighborhood beside the
contents. Nodes open API pages, and the expanded graph supports dragging and zoom.
The API viewer groups each topic's fences into one example selector. Host snippets
use JavaScript fences and run through `host-example.js`. The API check compares
function tables to Lua registrations and JS declarations, then samples the actual
Markdown scenes and their JavaScript equivalents with the shipped runtime.
`favicon.svg` is the logo. `runtime/` is not tracked: `scripts/build-page.mjs` copies it from
the skill's bundle (`tools/skill/vis-skills/assets/wasm/`, refreshed by `scripts/wasm-bundle.sh`),
with Pretendard Std from `src/examples/assets/`.
The homepage hero uses the embedding animation from `../assets/embed.lua` in
`scenes/hero.js`: one Lua scene and renderer feeding Native app, Web page, and VS Code.
Below it, three separate sections host the Interactive collection's bubble sort,
the README Skills animation with its editable Lua player, and the VS Code review MP4.

The ThorVG collections load editable Lua from `scenes/thorvg/` on selection. Their
scenes are migrated from the supplied `shapestroke5.lua` and `gitpage` renderer
notes; each file names its source, and existing trace revision comments are
retained. Structural colors use the same theme roles as the other examples and
follow the site's light/dark switch. Measured readback pixels retain their original
colors on paper surfaces inside the diagram. Computer Modern fonts and their license live in `assets/thorvg/`.
`ThorVG & Interaction` preserves gitpage's clickable stage headers and shape cards:
each selection plays its authored time range once and holds the final frame. The
homepage's Embed section contains the ThorVG VS Code review MP4 linked from the root README.
