# Object states

`vis-states` changes retained Objects between named visual states. A Controller
adds relative shift, rotation, scale, opacity, and progress to the state sampled
from the Scene timeline. Its interaction clock is advanced by the host.

## Build

```sh
meson setup build-interactive -Dmodules=interactive
meson compile -C build-interactive
```

The `interactive` selector builds this module with UI, Input, and the retained Lua
runtime. It installs `vis_states.h`, `libvis-states`, and `vis-states.pc`.
Lua-enabled hosts expose `vis.states(scene)`.

## Define and select states

```lua
local scene = vis.scene {}
local answer = scene:rectangle {id = "answer", size = {2, 0.8}}
local states = vis.states(scene)

states:define(answer, "answer.idle", {})
states:define(answer, "answer.correct", {
    shift = {0, 0.12},
    rotation = 0.03,
    scale = {1.08, 1.08},
})
states:transition(answer, "answer.correct", {duration = 0.18, curve = "smooth"})
return scene
```

A state is a complete relative overlay. The default pivot is the Object family's
local bounds center; an explicit origin supplies another pivot. Transform overlays
follow the sampled Object model. Opacity and progress multiply their sampled values.

C++ uses the same Controller:

```cpp
#include "vis_states.h"

using namespace vis;
auto scene = Scene::gen();
auto answer = Rectangle::gen({}, {2.0f, 0.8f});
scene->add(answer);
auto controller = states::Controller::gen(scene);

states::State correct;
correct.shift = {0.0f, 0.12f, 0.0f};
correct.rotation.z = 0.03f;
correct.scale = {1.08f, 1.08f, 1.0f};
controller->define(answer, "answer.correct", correct);
controller->transition(answer, "answer.correct", 0.18f, AnimCurve::preset(AnimCurvePreset::Smooth));
controller->advance(1.0f / 60.0f);
```

Retargeting samples the current overlay and starts the new transition there. A zero
duration selects the target immediately. Rendering combines the current overlay
with the authored Scene state at the requested Scene time.

## Host clock

A host performs semantic decisions and advances states once per interaction tick:

```text
input -> select state -> transition(...)
tick  -> advance(dt)
frame -> sample Scene time + state overlay
```

Advance States alongside a retained Lua Runtime when they share a cadence:

```lua
vis.runtime(scene, {
    fixed_step = 1 / 60,
    update = function(ctx, dt)
        states:advance(dt)
    end,
})
```

The two clocks let a host pause the authored animation while interaction feedback
settles. A capture host can step both clocks, or bake its decisions into Scene clips.

## Batches and events

`transition(Target*, count, ...)` accepts a batch as one transaction. Each accepted
target produces an Event with:

- state-clock time and transaction number;
- Scene-local Object ID and copied state name;
- duration and animation curve.

Read events with `eventCount` and `eventAt`, copy them into the host record, then
acknowledge them with `clearEvents`. Clearing the journal preserves definitions,
tracks, clock, and transaction sequence.

For replay, record semantic keys, Scene paths, state names, fixed ticks, and curve
fields alongside the scene revision. Recreate the scene and state catalog, then
apply batches in host order while stepping Scene and interaction clocks.

See [Architecture](architecture.md) for runtime composition and
[Scene composition](scene-composition.md) for authored animation.
