---
name: vis-game
description: "Build interactive viscript scenes and small games in retained Lua: UI, Input, fixed-step Runtime, Audio, and Motion state overlays. Not for authored timeline animation (vis-skills)."
---

# viscript Game

Build retained-Lua game loops against the delivery host. Start with a deterministic, readable scene and verify every interaction.

Route general authored animation to [vis-skills](../vis-skills/SKILL.md), VS Code hosts to [vis-vscode](../vis-vscode/SKILL.md), and engine, bindings, or build work to [vis-animation-dev](../vis-animation-dev/SKILL.md).

First apply the [viscript scene contract](../vis-skills/references/scene-contract.md) and [Lua API](../vis-skills/references/lua.md) to the authored base Scene. Then read [references/interaction-model.md](references/interaction-model.md) for UI/Input routing and coordinates and [references/review.md](references/review.md) for native input, retained Runtime, Audio, and delivery checks.

## Canonical contract

- UI, Input, Runtime, Audio, and Motion are independent sidecars; guard every used Lua namespace with its exact build-option message from the references. UI/Input never depend on one another; Runtime needs neither UI, Audio, nor Motion, and only its optional action bridge needs Input.
- In combined hosts, route pointers UI-first, forward only unhandled events to Input, preserve UI capture, and broadcast cancellation. Regions are top-left-origin logical Scene pixels; keyboard motion and cameras remain Input-owned.
- Sample input once per host frame. Consume transient edges, delta, and wheel only in the first fixed step; preserve held state for all steps. Runtime uses fixed-step catch-up with atomic overlays.
- Runtime opacity multiplies authored opacity: pre-author future-visible objects at opacity one off-frame. `ctx:sound` queues events; it is not playback. Audio hosts may consume the queue, and Runtime remains valid without Audio.
- Motion definitions are construction-only. Retained callbacks may retarget and sample them, but must call `motion:advance(dt)` explicitly; avoid overlapping ownership of the same Object across Motion and other transform sidecars.
- Review interactive behavior in the delivery host and review exports through their sampled playback path.
