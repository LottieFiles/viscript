# Lua API

The host provides the `vis` global. Create a Scene, author its objects and timeline, then return it:

```lua
local scene = vis.scene {camera = {height = 7}}
local ball = scene:circle {radius = 0.5, fill = "accent", id = "ball"}
scene:grow_from_center(ball, 0.6)
scene:wait(0.5)
return scene
```

Scene dimensions are pixels, 960×540 when left out; positions are world coordinates ([scene-contract.md](scene-contract.md)). Vectors are `{x, y}` or `{x, y, z}` arrays, ranges `{min, max, step}`, and colors theme roles or hex `#rgb`, `#rgba`, `#rrggbb`, or `#rrggbbaa`. Camera options are `target` and world-unit `height`, plus `eye`, `up`, `projection`, `fov`, `near`, and `far` in 3D.

[lua-more.md](lua-more.md) holds transform matrices, tables and matrices, pictures, masks and other paint, boolean shapes, contours, vector fields, stream lines, surfaces, `warp`, inset views, springs, Cell/Voxel sampling, child Scenes, scene transitions, and the optional modules.

## Objects

Scene and Object handles share child factories; the receiver becomes the parent.

| Factory | Main options |
|---|---|
| `group` | `matrix`, `typeset` |
| `space` | `x`, `y`, `z`, axis/grid style, `ticks` |
| `point` | `point`, pixel `radius` |
| `line`, `arrow` | `from`, `to`, `trim_distance`; arrow `tail`, `tip` |
| `vector` | `value`, `origin`, `trim_distance`, `tail`, `tip` |
| `circle` | `center`, world-unit `radius` |
| `arc` | `center`, `radius`, `start`, `angle`, `sector` |
| `angle` | `vertex`, `from`, `to`, `radius`, `other`, `right`, `label` |
| `rectangle` | `center`, `size`, `corner`, or `around`, `padding` |
| `brace` | `around`, or `from` and `to`; `direction`, `offset`, `depth`, `label`, `gap` |
| `polygon`, `route` | ordered `points`; route `tail`, `tip` |
| `path` | move/line/quadratic/cubic/close `commands` |
| `curve` | `from`, `control1`, `control2`, `to`, `samples` |
| `plot` | `points`, or `fn` and `x_range`; `y_range`, `discontinuities`, `endpoint_radius` |
| `area`, `riemann` | `fn` (riemann also `values`), `x_range`; area `base`; riemann `sample`, `negative` |
| `text`, `number` | text or numeric `value`, point, font, size, align, role, orientation |
| `spans` | strings and styled text/TeX parts, `split`, `wrap`, `leading` |
| `code` | `text`, `language`, `colors`, `leading` or `pitch` |
| `tex` | TeX string or parts, `display`, text styling |
| `connector` | endpoint handles `from`, `to`, `padding`, `tail`, `tip` |

Style options include `id`, `stroke`, `fill`, `width`, `opacity`, `progress`, `layer`, `dash`, and `dash_offset`. Groups pass their stroke, fill, and width to descendant shapes.

A connector follows its endpoints' outlines, or their bounds when no closed outline crosses its center ray, and `padding` adds world distance beyond each. `trim_distance` trims a line, arrow, or vector: one number trims both ends, `{start, finish}` each separately. Markers stay solid on dashed shafts.

`object:label{...}` creates a child Text; set its point, align, and layer explicitly. `scene:bounds(handle_or_list)` returns `left`, `right`, `bottom`, `top`, `width`, `height`, `center`, `min`, and `max` in world units. A Text counts with its layout box only when created with `measure = true`; otherwise only its anchor point counts. Frame handles with `rectangle{around = handles}`; `brace{around = handles, label = "term"}` returns the path and the label.

Arcs use radians counterclockwise from +x; a negative sweep runs clockwise, and `sector = true` closes through the center. An `angle` marks two rays from a vertex: `other = true` takes the outer angle, `right` is `"auto"`, `"never"`, or `"always"`, and its label follows the bisector.

## Axes and plots

`space` separates grid `width` from pixel `axis_width`. `ticks = true` uses each range's step; a table sets per-axis steps. `space:labels` takes strings or handles and returns the label handles in order; `side` and pixel `gap` place them.

