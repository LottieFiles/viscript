-- Educational-cinema pattern: one tracker drives every dependent, the same
-- ball carries the story, geometry comes before notation, and the camera
-- frames overview -> detail -> overview. No title: the curve is the subject.
local scene = vis.scene {
    width = 960, height = 540, theme = "3_blue_1_eyes",
    camera = {mode = "fixed", view = "2d", target = {0, 2}, height = 6},
}

-- Source truth: the function, its slope, and the start value.
local f = function(x) return 0.5 * x * x end
local df = function(x) return x end
local start = -2.6

local LAYER = {guide = 0, curve = 1, trail = 2, tangent = 3, ball = 4, label = 5}
local curve = scene:plot {id = "loss", fn = f, x_range = {-3, 3, 0.05}, stroke = "accent", width = 4, layer = LAYER.curve}
local trail = scene:plot {id = "trail", fn = f, x_range = {start, 0, 0.05}, stroke = "warning", width = 7,
    progress = 0, layer = LAYER.trail}
local tangent = scene:line {id = "tangent", from = {-0.9, 0}, to = {0.9, 0}, stroke = "danger", width = 3,
    layer = LAYER.tangent}
local ball = scene:group {id = "ball"}
ball:circle {center = {0, 0}, radius = 0.12, fill = "warning", stroke = "warning", layer = LAYER.ball}

-- One value, many dependents: bind once, then animate only the value.
local x = scene:tracker(start)
scene:bind(ball, x, function(v) return {shift = {v, f(v)}} end)
scene:bind(tangent, x, function(v)
    local a = math.atan(df(v))
    local c, s = math.cos(a), math.sin(a)
    return {transform = {c, -s, 0, v, s, c, 0, f(v), 0, 0, 1, 0, 0, 0, 0, 1}}
end)
scene:bind(trail, x, function(v) return {progress = (v - start) / -start} end)

-- Notation arrives after the motion it names, colored like the tangent.
local slope = scene:text {id = "slope", text = "slope = f'(w)", point = {-0.2, 1.9}, role = "code",
    fill = "danger", layer = LAYER.label}

-- Beat 1: the ball rolls part of the way; the viewer sees the tangent flatten.
scene:wait(0.6)
scene:track(x, -1.4, 2, "smooth")
-- Beat 2: zoom on the tangent and its label, dim the rest of the curve.
-- focus frames authored bounds, so frame the label plus a probe at the ball's spot.
local probe = scene:circle {id = "zoom-probe", center = {-1.4, f(-1.4)}, radius = 0.9, opacity = 0}
scene:fade(curve, 0.4, 0.6, "smooth")
scene:focus({probe, slope}, {duration = 0.9, curve = "smooth"})
scene:fade_in(slope, {duration = 0.5})
scene:wait(0.8)
-- Beat 3: pull back and finish the descent; the slope reaches zero at the bottom.
scene:focus(nil, {duration = 0.9, curve = "smooth"})  -- back to the opening camera
scene:fade(curve, 1, 0.4, "smooth")
scene:track(x, 0, 2, "smooth")
scene:indicate(ball, {duration = 0.6})
scene:wait(2)
return scene
