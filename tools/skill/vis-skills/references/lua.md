# Lua API

The host provides the `vis` global. Create a Scene, author its objects and timeline, then return it:

```lua
local scene = vis.scene {
    width = 960, height = 540, fps = 30, loop = false,
    theme = "pro_white",
    camera = {view = "2d", target = {0, 0}, height = 7},
}
local ball = scene:circle {center = {0, 0}, radius = 0.5, fill = "accent", id = "ball"}
scene:grow_from_center(ball, 0.6)
scene:wait(0.5)
return scene
```

Scene dimensions are pixels; positions are world coordinates. Follow [scene-contract.md](scene-contract.md) for the mapping. Vectors use `{x, y}` or `{x, y, z}` and numeric indices. Ranges use `{min, max, step}`. Colors use theme roles or hex `#rgb`, `#rgba`, `#rrggbb`, or `#rrggbbaa`.

The standalone theme is `"pro_white"`. VS Code scenes use `"adaptive_vscode"`; fixed presets also include `"pro_black"` and `"3_blue_1_eyes"`. Camera options include `target`, world-unit `height`, and for 3D, `eye`, `up`, `projection`, `fov`, `near`, and `far`.

Matrices are row-major and transform column vectors. Translation occupies entries 4, 8, and 12:

```lua
local matrix = {1,0,0,tx, 0,1,0,ty, 0,0,1,tz, 0,0,0,1}
```

Use this absolute local-to-parent transform for Group/Space `matrix`, `play`'s `transform`, and `scene:transform`.

## Objects

Scene and Object handles share child factories. The receiver becomes the parent.

| Factory | Main options |
|---|---|
| `group` | `matrix`, `typeset` |
| `space` | `x`, `y`, `z`, axis/grid style, `ticks`, `labels` |
| `point` | `point`, pixel `radius` |
| `line`, `arrow` | `from`, `to`, `buff`; arrow `tail`, `tip` |
| `vector` | `value`, `origin`, `buff`, `tail`, `tip` |
| `circle` | `center`, world-unit `radius` |
| `arc` | `center`, `radius`, `start`, `angle`, `sector` |
| `angle` | `vertex`, `from`, `to`, `radius`, `other`, `right`, `label` |
| `rectangle` | `center`, `size`, `corner`, or `around`, `padding` |
| `brace` | `around`, or `from` and `to`; `direction`, `buff`, `depth`, `label`, `gap` |
| `polygon`, `route` | ordered `points`; route `tail`, `tip` |
| `path` | move/line/quadratic/cubic/close `commands` |
| `curve` | `from`, `control1`, `control2`, `to`, `samples` |
| `plot` | `points`, or `fn` and `x_range`; `y_range`, `discontinuities`, `endpoint_radius` |
| `area` | `fn`, `x_range`, `base` |
| `riemann` | `fn` or `values`, `x_range`, `sample`, `negative` |
| `contour` | `fn` or `values`, `levels`, `x_range`, `y_range`, `colors`, `labels` |
| `vector_field` | `fn` or `values`, ranges, `length` or `scale`, `anchor`, `colors` |
| `stream_lines` | `fn`, ranges or `seeds`, `time`, `dt`, `direction`, `colors` |
| `surface` | row-major `points`, `size`, `mode`, `colors`, `mesh`, `shading`, `light` |
| `union`, `intersection`, `difference`, `exclusion` | `of`, a list of closed shape handles |
| `text`, `number` | text or numeric `value`, point, font, size, align, role, orientation |
| `spans` | strings and styled text/TeX parts, `split`, `wrap`, `leading` |
| `code` | `text`, `language`, `colors`, `leading` or `pitch` |
| `tex` | TeX string or parts, `display`, text styling |
| `matrix` | `rows`, `brackets`, `gap`, `justify`, `decimals` |
| `table` | `rows`, `header`, `padding`, `justify`, `stripes`, `rules` |
| `picture` | registered/file `asset`, or RGBA `pixels` and `size`; center, width, filter |
| `cell` | `size`, `color`, patches, `mode`, fractional `padding` |
| `connector` | endpoint handles `from`, `to`, `padding`, `tail`, `tip` |

