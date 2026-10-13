---
name: vis-animation-dev
description: "Change viscript itself from a source checkout: C++ engine, renderer, Lua/JS/WASM bindings, Meson builds, tests, examples, and the VS Code extension source. Not for writing scenes (vis-skills)."
---

# viscript Animation Development

Use this skill only inside a viscript source checkout. Repository paths, local build directories, internal tests, and implementation details belong here; never copy those assumptions into the distributable `vis-skills` authoring skill.

## Workflow

1. Identify the affected public surface: C++, Lua, JavaScript/TypeScript, C/WASM, renderer/export, or documentation.
2. Before changing scene behavior, read the sibling [vis-skills skill](../vis-skills/SKILL.md) and the relevant language reference. It defines the user-facing authoring contract that must remain coherent.
3. Read [references/repository-workflow.md](references/repository-workflow.md) for source locations, build variants, targeted tests, render checks, and repository example work.
4. For any public API or lifecycle change, read [references/api-synchronization.md](references/api-synchronization.md) and update every affected binding, declaration, document, skill reference, and test in the same change.
5. For VS Code extension source under `tools/vscode/`, read [references/vscode-extension.md](references/vscode-extension.md); use the `vis-vscode` skill for host behavior it must preserve.
6. Prefer a focused numeric, lifecycle, binding, or render invariant over a test that matches formatting or prose.

## Contributor invariants

- Treat `src/inc/vis.h` as the C++ public contract, `src/core/lua/visLua.cpp` as the Lua mapping, and `src/bindings/js/bindings.js` plus `src/bindings/js/bindings.d.ts` as the typed JavaScript builder contract.
- Preserve ownership and sealing semantics across languages: successful attachment transfers ownership; timeline scheduling captures explicit state; Viewport and Scene-transition attachment seals and transfers child Scenes.
- Keep rendering deterministic at arbitrary sample times. Authoring callbacks may materialize retained data, but renderers must not execute user callbacks.
- Keep the public `vis-skills` skill portable. It may mention installed modules or a CLI command available on `PATH`, but it must not require this repository's `build*`, `examples/`, `src/`, `src/test/`, or absolute filesystem paths.
- Put frontend availability, ownership, validation, and resource limits beside API declarations. Keep user documents focused on working authoring paths.
- Run the smallest relevant checks while iterating, then the full configured test suite and every affected native/WASM build before delivery.
