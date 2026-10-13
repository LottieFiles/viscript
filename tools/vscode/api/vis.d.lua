---@meta vis

-- Generated from api/vis-api.json. Do not edit directly.

---@alias vis.Color string
---@alias vis.ThemeColor 'background'|'foreground'|'muted'|'accent'|'secondary'|'success'|'warning'|'danger'|'info'|'surface'|'border'|'result'|'focus'
---@alias vis.Vec2 {[1]: number, [2]: number}
---@alias vis.Vec3 {[1]: number, [2]: number, [3]: number}
---@alias vis.Range {[1]: number, [2]: number, [3]?: number}
---@alias vis.Mat4 number[]
---@alias vis.Curve 'linear'|'smooth'|'ease_in'|'ease_out'|'ease_in_out'|'gentle'|'snappy'|{preset: string, strength?: number, reverse?: boolean}|{bezier: number[], strength?: number}
---@alias vis.CreateDirection 'forward'|'reverse'|'clockwise'|'counterclockwise'
---@alias vis.SceneTransitionStyleName 'match'|'fade'|'slide'|'zoom_in'|'zoom_out'
---@alias vis.PathCommand table

---@class vis.SpringResponseConfig
---@field mass number Positive finite mass in a consistent mass unit.
---@field stiffness number Positive finite stiffness in mass units/s^2; stiffness/mass must be finite and positive.
---@field damping number Non-negative finite damping in mass units/s; damping/mass must be finite.
---@field displacement number Finite initial displacement from equilibrium in scene units.
---@field velocity? number Finite initial velocity in scene units/s, default zero.

---@class vis.MotionState
---@field origin? vis.Vec2|vis.Vec3 Optional Object-local pivot; otherwise the Object-family bounds center is resolved when defining the state.
---@field shift? vis.Vec2|vis.Vec3 Parent-space translation relative to the authored Scene sample.
---@field rotation? number|vis.Vec2|vis.Vec3 Z radians or XYZ radians around origin.
---@field scale? vis.Vec2|vis.Vec3 XYZ scale around origin; a two-component value leaves Z at its default.
---@field opacity? number Multiplicative opacity in [0, 1].
---@field progress? number Multiplicative draw progress in [0, 1].

---@class vis.MotionTarget
---@field object vis.Object Live Object owned by the controller Scene.
---@field state string Previously defined state name for this Object.

---@class vis.MotionCurveRecord
---@field preset string Canonical preset name, or custom for a cubic Bézier curve.
---@field strength number Recorded curve strength.
---@field reverse boolean Whether the recorded curve is reversed.
---@field bezier? number[] Four control coordinates for a custom cubic Bézier curve.

---@class vis.MotionEvent
---@field time number Controller-local motion-clock time at acceptance.
---@field transaction string Controller-local uint64 transaction encoded as decimal text without precision loss.
---@field object integer Scene-local numeric Object identifier.
---@field state string Copied semantic state name.
---@field duration number Accepted transition duration.
---@field curve vis.MotionCurveRecord Accepted symbolic animation curve.

---@class vis.RuntimeConfig
---@field fixed_step? number Deterministic callback step in seconds; defaults to 1/120 and must be at most 1.
---@field max_steps? integer Maximum catch-up callbacks per host advance, from 1 to 64; defaults to 8.
---@field update fun(ctx:vis.Runtime, dt:number, time:number, tick:integer) Protected fixed-step callback retained with the Scene VM.

---@class vis.RuntimeObjectState
---@field origin? vis.Vec3 Optional fixed parent-space pivot; otherwise the authored Object family center is captured on first update.
---@field shift? vis.Vec3 Parent-space translation over the authored Scene state.
---@field rotation? number|vis.Vec3 Z radians or XYZ radians around origin.
---@field scale? vis.Vec3 XYZ scale around origin.
---@field opacity? number Multiplicative opacity in [0, 1].
---@field progress? number Multiplicative draw progress in [0, 1].
---@field fill? vis.Color|vis.ThemeColor Optional solid fill override resolved during the callback.

---@class vis.RuntimeSoundConfig
---@field bus? "music"|"effect"|"ui" Independent Audio bus description; defaults to effect.
---@field gain? number Linear playback gain in [0, 4].
---@field rate? number Playback rate in [0.125, 8].
---@field loop? boolean Whether an Audio-capable host should loop this immediate voice.

---@class vis.RuntimeActionState
---@field previous vis.Vec2 Aggregate action value in the previous consumed input frame.
---@field value vis.Vec2 Current clamped aggregate action value.
---@field delta vis.Vec2 Current value minus previous value.
---@field down boolean Whether any contributing source is currently held.
---@field pressed boolean Aggregate Down edge for the first fixed step consuming this host frame.
---@field released boolean Aggregate Up edge for the first fixed step consuming this host frame.

---@class vis.RuntimeKeyState
---@field begin number Time of the latest accepted key press.
---@field end number Time of the latest accepted key transition.
---@field previous boolean Held state in the previous consumed input frame.
---@field down boolean Current held state.
---@field pressed boolean Down edge for the first fixed step consuming this host frame.
---@field released boolean Up edge for the first fixed step consuming this host frame.

---@class vis.RuntimePointerState
---@field position vis.Vec2 Latest logical Scene pixel position.
---@field delta vis.Vec2 Accumulated logical-pixel motion in this input frame.
---@field wheel vis.Vec2 Accumulated wheel motion in this input frame.
---@field pointer integer Stable pointer identifier.
---@field previous_buttons integer Previous consumed button mask.
---@field buttons integer Current button mask.
---@field pressed integer Button Down-edge mask for the first fixed step consuming this frame.
---@field released integer Button Up-edge mask for the first fixed step consuming this frame.
---@field active boolean Whether the pointer has not been cancelled or released by the host.

---@class vis.Animation
---@field target vis.Object Distinct live target owned by this Scene.
---@field shift? vis.Vec2|vis.Vec3 Parent-space shift; mutually exclusive with transform.
---@field transform? vis.Mat4 Absolute local-to-parent transform; mutually exclusive with shift.
---@field opacity? number Target opacity from 0 to 1.
---@field stroke? vis.Color|vis.ThemeColor Target stroke color.
---@field fill? vis.Color|vis.ThemeColor Target fill color.
---@field dash_offset? number Target shaft dash offset; requires an existing non-empty dash pattern.
---@field tail? number Target solid tail-marker size for Arrow, Vector, Connector, or Route.
---@field tip? number Target solid tip-marker size for Arrow, Vector, Connector, or Route.
---@field arc? number Central angle of the circular arc every point travels.
---@field progress? number Target draw progress from 0 to 1.
---@field width? number Target stroke width in pixels.
---@field radius? number Target circle, arc or angle radius in world units, or point radius in pixels.
---@field center? vis.Vec2|vis.Vec3 Target arc center.
---@field vertex? vis.Vec2|vis.Vec3 Target angle vertex; from and to set its ray endpoints.
---@field start? number Target arc start direction in radians.
---@field angle? number Target arc sweep between -2 pi and 2 pi.
---@field size? number|vis.Vec2 Target rectangle size, or text size in points (em = 4/3 × size pixels, or world units for plane text).
---@field corner? number Target rectangle corner radius.
---@field trim_distance? number|vis.Vec2 Finite nonnegative object-local endpoint distances for a line, arrow or vector: one number for both ends, or {start, end}. Applied before transforms and drawing progress; omitted keeps the current trim. Consumed segments are empty, and warped geometry keeps its existing trim.
---@field gradient? vis.GradientConfig Target gradient stops and geometry.
---@field blur? number Target blur sigma in screen pixels.
---@field shadow? {color?: vis.Color|vis.ThemeColor, offset?: vis.Vec2, blur?: number} Target shadow; omitted fields keep their value.
---@field value? number|vis.Vec2|vis.Vec3 Target value of a number, matrix, or table entry, or the target value of a vector.
---@field points? vis.Vec2[]|vis.Vec3[] Target points of a polygon, route, or path of one contour, as many as the shape has, each moving in a straight line; a plot takes any number, cut as plot cuts them.
---@field fn? (fun(x:number, y?:number):number?)|vis.PlotFunctionSamples New plot graph fn(x), or contour child field fn(x,y), sampled once during authoring. Matching function grids and cuts blend y at fixed x and recut each sample; incompatible states switch halfway. A contour child keeps its grid ranges and level.
---@field x_range? vis.Range With fn, the samples of a plot's new graph; the plot keeps it for later states.
---@field y_range? vis.Vec2 With fn or points, the band a plot's new graph is cut to; the plot keeps it for later states.
---@field level? number Contour child's new scalar level, with fn or values. Omitted keeps its level. play traces the endpoint once; bind/track traces every baked sample.
---@field values? number[] Contour child's finite scalar grid values instead of fn, x varying fastest. Each sampled state charges grid samples to the point budget and retains traced plot geometry.
---@field discontinuities? number[] With fn or points, the x where a plot's new graph must not join; the plot keeps them for later states.
---@field from? vis.Vec2|vis.Vec3 Target start point of a line, arrow, or curve.
---@field to? vis.Vec2|vis.Vec3 Target end point of a line, arrow, or curve.
---@field origin? vis.Vec2|vis.Vec3 Target origin of a vector.
---@field control1? vis.Vec2|vis.Vec3 Target first control point of a curve.
---@field control2? vis.Vec2|vis.Vec3 Target second control point of a curve.
---@field text? string Target string of a text or label; characters turn from the left while the length moves to the new string's.
---@field retype? boolean With text, erase back to the beginning both strings share, then type the rest.
---@field layer? integer Target draw layer; the higher of the two holds while the play runs, and the objects below move with it.

---@class vis.DiagramLayersConfig
---@field zones? integer Sibling layer for zone backgrounds.
---@field routes? integer Sibling layer for edge routes.
---@field nodes? integer Sibling layer for nodes.
---@field annotations? integer Sibling layer for labels and annotations.

---@class vis.DiagramConfig
---@field id? string Stable generated subtree identifier.
---@field layout? "ranked"|"manual"|"grid"|"timeline" Bounded node layout strategy.
---@field direction? "lr"|"left_to_right"|"tb"|"top_to_bottom" Rank direction and automatic port-routing bias.
---@field origin? vis.Vec2 Layout origin; origin.x is time zero for timeline.
---@field node_size? vis.Vec2 Default node width and height.
---@field rank_gap? number Gap between consecutive ranks or grid columns.
---@field node_gap? number Gap between nodes in one rank or layout rows.
---@field time_unit? number Positive world-space width per timeline unit.
---@field zone_padding? number Padding around zone members.
---@field route_width? number Base route stroke width.
---@field arrow_length? number Arrowhead length.
---@field arrow_width? number Arrowhead width.
---@field corner? number Node and zone corner radius.
---@field layers? vis.DiagramLayersConfig Generated sibling layer assignments.

---@class vis.DiagramNodeConfig
---@field id string Unique node identifier.
---@field label? string Primary node label; defaults to id.
---@field detail? string Optional secondary node label.
---@field kind? "default"|"state"|"decision"|"terminal"|"entity"|"cell"|"task"|"evidence" Semantic Theme-derived node treatment.
---@field rank? integer Explicit ranked-layout rank.
---@field row? integer Zero-based grid or timeline row.
---@field column? integer Zero-based grid column.
---@field start? number Finite timeline start relative to origin.x.
---@field span? number Positive timeline duration in time units.
---@field position? vis.Vec2 Required node center for manual layout; optional ranked override.
---@field size? vis.Vec2 Per-node width and height override.

---@class vis.DiagramEdgeConfig
---@field id string Unique edge identifier.
---@field label? string Optional edge label.
---@field from vis.DiagramNode Source node handle from this Diagram.
---@field to vis.DiagramNode Destination node handle from this Diagram.
---@field from_port? "auto"|"left"|"right"|"top"|"bottom" Source attachment port.
---@field to_port? "auto"|"left"|"right"|"top"|"bottom" Destination attachment port.
---@field route? "auto"|"straight"|"orthogonal" Route strategy.
---@field kind? "directed"|"relation"|"return"|"error"|"optional" Semantic direction, marker, color, and dash treatment.
---@field waypoints? vis.Vec2[] One to sixteen explicit intermediate route points.
---@field flow? boolean Render the edge itself as one dashed directed route.
---@field flow_offset? number Initial shaft dash offset for a flow edge.

---@class vis.DiagramZoneConfig
---@field id string Unique zone identifier.
---@field label? string Optional zone label.
---@field members vis.DiagramNode[] One or more node handles from this Diagram.
---@field full_width? boolean Span all diagram nodes horizontally while retaining member-derived vertical bounds.

---@class vis.SceneConfig
---@field width? integer Canvas width in pixels.
---@field height? integer Canvas height in pixels.
---@field fps? number Authored frame rate.
---@field loop? boolean Whether playback loops.
---@field background? vis.Color Canvas background color.
---@field antialiasing? boolean Enable renderer antialiasing.
---@field theme? string|vis.ThemeConfig Built-in theme name (including adaptive_vscode in supporting IDE hosts) or custom theme.
---@field camera? vis.CameraConfig Camera view and interaction settings.

---@class vis.ThemeConfig
---@field preset? string Built-in Theme base preset.
---@field background? vis.Color Canvas background color.
---@field text? table H1, H2, H3, text, and code typography overrides.
---@field objects? vis.Color[] Automatic 1..10 color cycle shared in order by lhs, rhs, every later args member, and other peer categories.
---@field object_width? number Automatic object stroke width.
---@field gradient? boolean Enable automatic object gradients.
---@field end_gradient_stop? vis.Color Automatic gradient end color.
---@field axis? table Axis, grid, and label color overrides.
---@field colors? vis.SemanticThemeConfig Named semantic colors used by objects and emphasis effects.

---@class vis.SemanticThemeConfig
---@field foreground? vis.Color Primary foreground color.
---@field muted? vis.Color De-emphasized foreground color.
---@field accent? vis.Color Primary accent or non-result emphasis color.
---@field secondary? vis.Color Secondary emphasis color.
---@field success? vis.Color Successful or valid state color.
---@field warning? vis.Color Warning state color.
---@field danger? vis.Color Error or destructive state color.
---@field info? vis.Color Informational state color.
---@field surface? vis.Color Raised or grouped surface color.
---@field border? vis.Color Boundary and divider color.
---@field result? vis.Color Returned value, derived result, or output data-structure color.
---@field focus? vis.Color Transient current-subject or important-region color.

---@class vis.GradientConfig
---@field type? "linear"|"radial"|"conic" Gradient kind; required.
---@field stops? (vis.Color|vis.ThemeColor|table)[] 2 to 8 stops: bare colors are evenly spaced, or {offset, color} pairs with non-decreasing offsets in 0..1. Omit to ramp from the paint color to the end stop.
---@field from? vis.Vec2|vis.Vec3 Linear start point in local coordinates; requires to.
---@field to? vis.Vec2|vis.Vec3 Linear end point in local coordinates; requires from.
---@field center? vis.Vec2|vis.Vec3 Radial or conic center in local coordinates; defaults to the shape bbox center.
---@field radius? number Positive radial radius in local units; defaults to half the bbox diagonal or the circle radius.
---@field focal? vis.Vec2|vis.Vec3 Radial focal (start circle) center in local coordinates.
---@field focal_radius? number Non-negative radial focal (start circle) radius in local units.
---@field angle? number Conic start angle in degrees, clockwise from local +x as seen on screen.