Style options include `id`, `stroke`, `fill`, `width`, `opacity`, `progress`, `layer`, `gradient`, `dash`, `dash_offset`, `isolate`, `mask`, `blend`, `blur`, and `shadow`. Groups pass their stroke, fill, and width to descendant shapes. `isolate = true` composites a family as one picture. A mask selects another object's alpha or luma, with inverse modes available.

A connector follows sampled endpoint outlines and uses bounds when no closed outline crosses its center ray. `padding` adds world distance beyond each outline. `buff` trims a line, arrow, or vector: one number trims both ends; `{start, finish}` sets separate distances. Arrow and route markers stay solid when their shafts are dashed.

`object:label{...}` creates a child Text. Set its point, align, and layer explicitly. `scene:bounds(handle_or_list)` returns `left`, `right`, `bottom`, `top`, `width`, `height`, `center`, `min`, and `max` in world units. Use `rectangle{around = handles}` for a frame, or `brace{around = handles, label = "term"}` for an annotation; brace returns both the path and label.

Arcs use radians counterclockwise from +x; a negative sweep runs clockwise. `sector = true` closes through the center. Angles mark two rays from a vertex; `other = true` chooses the outer angle, and `right` selects `"auto"`, `"never"`, or `"always"`. A text or TeX label follows the bisector.

## Axes and fields

`space` separates grid `width` from pixel `axis_width`. `ticks = true` uses each range's step; a table selects per-axis steps. Axis labels accept strings or handles:

```lua
local axes = scene:space {
    x = {-5, 5, 1}, y = {-3, 3, 1},
    stroke = "#00000000", ticks = {x = math.pi / 2, y = 1},
}
axes:labels {x = {{-math.pi, "-pi"}, {math.pi, "pi"}}}
local graph = axes:plot {fn = math.tan, x_range = {-5, 5, 0.01}, y_range = {-3, 3}}
scene:create(graph, 2, "linear")
```

`space:labels` returns label handles in authored order. `side` and pixel `gap` place world labels. `placement = "screen"` places billboard endpoint text outward from the projected origin at each camera sample; see [surface-recipe.md](surface-recipe.md).

Plots use `y_range` to clip branches and `discontinuities` to separate known jumps. A non-finite sample breaks the line. `endpoint_radius` adds a solid marker at the active draw endpoint. A plot state may supply new `fn` or `points`; retained ranges and cuts apply to later states. Matching function grids blend vertically, while other geometry states interpolate matching pieces.

`area` fills between a graph and a numeric/function `base`. `riemann` returns rectangles from left to right, with left, right, or midpoint sampling. Refine bars with `match_transform(..., {shapes = true})`.

`contour` returns one Plot per level. Each child can be bound to a scalar field and animated through tracker samples. `labels = true` adds level-value text; a table sets `format`, local text `size`, `spacing`, `padding`, and `upright`. Grid values run row by row from the bottom left, x varying fastest.

`vector_field` returns arrows in grid order, colored by magnitude. `length` caps their visual length smoothly; `scale` uses physical vector lengths. `stream_lines` integrates the field from grid or explicit seeds with `dt`, `time`, and forward/backward/both tracing.

Surfaces use `"solid"`, `"mesh"`, or `"solid_mesh"`. Colors can be a colormap name, `{map, range, axis}`, a point callback, or one color per point. The available maps are viridis, plasma, inferno, magma, and cividis. `mesh = {11, 11}` draws a coarse mesh over a finer surface. Lighting follows the camera by default; `light = {direction = {-1, 1, 1}, ambient = 0.5}` fixes it in world space. In 3D, surfaces occlude strokes in their own layer; separate layers control foreground annotations.

## Text and code

Text `size` is in points: the em spans `4/3 * size` screen pixels for billboard text or world units for plane text. Theme roles select font, size, and color. Plain text and numbers use bundled Noto Sans Math outlines for common missing geometry symbols.

`text:measure()` returns width and height in world units. `measure = true` stores a layout box at creation. `baseline = true` pins the first baseline at `point`, with `align.x` selecting horizontal alignment. `reserve = "-0.00"` fixes a box from a sample string and the initial text. `tabular = true` gives digits, minus, and spaces equal advance cells.

