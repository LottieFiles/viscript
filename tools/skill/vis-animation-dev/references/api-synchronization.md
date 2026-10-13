# Cross-language API synchronization

Use this checklist whenever public scene construction, animation, rendering, saving, lifecycle, or error behavior changes.

## Sources of truth

1. Define the native type, ownership rule, validation, and `Result` behavior in `src/inc/vis.h` and the core implementation.
2. Map user-facing Lua spelling and table validation in `src/core/lua/visLua.cpp`.
3. Map JavaScript builder names, serialization, handles, and runtime methods in `src/bindings/js/bindings.js` and `src/bindings/js/client.js`.
4. Update `src/bindings/js/bindings.d.ts` and `src/bindings/js/client.d.ts` without widening types beyond runtime validation.
5. Update the C ABI and `src/bindings/wasm/visWasm.cpp` only when hosts need direct runtime access.

## Required audit

- Confirm defaults, units, coordinate space, accepted ranges, enum spellings, and failure atomicity match.
- Confirm snake_case Lua and camelCase JavaScript names describe the same operation.
- Confirm C++ raw-pointer ownership is represented safely by protected Lua/JavaScript handles.
- Check detached, attached, scheduled, removed, mounted, and failed-call states where relevant.
- Synchronize `src/inc/vis.h` with the public Lua and JavaScript references in `tools/skill/vis-skills/references/` whenever a portable user workflow changes. Keep repository-only paths in this developer skill; do not require a C++ authoring reference.
- Update public docs and representative examples; avoid duplicating an exhaustive API catalog in multiple places.

## Verification

Add tests at the lowest layer that owns the behavior, then binding tests for serialization and naming. Test success and at least one meaningful invalid call. For visual behavior, render at exact times around clip boundaries rather than relying only on the final frame. Run the native full suite and WASM build before committing.