---@class vis.ObjectConfig
---@field id? string Stable object identifier.
---@field opacity? number Opacity from 0 to 1.
---@field color? vis.Color|vis.ThemeColor Apply one literal or semantic Theme color to available stroke and fill paints.
---@field fill? vis.Color|vis.ThemeColor Literal or semantic Theme fill color.
---@field stroke? vis.Color|vis.ThemeColor Literal or semantic Theme stroke color.
---@field gradient? boolean|vis.Color|vis.ThemeColor|vis.GradientConfig true/false toggles the legacy two-stop linear ramp, a color sets its end stop, or a GradientConfig table selects a linear, radial, or conic gradient.
---@field width? number Stroke width in pixels.
---@field radius? number Style radius in pixels.
---@field dash? number[] Stroke dash pattern.
---@field dash_offset? number Stroke dash offset.
---@field progress? number Draw progress from 0 to 1.
---@field layer? integer Sibling paint order.
---@field isolate? boolean Draw this object and its descendants into one layer.
---@field mask? vis.Object|{target: vis.Object, mode?: string} Show this object only through another object.
---@field blend? string Blend mode with what is drawn before it, as CSS mix-blend-mode.
---@field blur? number Blur sigma in screen pixels, 0 to 256.
---@field shadow? {color?: vis.Color|vis.ThemeColor, offset?: vis.Vec2, blur?: number} Shadow beneath the object, offset in screen pixels.

---@class vis.GroupConfig: vis.ObjectConfig
---@field matrix? vis.Mat4 Local-to-parent transform.
---@field id? string Stable object identifier.
---@field typeset? boolean Audit every text below this Group as one typeset block.

---@class vis.SpaceConfig: vis.ObjectConfig
---@field axis_width? number Axis and tick stroke width in pixels, finite and non-negative; zero hides strokes. Defaults to max(grid width, 1.5).
---@field x? vis.Range Horizontal world range.
---@field y? vis.Range Vertical world range.
---@field z? vis.Range Depth world range.
---@field axis_x? vis.Color|vis.ThemeColor X-axis literal or semantic Theme color.
---@field axis_y? vis.Color|vis.ThemeColor Y-axis literal or semantic Theme color.
---@field axis_z? vis.Color|vis.ThemeColor Z-axis literal or semantic Theme color.
---@field numbers? boolean Draw axis numbers.
---@field number_mode? "fixed"|"relative" Number label sizing mode.
---@field number_size? number Number label size in points (em = 4/3 × size pixels).
---@field number_color? vis.Color|vis.ThemeColor Number label literal or semantic Theme color.
---@field ticks? boolean|vis.SpaceTicksConfig Ticks across the axes: true for one at every step of each range, or a step for each axis.
---@field tick_length? number Tick length in pixels, centered on the axis; 8 by default.
---@field labels? vis.SpaceLabelsConfig Labels beside the ticks, as Space:labels takes them.
---@field matrix? vis.Mat4 Local-to-parent transform.
---@field id? string Stable object identifier.

---@class vis.SpaceTicksConfig
---@field x? number|boolean Step between x ticks, which sit at its multiples inside the range but not at 0; 0 or false for none, true for the step of the range.
---@field y? number|boolean Step between y ticks, which sit at its multiples inside the range but not at 0; 0 or false for none, true for the step of the range.
---@field z? number|boolean Step between z ticks, which sit at its multiples inside the range but not at 0; 0 or false for none, true for the step of the range.

---@class vis.SpaceLabelsConfig
---@field placement? "world"|"screen" World placement is authored once (default). Screen placement keeps billboard Text ink outward from the projected space origin with a pixel gap at every sampled camera. Handles must be direct Text children of the Space. Place screen labels before authoring the Space's first warp; later warps retain its origin with the existing anchors. Adding or repositioning labels after a Space warp is refused even after seeking earlier. Screen placement does not take side or avoid other drawings or the canvas.
---@field x? {[1]: number, [2]: string|vis.Object}[] {value, label} pairs for the x axis: a string becomes a text in the space, and a handle keeps its parent and moves beside its tick.
---@field y? {[1]: number, [2]: string|vis.Object}[] {value, label} pairs for the y axis.
---@field z? {[1]: number, [2]: string|vis.Object}[] {value, label} pairs for the z axis.
---@field gap? number Pixels between the end of a tick and its label; 4 by default.
---@field size? number Size of the texts made from strings; the space's number_size by default.
---@field color? vis.Color|vis.ThemeColor Fill of the texts made from strings; the space's number_color by default.
---@field side? {x?: "below"|"above", y?: "left"|"right", z?: "left"|"right"} Side of each axis its labels sit on; below x and left of y and z by default.

---@class vis.PointConfig: vis.ObjectConfig
---@field point vis.Vec2|vis.Vec3 Point in parent coordinates.
---@field radius? number Marker radius in pixels.

---@class vis.LineConfig: vis.ObjectConfig
---@field trim_distance? number|vis.Vec2 Finite nonnegative object-local endpoint distances, before transforms and drawing progress: one number for both ends, or {start, end}. Defaults to zero; a consumed segment has no drawing, bounds or outline. Warped geometry keeps its existing trim.
---@field from vis.Vec2|vis.Vec3 Start point.
---@field to vis.Vec2|vis.Vec3 End point.
---@field stroke? vis.Color|vis.ThemeColor Literal or semantic Theme stroke color.
---@field width? number Stroke width in pixels.

---@class vis.ArrowConfig: vis.ObjectConfig
---@field trim_distance? number|vis.Vec2 Finite nonnegative object-local endpoint distances, before transforms and drawing progress: one number for both ends, or {start, end}. Defaults to zero; a consumed segment has no drawing, bounds or outline. Warped geometry keeps its existing trim.
---@field from vis.Vec2|vis.Vec3 Start point.
---@field to vis.Vec2|vis.Vec3 End point.
---@field tail? number Tail size in pixels.
---@field tip? number Tip size in pixels.

---@class vis.VectorConfig: vis.ObjectConfig
---@field trim_distance? number|vis.Vec2 Finite nonnegative object-local endpoint distances, before transforms and drawing progress: one number for both ends, or {start, end}. Defaults to zero; a consumed segment has no drawing, bounds or outline. Warped geometry keeps its existing trim.
---@field origin? vis.Vec2|vis.Vec3 Vector origin.
---@field value vis.Vec2|vis.Vec3 Vector value.
---@field tail? number Tail size in pixels.
---@field tip? number Tip size in pixels.

---@class vis.CircleConfig: vis.ObjectConfig
---@field center? vis.Vec2|vis.Vec3 Circle center.
---@field radius? number Radius in world units.

---@class vis.ArcConfig: vis.ObjectConfig
---@field center? vis.Vec2|vis.Vec3 Center in local coordinates; defaults to the origin.
---@field radius? number Positive radius in world units, 1 by default.
---@field start? number Radians counterclockwise from +x, 0 by default.
---@field angle? number Signed sweep between -2 pi and 2 pi; pi/2 by default.
---@field sector? boolean Close through the center to make a fillable pie; false by default.

---@class vis.AngleConfig: vis.ObjectConfig
---@field vertex? vis.Vec2|vis.Vec3 Vertex in local coordinates, the origin by default.
---@field from? vis.Vec2|vis.Vec3 First ray endpoint; defaults to {1, 0}.
---@field to? vis.Vec2|vis.Vec3 Second ray endpoint; defaults to {0, 1}.
---@field radius? number Positive radius in world units, 1 by default.
---@field other? boolean Select the complementary outer angle; false by default.
---@field right? "auto"|"never"|"always" Use a square at pi/2 within 0.00001 radians, never, or for every angle; auto by default.
---@field label? string|vis.TextConfig|vis.TexConfig Text or a text/tex descriptor attached as a child at the bisector.
---@field label_distance? number Non-negative bisector distance in world units; 1.7 times the initial radius by default.

---@class vis.Bounds
---@field left number Least x in world units.
---@field right number Greatest x in world units.
---@field bottom number Least y in world units.
---@field top number Greatest y in world units.
---@field width number right - left.
---@field height number top - bottom.
---@field center vis.Vec3 Middle of the bounds.
---@field min vis.Vec3 Corner of the least x, y and z.
---@field max vis.Vec3 Corner of the greatest x, y and z.

---@class vis.RectangleConfig: vis.ObjectConfig
---@field center? vis.Vec2|vis.Vec3 Rectangle center.
---@field size? vis.Vec2 Width and height in world units; or use around.
---@field corner? number Corner radius.
---@field around? vis.Object|vis.Object[] Handles to frame; the rectangle takes their bounds.
---@field padding? number|vis.Vec2 Space around the framed handles in world units (0.1 by default).

---@class vis.BraceConfig: vis.ObjectConfig
---@field around? vis.Object|vis.Object[] Handles the brace spans: it lies along the side of their bounds that faces direction, its ends where that side ends; or use from and to.
---@field from? vis.Vec2|vis.Vec3 With to, the ends of a segment the brace spans, in place of around.
---@field to? vis.Vec2|vis.Vec3 With from, the other end of the segment the brace spans.
---@field direction? "down"|"up"|"left"|"right"|vis.Vec2 Where the tip points: down by default, or for from and to to the right of the way from from to to.
---@field offset? number Nonnegative parent-space distance from the spanned bounds or segment to the brace; defaults to 0.25 times configured text size, camera-converted for billboard text.
---@field depth? number Positive parent-space distance from the brace ends to its tip; defaults to 0.4 times configured text size, camera-converted for billboard text.
---@field label? string|vis.Object A string set as a text at the tip, its ink gap away, or a handle in the same group moved there by its bounds; returned as the second value.
---@field gap? number Nonnegative parent-space distance from the tip to the label bounds (ink for a string label); defaults to 0.2 times configured text size, camera-converted for billboard text.
---@field size? number Size of the text the brace is set with, in points (em = 4/3 × size pixels for billboard text): the label's size, and the size the brace's defaults follow.
---@field font? string Registered font or asset name of the label.
---@field role? string Theme typography role whose font, size and color the label and the brace take.
---@field orientation? "billboard"|"plane" Billboard text keeps its pixel size; plane text, and its size, lie in world units.

---@class vis.PolygonConfig: vis.ObjectConfig
---@field points vis.Vec2[]|vis.Vec3[] At least three polygon vertices.

---@class vis.PlotFunctionSamples
---@field points vis.Vec2[]|vis.Vec3[] Exact clipped endpoint geometry; non-finite points break pieces and an empty list draws nothing.
---@field samples vis.Vec2[]|vis.Vec3[] Uncut column with increasing x: nan x marks a discontinuity, nan y an undefined value. At most 1000000 entries, copied for vertical blending.

---@class vis.PlotConfig: vis.ObjectConfig
---@field endpoint_radius? number Solid active draw endpoint radius in pixels, using the sampled stroke property color independently of the plot gradient. Positive also shows the first vertex at manually set progress 0; scheduled create and uncreate keep their empty boundaries blank.
---@field points? vis.Vec2[]|vis.Vec3[] Explicit plot samples; a point with a coordinate that is not finite breaks the line.
---@field fn? (fun(x:number):number?)|vis.PlotFunctionSamples Function sampled during authoring, or materialized samples from the JavaScript builder. Undefined values break the graph; retained columns blend vertically in play.
---@field x_range? vis.Range Inclusive sampling range.
---@field y_range? vis.Vec2 Band {lo, hi} the graph is cut to: it ends exactly on the band's edge where it leaves, starts again where it returns, and ends flat.
---@field discontinuities? number[] x where the graph must not join: fn is sampled a thousandth of a step on either side of each instead, a polyline breaks across each, and the plot ends flat.

---@class vis.AreaConfig: vis.ObjectConfig
---@field fn fun(x:number):number Function whose graph bounds the region.
---@field x_range vis.Range Sampling range {a, b, step}.
---@field base? number|fun(x:number):number Lower boundary: a constant (0 by default) or a function.

---@class vis.RiemannConfig: vis.ObjectConfig
---@field fn? fun(x:number):number Function whose heights the rectangles take.
---@field values? number[] Heights given directly, one per step; instead of fn.
---@field x_range vis.Range Interval and width {a, b, dx}.
---@field sample? "left"|"right"|"midpoint" Where each step reads fn.
---@field negative? vis.Color|vis.ThemeColor Fill of the rectangles below the axis.

---@class vis.ContourLabelConfig
---@field format? "%.0f"|"%.1f"|"%.2f"|"%.3f"|"%.4f"|"%.5f"|"%.6f" Fixed-point level format, zero to six decimals.
---@field size? number Positive plane text size in contour-local units.
---@field spacing? number Non-negative local repeat distance; zero places one label per connected curve.
---@field padding? number Non-negative local stroke-gap and label clearance padding.
---@field upright? boolean Keep the local tangent angle between -pi/2 and pi/2.

---@class vis.ContourConfig: vis.ObjectConfig
---@field fn? fun(x:number, y:number):number Finite scalar field sampled at construction and during 40 edge bisections; instead of values.
---@field values? number[] Finite scalar samples, x varying fastest, row by row from the bottom left; crossings follow the bilinear grid instead of fn.
---@field levels number[] 1 to 1024 finite levels, in child order; a single level uses {0}. Empty levels of the field keep an empty plot handle.
---@field x_range vis.Range Horizontal samples from min every step up to max, at least two.
---@field y_range vis.Range Vertical samples from min every step up to max, at least two. Grid at most 65536 samples, samples times levels at most 1000000.
---@field colors? (vis.Color|vis.ThemeColor)[]|"viridis"|"plasma"|"inferno"|"magma"|"cividis" One stroke color per level, or a colormap over the smallest to largest level; overrides stroke and color. Without it the common stroke or color applies.
---@field labels? boolean|vis.ContourLabelConfig Level text children with tangent rotation and stroke gaps; false by default. True uses default options. Placement follows sampled geometry and avoids the grid edge and earlier contour labels in the same parent.

---@class vis.VectorFieldConfig: vis.ObjectConfig
---@field fn? fun(x:number, y:number):number|vis.Vec2, number? Field returning two numbers or {x, y} at a point.
---@field values? vis.Vec2[] Vectors given directly, row by row from the bottom left; instead of fn.
---@field x_range vis.Range Horizontal samples {min, max, step}.
---@field y_range vis.Range Vertical samples {min, max, step}.
---@field length? number Length arrows approach as length * tanh(|F| / length).
---@field scale? number Multiple of each vector's own length to draw instead of length.
---@field anchor? "tail"|"middle" Start each arrow at its sample or center it there.
---@field colors? (vis.Color|vis.ThemeColor)[] 2 to 8 colors from the least magnitude to the greatest.
---@field magnitude? vis.Vec2 Magnitude range {low, high} the colors spread over.
---@field tip? number Arrowhead size in pixels.

