-- README hero: the viscript title over a stage that cycles through what a scene can hold:
-- math, geometry, motion and drawing. Each act slides in from the right and out to the left.
local scene = vis.scene {width = 1200, height = 600, loop = true, theme = "pro_white",
  camera = {height = 10}}
local INK = "foreground"

-- Title (fixed).
-- Title.
scene:text {text = "VISCRIPT", point = {0, 3.95}, size = 60, fill = INK, id = "title"}
local chip = scene:text {text = "MATH", point = {0, -4.4}, size = 13, role = "code", fill = "muted", id = "act"}

local SY = -0.3                 -- stage centre
local SLIDE = 2.0               -- entry/exit travel
local acts = {}
local function act(name) local g = scene:group {id = "act-" .. name}; acts[#acts + 1] = {g = g, name = name}; return g end

-- Act 1: MATH — a point on the unit circle traces sin θ.
local A1 = act("math")
local R, CX, WX0, WX1 = 1.7, -4.6, -2.2, 6.2
A1:circle {center = {CX, SY}, radius = R, stroke = INK, width = 2.5}
A1:line {from = {CX - R - 0.3, SY}, to = {WX1 + 0.3, SY}, stroke = "border", width = 1.5}
A1:line {from = {CX, SY - R - 0.3}, to = {CX, SY + R + 0.3}, stroke = "border", width = 1.5}
local K = (WX1 - WX0) / (2 * math.pi)
local wave = A1:plot {fn = function(x) return SY + R * math.sin((x - WX0) / K) end, x_range = {WX0, WX1, 0.02}, stroke = "accent", width = 4, progress = 0}
local radius = A1:line {from = {CX, SY}, to = {CX + R, SY}, stroke = INK, width = 2.5}
local proj = A1:line {from = {CX + R, SY}, to = {WX0, SY}, stroke = "warning", width = 1.5, dash = {6, 5}}
local p1 = A1:circle {center = {CX + R, SY}, radius = 0.13, fill = "warning", stroke = "warning"}
local p2 = A1:circle {center = {WX0, SY}, radius = 0.13, fill = "accent", stroke = "accent"}
local eq = A1:tex {tex = "y = \\sin\\theta", point = {3.8, SY + 2.5}, size = 30, fill = INK}
local theta = scene:tracker(0)
local function circ(t) return {CX + R * math.cos(t), SY + R * math.sin(t)} end
local function wav(t) return {WX0 + K * t, SY + R * math.sin(t)} end
scene:bind(radius, theta, function(t) return {to = circ(t)} end)
scene:bind(p1, theta, function(t) local c = circ(t); return {shift = {c[1] - CX - R, c[2] - SY}} end)
scene:bind(p2, theta, function(t) local w = wav(t); return {shift = {w[1] - WX0, w[2] - SY}} end)
scene:bind(proj, theta, function(t) return {from = circ(t), to = wav(t)} end)

-- Act 2: GEOMETRY — squares on the sides of a right triangle.
local A2 = act("geometry")
local O, a, b = {-1.5, SY - 0.9}, 2.0, 1.5      -- right angle at O, legs a (x) and b (y)
local P, Q = {O[1] + a, O[2]}, {O[1], O[2] + b}
local tri = A2:polygon {points = {O, P, Q}, fill = "#ffffff", stroke = INK, width = 3}
local sqA = A2:polygon {points = {O, P, {P[1], P[2] - a}, {O[1], O[2] - a}}, fill = "accent", stroke = INK, width = 2.5, opacity = 0.85}
local sqB = A2:polygon {points = {O, {O[1] - b, O[2]}, {Q[1] - b, Q[2]}, Q}, fill = "warning", stroke = INK, width = 2.5, opacity = 0.85}
local dx, dy = Q[1] - P[1], Q[2] - P[2]
local sqC = A2:polygon {points = {P, Q, {Q[1] + dy, Q[2] - dx}, {P[1] + dy, P[2] - dx}}, fill = "success", stroke = INK, width = 2.5, opacity = 0.85}
for _, s in ipairs({sqA, sqB, sqC}) do scene:set({target = s, progress = 0}) end
local pyth = A2:tex {tex = "a^2 + b^2 = c^2", point = {4.6, SY + 0.2}, size = 32, fill = INK}

-- Act 3: MOTION — springs released in a cascade, each ball trailing along its rail.
local A3 = act("motion")
local balls, rails, trails = {}, {}, {}
local COLORS = {"accent", "warning", "success", "danger", "secondary"}
for i = 1, 5 do
  local y = SY + 1.8 - (i - 1) * 0.9
  rails[i] = A3:line {from = {-6.5, y}, to = {7.2, y}, stroke = "border", width = 1.5}
  A3:rectangle {center = {-6.6, y}, size = {0.12, 0.5}, fill = INK, stroke = INK}
  trails[i] = A3:line {from = {-6.5, y}, to = {-5.6, y}, stroke = COLORS[i], width = 4}
  balls[i] = A3:circle {center = {-5.6, y}, radius = 0.28, fill = COLORS[i], stroke = INK, width = 2.5}
end
local clock = scene:tracker(0)
for i, ball in ipairs(balls) do
  local w = 2 * math.pi * (0.55 + 0.08 * i)
  local y = SY + 1.8 - (i - 1) * 0.9
  local function dx(t)
    local local_t = math.max(0, t - (i - 1) * 0.12)
    return 10.4 - vis.spring_response(local_t, {mass = 1, stiffness = w * w, damping = 0.55 * w, displacement = 10.4})
  end
  scene:bind(ball, clock, function(t) return {shift = {dx(t), 0}} end)
  scene:bind(trails[i], clock, function(t) return {to = {-5.6 + dx(t), y}} end)
end

-- Act 4: DRAWING — a rose curve stroked in, then filled.
local A4 = act("drawing")
local pts = {}
for i = 0, 720 do
  local th = i / 720 * 2 * math.pi
  local r = 2.4 * math.cos(4 * th)
  pts[#pts + 1] = {r * math.cos(th), SY + r * math.sin(th)}
end
local rose = A4:polygon {points = pts, fill = "#00000000", stroke = "accent", width = 3, progress = 0}
local core = A4:circle {center = {0, SY}, radius = 0.32, fill = "warning", stroke = INK, width = 2.5}
scene:set({target = core, opacity = 0})
local ring = A4:circle {center = {0, SY}, radius = 2.75, stroke = "border", width = 1.5, dash = {4, 6}}

-- Hidden off-stage to the right until their turn.
for i = 2, #acts do scene:set({target = acts[i].g, shift = {SLIDE, 0}, opacity = 0}) end

local IN = {duration = 0.6, curve = "ease_out"}
local OUT = {duration = 0.45, curve = "ease_in"}
local function enter(i)
  scene:play({{target = acts[i].g, shift = {-SLIDE, 0}, opacity = 1}, {target = chip, text = acts[i].name:upper()}}, IN)
end
local function leave(i) scene:play({target = acts[i].g, shift = {-SLIDE, 0}, opacity = 0}, OUT) end

-- 1. Math
scene:wait(0.3)
scene:parallel(
  function() scene:track(theta, 2 * math.pi, 2.6, "ease_in_out") end,
  function() scene:create(wave, {duration = 2.6, curve = "ease_in_out"}) end)
scene:indicate(eq, {duration = 0.5})
leave(1)
-- 2. Geometry
enter(2)
for _, sq in ipairs({sqA, sqB, sqC}) do scene:set({target = sq, progress = 1, opacity = 0, shift = {0, 0}}) end
scene:grow_from_center(sqA, {duration = 0.4, curve = {preset = "back", strength = 1.2}})
scene:grow_from_center(sqB, {duration = 0.4, curve = {preset = "back", strength = 1.2}})
scene:grow_from_center(sqC, {duration = 0.5, curve = {preset = "back", strength = 1.2}})
scene:indicate(pyth, {duration = 0.6})
scene:rotate(A2, 0.12, {about = {0, SY}, duration = 0.5, curve = "ease_in_out"})
scene:rotate(A2, -0.12, {about = {0, SY}, duration = 0.5, curve = "ease_in_out"})
leave(2)
-- 3. Motion
enter(3)
scene:track(clock, 2.6, 2.6, "linear")
leave(3)
-- 4. Drawing
enter(4)
scene:create(rose, {duration = 1.8, curve = "ease_in_out"})
scene:parallel(
  function() scene:play({target = rose, fill = "#0a66c233"}, 0.5, "ease_out") end,
  function() scene:play({target = core, opacity = 1}, 0.3, "ease_out") end,
  function() scene:rotate(A4, math.pi / 4, {about = {0, SY}, duration = 1.2, curve = "ease_in_out"}) end)
scene:wait(1.4)
leave(4)
-- Back to the opening act for the loop: reset it off-stage, then slide it in.
scene:track(theta, 0, 0.01)
scene:play({{target = wave, progress = 0}, {target = acts[1].g, shift = {2 * SLIDE, 0}}}, 0.01)
enter(1)
return scene
