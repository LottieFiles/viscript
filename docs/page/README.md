# viscript website

A thin HTML shell (`index.html`, `app.js`) that renders MDX pages. Three routes:

| Route | Source |
|---|---|
| Home `#/` | `content/index.mdx` |
| Docs `#/docs/<id>` | `content/docs/NN-<id>.mdx` (frontmatter: `title`, `group`, `summary`) |
| Examples `#/examples/<id>` | `content/examples.mdx` + `scenes/collections.js` |

A ` ```lua live ` fence becomes an editable player. MDX pages may use the
components in `components.js`: `Scene`, `Hero`, `Feature`, `BubbleSortDemo`, `SkillsDemo`, `Gallery`.

```sh
npm ci --prefix docs/page
node scripts/build-page.mjs   # content/**/*.mdx -> build/*.js
npm run check --prefix docs/page  # render every scene and validate interactive playback ranges
python3 -m http.server -d docs/page
```

For GitHub Pages, set **Settings → Pages → Build and deployment → Source** to
**GitHub Actions**. The [Documentation workflow](../../.github/workflows/pages.yml)
builds the MDX and deploys `docs/page` on matching pushes to `viscript`, or when
manually run on that branch. Pull requests run the build and scene checks.

`jsx-runtime.js` turns compiled MDX straight into DOM nodes; there is no framework.
`favicon.svg` is the logo. `runtime/` is refreshed by `scripts/wasm-bundle.sh`.
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
