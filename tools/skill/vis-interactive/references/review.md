# Runtime and interaction review

Use the real delivery host to verify normalized input, retained decisions, sounds, camera state, and teardown.

## Frame input

Begin one input frame, forward events in order, sample actions, advance the simulation, then draw. `begin(time)` captures previous state and clears frame edges, pointer delta, and wheel delta. Held state persists. A same-frame press and release exposes both edges.

`ActionMap` combines bindings under named actions. Held values sum and clamp per component; action edges describe the aggregate. `release(time)` ends held input on focus loss, cancellation, scene replacement, and teardown.

During fixed-step catch-up, pass transient edges and deltas to the first step and held state to every step. Keep the input snapshot until a simulation step consumes it.

## Retained Lua

Create the runtime after its authored objects and sidecar bindings. Detect `vis.runtime` and any Input/Audio/States features used by the scene, following [interaction-model.md](interaction-model.md).

```lua
local x = 0
local runtime = vis.runtime(scene, {
    fixed_step = 1 / 60, max_steps = 8,
    update = function(ctx, dt)
        local move = ctx:action("Move")
        x = x + move.value.x * dt
        ctx:update(player, {shift = {x, 0, 0}})
        if move.pressed then
            ctx:sound("audio/click.wav", {bus = "effect", gain = 0.5})
        end
    end,
})
runtime:bind_key("Move", "A", {-1, 0})
runtime:bind_key("Move", "D", {1, 0})
```

`ctx:action` returns previous/current/delta vectors and down/pressed/released flags. `ctx:key` and `ctx:pointer` expose input snapshots. `ctx:update` stages transforms, opacity, progress, and solid fill; `ctx:clear` removes overlays. The host advances Runtime using elapsed physical time independently of authored Scene time.

A step commits its staged overlays and sounds atomically. Runtime opacity multiplies authored opacity; prepare reusable pools at opacity one outside the visible frame, then move and reveal them together. `ctx:sound` queues data for host playback. Copy selected audio into the delivered asset layout and use paths relative to the scene.

Read API comments for callback ownership, state validation, resource budgets, and error recovery.

## Verification

- Render the initial state at delivery size with production fonts. Check affordances, units, containment, reset, and face/region alignment.
- Exercise control endpoints, intermediate values, hover, press, drag outside, release, and cancellation.
- Check PointerFollow at corners and interior points, heading changes across ±pi, leave, and reset.
- Check KeyMove down/up, held motion, opposing keys, modifiers, focus loss, and teardown.
- Confirm UI capture, unhandled Input routing, and cancellation through the combined host.
- Accumulate several events in one frame, including press/release; verify edge consumption during multi-step catch-up.
- Exercise zero-step, one-step, catch-up, and dropped-time advances. Inject a callback failure and inspect its rollback and last committed frame.
- Run the delivery's enabled-module combinations and autonomous runtime path.
- Recheck pause, seek, loop boundaries, resize, reload, camera reset, disposal, and the export sampling path.

Deliver the scene, asset layout, host setup, tested interactions, and concise verification results.