---@class vis.StreamLinesConfig: vis.ObjectConfig
---@field fn fun(x:number, y:number):number|vis.Vec2, number? Field returning two numbers or {x, y} at a point.
---@field x_range? vis.Range Grid of seeds across, which also bounds the lines.
---@field y_range? vis.Range Grid of seeds up, which also bounds the lines.
---@field seeds? vis.Vec2[] Points the lines start from.
---@field time? number Field time each line runs for.
---@field dt? number Runge-Kutta step.
---@field direction? "forward"|"backward"|"both" Which way from each seed to trace.
---@field colors? (vis.Color|vis.ThemeColor)[] 2 to 8 colors from the least mean speed to the greatest.
---@field magnitude? vis.Vec2 Speed range {low, high} the colors spread over.

---@class vis.RouteConfig: vis.ObjectConfig
---@field points vis.Vec2[]|vis.Vec3[] At least two ordered route vertices.
---@field tail? number Solid tail-marker size in pixels.
---@field tip? number Solid tip-marker size in pixels.

---@class vis.PathConfig: vis.ObjectConfig
---@field commands vis.PathCommand[] Move, line, quadratic, cubic, and close commands.
---@field samples? integer Curve sampling count.

---@class vis.CombineConfig: vis.ObjectConfig
---@field of vis.Object[] 2 to 64 circles, rectangles, polygons, and closed paths, folded from the left.

---@class vis.CurveConfig: vis.ObjectConfig
---@field from vis.Vec2|vis.Vec3 Start point.
---@field control1 vis.Vec2|vis.Vec3 First control point.
---@field control2 vis.Vec2|vis.Vec3 Second control point.
---@field to vis.Vec2|vis.Vec3 End point.
---@field samples? integer Curve sampling count.

---@class vis.SurfaceConfig: vis.ObjectConfig
---@field points vis.Vec3[] Row-major surface vertices.
---@field size vis.Vec2 Column and row count.
---@field mode? "solid"|"mesh"|"solid_mesh" Surface drawing mode.
---@field shading? boolean Enable surface shading.
---@field light? vis.SurfaceLightConfig Light fixed in the world in place of the camera headlight; needs shading.
---@field mesh? vis.Vec2 Mesh lines {columns, rows} of a mesh or solid_mesh surface, each 0 or 2 up to the points that way, spread evenly; omitted, every face draws its outline.
---@field colors? "viridis"|"plasma"|"inferno"|"magma"|"cividis"|vis.SurfaceColormapConfig|(vis.Color|vis.ThemeColor)[]|fun(x:number, y:number, z:number):vis.Color|vis.ThemeColor Face colors instead of the fill: a colormap over the heights of the points, a colormap table, one color per point, or a function of each point.

---@class vis.SurfaceLightConfig
---@field direction vis.Vec3 Direction from the surface toward the light, fixed in the world.
---@field ambient? number Share of its color a face keeps where the light does not reach, 0 to 1; a half by default.

---@class vis.SurfaceColormapConfig
---@field map "viridis"|"plasma"|"inferno"|"magma"|"cividis" Named colormap sampled at nine evenly spaced stops.
---@field range? vis.Vec2 Heights {low, high} of the first and last colors; the extent of the points by default, low above high reverses.
---@field axis? "x"|"y"|"z"|vis.Vec3 Direction heights run along; z by default.

---@class vis.TextConfig: vis.ObjectConfig
---@field tabular? boolean Center ASCII digits, minus and spaces in cells of the widest digit advance; false by default. Other characters keep their advances.
---@field reserve? string Complete sample string reserving the larger box of it and the starting string. Later strings may overflow.
---@field text string Text content.
---@field point? vis.Vec2|vis.Vec3 Text anchor point.
---@field font? string Registered font or asset name.
---@field size? number Font size in points (em = 4/3 × size pixels, or world units for plane text).
---@field align? vis.Vec2 Horizontal and vertical alignment.
---@field baseline? boolean Pin the first baseline at point, measured as the text is made; align.x still places the text along it and align.y is ignored.
---@field role? string Theme typography role.
---@field fill? vis.Color|vis.ThemeColor Literal or semantic Theme text color.
---@field orientation? "billboard"|"plane" Billboard text keeps its pixel size; plane text lies in world units.
---@field slant? number How far the glyphs lean right per unit of height above the baseline, -1 to 1.
---@field measure? boolean Measure the line box while the script runs so that layout can use it.

---@class vis.SpansConfig: vis.ObjectConfig
---@field split? "none"|"words"|"letters" Cut strings into one text per word or per character.
---@field wrap? number Width in world units at which a row wraps.
---@field leading? number Baseline-to-baseline spacing as a multiple of the line height; a line moves down only where its ink would reach into the line above.
---@field color? vis.Color|vis.ThemeColor Color of strings and parts that set none.
---@field point? vis.Vec2|vis.Vec3 Anchor point.
---@field align? vis.Vec2 Horizontal and vertical alignment.
---@field baseline? boolean Pin the first baseline at point; align.x still places the lines along it and align.y is ignored.
---@field size? number Font size in points (em = 4/3 × size pixels, or world units for plane text).
---@field font? string Registered font or asset name.
---@field role? string Theme typography role.
---@field orientation? "billboard"|"plane" Billboard text keeps its pixel size; plane text lies in world units.

---@class vis.CodeConfig: vis.ObjectConfig
---@field text string Source code.
---@field language? "plain"|"lua"|"python"|"javascript"|"typescript"|"c"|"cpp" Language that classifies the tokens.
---@field leading? number Positive line spacing as a multiple of the line height (default 1); exclusive with pitch.
---@field pitch? number Positive distance between baselines in world units; exclusive with leading.
---@field baseline? boolean Pin the first line baseline at point; align.x places it and align.y is ignored.
---@field color? vis.Color|vis.ThemeColor One color for every token.
---@field colors? table<string, vis.Color|vis.ThemeColor> Colors by token class: plain, keyword, string, number, comment, function, type, operator.
---@field point? vis.Vec2|vis.Vec3 Anchor point.
---@field align? vis.Vec2 Horizontal and vertical alignment.
---@field size? number Font size in points (em = 4/3 × size pixels, or world units for plane text).
---@field font? string Registered font name; defaults to the Theme code font, monospaced JetBrainsMono in every preset.
---@field orientation? "billboard"|"plane" Billboard text keeps its pixel size; plane text lies in world units.

---@class vis.TexConfig: vis.ObjectConfig
---@field tex string|(string|vis.TexPartConfig)[] TeX math, such as "a^2+b^2=c^2", or its parts, each set as a Group of its own below the formula.
---@field point? vis.Vec2|vis.Vec3 Where the formula's box, its width by its height and depth, is pinned.
---@field align? vis.Vec2 Horizontal and vertical alignment.
---@field baseline? boolean Pin the formula's baseline at point, not its box; align.x still places it along the baseline and align.y is ignored.
---@field size? number Font size in points (em = 4/3 × size pixels, or world units for plane text).
---@field font? string Registered font or asset name.
---@field role? string Theme typography role whose font, size and color the formula takes.
---@field orientation? "billboard"|"plane" Billboard glyphs keep their pixel size; plane glyphs lie in world units.
---@field display? boolean Display style, as $$...$$, unless false sets text style, as inline $...$.
---@field color? vis.Color|vis.ThemeColor Fill that colors the glyphs and shapes, as fill does.
---@field fill? vis.Color|vis.ThemeColor Fill that colors the glyphs and shapes.

---@class vis.TexPartConfig
---@field text? string TeX of the part, unless it comes first, as in {"c^2", color = "accent"}.
---@field color? vis.Color|vis.ThemeColor Fill that colors the part, as fill does.
---@field fill? vis.Color|vis.ThemeColor Fill that colors the part.
---@field id? string Stable object identifier.
---@field opacity? number Opacity from 0 to 1.
---@field progress? number Draw progress from 0 to 1.
---@field layer? integer Sibling paint order.
---@field isolate? boolean Draw this object and its descendants into one layer.
---@field mask? vis.Object|{target: vis.Object, mode?: string} Show this object only through another object.
---@field blend? string Blend mode with what is drawn before it, as CSS mix-blend-mode.
---@field blur? number Blur sigma in screen pixels, 0 to 256.
---@field shadow? {color?: vis.Color|vis.ThemeColor, offset?: vis.Vec2, blur?: number} Shadow beneath the object, offset in screen pixels.

---@class vis.TexSpanConfig
---@field tex string|(string|vis.TexPartConfig)[] TeX math set inline in the row, such as "x^2", or its parts, each a Group of its own below the formula.
---@field display? boolean Display style, as $$...$$; an inline formula takes text style unless true.
---@field size? number Font size in points (em = 4/3 × size pixels, or world units for plane text); the row's unless set.
---@field font? string Registered font or asset name; the row's unless set.
---@field role? string Theme typography role whose font, size and color the formula takes; the row's unless set.
---@field rise? number Raises the baseline by this fraction of the row's line height; negative lowers it.
---@field color? vis.Color|vis.ThemeColor Fill that colors the formula, as fill does; the row's unless set.
---@field fill? vis.Color|vis.ThemeColor Fill that colors the formula; the row's unless set.
---@field id? string Stable object identifier.
---@field opacity? number Opacity from 0 to 1.
---@field progress? number Draw progress from 0 to 1.
---@field layer? integer Sibling paint order.
---@field isolate? boolean Draw this object and its descendants into one layer.
---@field mask? vis.Object|{target: vis.Object, mode?: string} Show this object only through another object.
---@field blend? string Blend mode with what is drawn before it, as CSS mix-blend-mode.
---@field blur? number Blur sigma in screen pixels, 0 to 256.
---@field shadow? {color?: vis.Color|vis.ThemeColor, offset?: vis.Vec2, blur?: number} Shadow beneath the object, offset in screen pixels.

---@class vis.NumberConfig: vis.ObjectConfig
---@field tabular? boolean Center ASCII digits, minus and spaces in cells of the widest digit advance; false by default. Other characters keep their advances.
---@field reserve? string Complete sample string reserving the larger box of it and the starting string. Later strings may overflow.
---@field value number Value the text writes; play and set count it.
---@field decimals? integer Digits after the point, 0 to 6.
---@field sign? boolean Write + before a positive value.
---@field separator? string Group the digits in threes with this string.
---@field prefix? string Text before the number.
---@field suffix? string Text after the number.
---@field slant? number How far the glyphs lean right per unit of height above the baseline, -1 to 1.
---@field measure? boolean Box the number it starts with.
---@field point? vis.Vec2|vis.Vec3 Anchor point.
---@field align? vis.Vec2 Horizontal and vertical alignment.
---@field baseline? boolean Pin the first baseline at point, measured as the text is made; align.x still places the text along it and align.y is ignored.
---@field size? number Font size in points (em = 4/3 × size pixels, or world units for plane text).
---@field font? string Registered font or asset name.
---@field role? string Theme typography role.
---@field orientation? "billboard"|"plane" Billboard text keeps its pixel size; plane text lies in world units.

---@class vis.MatrixConfig: vis.ObjectConfig
---@field tabular? boolean Center ASCII digits, minus and spaces in cells of the widest digit advance; false by default. Other characters keep their advances.
---@field reserve? string|string[] Sample for every column, or one string per column, reserving at least that width. Later strings may overflow.
---@field rows (number|string)[][] Rows of entries; numbers count.
---@field brackets? "square"|"round"|"bar"|"none" Bracket style.
---@field gap? number|vis.Vec2 Column and row spacing in world units.
---@field justify? "left"|"center"|"right"|string[] Entry alignment in every column, or one per column.
---@field decimals? integer Digits after the point for number entries.
---@field point? vis.Vec2|vis.Vec3 Anchor point.
---@field align? vis.Vec2 Horizontal and vertical alignment.
---@field size? number Font size in points (em = 4/3 × size pixels, or world units for plane text).
---@field font? string Registered font or asset name.
---@field role? string Theme typography role.
---@field orientation? "billboard"|"plane" Billboard text keeps its pixel size; plane text lies in world units.

---@class vis.TableConfig: vis.ObjectConfig
---@field rows (number|string)[][] Rows of entries below the header; numbers count.
---@field header? (number|string)[] Header row, one entry per column.
---@field padding? number|vis.Vec2 Space around each entry in its cell.
---@field justify? "left"|"center"|"right"|string[] Entry alignment in every column, or one per column.
---@field decimals? integer Digits after the point for number entries.
---@field stripes? boolean|vis.Color|vis.ThemeColor Shade every other body row.
---@field rules? "header"|"rows"|"columns"|"grid"|"none" Which rules to draw.
---@field point? vis.Vec2|vis.Vec3 Anchor point.
---@field align? vis.Vec2 Horizontal and vertical alignment.
---@field size? number Font size in points (em = 4/3 × size pixels, or world units for plane text).
---@field font? string Registered font or asset name.
---@field role? string Theme typography role.
---@field orientation? "billboard"|"plane" Billboard text keeps its pixel size; plane text lies in world units.

---@class vis.PictureConfig: vis.ObjectConfig
---@field asset? string Registered asset name, or a path relative to the script (svg, png, jpg, webp).
---@field pixels? vis.Color[] Dense row-major inline pixels.
---@field size? vis.Vec2 Inline pixel dimensions.
---@field center? vis.Vec2|vis.Vec3 Picture center.
---@field width? number Picture width in world units.
---@field filter? "bilinear"|"nearest" Sampling filter.

---@class vis.CellConfig: vis.ObjectConfig
---@field color? vis.Color|vis.ThemeColor Base literal or semantic Theme cell color.
---@field origin? vis.Vec2|vis.Vec3 Grid origin.
---@field size vis.Vec2 Grid columns and rows.
---@field mode? "full"|"padd" Cell rendering mode.
---@field padding? number Cell padding.
---@field depth? number Cell depth.
---@field texture? string Texture asset name.
---@field source? table Source texture region.
---@field destination? table Destination cell region.
---@field patches? table[] Region color patches supporting literal or semantic Theme colors.

---@class vis.ConnectorConfig: vis.ObjectConfig
---@field from vis.Object Source object.
---@field to vis.Object Destination object.
---@field padding? number World distance beyond each outline along the connector.
---@field tail? number Tail size in pixels.
---@field tip? number Tip size in pixels.

---@class vis.ViewportConfig
---@field x number Normalized left coordinate.
---@field y number Normalized top coordinate.
---@field width number Normalized width.
---@field height number Normalized height.

---@class vis.ViewConfig
---@field camera? vis.LookConfig The camera the view opens with, as a look over the scene's camera; its height or field of view spans the rectangle's height.
---@field optical? boolean Grow stroke widths, point radii, dashes and billboard text with the view's magnification over the scene's camera; true by default.
---@field background? vis.Color|vis.ThemeColor Fill below what the view draws; the scene's background by default, and a transparent color lets the frame show through.
---@field opacity? number Starting opacity of the whole view, from 0 to 1.
---@field layer? integer The view draws the objects on this layer and below and sits above them; objects on higher layers draw over it. Above every layer by default.
---@field hide? vis.Object|vis.Object[] Objects the view leaves out, each with everything below it, up to 256.
---@field follow? vis.Object An object whose drawn center the view's camera keeps as its target.