`number` animates `value`; `decimals`, `separator`, `sign`, `prefix`, and `suffix` format it. `matrix` and `table` return row groups with entry texts. Access parts with `row(r)`, `entry(r, c)`, and `column(c)`; indices start at 1. Matrix children also include its brackets; table children include stripes and rules.

`spans` returns measured texts and formulas on shared baselines. Strings retain their spaces; `split = "words"` or `"letters"` creates separate handles. Parts set local styling and `rise`, and `wrap` sets a width in world units. `leading` sets baseline spacing. Texts/formulas with no explicit ID use `<row-id>/<n>`.

`code` returns line groups containing token handles. Languages include plain, Lua, Python, JavaScript, TypeScript, C, and C++. Tokens use semantic theme colors and the code role's monospaced font. IDs use `<block-id>/<line>/<token>`.

```lua
local listing = scene:code {text = "local total = 0\nreturn total", language = "lua", id = "listing"}
local selected = listing:search("total", {match = "all", whole = true})
scene:highlight(listing, selected)
local next_listing = scene:edit(listing, {replace = "total", with = "sum", match = "all"})
```

`search` returns covered tokens. `match` selects first, last, all, or an occurrence number; `lines` selects a line or range. `edit` applies replace/insert/remove/append/prepend operations in order and returns the new block. It keeps the original styling and parent, then animates the diff. `highlight` selects line indices or handles; call it without a selection to restore the whole block.

With the TeX module, `tex` returns a typeset Group of glyphs and filled geometry:

```lua
local equation = scene:tex {tex = {"a^2", "+", "b^2", "=", {"c^2", id = "hyp"}}, id = "eq"}
scene:indicate(equation:part("c^2"))
local caption = scene:spans {"The area is ", {tex = "\\pi r^2"}, "."}
```

`part(tex, occurrence)` selects a formula part or glyph by TeX. Split into parts to keep terms together during matching. `display = false` uses inline styles; `{tex = ...}` spans parts share the text baseline. `match_transform` pairs formulas by TeX and can override pairs with handles.

## Layout and timeline

Use `move_to`, `next_to`, `align_to`, `arrange`, and `arrange_grid` for initial placement. Coordinates are relative to the direct parent. Measure text before arranging it. For an animated insertion, use the Scene's timeline `arrange` with sibling handles.

Most timeline calls take duration and curve, then advance the cursor. `wait` holds, `seek` moves the authoring cursor, `mark` names a time, and `at` authors at a time while restoring the previous cursor. `parallel` starts branches at the same cursor.

| Purpose | Calls |
|---|---|
| Draw/reveal | `create`, `uncreate`, `fill_reveal`, `draw_border_then_fill`, `write` |
| Fade/grow | `fade_in`, `fade_out`, `grow_from_center`, `grow_from_edge`, `shrink_to_center` |
| Emphasize | `indicate`, `wiggle`, `circumscribe`, `passing_flash`, `flash`, `ripple` |
| Move/style | `shift`, `rotate`, `scale`, `transform`, `move_along`, `fade`, `stroke`, `fill` |
| Replace | `morph`, `replacement_transform`, `fade_transform`, `match_transform`, `transition` |
| State | `play`, `set`, `save`, `restore`, `clone`, `warp`, `remove` |

`play` accepts one descriptor or a list. Each names `target` and properties such as shift, transform, opacity, colors, points, text, value, and layer. `lag` offsets successive targets. `set` applies a state immediately; `save` and `restore` keep a stack of states per target.

```lua
scene:play({target = ball, shift = {2, 0}, fill = "success"}, 1, "smooth")
scene:save(ball)
scene:indicate(ball)
scene:restore(ball, 0.5)
scene:parallel(
    function() scene:shift(ball, {-1, 0}, 1) end,
    function() scene:fade(ball, 0.5, 1) end
)
```

Text changes interpolate characters. `retype = true` erases to the shared prefix before typing the new suffix. Geometry descriptors use the factory's names, such as `points`, `from`, `to`, `control1`, and `control2`. `arc` bends movement by a central angle. Layer changes use the higher layer during the transition.

