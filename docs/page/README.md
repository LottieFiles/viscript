# viscript website

A thin HTML shell (`index.html`, `app.js`) that renders MDX pages. Three routes:

| Route | Source |
|---|---|
| Home `#/` | `content/index.mdx` |
| Docs `#/docs/<id>` | `content/docs/NN-<id>.mdx` (frontmatter: `title`, `group`, `summary`) |
| Examples `#/examples/<id>` | `content/examples.mdx` + `scenes/collections.js` |

A ` ```lua live ` fence becomes an editable player. MDX pages may use the
components in `components.js`: `Scene`, `Hero`, `Features`, `Feature`, `Gallery`.

```sh
npm ci --prefix docs/page
node scripts/build-page.mjs   # content/**/*.mdx -> build/*.js
node scripts/check-page.mjs   # load and render every scene against the WASM runtime
python3 -m http.server -d docs/page
```

`jsx-runtime.js` turns compiled MDX straight into DOM nodes; there is no framework.
`favicon.svg` is the logo. `runtime/` is refreshed by `scripts/wasm-bundle.sh`.
