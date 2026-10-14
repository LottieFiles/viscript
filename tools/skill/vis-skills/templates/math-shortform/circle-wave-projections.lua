-- One rotating coordinate writes two phase-locked projection waves.
local scene = vis.scene {
    width = 960, height = 540, fps = 30, loop = false,
    camera = {mode = "fixed", view = "2d", target = {0, 0}, height = 7.2},
}

local center, radius, steps = {-3.75, 0}, 1.35, 36
local graph = {x0 = -1.35, width = 6.2, scale = 0.78, sineY = 1.35, cosineY = -1.35}
local function diamond(point, r)
    return {{point[1] - r, point[2]}, {point[1], point[2] + r},
            {point[1] + r, point[2]}, {point[1], point[2] - r}}
end
local function state(step)
    local theta = 2 * math.pi * step / steps
    local point = {center[1] + radius * math.cos(theta), center[2] + radius * math.sin(theta)}
    local x = graph.x0 + graph.width * step / steps
    local sinePoint = {x, graph.sineY + graph.scale * math.sin(theta)}
    local cosinePoint = {x, graph.cosineY + graph.scale * math.cos(theta)}
    local sineTrace, cosineTrace = {}, {}
    for sample = 0, steps do
        local visible = math.min(sample, step)
        local phase = 2 * math.pi * visible / steps
        local sx = graph.x0 + graph.width * visible / steps
        sineTrace[#sineTrace + 1] = {sx, graph.sineY + graph.scale * math.sin(phase)}
        cosineTrace[#cosineTrace + 1] = {sx, graph.cosineY + graph.scale * math.cos(phase)}
    end
    return {
        point = point, sinePoint = sinePoint, cosinePoint = cosinePoint,
        dot = diamond(point, 0.09), radius = {center, point},
        sineComponent = {{center[1], center[2]}, {center[1], point[2]}},
        cosineComponent = {{center[1], center[2]}, {point[1], center[2]}},
        sineBridge = {point, sinePoint}, cosineBridge = {point, cosinePoint},
        sineTrace = sineTrace, cosineTrace = cosineTrace,
    }
end
local function members(step)
    local s = state(step)
    return {
        dot = scene:polygon {id = "projection-dot-" .. step, points = s.dot, fill = "foreground", stroke = "foreground", layer = 30},
        radius = scene:plot {id = "projection-radius-" .. step, points = s.radius, stroke = "foreground", width = 2, layer = 20},
        sineComponent = scene:plot {id = "projection-sine-component-" .. step, points = s.sineComponent, stroke = "accent", width = 2, layer = 20},
        cosineComponent = scene:plot {id = "projection-cos-component-" .. step, points = s.cosineComponent, stroke = "secondary", width = 2, layer = 20},
        sineBridge = scene:plot {id = "projection-sine-bridge-" .. step, points = s.sineBridge, stroke = "accent", width = 1, opacity = 0.42, layer = 5},
        cosineBridge = scene:plot {id = "projection-cos-bridge-" .. step, points = s.cosineBridge, stroke = "secondary", width = 1, opacity = 0.42, layer = 5},
        sineTrace = scene:plot {id = "projection-sine-trace-" .. step, points = s.sineTrace, stroke = "accent", width = 2, layer = 10},
        cosineTrace = scene:plot {id = "projection-cos-trace-" .. step, points = s.cosineTrace, stroke = "secondary", width = 2, layer = 10},
    }
end
local function array(m)
    return {m.dot, m.radius, m.sineComponent, m.cosineComponent,
            m.sineBridge, m.cosineBridge, m.sineTrace, m.cosineTrace}
end

-- Stable structure stays quiet while the shared phase does the explanatory work.
local circle = scene:circle {id = "projection-circle", center = center, radius = radius, fill = "#00000000", stroke = "border", width = 1.25}
local axes = {
    scene:line {id = "projection-circle-x", from = {center[1] - 1.7, 0}, to = {center[1] + 1.7, 0}, stroke = "border", width = 1},
    scene:line {id = "projection-circle-y", from = {center[1], -1.7}, to = {center[1], 1.7}, stroke = "border", width = 1},
    scene:line {id = "projection-sine-axis", from = {graph.x0, graph.sineY}, to = {graph.x0 + graph.width, graph.sineY}, stroke = "border", width = 1},
    scene:line {id = "projection-cos-axis", from = {graph.x0, graph.cosineY}, to = {graph.x0 + graph.width, graph.cosineY}, stroke = "border", width = 1},
}
local live = members(0)
scene:create({circle, axes[1], axes[2], axes[3], axes[4]}, 0.55, "ease_out", 0.04)
scene:create(array(live), 0.45, "ease_out", 0.02)
scene:wait(0.35)

-- Constant-point traces accumulate while every dependent reaches the same phase.
for step = 1, steps do
    local target = members(step)
    scene:morph(array(live), array(target), 0.065, "linear", 0)
    live = target
end

local conclusion = scene:text {
    id = "projection-conclusion", text = "one phase  ·  two projections",
    point = {1.85, 2.75}, role = "code", fill = "result", align = {0.5, 0.5}, layer = 40,
}
scene:fade_in(conclusion, {shift = {0, -0.08}, duration = 0.35, curve = "gentle"})
scene:wait(1.8)
return scene
