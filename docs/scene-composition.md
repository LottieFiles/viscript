# Scene composition

A Scene is an Object tree and a timeline. Render it at any time to reconstruct its
geometry, style, camera, and visibility. Groups organize related content; Spaces
add coordinate axes and ranges.

## Objects and coordinates

Scene factories create roots. Object factories create children, so a shape can own
its annotation and a Group can contain a complete visual unit.

```lua
local scene = vis.scene {}
local node = scene:group {id = "node"}
node:circle {radius = 0.8}
node:text {text = "A", id = "label", measure = true}
scene:shift(node, {2, 0}, 1)
return scene
```

Every Object has an affine local-to-parent model. Ancestor matrices compose in tree
order. Two-component coordinates use Z=0.

```text
worldPoint = root.model * ... * parent.model * object.model * localPoint
```

Group opacity and drawing progress multiply through its descendants. A painted
Object applies them to its own paint; a Space also applies them to its placed axis
labels. All descendants share their ancestors' visibility intervals.

`children` enumerates direct children in insertion order. `find(id)` looks up a
stable string ID. Group `row`, `entry`, and `column` helpers select structured
children. Code `search` returns token handles for matching text.

## Coordinate spaces

A Space owns ranges, axes, grid, ticks, and labels, and supplies coordinate
conversion. Its model transforms both the coordinate system and its children.

```lua
local space = scene:space {
    id = "axes", x = {-3, 3, 1}, y = {-2, 2, 1},
    ticks = {x = 1, y = 1}, axis_width = 1.5,
}
space:labels {x = {{-3, "-3"}, {3, "3"}}}
```

World labels sit beside their ticks. `placement="screen"` positions Text ink beyond
the projected axis anchor and follows the sampled camera. Generated labels receive
IDs below the Space; supplied handles retain their IDs.

Arcs retain a center, radius, start direction, and signed sweep in radians. Angles
retain a vertex and two ray endpoints. Their labels follow the sampled bisector;
a right angle can use a square marker. The geometry and labels share drawing and
animation behavior.

Riemann sums, vector fields, stream lines, and contours produce Groups of ordinary
Objects during authoring. Trackers can bake changing plots and contour fields into
the timeline. [Cell and Voxel fields](cell-voxel.md) retain sampled colors instead.

## Ownership and authoring

| Operation | Owner after success |
|---|---|
| C++ `Object::gen` | Caller |
| `Object::add(child)` | Parent Object |
| `Scene::add(root)` | Scene |
| Lua / JavaScript factory | Factory receiver |
| `Scene::clone` | Scene |
| `Scene::viewport(child)` | Parent Scene |
| `Scene::transition(stages)` | Parent Scene |

Configure C++ definitions before attachment. Attachment captures their base state.
Use `Scene::update(object)` to refresh direct field edits during initial layout.
Lua and JavaScript layout methods perform that refresh automatically.

The authoring sequence is:

```text
configure -> attach -> measure and lay out -> schedule -> sample -> destroy
```

Scheduling seals attached definitions and stores later changes as clips. Objects
added later are born at the current cursor. Cloning copies the state visible there,
including the subtree, into a new attached family. Mounted Scenes keep their
independent object namespaces and are owned by the composition.

Themes resolve omitted styles at attachment. See [Themes](themes.md).

## Bounds and text layout

Use geometry bounds to position Objects in their parent's coordinates. `move_to`,
`next_to`, `align_to`, `arrange`, and `arrange_grid` calculate and store the resulting
model. `scene:bounds` expresses an Object or list's family bounds in world units.

Text contributes an anchor or its authored box. `measure=true` creates a box from
the Scene font and size. `baseline=true` pins the first baseline; `reserve` retains
space for changing strings and numbers.

```lua
local value = scene:number {
    value = 0, reserve = "-000.00", decimals = 2,
    point = {0, 0}, align = {1, 0}, baseline = true, id = "value",
}
scene:play({target = value, value = 42.5}, 1)
```

