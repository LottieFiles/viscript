# viscript website

One website with a shared HTML shell (`index.html`, `app.js`). Docs use MDX; the API article content is compiled from Obsidian-compatible Markdown by Quartz. Four routes:

| Route | Source |
|---|---|
| Home `#/` | `content/index.mdx` |
| Docs `#/docs/<id>` | `content/docs/NN-<id>.mdx` (frontmatter: `title`, `group`, `parent`) |
| API `#/api/<id>` | `../api/content/**/*.md` → Quartz HTML fragments |
| Examples `#/examples/<id>` | `content/examples.mdx` + `scenes/collections.js` |

A ` ```lua live ` fence becomes an editable player. MDX pages may use the
components in `components.js`: `Scene`, `Hero`, `Feature`, `BubbleSortDemo`, `SkillsDemo`, `Gallery`.

```sh
npm ci --prefix docs/page && npm ci --prefix docs/api && npm ci --prefix tools/player
node scripts/build-page.mjs   # MDX + Quartz API content + shared player -> build/
npm run check --prefix docs/page  # render every scene and validate interactive playback ranges
node scripts/check-page-api.mjs  # check the API catalog against Lua registrations and JS declarations
node --test scripts/page-resources.test.mjs  # the fonts and images the page offers
npm start --prefix docs/page  # build, then serve the page at http://localhost:8000
```

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
builds the MDX and deploys `docs/page` on matching pushes to `viscript`, or when
manually run on that branch. Pull requests run the build and scene checks.

`jsx-runtime.js` turns compiled MDX straight into DOM nodes; there is no framework.
The API navigation follows each Markdown page's `parent` metadata as a collapsible page tree.
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
