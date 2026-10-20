# Lua API: data, paint, fields, views, and composition

Read this beside [lua.md](lua.md) when a Scene sets a transform matrix, shows a table, matrix, or picture, uses masks or other paint, combines shapes, samples a field or surface, warps geometry, adds an inset view, drives a spring, fills Cells or Voxels, or mounts child Scenes.

## More factories

| Factory | Main options |
|---|---|
| `matrix` | `rows`, `brackets`, `gap`, `justify`, `decimals` |
| `table` | `rows`, `header`, `padding`, `justify`, `stripes`, `rules` |
| `picture` | registered/file `asset`, or RGBA `pixels` and `size`; center, width, filter |
| `union`, `intersection`, `difference`, `exclusion` | `of`, a list of closed shape handles |
| `contour` | `fn` or `values`, `levels`, `x_range`, `y_range`, `colors`, `labels` |
| `vector_field` | `fn` or `values`, ranges, `length` or `scale`, `anchor`, `colors` |
| `stream_lines` | `fn`, ranges or `seeds`, `time`, `dt`, `direction`, `colors` |
| `surface` | row-major `points`, `size`, `mode`, `colors`, `mesh`, `shading`, `light` |
| `cell` | `size`, `color`, patches, `mode`, fractional `padding` |

`matrix` and `table` return row groups with entry texts: `row(r)`, `entry(r, c)`, and `column(c)`, from 1. Matrix children include its brackets; table children include stripes and rules. A matrix string entry is inline TeX when the tex module is built in (`"\\cos\\theta"`, `"x^2"`; `"\\text{word}"` for words) and plain text otherwise; numbers stay countable texts, and `""` stays blank. Table strings are always text.

## Transforms and paint

Matrices are row-major and transform column vectors; translation occupies entries 4, 8, and 12:

```lua
local matrix = {1,0,0,tx, 0,1,0,ty, 0,0,1,tz, 0,0,0,1}
```

This absolute local-to-parent transform serves Group/Space `matrix`, `tween`'s `transform`, and `scene:transform`.

More style options: `gradient`, `blend`, `blur`, `shadow`, `isolate = true` to composite a family as one picture, and `mask`, which selects another object's alpha or luma, with inverse modes.

## Text replacement and matching

Tweening `text` with `retype = true` erases to the shared prefix before typing the new suffix (use it when typing from an empty string too); `sweep = true` mixes both strings in a character sweep. Both need `text` and cannot both be true.

```lua
scene:tween({target = caption, text = "Ready to export", retype = true}, 1)
scene:tween({target = caption, text = "EXPORTED", sweep = true}, 1)
```

`match_transform` options are `pairs`, `strings`, `diff`, `shapes`, `fade`, `lag`, `arc`, `path`, and `copy`. In 3D, `space:labels {placement = "screen"}` sets billboard endpoint text outward from the projected origin at each camera sample; see [surface-recipe.md](surface-recipe.md).

## Fields and surfaces

`contour` returns one Plot per level. Each child can be bound to a scalar field and animated through tracker samples. `labels = true` adds level-value text; a table sets `format`, local text `size`, `spacing`, `padding`, and `upright`. Grid values run row by row from the bottom left, x varying fastest.

`vector_field` returns arrows in grid order, colored by magnitude. `length` caps their visual length smoothly; `scale` uses physical vector lengths. `stream_lines` integrates the field from grid or explicit seeds with `dt`, `time`, and forward/backward/both tracing.

Surfaces use `"solid"`, `"mesh"`, or `"solid_mesh"`. Colors can be a colormap name, `{map, range, axis}`, a point callback, or one color per point. The available maps are viridis, plasma, inferno, magma, and cividis. `mesh = {11, 11}` draws a coarse mesh over a finer surface. Lighting follows the camera by default; `light = {direction = {-1, 1, 1}, ambient = 0.5}` fixes it in world space. In 3D, surfaces occlude strokes in their own layer; separate layers control foreground annotations. See [surface-recipe.md](surface-recipe.md).

## Warp

`warp` maps authored points in each target's parent space. It subdivides straight segments using `step` before bending them. Geometry stays warped after the animation, so later bounds, cloning, and motion paths use its new outline.

```lua
scene:warp(ball, function(x, y) return x + 0.2 * y * y, y end, 1, "smooth", {step = 0.05})
```

## Inset views

A view renders the same objects through another camera. Its `look`, `focus`, `orbit`, and `fade` calls use the timeline. `follow` tracks an object in 2D; `optical`, `background`, `layer`, `hide`, and `opacity` style the inset.

```lua
local lens = scene:view({x = 0.6, y = 0.05, width = 0.35, height = 0.35},
    {camera = {target = ball, height = 1}, follow = ball, layer = 1})
lens:focus(ball)
```

## Springs and sampled fields

`vis.spring_response(seconds, options)` samples a physical spring's displacement from equilibrium. Options are mass, stiffness, damping, displacement, and initial velocity. Use a seconds tracker and a linear track so physical time stays uniform:

```lua
local elapsed = scene:tracker(0)
local omega = 2 * math.pi
scene:bind(ball, elapsed, function(time)
    return {shift = {vis.spring_response(time,
        {mass = 1, stiffness = omega * omega, damping = 0.3 * omega, displacement = 2}), 0}}
end)
scene:wait(0.5)
scene:track(elapsed, 7.5, 7.5, "linear")
```

`space:cell(fn, config)` and `space:voxel(fn, config)` sample colors over space ranges and time during authoring. Use duration, fps, mode, and fractional padding to control the field.

```lua
local plane = scene:space {x = {-2, 2, 0.1}, y = {-2, 2, 0.1}}
plane:cell(function(x, y, time)
    return x*x + y*y < (1 + time)^2 and "accent" or "#00000000"
end, {duration = 1, fps = 24, mode = "padd", padding = 0.06})
```

## Scene composition and modules

`scene:viewport(child, bounds)` mounts a child Scene. Bounds use normalized `{x, y, width, height}` from the frame's top left. `scene:scene_transition(stages, options)` plays a sequence, with each incoming stage starting at local time zero. Finish authoring children and stages before mounting: successful attachment seals and transfers them, so they cannot be edited or reused in another mount.

Transition styles are match, fade, slide, zoom_in, and zoom_out. `hold` sets fully shown time; `live = false` shows completed stages. `from` chooses a slide side, `area` chooses a zoom region, `area_margin` adds object padding, and `match` carries equal-ID objects. `styles` sets each transition separately:

```lua
scene:scene_transition({overview, detail, summary}, {
    duration = 0.8,
    styles = {{style = "zoom_in", area = "chart", area_margin = 0},
              {style = "slide", from = "bottom", match = true}},
})
```

Use [vis-interactive](../../vis-interactive/SKILL.md) for UI, Input, action maps, retained runtime, audio, and simulation loops. Authored sound cues use `vis.audio.cue(scene, options)` when Audio is enabled.

`vis.states(scene)` provides named object states, atomic transitions, and interaction-clock retargeting. `vis.diagram(scene)` provides semantic nodes, edges, groups, and audit metadata; see [diagram/native-authoring.md](diagram/native-authoring.md). Enable the corresponding modules in the build.