---@class vis.SceneTransitionConfig: vis.TimingOptions
---@field duration? number Transition duration in seconds.
---@field curve? vis.Curve Animation curve.
---@field hold? number|number[] Seconds fully shown: one non-negative value for all stages or one per stage.
---@field live? boolean Play stages from the start of their incoming transition, holding the last frame when finished; true by default. False shows completed frames.
---@field viewport? vis.ViewportConfig Normalized viewport.
---@field style? "match"|"fade"|"slide"|"zoom_in"|"zoom_out" How every transition hands one scene over to the next; match by default.
---@field from? "left"|"right"|"top"|"bottom" Side a slide pushes the next scene in from; right by default.
---@field area? vis.Object|string|vis.ViewportConfig Zoom area: an object geometry box at its stage end, its id, or a normalized part of the viewport.
---@field area_margin? number Object zoom margin per side as a fraction of its larger normalized geometry side, limited by viewport space. Finite and non-negative; 0.05 by default. Zero uses exact geometry before aspect widening and clamping. Only for object or id areas.
---@field match? boolean Let a slide or zoom also carry equal-id objects above both scenes.
---@field styles? (vis.SceneTransitionStyleName|vis.SceneTransitionStyleConfig)[] A style name or style table for each transition, in order, instead of style, from, area, area_margin and match.

---@class vis.SceneTransitionStyleConfig
---@field style? "match"|"fade"|"slide"|"zoom_in"|"zoom_out" How this transition hands one scene over to the next; match by default.
---@field from? "left"|"right"|"top"|"bottom" Side a slide pushes the next scene in from; right by default.
---@field area? vis.Object|string|vis.ViewportConfig Zoom area: an object geometry box at its stage end, its id, or a normalized part of the viewport.
---@field area_margin? number Object zoom margin per side as a fraction of its larger normalized geometry side, limited by viewport space. Finite and non-negative; 0.05 by default. Zero uses exact geometry before aspect widening and clamping. Only for object or id areas.
---@field match? boolean Let a slide or zoom also carry equal-id objects above both scenes.

---@class vis.FadeConfig: vis.TimingOptions
---@field duration? number Duration in seconds.
---@field curve? vis.Curve Animation curve.
---@field shift? vis.Vec2|vis.Vec3 Parent-space offset.
---@field scale? number Relative scale.

---@class vis.RotateConfig: vis.AnimationOptions
---@field duration? number Duration in seconds.
---@field curve? vis.Curve Animation curve.
---@field lag? number Delay of each later target in seconds.
---@field about? "center"|"left"|"right"|"top"|"bottom"|"top_left"|"top_right"|"bottom_left"|"bottom_right"|vis.Vec2|vis.Vec3|vis.Object Pivot: an anchor of the target's bounds, a parent-space point, or another object's center.
---@field axis? vis.Vec3 Rotation axis.
---@field shift? vis.Vec2|vis.Vec3 Parent-space shift in the same clip.
---@field arc? number Central angle of the arc the pivot travels.

---@class vis.ScaleConfig: vis.AnimationOptions
---@field duration? number Duration in seconds.
---@field curve? vis.Curve Animation curve.
---@field lag? number Delay of each later target in seconds.
---@field about? "center"|"left"|"right"|"top"|"bottom"|"top_left"|"top_right"|"bottom_left"|"bottom_right"|vis.Vec2|vis.Vec3|vis.Object Pivot: an anchor of the target's bounds, a parent-space point, or another object's center.
---@field shift? vis.Vec2|vis.Vec3 Parent-space shift in the same clip.
---@field arc? number Central angle of the arc the pivot travels.

---@class vis.MoveAlongConfig: vis.AnimationOptions
---@field duration? number Duration in seconds.
---@field curve? vis.Curve Animation curve.
---@field lag? number Delay of each later target in seconds.
---@field from? number Fraction of the outline to start at.
---@field to? number Fraction of the outline to end at; below from travels backwards.
---@field orient? boolean Turn the target with the path.

---@class vis.AdvectConfig: vis.TimingOptions
---@field duration? number Duration in seconds.
---@field curve? vis.Curve Curve of the field time; linear by default.
---@field speed? number Field time per second; negative runs the flow backward.

---@class vis.WarpConfig
---@field step? number Longest straight piece, in the parent space of each target, that the warp maps without cutting it first; 0.05 by default.

---@class vis.WarpSamples
---@field x_range vis.Range Horizontal samples {min, max, step}, two or more.
---@field y_range vis.Range Vertical samples {min, max, step}, two or more.
---@field values vis.Vec2[] Image of each sample, row by row from the bottom left; a Catmull-Rom spline through them maps every point, which must lie inside the samples, and keeps its z.

---@class vis.MatchTransformConfig
---@field path? vis.Object Snapshot outline guide, at most 2048 samples: add its arc-length detour from its endpoint chord to each paired piece while preserving individual endpoints and spacing. No live connectors or nonzero arc.
---@field arc? number Central angle of the arc matched parts travel.
---@field lag? number Delay of each later pair in seconds.
---@field pairs? vis.Object[][] Explicit {source_piece, target_piece} pairs.
---@field strings? boolean Match texts with equal strings, and the pieces of formulas by their TeX; false keeps only the given pairs.
---@field diff? boolean Match lines and tokens in order, as a diff.
---@field shapes? boolean Split or merge shapes between counts.
---@field fade? number Fraction of the duration unmatched parts fade over.
---@field copy? boolean Match a copy of the source made at the cursor and leave the source in place; the source may be moving, and pairs still name its pieces.

---@class vis.MorphConfig
---@field pairs? number[][] Explicit one-based source and target contour indices; each piece pairs once, and remaining pieces pair automatically. The same indices apply to each object pair in an array call. Surfaces take no pairs.
---@field lag? number Delay of each later pair in seconds.
---@field copy? boolean Morph a copy of each source made at the cursor and leave the source in place; the copy is removed when the morph ends.

---@class vis.SearchConfig
---@field match? "first"|"last"|"all"|integer Which matches to take: the first, the last, every one, or the nth.
---@field lines? integer|integer[] A line, or {first, last} lines, to look in, counting from 1.
---@field whole? boolean Take only matches that cut no token.

---@class vis.CodeEdit
---@field replace? string|integer|integer[] Text to find, or a line or {first, last} lines, to replace.
---@field with? string Text that replace puts in.
---@field insert? string Text to put before or after a match, or lines to put before or after a line.
---@field before? string|integer Text to find, or a line, that insert goes in front of.
---@field after? string|integer Text to find, or a line, that insert follows.
---@field remove? string|integer|integer[] Text to find, or a line or {first, last} lines, to remove.
---@field append? string Lines to add after the last line.
---@field prepend? string Lines to add before the first line.
---@field match? "first"|"last"|"all"|integer Which matches to take: the first, the last, every one, or the nth.
---@field lines? integer|integer[] A line, or {first, last} lines, to look in, counting from 1.
---@field whole? boolean Take only matches that cut no token.

---@class vis.EditConfig
---@field id? string Id of the new block; by default the block's id with the first free .2, .3, and so on.
---@field arc? number Central angle of the arc matched parts travel.
---@field lag? number Delay of each later pair in seconds.
---@field fade? number Fraction of the duration unmatched parts fade over.

---@class vis.HighlightConfig: vis.TimingOptions
---@field duration? number Duration in seconds.
---@field curve? vis.Curve Animation curve.
---@field dim? number Opacity of what is not highlighted.

---@class vis.ArrangeConfig
---@field direction? vis.Vec2|vis.Vec3 Direction of the row.
---@field gap? number Space between neighbors in world units.
---@field center? vis.Vec2|vis.Vec3 Point the row centers on in the parent.

---@class vis.LookConfig
---@field view? "2d"|"3d" Camera view.
---@field target? vis.Vec2|vis.Vec3|vis.Object|vis.Object[] Point to aim at, or objects to center on without zooming.
---@field eye? vis.Vec3 3D camera eye.
---@field up? vis.Vec3 3D camera up vector.
---@field fov? number Perspective field of view in radians.
---@field height? number Orthographic view height.
---@field near? number Near clipping plane.
---@field far? number Far clipping plane.
---@field projection? "perspective"|"orthographic" 3D projection mode.

---@class vis.OrbitConfig: vis.TimingOptions
---@field duration? number Duration in seconds.
---@field curve? vis.Curve Animation curve.
---@field axis? vis.Vec3 Axis to orbit about; the camera's up by default.

---@class vis.FocusConfig: vis.TimingOptions
---@field duration? number Duration in seconds.
---@field curve? vis.Curve Animation curve.
---@field padding? number Margin on each side as a fraction of the framed span.

---@class vis.CloneConfig
---@field id? string Id of the copy; without one it takes the source's id and :copy, then :copy2 and so on. The ids below the copy follow the copy's.
---@field parent? vis.Object Object the copy is attached below; beside the source by default.
---@field shift? vis.Vec2|vis.Vec3 Move the copy by this vector before it is attached; a connector follows its endpoints instead.
---@field opacity? number Opacity from 0 to 1.
---@field color? vis.Color|vis.ThemeColor Apply one literal or semantic Theme color to available stroke and fill paints.
---@field fill? vis.Color|vis.ThemeColor Literal or semantic Theme fill color.
---@field stroke? vis.Color|vis.ThemeColor Literal or semantic Theme stroke color.
---@field gradient? boolean|vis.Color|vis.ThemeColor|vis.GradientConfig true/false toggles the legacy two-stop linear ramp, a color sets its end stop, or a GradientConfig table selects a linear, radial, or conic gradient.
---@field width? number Stroke width in pixels.
---@field radius? number Style radius in pixels.
---@field dash? number[] Stroke dash pattern.
---@field dash_offset? number Stroke dash offset.
---@field progress? number Draw progress from 0 to 1.
---@field layer? integer Sibling paint order.
---@field isolate? boolean Draw this object and its descendants into one layer.
---@field mask? vis.Object|{target: vis.Object, mode?: string} Show this object only through another object.
---@field blend? string Blend mode with what is drawn before it, as CSS mix-blend-mode.
---@field blur? number Blur sigma in screen pixels, 0 to 256.
---@field shadow? {color?: vis.Color|vis.ThemeColor, offset?: vis.Vec2, blur?: number} Shadow beneath the object, offset in screen pixels.

---@class vis.IndicateConfig: vis.TimingOptions
---@field duration? number Duration in seconds.
---@field curve? vis.Curve Animation curve.
---@field color? vis.Color|vis.ThemeColor Temporary emphasis color; defaults to the Theme focus role.
---@field scale? number Temporary scale.

---@class vis.CircumscribeConfig: vis.TimingOptions
---@field duration? number Duration in seconds; 1 by default.
---@field curve? vis.Curve Animation curve.
---@field shape? "rectangle"|"ellipse" The frame; an ellipse passes through the corners of the padded bounds. Rectangle by default.
---@field color? vis.Color|vis.ThemeColor Frame color; defaults to the Theme focus role.
---@field width? number Frame stroke width in pixels, 3 by default; the bright stretch is twice as wide.
---@field padding? number World units between the target's bounds and the frame; 0.1 by default.
---@field window? number Share of the outline the bright stretch covers, greater than 0 and at most 1; 0.3 by default.

---@class vis.PassingFlashConfig: vis.TimingOptions
---@field duration? number Duration in seconds; 1 by default.
---@field curve? vis.Curve Animation curve.
---@field color? vis.Color|vis.ThemeColor Stretch color; defaults to the Theme focus role.
---@field width? number Stretch stroke width in pixels; 6 by default.
---@field window? number Share of the outline the bright stretch covers, greater than 0 and at most 1; 0.3 by default.

---@class vis.FlashConfig: vis.TimingOptions
---@field duration? number Duration in seconds; 1 by default.
---@field curve? vis.Curve Animation curve.
---@field color? vis.Color|vis.ThemeColor Line color; defaults to the Theme focus role.
---@field width? number Line stroke width in pixels; 3 by default.
---@field padding? number World units between the target's bounds, or the point, and where the lines start; 0.1 by default.
---@field size? number World length of each line; 0.2 by default.
---@field lines? integer How many lines burst out, from 1 to 64; 12 by default.

---@class vis.RippleConfig: vis.TimingOptions
---@field duration? number Duration in seconds; 1 by default.
---@field curve? vis.Curve Curve of the growth, snappy by default; the fade runs evenly.
---@field color? vis.Color|vis.ThemeColor Ring color; defaults to the Theme focus role.
---@field width? number Ring stroke width in pixels; 3 by default.
---@field padding? number World units between the target's outline and where the ring starts; 0 by default.
---@field size? number World units the ring grows on every side; 0.4 by default.

---@class vis.WiggleConfig: vis.TimingOptions
---@field duration? number Duration in seconds; 2 by default.
---@field curve? vis.Curve Animation curve.
---@field scale? number Peak growth about the bounds center; 1.1 by default.
---@field angle? number Widest turn either way in radians; 0.0628 by default.
---@field swings? number How many times the target tips to a side; 6 by default.

---@class vis.RegionConfig
---@field x number Logical-pixel left coordinate in the Canvas.
---@field y number Logical-pixel top coordinate in the Canvas.
---@field width number Positive logical-pixel hit-region width.
---@field height number Positive logical-pixel hit-region height.

---@class vis.UITransformConfig
---@field shift? vis.Vec3 Runtime translation composed after the authored timeline.
---@field scale? vis.Vec3 Runtime scale composed after the authored timeline.
---@field rotation? vis.Vec3 Runtime XYZ rotation in radians composed after the authored timeline.
---@field opacity? number Runtime opacity factor from 0 to 1.
---@field progress? number Runtime draw-progress factor from 0 to 1.

---@class vis.PanelButtonConfig
---@field visual vis.Object Independent Scene object reserved as this control's idle button visual.
---@field hover_visual? vis.Object Optional independent, non-nested Scene object reserved for this button and shown while the logical pointer is inside its region.
---@field pressed_visual? vis.Object Optional independent, non-nested Scene object reserved for this button and shown while the primary pointer is pressed inside its region.
---@field region vis.RegionConfig Explicit logical-pixel pointer region.
---@field camera? "move"|"pan"|"orbit"|"zoom"|"reset"|"view2d"|"view3d" Camera action performed after a completed click; use exactly one of camera or target.
---@field delta? vis.Vec2 Pan, orbit, or zoom action delta.
---@field target? vis.Object Scene object receiving a click-selected transform; use exactly one of target or camera.
---@field transform? vis.UITransformConfig Transform selected after a completed click; the last clicked Button wins when several Buttons share a target.
---@field duration? number Optional smooth target-transition duration in Scene seconds; zero changes immediately.

---@class vis.PanelToggleButtonConfig
---@field visual vis.Object Independent Scene object reserved as this control's idle visual.
---@field hover_visual? vis.Object Optional independent, non-nested Scene object shown while the logical pointer is inside the control region.
---@field pressed_visual? vis.Object Optional independent, non-nested Scene object shown while the primary pointer is pressed inside the control region.
---@field region vis.RegionConfig Explicit logical-pixel pointer region.
---@field target vis.Object Scene object receiving the retained binary-state transform.
---@field off vis.UITransformConfig Target transform while the retained value is false.
---@field on vis.UITransformConfig Target transform while the retained value is true.
---@field value? boolean Initial retained state; false by default.
---@field duration? number Optional smooth off/on transition duration in Scene seconds; zero changes immediately.

