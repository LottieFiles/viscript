# Contributing

Build and test a change with the modules it affects:

```sh
meson setup build -Dbundled_thorvg=true -Dmodules=all
meson compile -C build
meson test -C build --print-errorlogs
```

For the VS Code extension, run `npm ci`, `npm ci --prefix ../player`, and `npm test`
in `tools/vscode`; its build also bundles the shared player.

`meson test` also runs `api-sync`, which holds the editor's API schema,
`tools/vscode/api/vis-api.json`, to the Lua binding and checks the LuaLS definitions made from
it. After changing the binding, update the schema. The definitions, `api/vis.d.lua`, are not
tracked: `npm run compile` in `tools/vscode` writes them, and so does packaging the extension.

## Comments and guides

Guides explain how to use the current engine. API comments describe ownership,
units, supported inputs, error results, and resource limits. Internal comments
explain decisions that the code alone does not make clear.

Each source file opens with a comment that says what it holds. `visLua.cpp` and `visScene.cpp`
list their sections at the top; each section is marked `// ---------- name ----------`.

## Names

Files are `vis` and what they hold, in CamelCase: `visScene.cpp`, `visLuaValues.h`. A module
under `src/core/<module>/` has `visX.cpp` for the engine side, `visXLua.cpp` for its Lua
binding, and `visXLuaExtension.h` for what the binding shares with the core loader.

Functions and structs that one file uses are `static` and start with an underscore
(`_options`, `_fieldNumber`); helpers shared through a private header are `static inline`.
Functions a header declares for other files take no underscore (`fingerprint`, `diffMatch`,
`luaOpenApi`). Structs private to the Lua binding start with `Lua` (`LuaScene`, `LuaSpan`), and
the private side of a public type is its `Impl`.

The core writes limits and messages as `constexpr` constants in capitals (`LUA_CLIP_LIMIT`,
`TIMELINE_UNBORN`); the modules and the public header use CamelCase (`SourceLimit`,
`Scene::RuntimeLimit`). Keep to the style of the file you are in. Public types are made with a
static `gen()` and report failure as a `Result` or a null pointer; public methods are `noexcept`.

## The standard library

The engine is built without exceptions and RTTI and owns its memory in arrays it grows itself:
allocate with `new (std::nothrow)`, report a null result as `Result::OutOfMemory` or a null
object, and free in the owner's destructor. The standard library serves what has no failure of
its own: `<cmath>`, `<cstring>`, `<limits>`, `<algorithm>`, and `std::unique_ptr` for an array
a function frees before it returns. Containers, strings and streams stay out of `src/core`,
`src/inc` and `src/cli`; tests and tools may use them.

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