`spans` measures styled Text parts and places them on shared baselines. `wrap` sets
a line width; `leading` sets baseline pitch. TeX parts share this layout through the
[TeX module](tex.md). Code blocks retain measured token Text; matrix and table
helpers retain structured entries.

`rectangle{around=...}` and `brace{around=...}` frame authored bounds. A brace can
also span explicit `from` and `to` points and place a label beside its tip.

For a layout change during animation, `scene:arrange` calculates targets from the
current cursor and schedules their movement. For final measurement,
`Renderer::bounds` and `layout` report sampled paint in root pixels, including
fonts, transforms, projection, and composition. See [Layout audit](layout-audit.md).

## Draw order and composition

Paint sorts by integer layer, then camera depth, then stable attachment order.
Lower layers draw first. In 3D, sampled surfaces split same-layer strokes where a
face covers them. An animated `layer` changes at a step: the higher layer holds
through the clip, and the target's descendants move by the same layer difference.

`isolate=true` draws a family as one offscreen picture. Group opacity then fades the
combined picture. Masks, blend modes, blur, and shadow also create composition
layers.

```lua
local card = scene:group {
    isolate = true, opacity = 0.8, layer = 2,
    shadow = {color = "#00000066", offset = {0, -4}, blur = 8},
}
card:rectangle {size = {3, 2}, fill = "surface"}
```

`mask=target` uses the target's sampled alpha or luminance as a window. Modes are
`alpha`, `inverse_alpha`, `luma`, and `inverse_luma`. Set the mask to `nil` to remove
it. A mask target contributes to the mask layer.

`blend` selects modes such as `multiply`, `screen`, `overlay`, `difference`, or
`add` against earlier paint in the enclosing layer. `blur` uses a Gaussian sigma in
screen pixels. `shadow` supplies color, pixel offset, and blur. Both can animate;
Group opacity fades the resulting picture and its shadow together.

## Connectors and paths

A Connector derives its endpoints from sampled Object outlines or bounds. It follows
node motion, size changes, and geometry without a callback. `padding` moves an end
beyond the attachment point. Animate endpoints to move an edge.

`move_along` follows a copied path outline, optionally rotating with its heading. A
Connector supplies a live path, so the follower tracks the edge's sampled endpoints.

## Timeline

Animation calls begin at the Scene cursor and advance it. `wait` adds a pause;
`at` schedules at an absolute time; `parallel` starts callbacks at the same beat.
Markers name useful times for host transport and narration.

Animation calls share `{duration, curve}` options. Add `lag` when staggering targets,
and reuse the table across calls:

```lua
local timing = {duration = 0.8, curve = "ease_in_out"}
scene:shift(ball, {2, 0}, timing)
scene:rotate(ball, math.pi / 2, timing)
scene:create({ball, arrow}, {duration = 0.8, curve = "smooth", lag = 0.15})
```

Positional calls remain supported:

```lua
scene:parallel(
    function() scene:shift(ball, {4, 0}, 3) end,
    function() scene:fade_in(label, 1); scene:mark("label") end
)
scene:at("label", function() scene:create(arrow, 0.6) end)
```

A `play` descriptor changes several properties together:

```lua
scene:play({
    {target = left, shift = {-1, 0}, fill = "accent", opacity = 0.8},
    {target = right, shift = {1, 0}, stroke = "danger"},
}, 0.8, "ease_in_out", 0.15)
```

Later descriptors are staggered by `duration * lag`. Properties use separate
channels, so a move and fade compose. Overlapping model motions apply their changes
in stored order. `set` applies a descriptor at the cursor; `save` and `restore` keep
and return to sampled family states.

| Helper | Effect |
|---|---|
| `create`, `uncreate` | Draw or erase geometry in path order |
| `write` | Trace outlines and fill their drawn prefix |
| `fill_reveal`, `draw_border_then_fill` | Reveal fill after an outline |
| `fade_in`, `fade_out` | Change opacity with optional shift and scale |
| `grow_from_center`, `grow_from_edge`, `shrink_to_center` | Reveal or remove by scaling |
| `rotate`, `scale`, `move_along` | Move around a pivot or along a path |
| `indicate`, `wiggle` | Apply temporary emphasis to the target |
| `circumscribe`, `passing_flash`, `flash`, `ripple` | Draw temporary emphasis shapes |
| `highlight` | Emphasize selected children while dimming their peers |