---@class vis.SliderBindingConfig
---@field target vis.Object Scene object receiving this synchronized Slider transform.
---@field from? vis.UITransformConfig Transform at slider value 0.
---@field to? vis.UITransformConfig Transform at slider value 1.

---@class vis.PanelSliderConfig
---@field visual vis.Object Independent Scene object reserved as this control's slider visual.
---@field target? vis.Object Scene object receiving the interpolated runtime transform. Use this single-target form or bindings, never both.
---@field bindings? vis.SliderBindingConfig[] One to 256 synchronized target mappings. Cannot be combined with top-level target, from, or to.
---@field region vis.RegionConfig Explicit logical-pixel pointer region.
---@field axis? "horizontal"|"vertical" Logical-pixel slider direction.
---@field value? number Initial clamped value from 0 to 1.
---@field from? vis.UITransformConfig Transform at slider value 0.
---@field to? vis.UITransformConfig Transform at slider value 1.

---@class vis.PanelSampleAreaConfig
---@field region vis.RegionConfig Logical-pixel click region. A successful sample stores normalized top-left-origin coordinates: (0, 0) at the region top-left and (1, 1) at the bottom-right.
---@field targets vis.Object[] Between 1 and 64 distinct Scene-owned configured candidates with no ancestor/descendant relationship. The topmost candidate whose isolated family composite has nonzero raster alpha wins; returned identity is that configured candidate, not an internal leaf paint.
---@field marker? vis.Object Optional independent childless Scene Object or Group hidden until the first successful sample, then shifted after the current composed state is sampled.
---@field marker_origin? vis.Vec3 Marker parent-space runtime shift at normalized u=0 and v=0; marker is required when this field is supplied.
---@field marker_x? vis.Vec3 Marker parent-space runtime-shift contribution at normalized u=1; marker is required when this field is supplied.
---@field marker_y? vis.Vec3 Marker parent-space runtime-shift contribution at normalized top-to-bottom v=1; marker is required when this field is supplied.
---@field swatch? vis.Object Optional independent, non-gradient Scene Rectangle whose solid fill becomes the selected candidate-family composite in straight RGBA, isolated from the background and non-family Objects and rasterized with the Scene AA policy.

---@class vis.InputPointerFollowConfig
---@field target vis.Object Scene object receiving the pointer-derived runtime transform.
---@field region vis.RegionConfig Logical-pixel pointer region mapped to normalized coordinates u and v.
---@field target_origin? vis.Vec3 Parent-space home pivot moved to the mapped point and used for pointer-facing rotation; defaults to the parent-space origin.
---@field map_origin vis.Vec3 Parent-space position at the region's top-left corner.
---@field map_x vis.Vec3 Parent-space contribution at normalized u=1.
---@field map_y vis.Vec3 Parent-space contribution at normalized top-to-bottom v=1.
---@field period? number Smooth transition period in Scene seconds for each newest pointer sample; defaults to 0.18.
---@field rotate? boolean Rotate the target toward the newest mapped pointer position; true by default.
---@field angle_offset? number Additional Z rotation in radians after the pointer-facing angle.
---@field reset_on_leave? boolean Smoothly restore the authored transform after pointer cancel or leave.

---@class vis.InputKeyMoveConfig
---@field target vis.Object Scene object receiving continuous held-key motion.
---@field key "ArrowLeft"|"ArrowRight"|"ArrowUp"|"ArrowDown"|"vis.Space"|"Enter"|"Escape"|"Shift"|"Tab"|"A-Z"|"0-9"|"Plus"|"Minus" Supported logical key name.
---@field shift vis.Vec3 Parent-space displacement applied per period while the key is down.
---@field period? number Initial acceleration and displacement period in Scene seconds; zero applies one immediate Down-edge shift; defaults to 0.12.

---@class vis.CameraConfig
---@field mode? "fixed"|"interactive" Camera interaction mode.
---@field view? "2d"|"3d" Camera view.
---@field target? vis.Vec2|vis.Vec3 Camera target.
---@field eye? vis.Vec3 3D camera eye.
---@field up? vis.Vec3 3D camera up vector.
---@field fov? number Perspective field of view in radians.
---@field height? number Orthographic view height.
---@field near? number Near clipping plane.
---@field far? number Far clipping plane.
---@field projection? "perspective"|"orthographic" 3D projection mode.

---@class vis.SampleConfig
---@field mode? string Sampling display mode.
---@field padding? number Gap between cells.
---@field duration? number Sampling duration in seconds.
---@field fps? number Sampling frame rate.

---@class vis.TimingOptions
---@field duration? number Seconds; omitted keeps the operation's default.
---@field curve? vis.Curve Timing curve, smooth by default.
---@field easing? vis.Curve Alias for curve; supply only one spelling.

---@class vis.AnimationOptions: vis.TimingOptions
---@field lag? number Each later target starts duration * lag seconds after the previous one.

---@class vis.RevealOptions: vis.AnimationOptions
---@field direction? vis.CreateDirection Drawing order, forward by default.

---@class vis.MorphOptions: vis.AnimationOptions
---@field lag? number Delay of each later pair in seconds.
---@field pairs? number[][] Explicit one-based source and target contour indices; each piece pairs once, and remaining pieces pair automatically. The same indices apply to each object pair in an array call. Surfaces take no pairs.
---@field copy? boolean Morph a copy of each source made at the cursor and leave the source in place; the copy is removed when the morph ends.

---@class vis.MatchOptions: vis.AnimationOptions
---@field lag? number Delay of each later pair in seconds.
---@field path? vis.Object Snapshot outline guide, at most 2048 samples: add its arc-length detour from its endpoint chord to each paired piece while preserving individual endpoints and spacing. No live connectors or nonzero arc.
---@field arc? number Central angle of the arc matched parts travel.
---@field pairs? vis.Object[][] Explicit {source_piece, target_piece} pairs.
---@field strings? boolean Match texts with equal strings, and the pieces of formulas by their TeX; false keeps only the given pairs.
---@field diff? boolean Match lines and tokens in order, as a diff.
---@field shapes? boolean Split or merge shapes between counts.
---@field fade? number Fraction of the duration unmatched parts fade over.
---@field copy? boolean Match a copy of the source made at the cursor and leave the source in place; the source may be moving, and pairs still name its pieces.

---@class vis.EditOptions: vis.AnimationOptions
---@field lag? number Delay of each later pair in seconds.
---@field id? string Id of the new block; by default the block's id with the first free .2, .3, and so on.
---@field arc? number Central angle of the arc matched parts travel.
---@field fade? number Fraction of the duration unmatched parts fade over.

---@class vis.ArrangeOptions: vis.TimingOptions
---@field direction? vis.Vec2|vis.Vec3 Direction of the row.
---@field gap? number Space between neighbors in world units.
---@field center? vis.Vec2|vis.Vec3 Point the row centers on in the parent.

---@class vis.WarpOptions: vis.TimingOptions
---@field step? number Longest straight piece, in the parent space of each target, that the warp maps without cutting it first; 0.05 by default.

---@class vis.Object
local Object = {}

---Create a semantic object container.
---@param config? vis.GroupConfig
---@return vis.Group
function Object:group(config) end

---Create a 2D or 3D coordinate space.
---@param config? vis.SpaceConfig
---@return vis.Space
function Object:space(config) end

---Create a point marker.
---@param config? vis.PointConfig
---@return vis.Object
function Object:point(config) end

---Create a line segment.
---@param config? vis.LineConfig
---@return vis.Object
function Object:line(config) end

---Create an arrow between two points.
---@param config? vis.ArrowConfig
---@return vis.Object
function Object:arrow(config) end

---Create a vector from an origin and value.
---@param config? vis.VectorConfig
---@return vis.Object
function Object:vector(config) end

---Create a circle.
---@param config? vis.CircleConfig
---@return vis.Object
function Object:circle(config) end

---Create a circular arc, stroke-only unless sector is true.
---@param config? vis.ArcConfig
---@return vis.Object
function Object:arc(config) end

---Mark the inner angle between two points seen from a vertex, with a label that follows its bisector.
---@param config? vis.AngleConfig
---@return vis.Object
function Object:angle(config) end

---Create a rectangle.
---@param config? vis.RectangleConfig
---@return vis.Object
function Object:rectangle(config) end

---Create a snapshot filled brace beside object bounds or a segment; return the brace and optional label.
---@param config? vis.BraceConfig
---@return vis.Object, vis.Object?
function Object:brace(config) end

---Create a polygon from at least three points.
---@param config? vis.PolygonConfig
---@return vis.Object
function Object:polygon(config) end

---Create a sampled function or polyline plot.
---@param config? vis.PlotConfig
---@return vis.Object
function Object:plot(config) end

---Trace scalar level curves as one plot per level, with maximal pieces and repeated endpoints on loops.
---@param config? vis.ContourConfig
---@return vis.Group
function Object:contour(config) end

---Create the region between a function's graph and a base as a polygon.
---@param config? vis.AreaConfig
---@return vis.Object
function Object:area(config) end

---Create a Group of the rectangles of a Riemann sum.
---@param config? vis.RiemannConfig
---@return vis.Group
function Object:riemann(config) end

---Create a Group of arrows sampling a vector field, colored by magnitude.
---@param config? vis.VectorFieldConfig
---@return vis.Group
function Object:vector_field(config) end

---Create a Group of the lines a vector field carries points along.
---@param config? vis.StreamLinesConfig
---@return vis.Group
function Object:stream_lines(config) end

---Create one directed polyline route whose dashed shaft and solid endpoint markers share one timeline state.
---@param config? vis.RouteConfig
---@return vis.Object
function Object:route(config) end

---Create a path from keyed contour commands.
---@param config? vis.PathConfig
---@return vis.Object
function Object:path(config) end

---Create the region any of the closed shapes covers as a path.
---@param config? vis.CombineConfig
---@return vis.Object
function Object:union(config) end

---Create the region all of the closed shapes cover as a path.
---@param config? vis.CombineConfig
---@return vis.Object
function Object:intersection(config) end

---Create the region the first closed shape covers and none of the rest do as a path.
---@param config? vis.CombineConfig
---@return vis.Object
function Object:difference(config) end

---Create the region an odd number of the closed shapes cover as a path.
---@param config? vis.CombineConfig
---@return vis.Object
function Object:exclusion(config) end

---Create a cubic Bézier curve.
---@param config? vis.CurveConfig
---@return vis.Object
function Object:curve(config) end

---Create a row-major 3D surface mesh.
---@param config? vis.SurfaceConfig
---@return vis.Object
function Object:surface(config) end

---Create a text object.
---@param config? vis.TextConfig
---@return vis.Object
function Object:text(config) end

---Create texts side by side on shared baselines, one measured Text per string or part; with the tex module, a {tex = ...} part (TexSpanConfig) is a formula set inline.
---@param config? vis.SpansConfig
---@return vis.Group
function Object:spans(config) end

---Create syntax-colored source code as a Group of lines, each a Group of measured tokens.
---@param config? vis.CodeConfig
---@return vis.Group
function Object:code(config) end

---With the tex module (-Dmodules=tex), set TeX math in the Scene's fonts as a typeset Group of glyphs and filled shapes, or of one Group per part when tex is a list.
---@param config? vis.TexConfig
---@return vis.Group
function Object:tex(config) end

---Create a text that writes a number; play and set count its value.
---@param config? vis.NumberConfig
---@return vis.Object
function Object:number(config) end

---Create entries in rows and columns between brackets; numbers count.
---@param config? vis.MatrixConfig
---@return vis.Group
function Object:matrix(config) end

---Create entries in grid cells under an optional header, with stripes and rules.
---@param config? vis.TableConfig
---@return vis.Group
function Object:table(config) end

---Create a picture from an SVG/PNG/JPG/WebP asset (SVG stays vector) or inline pixels.
---@param config? vis.PictureConfig
---@return vis.Object
function Object:picture(config) end

---Create a dense color cell grid.
---@param config? vis.CellConfig
---@return vis.Object
function Object:cell(config) end

---Join sampled closed outlines of two endpoint families, using bounds when no outline crosses the center ray. Groups use the outermost crossing.
---@param config? vis.ConnectorConfig
---@return vis.Object
function Object:connector(config) end

---Create an ordinary child Text as this object's semantic label, using the complete Text configuration without automatic placement or color binding.
---@param config vis.TextConfig
---@return vis.Object
function Object:label(config) end

---Move this object to a parent-space point before animation begins.
---@param point vis.Vec2|vis.Vec3
function Object:move_to(point) end

---Place this object next to a sibling.
---@param other vis.Object
---@param direction vis.Vec2|vis.Vec3
---@param gap? number
function Object:next_to(other, direction, gap) end

---Align this object to a sibling.
---@param other vis.Object
---@param direction vis.Vec2|vis.Vec3
function Object:align_to(other, direction) end

---Show this object only through another object; nil removes the mask.
---@param target vis.Object|nil
---@param mode? "alpha"|"inverse_alpha"|"luma"|"inverse_luma"
function Object:mask(target, mode) end

---Return the handles of this object's direct children in the order they were added.
---@return vis.Object[]
function Object:children() end

---Return the width and height of a text in world units.
---@return number, number
function Object:measure() end

---Arrange Group children in one direction.
---@param direction vis.Vec2|vis.Vec3
---@param gap? number
function Object:arrange(direction, gap) end

---Arrange Group children in a grid.
---@param columns integer
---@param columnGap? number
---@param rowGap? number
function Object:arrange_grid(columns, columnGap, rowGap) end

---@class vis.Group: vis.Object
local Group = {}

---Create a semantic object container.
---@param config? vis.GroupConfig
---@return vis.Group
function Group:group(config) end

---Create a 2D or 3D coordinate space.
---@param config? vis.SpaceConfig
---@return vis.Space
function Group:space(config) end

---Create a point marker.
---@param config? vis.PointConfig
---@return vis.Object
function Group:point(config) end

---Create a line segment.
---@param config? vis.LineConfig
---@return vis.Object
function Group:line(config) end

---Create an arrow between two points.
---@param config? vis.ArrowConfig
---@return vis.Object
function Group:arrow(config) end

---Create a vector from an origin and value.
---@param config? vis.VectorConfig
---@return vis.Object
function Group:vector(config) end

---Create a circle.
---@param config? vis.CircleConfig
---@return vis.Object
function Group:circle(config) end

---Create a circular arc, stroke-only unless sector is true.
---@param config? vis.ArcConfig
---@return vis.Object
function Group:arc(config) end

---Mark the inner angle between two points seen from a vertex, with a label that follows its bisector.
---@param config? vis.AngleConfig
---@return vis.Object
function Group:angle(config) end

---Create a rectangle.
---@param config? vis.RectangleConfig
---@return vis.Object
function Group:rectangle(config) end