A curve can be a preset (`"linear"`, `"smooth"`, `"gentle"`, `"ease_in_out"`, and others), a cubic Bézier table, or a preset table with strength and reverse settings. A table such as `{preset = "back", strength = 0.6}` tunes a preset; `{bezier = {x1, y1, x2, y2}, strength = 1}` defines a custom curve.

`morph` resamples outlines and hands ownership to the target. Multi-piece paths pair automatically; explicit one-based contour `pairs` override selected matches. `{copy = true}` preserves the source. `clone` snapshots an object's current family, with an optional parent, ID, style, and world shift.

`fade_transform` replaces an identity by cross-fading; `transition` cross-fades without consuming handles. `match_transform` carries matching text, TeX, and optional shapes between objects. Options include `pairs`, `strings`, `diff`, `shapes`, `fade`, `lag`, `arc`, `path`, and `copy`. Create the target immediately before a replacement.

`warp` maps authored points in each target's parent space. It subdivides straight segments using `step` before bending them. Geometry stays warped after the animation, so later bounds, cloning, and motion paths use its new outline.

```lua
scene:warp(ball, function(x, y) return x + 0.2 * y * y, y end, 1, "smooth", {step = 0.05})
```

## Cameras and views

`look` changes camera fields and can aim at handles while retaining the eye offset. `focus` frames handles with padding; `focus()` returns to the opening camera. `orbit` rotates a 3D camera around its target.

```lua
scene:focus(ball, {padding = 0.3, duration = 1})
scene:look({target = ball}, 0.5)
scene:focus()
local lens = scene:view({x = 0.6, y = 0.05, width = 0.35, height = 0.35},
    {camera = {target = ball, height = 1}, follow = ball, layer = 1})
lens:focus(ball)
```

A view renders the same objects through another camera. Its `look`, `focus`, `orbit`, and `fade` calls use the timeline. `follow` tracks an object in 2D; `optical`, `background`, `layer`, `hide`, and `opacity` style the inset.

## Trackers and sampling

Trackers express continuous relationships. Bind each dependent to a callback, then animate the tracker:

```lua
local x = scene:tracker(-2)
scene:bind(ball, x, function(v) return {shift = {v, v * v / 2}} end)
scene:track(x, 2, 3, "linear")
```

A callback receives tracker values in order and returns a property state. `shift` is relative to the bound pose; `transform` is absolute. `track` bakes frame samples into retained clips. Overlapping tracks of different trackers merge on a bound object. Keep callbacks pure. For discrete counters and reveals, use the same `at` event and sample index for every dependent state.

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

`scene:viewport(child, bounds)` mounts a child Scene. Bounds use normalized `{x, y, width, height}` from the frame's top left. `scene:scene_transition(stages, options)` plays a sequence, with each incoming stage starting at local time zero.

Transition styles are match, fade, slide, zoom_in, and zoom_out. `hold` sets fully shown time; `live = false` shows completed stages. `from` chooses a slide side, `area` chooses a zoom region, `area_margin` adds object padding, and `match` carries equal-ID objects. `styles` sets each transition separately:

```lua
scene:scene_transition({overview, detail, summary}, {
    duration = 0.8,
    styles = {{style = "zoom_in", area = "chart", area_margin = 0},
              {style = "slide", from = "bottom", match = true}},
})
```

Use [vis-game](../../vis-game/SKILL.md) for UI, Input, action maps, retained runtime, audio, and simulation loops. Authored sound cues use `vis.audio.cue(scene, options)` when Audio is enabled.

`vis.motion(scene)` provides semantic states, atomic transitions, and interaction-clock retargeting. `vis.diagram(scene)` provides semantic nodes, edges, groups, and audit metadata; see [diagram/native-authoring.md](diagram/native-authoring.md). Enable the corresponding modules in the build.

Use `math.randomseed(seed)` for repeatable authored randomness. `vis.keys(table)` gives ordered keys; `vis.merge(defaults, options)` combines component options with later values winning. Register fonts and memory-backed assets before loading a scene that measures or uses them. Review every authored timeline with [review.md](review.md).
