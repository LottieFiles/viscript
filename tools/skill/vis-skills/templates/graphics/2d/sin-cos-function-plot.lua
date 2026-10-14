-- Compare phase-aligned sine and cosine plots on one shared coordinate Space.
local scene = vis.scene {
    width = 960, height = 540, fps = 30, loop = false,
    camera = {mode = "fixed", view = "2d", target = {0, 0}, height = 6.2},
}

local periodMultiple, samples = 2, 160
local x0, x1 = -periodMultiple * math.pi, periodMultiple * math.pi
local sine_points, cosine_points = {}, {}
for i = 0, samples do
    local x = x0 + (x1 - x0) * i / samples
    sine_points[#sine_points + 1] = {x, math.sin(x)}
    cosine_points[#cosine_points + 1] = {x, math.cos(x)}
end

local plane = scene:space {
    id = "trig-space", x = {x0, x1, math.pi / 2}, y = {-1.5, 1.5, 0.5},
    matrix = {0.78, 0, 0, 0, 0, 1.25, 0, -0.2, 0, 0, 1, 0, 0, 0, 0, 1},
    color = "border", axis_x = "foreground", axis_y = "foreground", numbers = false,
}
local sine = plane:plot {id = "trig-sine", points = sine_points, stroke = "accent", width = 2, layer = 10}
local cosine = plane:plot {id = "trig-cosine", points = cosine_points, stroke = "secondary", width = 2, layer = 10}

-- One full-period landmark is derived from the same domain values.
local period_x = periodMultiple * math.pi
local period = plane:line {
    id = "trig-period", from = {period_x, -1.18}, to = {period_x, 1.18},
    stroke = "result", width = 1.5, layer = 20,
}
local labels = {
    scene:text {id = "trig-sine-label", text = "sin(x)", point = {-4.75, 2.45}, role = "code", fill = "accent", align = {0, 0.5}, layer = 40},
    scene:text {id = "trig-cosine-label", text = "cos(x)", point = {-4.75, 2.05}, role = "code", fill = "secondary", align = {0, 0.5}, layer = 40},
    scene:text {id = "trig-period-label", text = string.format("x = %dπ", periodMultiple), point = {4.75, 2.45}, role = "code", fill = "result", align = {1, 0.5}, layer = 40},
}

-- The two curves reveal together so their quarter-period offset remains comparable.
scene:create(plane, 0.42, "ease_out")
scene:create({sine, cosine}, 1.10, "ease_out", 0)
scene:create(period, 0.34, "ease_out")
scene:fade_in(labels[1], {shift = {0.12, 0}, duration = 0.24, curve = "ease_out"})
scene:fade_in(labels[2], {shift = {0.12, 0}, duration = 0.24, curve = "ease_out"})
scene:fade_in(labels[3], {shift = {0, -0.10}, duration = 0.24, curve = "ease_out"})
scene:wait(1.1)
return scene