---Create a snapshot filled brace beside object bounds or a segment; return the brace and optional label.
---@param config? vis.BraceConfig
---@return vis.Object, vis.Object?
function Group:brace(config) end

---Create a polygon from at least three points.
---@param config? vis.PolygonConfig
---@return vis.Object
function Group:polygon(config) end

---Create a sampled function or polyline plot.
---@param config? vis.PlotConfig
---@return vis.Object
function Group:plot(config) end

---Trace scalar level curves as one plot per level, with maximal pieces and repeated endpoints on loops.
---@param config? vis.ContourConfig
---@return vis.Group
function Group:contour(config) end

---Create the region between a function's graph and a base as a polygon.
---@param config? vis.AreaConfig
---@return vis.Object
function Group:area(config) end

---Create a Group of the rectangles of a Riemann sum.
---@param config? vis.RiemannConfig
---@return vis.Group
function Group:riemann(config) end

---Create a Group of arrows sampling a vector field, colored by magnitude.
---@param config? vis.VectorFieldConfig
---@return vis.Group
function Group:vector_field(config) end

---Create a Group of the lines a vector field carries points along.
---@param config? vis.StreamLinesConfig
---@return vis.Group
function Group:stream_lines(config) end

---Create one directed polyline route whose dashed shaft and solid endpoint markers share one timeline state.
---@param config? vis.RouteConfig
---@return vis.Object
function Group:route(config) end

---Create a path from keyed contour commands.
---@param config? vis.PathConfig
---@return vis.Object
function Group:path(config) end

---Create the region any of the closed shapes covers as a path.
---@param config? vis.CombineConfig
---@return vis.Object
function Group:union(config) end

---Create the region all of the closed shapes cover as a path.
---@param config? vis.CombineConfig
---@return vis.Object
function Group:intersection(config) end

---Create the region the first closed shape covers and none of the rest do as a path.
---@param config? vis.CombineConfig
---@return vis.Object
function Group:difference(config) end

---Create the region an odd number of the closed shapes cover as a path.
---@param config? vis.CombineConfig
---@return vis.Object
function Group:exclusion(config) end

---Create a cubic Bézier curve.
---@param config? vis.CurveConfig
---@return vis.Object
function Group:curve(config) end

---Create a row-major 3D surface mesh.
---@param config? vis.SurfaceConfig
---@return vis.Object
function Group:surface(config) end

---Create a text object.
---@param config? vis.TextConfig
---@return vis.Object
function Group:text(config) end

---Create texts side by side on shared baselines, one measured Text per string or part; with the tex module, a {tex = ...} part (TexSpanConfig) is a formula set inline.
---@param config? vis.SpansConfig
---@return vis.Group
function Group:spans(config) end

---Create syntax-colored source code as a Group of lines, each a Group of measured tokens.
---@param config? vis.CodeConfig
---@return vis.Group
function Group:code(config) end

---With the tex module (-Dmodules=tex), set TeX math in the Scene's fonts as a typeset Group of glyphs and filled shapes, or of one Group per part when tex is a list.
---@param config? vis.TexConfig
---@return vis.Group
function Group:tex(config) end

---Create a text that writes a number; play and set count its value.
---@param config? vis.NumberConfig
---@return vis.Object
function Group:number(config) end

---Create entries in rows and columns between brackets; numbers count.
---@param config? vis.MatrixConfig
---@return vis.Group
function Group:matrix(config) end

---Create entries in grid cells under an optional header, with stripes and rules.
---@param config? vis.TableConfig
---@return vis.Group
function Group:table(config) end

---Create a picture from an SVG/PNG/JPG/WebP asset (SVG stays vector) or inline pixels.
---@param config? vis.PictureConfig
---@return vis.Object
function Group:picture(config) end

---Create a dense color cell grid.
---@param config? vis.CellConfig
---@return vis.Object
function Group:cell(config) end

---Join sampled closed outlines of two endpoint families, using bounds when no outline crosses the center ray. Groups use the outermost crossing.
---@param config? vis.ConnectorConfig
---@return vis.Object
function Group:connector(config) end

---Create an ordinary child Text as this object's semantic label, using the complete Text configuration without automatic placement or color binding.
---@param config vis.TextConfig
---@return vis.Object
function Group:label(config) end

---Move this object to a parent-space point before animation begins.
---@param point vis.Vec2|vis.Vec3
function Group:move_to(point) end

---Place this object next to a sibling.
---@param other vis.Object
---@param direction vis.Vec2|vis.Vec3
---@param gap? number
function Group:next_to(other, direction, gap) end

---Align this object to a sibling.
---@param other vis.Object
---@param direction vis.Vec2|vis.Vec3
function Group:align_to(other, direction) end

---Show this object only through another object; nil removes the mask.
---@param target vis.Object|nil
---@param mode? "alpha"|"inverse_alpha"|"luma"|"inverse_luma"
function Group:mask(target, mode) end

---Return the handles of this object's direct children in the order they were added.
---@return vis.Object[]
function Group:children() end

---Return the width and height of a text in world units.
---@return number, number
function Group:measure() end

---Arrange Group children in one direction.
---@param direction vis.Vec2|vis.Vec3
---@param gap? number
function Group:arrange(direction, gap) end

---Arrange Group children in a grid.
---@param columns integer
---@param columnGap? number
---@param rowGap? number
function Group:arrange_grid(columns, columnGap, rowGap) end

---Return the nth Group below this one, such as a row of a matrix or table.
---@param row integer
---@return vis.Group
function Group:row(row) end

---Return the cth object of the rth row.
---@param row integer
---@param column integer
---@return vis.Object
function Group:entry(row, column) end

---Return the cth object of every row that has one, in order.
---@param column integer
---@return vis.Object[]
function Group:column(column) end

---Return the tokens of a code block that the matches of a plain text cover, in order; a match that covers part of a token takes the whole token.
---@param text string
---@param options? vis.SearchConfig
---@return vis.Object[]
function Group:search(text, options) end

---With the tex module, return the nth part of a formula whose TeX reads tex, blanks aside, or else its nth glyph or shape whose TeX does, such as "c" or "\frac" for a bar; on a part, the nth of its own glyphs and shapes.
---@param tex string
---@param nth? integer
---@return vis.Object
function Group:part(tex, nth) end

---@class vis.Space: vis.Object
local Space = {}

---Create a semantic object container.
---@param config? vis.GroupConfig
---@return vis.Group
function Space:group(config) end

---Create a 2D or 3D coordinate space.
---@param config? vis.SpaceConfig
---@return vis.Space
function Space:space(config) end

---Create a point marker.
---@param config? vis.PointConfig
---@return vis.Object
function Space:point(config) end

---Create a line segment.
---@param config? vis.LineConfig
---@return vis.Object
function Space:line(config) end

---Create an arrow between two points.
---@param config? vis.ArrowConfig
---@return vis.Object
function Space:arrow(config) end

---Create a vector from an origin and value.
---@param config? vis.VectorConfig
---@return vis.Object
function Space:vector(config) end

---Create a circle.
---@param config? vis.CircleConfig
---@return vis.Object
function Space:circle(config) end

---Create a circular arc, stroke-only unless sector is true.
---@param config? vis.ArcConfig
---@return vis.Object
function Space:arc(config) end

---Mark the inner angle between two points seen from a vertex, with a label that follows its bisector.
---@param config? vis.AngleConfig
---@return vis.Object
function Space:angle(config) end

---Create a rectangle.
---@param config? vis.RectangleConfig
---@return vis.Object
function Space:rectangle(config) end

---Create a snapshot filled brace beside object bounds or a segment; return the brace and optional label.
---@param config? vis.BraceConfig
---@return vis.Object, vis.Object?
function Space:brace(config) end

---Create a polygon from at least three points.
---@param config? vis.PolygonConfig
---@return vis.Object
function Space:polygon(config) end

---Create a sampled function or polyline plot.
---@param config? vis.PlotConfig
---@return vis.Object
function Space:plot(config) end

---Trace scalar level curves as one plot per level, with maximal pieces and repeated endpoints on loops.
---@param config? vis.ContourConfig
---@return vis.Group
function Space:contour(config) end

---Create the region between a function's graph and a base as a polygon.
---@param config? vis.AreaConfig
---@return vis.Object
function Space:area(config) end

---Create a Group of the rectangles of a Riemann sum.
---@param config? vis.RiemannConfig
---@return vis.Group
function Space:riemann(config) end

---Create a Group of arrows sampling a vector field, colored by magnitude.
---@param config? vis.VectorFieldConfig
---@return vis.Group
function Space:vector_field(config) end

---Create a Group of the lines a vector field carries points along.
---@param config? vis.StreamLinesConfig
---@return vis.Group
function Space:stream_lines(config) end

---Create one directed polyline route whose dashed shaft and solid endpoint markers share one timeline state.
---@param config? vis.RouteConfig
---@return vis.Object
function Space:route(config) end

---Create a path from keyed contour commands.
---@param config? vis.PathConfig
---@return vis.Object
function Space:path(config) end

---Create the region any of the closed shapes covers as a path.
---@param config? vis.CombineConfig
---@return vis.Object
function Space:union(config) end

---Create the region all of the closed shapes cover as a path.
---@param config? vis.CombineConfig
---@return vis.Object
function Space:intersection(config) end

---Create the region the first closed shape covers and none of the rest do as a path.
---@param config? vis.CombineConfig
---@return vis.Object
function Space:difference(config) end

---Create the region an odd number of the closed shapes cover as a path.
---@param config? vis.CombineConfig
---@return vis.Object
function Space:exclusion(config) end

---Create a cubic Bézier curve.
---@param config? vis.CurveConfig
---@return vis.Object
function Space:curve(config) end

---Create a row-major 3D surface mesh.
---@param config? vis.SurfaceConfig
---@return vis.Object
function Space:surface(config) end

---Create a text object.
---@param config? vis.TextConfig
---@return vis.Object
function Space:text(config) end

---Create texts side by side on shared baselines, one measured Text per string or part; with the tex module, a {tex = ...} part (TexSpanConfig) is a formula set inline.
---@param config? vis.SpansConfig
---@return vis.Group
function Space:spans(config) end

---Create syntax-colored source code as a Group of lines, each a Group of measured tokens.
---@param config? vis.CodeConfig
---@return vis.Group
function Space:code(config) end

---With the tex module (-Dmodules=tex), set TeX math in the Scene's fonts as a typeset Group of glyphs and filled shapes, or of one Group per part when tex is a list.
---@param config? vis.TexConfig
---@return vis.Group
function Space:tex(config) end

---Create a text that writes a number; play and set count its value.
---@param config? vis.NumberConfig
---@return vis.Object
function Space:number(config) end

---Create entries in rows and columns between brackets; numbers count.
---@param config? vis.MatrixConfig
---@return vis.Group
function Space:matrix(config) end

---Create entries in grid cells under an optional header, with stripes and rules.
---@param config? vis.TableConfig
---@return vis.Group
function Space:table(config) end

---Create a picture from an SVG/PNG/JPG/WebP asset (SVG stays vector) or inline pixels.
---@param config? vis.PictureConfig
---@return vis.Object
function Space:picture(config) end

---Join sampled closed outlines of two endpoint families, using bounds when no outline crosses the center ray. Groups use the outermost crossing.
---@param config? vis.ConnectorConfig
---@return vis.Object
function Space:connector(config) end

---Create an ordinary child Text as this object's semantic label, using the complete Text configuration without automatic placement or color binding.
---@param config vis.TextConfig
---@return vis.Object
function Space:label(config) end

---Move this object to a parent-space point before animation begins.
---@param point vis.Vec2|vis.Vec3
function Space:move_to(point) end

---Place this object next to a sibling.
---@param other vis.Object
---@param direction vis.Vec2|vis.Vec3
---@param gap? number
function Space:next_to(other, direction, gap) end

---Align this object to a sibling.
---@param other vis.Object
---@param direction vis.Vec2|vis.Vec3
function Space:align_to(other, direction) end

---Show this object only through another object; nil removes the mask.
---@param target vis.Object|nil
---@param mode? "alpha"|"inverse_alpha"|"luma"|"inverse_luma"
function Space:mask(target, mode) end

---Return the handles of this object's direct children in the order they were added.
---@return vis.Object[]
function Space:children() end

---Return the width and height of a text in world units.
---@return number, number
function Space:measure() end

---Arrange Group children in one direction.
---@param direction vis.Vec2|vis.Vec3
---@param gap? number
function Space:arrange(direction, gap) end

---Arrange Group children in a grid.
---@param columns integer
---@param columnGap? number
---@param rowGap? number
function Space:arrange_grid(columns, columnGap, rowGap) end

---Sample a deterministic 2D color field.
---@param sample fun(x:number, y:number, time:number):vis.Color
---@param config? vis.SampleConfig
---@return vis.Object
function Space:cell(sample, config) end

---Sample a deterministic 3D color field.
---@param sample fun(x:number, y:number, z:number, time:number):vis.Color
---@param config? vis.SampleConfig
---@return vis.Object
function Space:voxel(sample, config) end

---Place labels beside the ticks of the space and return their handles in order. A string becomes a text in the space and may come at any time; a handle of the scene, such as a formula, keeps its parent and must be placed before the first play.
---@param config vis.SpaceLabelsConfig
---@return vis.Object[]
function Space:labels(config) end

---@class vis.Scene
local Scene = {}

---Create a semantic object container.
---@param config? vis.GroupConfig
---@return vis.Group
function Scene:group(config) end

---Create a 2D or 3D coordinate space.
---@param config? vis.SpaceConfig
---@return vis.Space
function Scene:space(config) end

---Create a point marker.
---@param config? vis.PointConfig
---@return vis.Object
function Scene:point(config) end

---Create a line segment.
---@param config? vis.LineConfig
---@return vis.Object
function Scene:line(config) end

---Create an arrow between two points.
---@param config? vis.ArrowConfig
---@return vis.Object
function Scene:arrow(config) end

---Create a vector from an origin and value.
---@param config? vis.VectorConfig
---@return vis.Object
function Scene:vector(config) end

---Create a circle.
---@param config? vis.CircleConfig
---@return vis.Object
function Scene:circle(config) end

---Create a circular arc, stroke-only unless sector is true.
---@param config? vis.ArcConfig
---@return vis.Object
function Scene:arc(config) end

---Mark the inner angle between two points seen from a vertex, with a label that follows its bisector.
---@param config? vis.AngleConfig
---@return vis.Object
function Scene:angle(config) end

---Create a rectangle.
---@param config? vis.RectangleConfig
---@return vis.Object
function Scene:rectangle(config) end

---Create a snapshot filled brace beside object bounds or a segment; return the brace and optional label.
---@param config? vis.BraceConfig
---@return vis.Object, vis.Object?
function Scene:brace(config) end

---Create a polygon from at least three points.
---@param config? vis.PolygonConfig
---@return vis.Object
function Scene:polygon(config) end

---Create a sampled function or polyline plot.
---@param config? vis.PlotConfig
---@return vis.Object
function Scene:plot(config) end