Direction options control outline traversal. Fill animation can use two beats:

```lua
scene:create(path, 0.8, "linear")
scene:fill_reveal(path, 0.35, "ease_out")
```

A plain Text can animate to another string; `retype=true` erases their differing
suffix and types the replacement. Geometry descriptors change retained points or
resample a function plot. `warp` bakes a point map into geometry and animates each
point toward its image.

## Replacement and matching

`morph` replaces the source with aligned target geometry.
Continue the chain using the target handle:

```lua
scene:wait(0.3)
local square = scene:rectangle {size = {2.4, 2.4}, id = "square"}
scene:morph(circle, square, 1, "ease_in_out")
scene:indicate(square)
```

`copy=true` retains the original and replaces a sampled copy. `fade_transform`
cross-fades families. `match_transform` pairs explicit pieces, equal Text strings,
or compatible shapes and moves them into the target. TeX supplies formula-key
pairing; code `edit` builds a replacement block and uses a line/token diff.

## Curves and trackers

Animation curves select timing: `linear`, `smooth`, `ease_in`, `ease_out`,
`ease_in_out`, `gentle`, `snappy`, `back`, `bounce`, or `elastic`. A table supplies
strength, reversed timing, or cubic Bezier controls:

```lua
scene:shift(card, {4, 0}, 0.8, {preset = "back", strength = 1.35})
scene:shift(card, {-4, 0}, 0.8, {bezier = {0.18, 0.90, 0.28, 1}, reverse = true})
```

Trackers sample bound functions at frame cadence during authoring and retain
ordinary clips. At intermediate seek times, those states interpolate. Physical
spring displacement can be sampled with `vis.spring_response` and bound to an
elapsed-time tracker.

## Cameras and Views

`look` changes camera pose and lens. `focus` frames Object bounds or restores the
initial camera. `orbit` retains a signed camera turn around its target, including
complete revolutions. Camera clips and Object clips share Scene time.

A View shows the same Objects through another camera inside a frame rectangle:

```lua
local detail = scene:view({x = 0.65, y = 0.05, width = 0.3, height = 0.3}, {
    camera = {height = 2}, layer = 2,
})
```

Views sample the same content and time. `follow` centers on a moving Object;
`hidden` excludes selected families. Optical views scale strokes and billboard
text with magnification. View camera movement and fades have their own tracks.

A Viewport mounts an independently authored child Scene in a normalized rectangle.
It retains the child's camera, Theme, background, and namespace. Parent and child
sample the same global time.

## Scene transitions

A transition sequence plays independently authored stages, holding each for `hold`
and handing it over during `duration`. Live stages start at local time zero during
their incoming transition; `live=false` uses completed frames.

```lua
local root = vis.scene {}
local first = vis.scene {}
local second = vis.scene {}
first:circle {center = {-2, 0}, radius = 0.8, id = "subject"}
second:rectangle {center = {2, 0}, size = {2, 1}, id = "subject"}
root:scene_transition({first, second}, {duration = 1, hold = 0.5, curve = "smooth"})
return root
```

| Style | Handover |
|---|---|
| `match` | Pair equal IDs and cross-fade remaining content; default |
| `fade` | Dissolve the incoming stage over the outgoing stage |
| `slide` | Push stages edge to edge from the selected side |
| `zoom_in` | Enlarge an area into the viewport |
| `zoom_out` | Shrink from a detail area into the incoming stage |

`styles` selects each boundary independently. A zoom `area` can be an Object ID,
handle, or normalized rectangle. `match=true` adds ID matching to a slide or zoom.
Sequence span is the sum of stage holds plus `duration * (stageCount - 1)`.

See [Architecture](architecture.md) for rendering and
[Lua API](../tools/skill/vis-skills/references/lua.md) for complete call signatures.