```lua
local axes = scene:space {x = {-5, 5, 1}, y = {-3, 3, 1}, stroke = "#00000000", ticks = {x = math.pi / 2, y = 1}}
axes:labels {x = {{-math.pi, "-pi"}, {math.pi, "pi"}}}
local graph = axes:plot {fn = math.tan, x_range = {-5, 5, 0.01}, y_range = {-3, 3}}
scene:create(graph, 2, "linear")
```

`y_range` clips branches, `discontinuities` separates known jumps, and a non-finite sample breaks the line. `endpoint_radius` adds a marker at the active draw endpoint. A plot state may supply new `fn` or `points`; matching function grids blend vertically. `area` fills between a graph and a numeric or function `base`; `riemann` returns rectangles left to right with left, right, or midpoint sampling, refined with `match_transform(..., {shapes = true})`.

## Text and code

Text `size` is in points ([coordinates](scene-contract.md#coordinates-and-identity)); theme roles select font, size, and color. A font can be a comma list (`font = "Source Serif 4, Pretendard"`): each character takes the first loaded font that has it.

`local w, h = text:measure()` returns world width and height in the registered font, and `measure = true` stores that box at creation so `bounds`, `around`, and arranging see it. Create a Text that appears later with `opacity = 0`, measure and place it before the timeline, and reveal it in its beat. `baseline = true` pins the first baseline at `point`; `reserve = "-0.00"` fixes a box from a sample string; `tabular = true` gives digits equal widths.

`number` animates `value`, formatted by `decimals`, `separator`, `sign`, `prefix`, and `suffix`. `spans` returns measured texts and formulas on shared baselines: `split = "words"` or `"letters"` makes separate handles, `wrap` sets a world width, and unnamed parts get `<row-id>/<n>` IDs.

`code` returns line groups of token handles (IDs `<block-id>/<line>/<token>`) in the code role's monospaced font, for plain, Lua, Python, JavaScript, TypeScript, C, and C++; `plain` keeps every token in `foreground`.

```lua
local listing = scene:code {text = "local total = 0\nreturn total", language = "lua", id = "listing"}
local selected = listing:search("total", {match = "all", whole = true})
scene:highlight(listing, selected)
local next_listing = scene:edit(listing, {replace = "total", with = "sum", match = "all"})
```

`search` raises an error when nothing matches; `match` is first, last, all, or an occurrence number, and `lines` a line or range. `edit` applies replace/insert/remove/append/prepend in order and animates the diff into a new block with the same style and parent. `highlight` without a selection restores the whole block.

With the TeX module, `tex` returns a typeset Group. `part(tex, occurrence)` selects a part or glyph; split a formula into parts to keep terms together for `match_transform`, which pairs formulas by TeX.

```lua
local equation = scene:tex {tex = {"a^2", "+", "b^2", "=", {"c^2", id = "hyp"}}, id = "eq"}
scene:indicate(equation:part("c^2"))
local caption = scene:spans {"The area is ", {tex = "\\pi r^2"}, "."}
```

## Layout and timeline

Place with `move_to`, `next_to`, `align_to`, `arrange`, and `arrange_grid`, relative to the direct parent and after measuring text; the timeline's `arrange` animates an insertion among siblings.

Timeline calls advance the cursor. `wait` holds, `seek` moves the cursor, `mark` names a time, `at` authors at a time and restores the cursor, and `parallel` starts branches together. `scene:duration()` is the later of the cursor (`scene:time()`) and the latest time any call reached, so after an `at` call `wait(total - scene:duration())` stops short; pad to a total with `scene:seek(total)`.

Animation calls take a timing table with `duration`, `curve`, and, when they stagger targets, `lag`:

```lua
local timing = {duration = 0.6, curve = "ease_in_out"}
scene:shift(ball, {2, 0}, timing)
scene:create({ball, label}, {duration = 0.6, lag = 0.15})
```

Move, draw, and replace calls also take a positional duration and curve: `scene:shift(ball, {2, 0}, 1, "smooth")`. Effects and the camera take only the table: `fade_in` and `fade_out` (one handle each), `indicate`, `wiggle`, `circumscribe`, `flash`, `ripple`, `passing_flash`, and `focus`, as in `scene:fade_in(label, {duration = 0.4})`. To fade several handles at once, `tween` a list of `{target = handle, opacity = 1}` descriptors.

| Purpose | Calls |
|---|---|
| Draw/reveal | `create`, `uncreate`, `fill_reveal`, `draw_border_then_fill`, `write` |
| Fade/grow | `fade_in`, `fade_out`, `grow_from_center`, `grow_from_edge`, `shrink_to_center` |
| Emphasize | `indicate`, `wiggle`, `circumscribe`, `passing_flash`, `flash`, `ripple` |
| Move/style | `shift`, `rotate`, `scale`, `transform`, `move_along`, `fade`, `stroke`, `fill` |
| Replace | `morph`, `fade_transform`, `match_transform`, `transition` |
| State | `tween`, `set`, `save`, `restore`, `clone`, `warp`, `remove` |

`tween` takes one descriptor or a list naming each target once, with properties such as shift, transform, position, opacity, colors, points, text, value, and layer. A descriptor moves with only one of `shift` (by a vector), `transform` (the local matrix), or `position` (the bounds' center to a point in the parent's space, failing on an object without bounds); `arc` bends the move by a central angle. Geometry uses the factory's names (`points`, `from`, `to`, `size`, `radius`); `center` animates arcs only. `set` applies the same descriptors at once, `scene:set({target = label, opacity = 0})`: a starting state before the timeline, a change at the cursor after it. `save` and `restore` keep a stack per target.

```lua
scene:tween({target = ball, shift = {2, 0}, fill = "success"}, 1, "smooth")
scene:tween({target = ball, position = {0, 1}, arc = math.pi / 2}, 1)
scene:parallel(
    function() scene:shift(ball, {-1, 0}, 1) end,
    function() scene:fade(ball, 0.5, 1) end
)
```

Tweening `text` fades the whole old string out and the new one in (`retype` and `sweep` are in lua-more.md); a numeric `value` interpolates. A curve is a preset (`"linear"`, `"smooth"`, `"ease_in"`, `"ease_out"`, `"ease_in_out"`, `"gentle"`, `"snappy"`, `"back"`), a tuned preset such as `{preset = "back", strength = 0.6}`, or `{bezier = {x1, y1, x2, y2}}`.

`morph` resamples outlines from source to target and pairs pieces automatically (`pairs` overrides; `{copy = true}` keeps the source); `clone` snapshots a family. `fade_transform` cross-fades into a target created at the current cursor with no clips, and drops the source. `match_transform` carries matching text, TeX, and optionally shapes into such a target, from a source at rest unless `copy = true`. `transition` fades handles out and others in without removing them; create the incoming ones at `opacity = 0`.

## Cameras

`look` changes camera fields and can aim at handles, keeping the eye offset. `focus` frames handles in a 2D view with fractional padding (`look` frames a 3D one), `focus()` returns to the opening camera, and `orbit` turns a 3D camera around its target.

```lua
scene:focus(ball, {padding = 0.3, duration = 1})
scene:focus()
```

## Trackers

Trackers express continuous relationships. Bind each dependent to a callback, then animate the tracker:

```lua
local x = scene:tracker(-2)
scene:bind(ball, x, function(v) return {shift = {v, v * v / 2}} end)
scene:track(x, 2, 3, "linear")
```

A callback receives tracker values in order and returns a property state: `shift` relative to the bound pose or an absolute `transform`, never `position` or `text`. A live readout is a `number` whose `value` the callback returns: `scene:number {value = 1.6, decimals = 2, prefix = "x = "}`. `track` bakes frame samples into clips, and overlapping tracks of different trackers merge on a bound object. Keep callbacks pure, and give a discrete counter or reveal one `at` event and sample index across all its dependents.

## Sandbox

Scripts run without `pairs`, `next`, `print`, `pcall`, `load`, `require`, metatables, `os`, `io`, `debug`, and the string pattern functions (`find`, `match`, `gmatch`, `gsub`). Iterate with `ipairs` or `vis.keys(table)` (ordered keys), compare strings with `string.sub` and `string.byte`, and stop with `error(...)` to see a value. `math.randomseed(seed)` repeats authored randomness; `vis.merge(defaults, options)` combines options, later values winning. Register fonts and assets before loading a scene that measures or uses them.