---Trace scalar level curves as one plot per level, with maximal pieces and repeated endpoints on loops.
---@param config? vis.ContourConfig
---@return vis.Group
function Scene:contour(config) end

---Create the region between a function's graph and a base as a polygon.
---@param config? vis.AreaConfig
---@return vis.Object
function Scene:area(config) end

---Create a Group of the rectangles of a Riemann sum.
---@param config? vis.RiemannConfig
---@return vis.Group
function Scene:riemann(config) end

---Create a Group of arrows sampling a vector field, colored by magnitude.
---@param config? vis.VectorFieldConfig
---@return vis.Group
function Scene:vector_field(config) end

---Create a Group of the lines a vector field carries points along.
---@param config? vis.StreamLinesConfig
---@return vis.Group
function Scene:stream_lines(config) end

---Create one directed polyline route whose dashed shaft and solid endpoint markers share one timeline state.
---@param config? vis.RouteConfig
---@return vis.Object
function Scene:route(config) end

---Create a path from keyed contour commands.
---@param config? vis.PathConfig
---@return vis.Object
function Scene:path(config) end

---Create the region any of the closed shapes covers as a path.
---@param config? vis.CombineConfig
---@return vis.Object
function Scene:union(config) end

---Create the region all of the closed shapes cover as a path.
---@param config? vis.CombineConfig
---@return vis.Object
function Scene:intersection(config) end

---Create the region the first closed shape covers and none of the rest do as a path.
---@param config? vis.CombineConfig
---@return vis.Object
function Scene:difference(config) end

---Create the region an odd number of the closed shapes cover as a path.
---@param config? vis.CombineConfig
---@return vis.Object
function Scene:exclusion(config) end

---Create a cubic Bézier curve.
---@param config? vis.CurveConfig
---@return vis.Object
function Scene:curve(config) end

---Create a row-major 3D surface mesh.
---@param config? vis.SurfaceConfig
---@return vis.Object
function Scene:surface(config) end

---Create a text object.
---@param config? vis.TextConfig
---@return vis.Object
function Scene:text(config) end

---Create texts side by side on shared baselines, one measured Text per string or part; with the tex module, a {tex = ...} part (TexSpanConfig) is a formula set inline.
---@param config? vis.SpansConfig
---@return vis.Group
function Scene:spans(config) end

---Create syntax-colored source code as a Group of lines, each a Group of measured tokens.
---@param config? vis.CodeConfig
---@return vis.Group
function Scene:code(config) end

---With the tex module (-Dmodules=tex), set TeX math in the Scene's fonts as a typeset Group of glyphs and filled shapes, or of one Group per part when tex is a list.
---@param config? vis.TexConfig
---@return vis.Group
function Scene:tex(config) end

---Create a text that writes a number; play and set count its value.
---@param config? vis.NumberConfig
---@return vis.Object
function Scene:number(config) end

---Create entries in rows and columns between brackets; numbers count.
---@param config? vis.MatrixConfig
---@return vis.Group
function Scene:matrix(config) end

---Create entries in grid cells under an optional header, with stripes and rules.
---@param config? vis.TableConfig
---@return vis.Group
function Scene:table(config) end

---Create a picture from an SVG/PNG/JPG/WebP asset (SVG stays vector) or inline pixels.
---@param config? vis.PictureConfig
---@return vis.Object
function Scene:picture(config) end

---Create a dense color cell grid.
---@param config? vis.CellConfig
---@return vis.Object
function Scene:cell(config) end

---Join sampled closed outlines of two endpoint families, using bounds when no outline crosses the center ray. Groups use the outermost crossing.
---@param config? vis.ConnectorConfig
---@return vis.Object
function Scene:connector(config) end

---Mount a completed child scene into a normalized viewport.
---@param scene vis.Scene
---@param viewport vis.ViewportConfig
function Scene:viewport(scene, viewport) end

---Draw this scene through an inset camera in a normalized rectangle; return the View. At most 8 views per scene.
---@param rect vis.ViewportConfig
---@param options? vis.ViewConfig
---@return vis.View
function Scene:view(rect, options) end

---Sequence complete child scenes in one viewport.
---@param scenes vis.Scene[]
---@param config vis.SceneTransitionConfig
function Scene:scene_transition(scenes, config) end

---Animate one descriptor or an array of descriptors.
---@overload fun(self: vis.Scene, animations: vis.Animation|vis.Animation[], options: vis.AnimationOptions)
---@param animations vis.Animation|vis.Animation[]
---@param duration? number
---@param curve? vis.Curve
---@param lag? number
function Scene:play(animations, duration, curve, lag) end

---Apply descriptors at once: the starting state before the timeline starts, at the cursor afterwards.
---@param animations vis.Animation|vis.Animation[]
function Scene:set(animations) end

---Push each target family's cursor state without advancing time. Saves nest; active text and geometry changes save their destinations. Warp geometry stays as authored.
---@param target vis.Object|vis.Object[]
function Scene:save(target) end

---Pop each target family's latest saved state and restore it immediately or over duration. Removed descendants stay removed; restores do not undo warps or switch plain/counting text modes.
---@overload fun(self: vis.Scene, target: vis.Object|vis.Object[], options: vis.AnimationOptions)
---@param target vis.Object|vis.Object[]
---@param duration? number
---@param curve? vis.Curve
---@param lag? number
function Scene:restore(target, duration, curve, lag) end

---Reveal targets by drawing their geometry.
---@overload fun(self: vis.Scene, target: vis.Object|vis.Object[], options: vis.RevealOptions)
---@param target vis.Object|vis.Object[]
---@param duration? number
---@param curve? vis.Curve
---@param lag? number
---@param direction? vis.CreateDirection
function Scene:create(target, duration, curve, lag, direction) end

---Hide targets by reversing their drawn geometry.
---@overload fun(self: vis.Scene, target: vis.Object|vis.Object[], options: vis.RevealOptions)
---@param target vis.Object|vis.Object[]
---@param duration? number
---@param curve? vis.Curve
---@param lag? number
---@param direction? vis.CreateDirection
function Scene:uncreate(target, duration, curve, lag, direction) end

---Reveal target fills.
---@overload fun(self: vis.Scene, target: vis.Object|vis.Object[], options: vis.AnimationOptions)
---@param target vis.Object|vis.Object[]
---@param duration? number
---@param curve? vis.Curve
---@param lag? number
function Scene:fill_reveal(target, duration, curve, lag) end

---Draw target borders and then reveal their fills.
---@overload fun(self: vis.Scene, target: vis.Object|vis.Object[], options: vis.RevealOptions)
---@param target vis.Object|vis.Object[]
---@param duration? number
---@param curve? vis.Curve
---@param lag? number
---@param direction? vis.CreateDirection
function Scene:draw_border_then_fill(target, duration, curve, lag, direction) end

---Reveal text or drawable targets in writing order.
---@overload fun(self: vis.Scene, target: vis.Object|vis.Object[], options: vis.RevealOptions)
---@param target vis.Object|vis.Object[]
---@param duration? number
---@param curve? vis.Curve
---@param lag? number
---@param direction? vis.CreateDirection
function Scene:write(target, duration, curve, lag, direction) end

---Animate targets by a parent-space vector.
---@overload fun(self: vis.Scene, target: vis.Object, vector: vis.Vec2|vis.Vec3, options: vis.TimingOptions)
---@param target vis.Object
---@param vector vis.Vec2|vis.Vec3
---@param duration? number
---@param curve? vis.Curve
function Scene:shift(target, vector, duration, curve) end

---Animate a target to an absolute local-to-parent matrix.
---@overload fun(self: vis.Scene, target: vis.Object, matrix: vis.Mat4, options: vis.TimingOptions)
---@param target vis.Object
---@param matrix vis.Mat4
---@param duration? number
---@param curve? vis.Curve
function Scene:transform(target, matrix, duration, curve) end

---Turn targets about a pivot in one rigid clip.
---@param target vis.Object|vis.Object[]
---@param radians number
---@param options? vis.RotateConfig
function Scene:rotate(target, radians, options) end

---Resize targets about a pivot.
---@param target vis.Object|vis.Object[]
---@param factor number|vis.Vec2|vis.Vec3
---@param options? vis.ScaleConfig
function Scene:scale(target, factor, options) end

---Carry targets' bounds centers along a copied outline or a connector's live trimmed ends. Connector dependencies must not form a cycle.
---@param target vis.Object|vis.Object[]
---@param path vis.Object
---@param options? vis.MoveAlongConfig
function Scene:move_along(target, path, options) end

---Carry targets' bounds centers along the flow of a vector field.
---@param target vis.Object|vis.Object[]
---@param fn fun(x:number, y:number):number|vis.Vec2, number?
---@param options? vis.AdvectConfig
function Scene:advect(target, fn, options) end

---Map target families in each target's parent space, subdividing straight segments first. Evaluate fn during authoring, or supply sampled grid images.
---@overload fun(self: vis.Scene, target: vis.Object|vis.Object[], fn: vis.WarpSamples|fun(x:number, y:number, z:number):number|vis.Vec2|vis.Vec3, number?, number?, options: vis.WarpOptions)
---@param target vis.Object|vis.Object[]
---@param fn vis.WarpSamples|fun(x:number, y:number, z:number):number|vis.Vec2|vis.Vec3, number?, number?
---@param duration? number
---@param curve? vis.Curve
---@param options? vis.WarpConfig
function Scene:warp(target, fn, duration, curve, options) end

---Animate target opacity.
---@overload fun(self: vis.Scene, target: vis.Object, opacity: number, options: vis.TimingOptions)
---@param target vis.Object
---@param opacity number
---@param duration? number
---@param curve? vis.Curve
function Scene:fade(target, opacity, duration, curve) end

---Fade a target in with optional shift and scale effects.
---@param target vis.Object
---@param options? vis.FadeConfig
function Scene:fade_in(target, options) end

---Fade a target out with optional shift and scale effects.
---@param target vis.Object
---@param options? vis.FadeConfig
function Scene:fade_out(target, options) end

---Reveal a target by growing it from its center.
---@overload fun(self: vis.Scene, target: vis.Object, options: vis.TimingOptions)
---@param target vis.Object
---@param duration? number
---@param curve? vis.Curve
function Scene:grow_from_center(target, duration, curve) end

---Reveal a target by growing one dimension from a fixed geometry-bounds edge.
---@overload fun(self: vis.Scene, target: vis.Object, edge: "left"|"right"|"bottom"|"top", options: vis.TimingOptions)
---@param target vis.Object
---@param edge "left"|"right"|"bottom"|"top"
---@param duration? number
---@param curve? vis.Curve
function Scene:grow_from_edge(target, edge, duration, curve) end

---Hide a target by shrinking it to its center.
---@overload fun(self: vis.Scene, target: vis.Object, options: vis.TimingOptions)
---@param target vis.Object
---@param duration? number
---@param curve? vis.Curve
function Scene:shrink_to_center(target, duration, curve) end

---Briefly emphasize a target, or each of a list of targets at once.
---@param target vis.Object|vis.Object[]
---@param options? vis.IndicateConfig
function Scene:indicate(target, options) end

---Grow a target a little while it turns back and forth, then leave it as it was.
---@param target vis.Object
---@param options? vis.WiggleConfig
function Scene:wiggle(target, options) end

---Draw a frame around a target on and away while a bright stretch runs once around it; the frame's shapes exist only while it plays.
---@param target vis.Object
---@param options? vis.CircumscribeConfig
function Scene:circumscribe(target, options) end

---Run a bright stretch once along the outline of a line, curve, shape, path or connector; its shape exists only while it plays.
---@param target vis.Object
---@param options? vis.PassingFlashConfig
function Scene:passing_flash(target, options) end

---Burst short lines out of a target, or out of a point in world coordinates, drawing each on and away; its lines exist only while it plays.
---@param target vis.Object|vis.Vec2|vis.Vec3
---@param options? vis.FlashConfig
function Scene:flash(target, options) end

---Grow a ring outward from the outline of a target while it fades away; its ring exists only while it plays.
---@param target vis.Object
---@param options? vis.RippleConfig
function Scene:ripple(target, options) end

---Morph source geometry into a newly attached target. Source draws the interpolation, then leaves the timeline as target takes over at completion. Both remain Scene-owned; copy preserves source by morphing a cursor-state copy. One-based contour pairs override automatic matching.
---@overload fun(self: vis.Scene, source: vis.Object|vis.Object[], target: vis.Object|vis.Object[], options: vis.MorphOptions)
---@param source vis.Object|vis.Object[]
---@param target vis.Object|vis.Object[]
---@param duration? number
---@param curve? vis.Curve
---@param lag? number|vis.MorphConfig
function Scene:morph(source, target, duration, curve, lag) end

---Cross-fade disjoint families and remove source from the timeline at completion. Both remain Scene-owned for earlier samples. Target must be newly attached at the cursor without clips; source clips must finish by the fade end.
---@overload fun(self: vis.Scene, source: vis.Object, target: vis.Object, options: vis.TimingOptions)
---@param source vis.Object
---@param target vis.Object
---@param duration? number
---@param curve? vis.Curve
function Scene:fade_transform(source, target, duration, curve) end

---Carry matching descendants into a newly attached target, using explicit pairs, strings, diff, shapes or TeX. Source must be at rest unless copy snapshots it. Completion removes source or its temporary copy from the timeline; Scene ownership stays.
---@overload fun(self: vis.Scene, source: vis.Object, target: vis.Object, options: vis.MatchOptions)
---@param source vis.Object
---@param target vis.Object
---@param duration? number
---@param curve? vis.Curve
---@param options? vis.MatchTransformConfig
function Scene:match_transform(source, target, duration, curve, options) end

---Make the code block that a block's source turns into after edits, applied in order, with the block's look and place, and carry the block into it as a diff.
---@overload fun(self: vis.Scene, block: vis.Group, edits: vis.CodeEdit|vis.CodeEdit[], options: vis.EditOptions): vis.Group
---@param block vis.Group
---@param edits vis.CodeEdit|vis.CodeEdit[]
---@param duration? number
---@param curve? vis.Curve
---@param options? vis.EditConfig
---@return vis.Group
function Scene:edit(block, edits, duration, curve, options) end

---Dim a block but a range of its lines or the given handles; neither clears.
---@param target vis.Object
---@param first? integer|vis.Object|vis.Object[]|vis.HighlightConfig
---@param last? integer|vis.HighlightConfig
---@param options? vis.HighlightConfig
function Scene:highlight(target, first, last, options) end

---Animate a Group's children, or a list of handles, into a row or column.
---@overload fun(self: vis.Scene, targets: vis.Group|vis.Object[], options: vis.ArrangeOptions)
---@param targets vis.Group|vis.Object[]
---@param duration? number
---@param curve? vis.Curve
---@param options? vis.ArrangeConfig
function Scene:arrange(targets, duration, curve, options) end

---Cross-fade independent outgoing and incoming targets.
---@overload fun(self: vis.Scene, outgoing: vis.Object, incoming: vis.Object, options: vis.TimingOptions)
---@param outgoing vis.Object
---@param incoming vis.Object
---@param duration? number
---@param curve? vis.Curve
function Scene:transition(outgoing, incoming, duration, curve) end

