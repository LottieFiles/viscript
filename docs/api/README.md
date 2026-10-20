# API Markdown

The API lives in the existing VISCRIPT website at `#/api`. Quartz compiles its
article content; the site keeps its header, navigation, page tree, theme and
shared animation player. There is one website and one deployment.

Open [`content/`](content) as an Obsidian vault. Each category has an `index.md`
and focused child pages. Use ordinary relative Markdown links or Obsidian
wikilinks. The Viscript Obsidian plugin renders the scene fences in the vault.

To build the plugin from the checkout and prepare this vault, run from the repository root:

```sh
npm ci --prefix tools/player
npm ci --prefix tools/obsidian
npm run vault:setup --prefix docs/api
```

In Obsidian, choose **Open folder as vault** and select `docs/api/content`.
On a fresh vault, setup opens **Shapes** in reading view, with the overview in another tab and
the graph beside it. **Show source** defaults to enabled for the API vault, so scenes appear
with their scripts. Use the player's code button or the plugin's **Show source**
setting to hide them. Lua/JS counterparts share one player with language buttons.
Viscript is installed and listed as enabled. On the first
open in a new Obsidian profile, choose **Trust author and enable plugins**.
The plugin includes its WASM runtime and fonts; no additional downloads are needed
for these scene examples. Repeating setup updates the plugin and keeps the existing
workspace and settings.

Audio examples use the bundled `content/assets/tone.wav` and the steady
`content/assets/gain.wav` for gain fades. In Obsidian, click **Play**
for timeline cues, or focus an interactive scene and press its indicated key. The
browser runtime supports WAV audio; a file is found by the same relative lookup
Obsidian uses for links.

Each page has stable navigation metadata:

```yaml
---
title: Shapes
id: shapes
parent: object
order: 1
---
```

Keep the reference tables concise: Lua signature, JavaScript signature, and the
function's purpose. A dash means that language does not expose the function.

Examples are complete source inside Markdown, with an ID comment for the viewer
and coverage check. A JavaScript counterpart immediately follows its Lua fence:

````markdown
### scene.circle

<!-- api-example: scene.circle -->

```viscript
local scene = vis.scene {}
scene:circle {radius = 1, fill = "accent"}
return scene
```

```viscript-js
const scene = vis.scene();
scene.circle({radius: 1, fill: "accent"});
return scene;
```
````

Keep static examples free of timeline padding. Use `wait` only when timing is
part of the API being explained. Any extra animation should demonstrate the
feature in one line per language.

Use `viscript interactive` for examples that accept input. Related APIs can share
one example: add `<!-- api-covers: scene.fade_in scene.fade_out -->` after the ID
comment. The same example ID repeated on another page must have identical source.
Host examples use an ordinary `javascript` fence after the ID comment, with the
variables documented in [Shared Host Setup](content/Host/setup.md).

Build and serve the whole website from the repository root (Node 22+):

```sh
npm ci --prefix tools/player
npm ci --prefix docs/page
npm ci --prefix docs/api
npm start --prefix docs/page
```

Open `http://localhost:8000/#/api`. After editing Markdown, rebuild with
`npm run build --prefix docs/page`. Run `npm run check --prefix docs/page` to
check API coverage against the bindings and render every published Lua/JS scene.

`quartz.config.ts` selects the Markdown transformers, `plugins/viscript.ts` marks
live scene fences, and `render-content.ts` emits article fragments, page metadata
and `graph.json`. The graph follows Markdown links and the `parent` hierarchy,
with categories grouped by the page tree. `scripts/build-api.mjs` places these
in `docs/page/build/api/` for the
existing router. API examples use the same render worker and runtime bundle as
the rest of the site.

Quartz is pinned to the official [v4.5.2 source](https://github.com/jackyzha0/quartz/tree/v4.5.2)
and verified by `package-lock.json`. Installation links its engine into the
workspace; dependencies, generated HTML and build caches are ignored by Git.
