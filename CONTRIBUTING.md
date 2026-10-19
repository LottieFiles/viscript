# Contributing

Build and test a change with the modules it affects:

```sh
meson setup build -Dbundled_thorvg=true -Dmodules=all -Dengines=cpu
meson compile -C build
meson test -C build --print-errorlogs
```

For the VS Code extension, run `npm ci` and `npm test` in `tools/vscode`.

`meson test` also runs `api-sync`, which holds the editor's API schema,
`tools/vscode/api/vis-api.json`, to the Lua binding. After changing the binding, update the
schema and run `npm run generate:api` in `tools/vscode` to regenerate `vis.d.lua`.

## Comments and guides

Guides explain how to use the current engine. API comments describe ownership,
units, supported inputs, error results, and resource limits. Internal comments
explain decisions that the code alone does not make clear.

## Commit messages

Use `module: purpose` or `module/feature: purpose`, with a short lowercase prefix
from the affected area. Keep the subject short. Use the body to explain the
reason and relevant validation, with wrapped prose.

API changes list affected C++ and C signatures. Use `+` for additions, `-` for
removals, and `*` for changes, followed by the old and new signatures.

```text
api: introduce scene construction

C++:
 + static Scene* Scene::gen(const Config& config = {}) noexcept

CAPI:
 + VisEngine vis_create(void)
```

Keep a feature, its tests, and related corrections together so each commit
builds and can be tested on its own.