---Animate a target stroke to a literal or semantic Theme color.
---@overload fun(self: vis.Scene, target: vis.Object, color: vis.Color|vis.ThemeColor, options: vis.TimingOptions)
---@param target vis.Object
---@param color vis.Color|vis.ThemeColor
---@param duration? number
---@param curve? vis.Curve
function Scene:stroke(target, color, duration, curve) end

---Animate a target fill to a literal or semantic Theme color.
---@overload fun(self: vis.Scene, target: vis.Object, color: vis.Color|vis.ThemeColor, options: vis.TimingOptions)
---@param target vis.Object
---@param color vis.Color|vis.ThemeColor
---@param duration? number
---@param curve? vis.Curve
function Scene:fill(target, color, duration, curve) end

---Animate the scene camera; a target of objects centers it on their bounds.
---@overload fun(self: vis.Scene, camera: vis.LookConfig, options: vis.TimingOptions)
---@param camera vis.LookConfig
---@param duration? number
---@param curve? vis.Curve
function Scene:look(camera, duration, curve) end

---Circle the 3D camera about its target by any angle.
---@param radians number
---@param options? vis.OrbitConfig
function Scene:orbit(radians, options) end

---Frame objects with the 2D camera; nil returns to the opening camera from either view.
---@param target vis.Object|vis.Object[]|nil
---@param options? vis.FocusConfig
function Scene:focus(target, options) end

---Advance the timeline cursor.
---@param duration number
function Scene:wait(duration) end

---Author tracks that start together; the timeline continues after the longest.
---@param track fun()|fun()[]
---@param ... fun()
function Scene:parallel(track, ...) end

---Author fn from a time or marker, then return to the current time.
---@param time number|string
---@param fn fun()
function Scene:at(time, fn) end

---Name the current authoring time.
---@param name string
function Scene:mark(name) end

---Return the current authoring time, or the time of a marker.
---@param name? string
---@return number
function Scene:time(name) end

---Move the authoring time to a time or marker without returning.
---@param time number|string
function Scene:seek(time) end

---End the target family's visibility at the cursor. Objects remain Scene-owned and available when sampling earlier times.
---@param target vis.Object
function Scene:remove(target) end

---Copy an object and what is below it in the state they show at the cursor, without their animations, beside it or below options.parent; returns the copy and a table that gives the copy of each object below the source by its handle.
---@param source vis.Object
---@param options? vis.CloneConfig
---@return vis.Object, table<vis.Object, vis.Object>
function Scene:clone(source, options) end

---Return the handle of the object with this id, or nil.
---@param id string
---@return vis.Object|nil
function Scene:find(id) end

---Return where a handle, or a list of handles, lies in world units, through the groups above each, as everything authored so far leaves them: the bounds rectangle{around} frames.
---@param target vis.Object|vis.Object[]
---@return vis.Bounds
function Scene:bounds(target) end

---Return the authored scene duration in seconds.
---@return number
function Scene:duration() end

---Experimental Scene-owned interaction panel; available only with optional UI support.
---@class vis.Panel
local Panel = {}

---Experimental: add a pointer button bound to exactly one camera action or target transform. Optional authored hover and pressed visuals are selected without mutating Scene objects; the last clicked Button transform wins for a shared target. The Panel-wide 256-control limit applies.
---@param config vis.PanelButtonConfig
---@return vis.Panel
function Panel:button(config) end

---Experimental: add a pointer button that retains one binary state and smoothly alternates a target between explicit off and on transforms after each completed click.
---@param config vis.PanelToggleButtonConfig
---@return vis.Panel
function Panel:toggle_button(config) end

---Experimental: add a clamped horizontal or vertical control that interpolates either one target transform or a bounded list of synchronized target transforms.
---@param config vis.PanelSliderConfig
---@return vis.Panel
function Panel:slider(config) end

---Experimental: add a click-only 2D sampler. A completed primary click evaluates the configured candidates in the currently composed Scene at the active render density, selects the topmost configured candidate whose isolated family composite has nonzero raster alpha under the Scene AA policy, then commits the optional marker and swatch bindings. Sampling has no Lua callback and does not run on hover or drag.
---@param config vis.PanelSampleAreaConfig
---@return vis.Panel
function Panel:sample_area(config) end

---Experimental: add a persistent camera pan delta without mutating the authored camera timeline.
---@param delta vis.Vec2
---@return vis.Panel
function Panel:camera_move(delta) end

---Experimental: add a persistent 3D camera orbit delta without mutating the authored camera timeline.
---@param delta vis.Vec2
---@return vis.Panel
function Panel:camera_orbit(delta) end

---Experimental: add a persistent camera zoom delta without mutating the authored camera timeline.
---@param delta number
---@return vis.Panel
function Panel:camera_zoom(delta) end

---Experimental: clear persistent Panel camera deltas and its view override.
---@return vis.Panel
function Panel:camera_reset() end

---Experimental: override the sampled camera with a 2D or 3D view until Panel camera reset.
---@param view "2d"|"3d"
---@return vis.Panel
function Panel:camera_view(view) end

---Experimental semantic diagram builder; available only with optional Diagram support.
---@class vis.Diagram
local Diagram = {}

---Add a semantic node and return its builder-owned handle.
---@param config vis.DiagramNodeConfig
---@return vis.DiagramNode
function Diagram:node(config) end

---Add a routed edge between two nodes and return its builder-owned handle.
---@param config vis.DiagramEdgeConfig
---@return vis.DiagramEdge
function Diagram:connect(config) end

---Add a labeled zone around one or more nodes.
---@param config vis.DiagramZoneConfig
---@return vis.DiagramZone
function Diagram:zone(config) end

---Resolve layout and attach one ordinary Object subtree to the Scene.
---@return vis.DiagramBuilt
function Diagram:build() end

---Opaque node handle owned by one Diagram builder.
---@class vis.DiagramNode
local DiagramNode = {}

---Opaque edge handle owned by one Diagram builder.
---@class vis.DiagramEdge
local DiagramEdge = {}

---Opaque zone handle owned by one Diagram builder.
---@class vis.DiagramZone
local DiagramZone = {}

---Built Diagram object tree and its stable semantic lookups.
---@class vis.DiagramBuilt
local DiagramBuilt = {}

---Return the complete generated Diagram subtree.
---@return vis.Group
function DiagramBuilt:root() end

---Return the generated zone layer.
---@return vis.Group
function DiagramBuilt:zones() end

---Return the generated edge-route layer.
---@return vis.Group
function DiagramBuilt:routes() end

---Return the generated node layer.
---@return vis.Group
function DiagramBuilt:nodes() end

---Return the generated annotation layer.
---@return vis.Group
function DiagramBuilt:annotations() end

---Return the generated node group; its text remains in the annotation layer.
---@param node vis.DiagramNode
---@return vis.Object
function DiagramBuilt:node(node) end

---Return the generated body object for a node.
---@param node vis.DiagramNode
---@return vis.Object
function DiagramBuilt:node_body(node) end

---Return the generated label object for a node.
---@param node vis.DiagramNode
---@return vis.Object
function DiagramBuilt:node_label(node) end

---Return the optional generated detail object for a node.
---@param node vis.DiagramNode
---@return vis.Object|nil
function DiagramBuilt:node_detail(node) end

---Return the complete generated route object for an edge.
---@param edge vis.DiagramEdge
---@return vis.Object
function DiagramBuilt:edge(edge) end

---Return the optional generated label object for an edge.
---@param edge vis.DiagramEdge
---@return vis.Object|nil
function DiagramBuilt:edge_label(edge) end

---Return the generated zone group; its text remains in the annotation layer.
---@param zone vis.DiagramZone
---@return vis.Object
function DiagramBuilt:zone(zone) end

---Return the generated body object for a zone.
---@param zone vis.DiagramZone
---@return vis.Object
function DiagramBuilt:zone_body(zone) end

---Return the generated label object for a zone.
---@param zone vis.DiagramZone
---@return vis.Object
function DiagramBuilt:zone_label(zone) end

---Scene-owned input binding controller; available only with optional Input support.
---@class vis.InputController
local InputController = {}

---Bind passive pointer motion in one logical-pixel region to an exact parent-space target map, optionally rotating the target toward the newest pointer sample.
---@param config vis.InputPointerFollowConfig
---@return vis.InputController
function InputController:pointer_follow(config) end

---Bind one supported key as retained Down state that moves continuously until keyup and ignores platform repeat.
---@param config vis.InputKeyMoveConfig
---@return vis.InputController
function InputController:key_move(config) end

---Experimental retained Lua fixed-step context; available only with optional Lua Runtime support.
---@class vis.Runtime
local Runtime = {}

---Bind one logical key contribution to a named fixed-step action during authoring.
---@param action string
---@param key string
---@param value? vis.Vec2
---@return vis.Runtime
function Runtime:bind_key(action, key, value) end

---Bind one pointer button contribution to a named fixed-step action during authoring.
---@param action string
---@param button integer
---@param value? vis.Vec2
---@param pointer? integer
---@return vis.Runtime
function Runtime:bind_pointer(action, button, value, pointer) end

---Read a named Input action inside the fixed-step callback; transient edges are visible only in the first catch-up step.
---@param action string
---@return vis.RuntimeActionState
function Runtime:action(action) end

---Read one logical key snapshot inside the fixed-step callback.
---@param key string
---@return vis.RuntimeKeyState
function Runtime:key(key) end

---Read one logical-pixel pointer snapshot inside the fixed-step callback, or nil before that pointer is observed.
---@param pointer? integer
---@return vis.RuntimePointerState|nil
function Runtime:pointer(pointer) end

---Queue one bounded data-only sound event inside the fixed-step callback; return false when the 64-event host-advance queue is full.
---@param asset string
---@param options? vis.RuntimeSoundConfig
---@return boolean
function Runtime:sound(asset, options) end

---Atomically stage one bounded render-time Object overlay inside the fixed-step callback.
---@param object vis.Object
---@param state vis.RuntimeObjectState
---@return vis.Runtime
function Runtime:update(object, state) end

---Atomically clear one Object overlay, or every runtime overlay when object is omitted, inside the fixed-step callback.
---@param object? vis.Object
---@return vis.Runtime
function Runtime:clear(object) end

---Scene-owned semantic state-transition controller; available only with optional Motion support.
---@class vis.MotionController
local MotionController = {}

---Define one immutable complete semantic overlay state during scene construction.
---@param object vis.Object
---@param name string
---@param state vis.MotionState
---@return vis.MotionController
function MotionController:define(object, name, state) end

---Retarget one Object or atomically retarget an array of Objects to named motion states.
---@overload fun(self: vis.MotionController, targets: vis.MotionTarget[], duration?: number, curve?: vis.Curve): vis.MotionController
---@overload fun(self: vis.MotionController, object: vis.Object, state: string, options: vis.TimingOptions): vis.MotionController
---@overload fun(self: vis.MotionController, targets: vis.MotionTarget[], options: vis.TimingOptions): vis.MotionController
---@param object vis.Object
---@param state string
---@param duration? number
---@param curve? vis.Curve
---@return vis.MotionController
function MotionController:transition(object, state, duration, curve) end

---Advance the independent motion clock by a non-negative elapsed duration.
---@param elapsed number
---@return vis.MotionController
function MotionController:advance(elapsed) end

---Return the current complete relative overlay for a tracked Object.
---@param object vis.Object
---@return vis.MotionState
function MotionController:sample(object) end

---Return the independent motion clock in seconds.
---@return number
function MotionController:time() end

---Return whether any motion track has not reached its target.
---@return boolean
function MotionController:active() end

---Return the number of immutable Object-state definitions.
---@return integer
function MotionController:definition_count() end

---Return the number of undrained accepted transition events.
---@return integer
function MotionController:event_count() end

---Return one one-based native acknowledgement event, or nil when the index is outside the journal.
---@param index integer
---@return vis.MotionEvent|nil
function MotionController:event(index) end

---Clear acknowledged native events without changing definitions, tracks, clock, or transaction sequence.
---@return vis.MotionController
function MotionController:clear_events() end

---A second view of a scene's live objects made by Scene:view; its camera moves and the whole view fades with the calls the scene's camera takes.
---@class vis.View
local View = {}

---Animate the view's camera as Scene:look animates the scene's; a target of objects centers it on their bounds.
---@overload fun(self: vis.View, camera: vis.LookConfig, options: vis.TimingOptions)
---@param camera vis.LookConfig
---@param duration? number
---@param curve? vis.Curve
function View:look(camera, duration, curve) end

---Frame objects in the view's rectangle with its 2D camera; nil returns to the camera the view opened with.
---@param target vis.Object|vis.Object[]|nil
---@param options? vis.FocusConfig
function View:focus(target, options) end

---Circle the view's 3D camera about its target by any angle.
---@param radians number
---@param options? vis.OrbitConfig
function View:orbit(radians, options) end

---Animate the opacity of the whole view; the fades of one view follow one another.
---@overload fun(self: vis.View, opacity: number, options: vis.TimingOptions)
---@param opacity number
---@param duration? number
---@param curve? vis.Curve
function View:fade(opacity, duration, curve) end

---Experimental interactive UI module. It is available only when viscript is built with optional UI support; one Panel is limited to 256 controls and bindings.
---@class vis.UI
local UI = {}

---Create the single experimental, Scene-owned UI Panel that composes interaction after timeline sampling.
---@param scene vis.Scene
---@return vis.Panel
function UI.panel(scene) end

---Experimental keyboard, pointer, and camera Input module. It is optional and independent from UI; one Controller is limited to 256 bindings.
---@class vis.Input
local Input = {}

---Create the single Scene-owned Input Controller that composes pointer and keyboard state after timeline sampling.
---@param scene vis.Scene
---@return vis.InputController
function Input.controller(scene) end

---@class vis.Library
---@field ui? vis.UI Experimental interactive UI module. It is available only when viscript is built with optional UI support; one Panel is limited to 256 controls and bindings.
---@field input? vis.Input Experimental keyboard, pointer, and camera Input module. It is optional and independent from UI; one Controller is limited to 256 bindings.
local Library = {}

---Sample physical displacement for a mass-spring-damper system at non-negative elapsed seconds. Supports all damping regimes and initial velocity without a settling clamp; invalid inputs or a non-finite result are errors.
---@param seconds number
---@param options vis.SpringResponseConfig
---@return number
function Library.spring_response(seconds, options) end

---Create a viscript scene.
---@param config? vis.SceneConfig
---@return vis.Scene
function Library.scene(config) end

---Create an experimental semantic Diagram builder for a Scene. Available only when the optional Diagram module is enabled.
---@param scene vis.Scene
---@param config vis.DiagramConfig
---@return vis.Diagram
function Library.diagram(scene, config) end

---Create the optional experimental retained Lua fixed-step runtime for one Scene.
---@param scene vis.Scene
---@param config vis.RuntimeConfig
---@return vis.Runtime
function Library.runtime(scene, config) end

---Create the optional Scene-owned semantic motion controller. The host must advance its independent clock explicitly.
---@param scene vis.Scene
---@return vis.MotionController
function Library.motion(scene) end

---@type vis.Library
vis = {}
