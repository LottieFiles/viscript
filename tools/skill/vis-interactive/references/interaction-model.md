# Interaction model

Apply the viscript scene, typography, layout, motion, and review workflow to the authored base scene. Then add UI, Input, States, Runtime, or Audio for the interaction the subject needs.

## Modules

| Module | Build option | Lua entry | Purpose |
|---|---|---|---|
| UI | `-Dmodules=interactive` | `vis.ui.panel(scene)` | Buttons, toggles, sliders, pointer capture, rendered sampling |
| Input | `-Dmodules=interactive` | `vis.input.controller(scene)` | Object bindings, frame input/actions, camera gestures |
| Runtime | `-Dmodules=interactive` | `vis.runtime(scene, config)` | Retained Lua and fixed-step object overlays |
| Audio | `-Dmodules=audio` | `vis.audio` | Authored cues and host playback |
| States | `-Dmodules=interactive` | `vis.states(scene)` | Named states and capture-current retargeting |

Feature-detect each module used by the delivered scene. For example:

```lua
if not vis.input then
    error("this scene requires a viscript build configured with -Dmodules=interactive")
end
```

Each module has its own ownership and validation contract in its API code comments. UI and Input can run independently; Runtime's action adapter uses Input, and sound events can be consumed by an Audio host.

## Controls and object bindings

Create target objects and separate control faces, then register their logical-pixel regions:

```lua
local panel = vis.ui.panel(scene)
panel:button {
    visual = button_face,
    region = {x = 24, y = 24, width = 96, height = 40},
    target = player, transform = {shift = {1, 0, 0}}, duration = 0.25,
}
local input = vis.input.controller(scene)
input:key_move {target = player, key = "D", shift = {1, 0, 0}, period = 0.12}
```

| UI declaration | Behavior |
|---|---|
| `button` | Apply a target transform or camera action after a completed click |
| `toggle_button` | Alternate a target between `off` and `on` transforms |
| `slider` | Interpolate target endpoints, or synchronized `bindings`, from a normalized value |
| `sample_area` | Sample configured object families at a completed click's release point |

Button and Toggle Button may use `hover_visual` and `pressed_visual`. The Panel selects authored faces through composed opacity. Transforms include shift, scale, rotation, opacity, and progress.

SampleArea returns logical position, normalized `{u, v}`, object identity, and straight RGBA. An optional marker follows `marker_origin + marker_x*u + marker_y*v`; an optional swatch shows the sampled color. A miss preserves the previous sample. Route renderer errors through the host's input-error hook.

PointerFollow maps passive motion into a target's parent space:

```lua
input:pointer_follow {
    target = marker, region = {80, 90, 800, 420},
    target_origin = {0, 0}, map_origin = {-4, 2.1},
    map_x = {8, 0}, map_y = {0, -4.2},
    period = 0.18, rotate = true, reset_on_leave = true,
}
```

KeyMove holds movement from keydown until keyup. A positive period ramps toward `shift / period`; zero applies the shift on the press. Use distinct logical bindings and preserve editor shortcuts.

## Host routing and coordinates

In a combined host, offer pointers to UI first, preserve captured gestures, and forward unhandled events to Input. Broadcast cancellation and release held input on focus loss. Input owns keyboard motion and camera gestures.

Regions use top-left-origin logical Scene pixels. Convert display positions with:

```text
x = (clientX - canvasLeft) * sceneWidth  / displayedCanvasWidth
y = (clientY - canvasTop)  * sceneHeight / displayedCanvasHeight
```

Geometry and overlays remain in direct-parent world units. Align regions with projected painted bounds. For a fixed 2D camera:

```text
px = width/2 + (worldX - targetX) * height/cameraHeight
py = height/2 - (worldY - targetY) * height/cameraHeight
```

PointerFollow computes normalized coordinates in its region, then `map_origin + map_x*u + map_y*v`. Choose `map_y` explicitly for the desired vertical direction.

## Composed state and States

Rendering samples the authored timeline, composes registered object/fill modifiers, composes camera modifiers, then draws. Retargeted transitions capture the current composed state. Use a semantic Group when several painted objects represent one state, and provide a visible camera-reset path.

```lua
local states = vis.states(scene)
states:define(card, "card.idle", {})
states:define(card, "card.correct", {shift = {0, 0.12}, scale = {1.08, 1.08}})
states:transition(card, "card.correct", 0.18, "smooth")
states:transition({{object = card, state = "card.correct"}}, 0.18, "smooth")
states:advance(1 / 60)
local visible = states:sample(card)
```

States uses its own clock. Define states during construction, capture the handle in retained decisions, and call `advance(dt)` once per simulation step when the clocks should share a cadence. Pass one object and state, or an array of `{object, state}` entries, to `transition`. A batch applies atomically. `event_count`, `event`, and `clear_events` expose acknowledgements.

See [review.md](review.md) for retained runtime setup and interaction checks.
