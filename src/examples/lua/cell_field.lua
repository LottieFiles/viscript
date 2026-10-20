local p = {
    muted = "#667085",
    grid = "#d8dadd",
    blue = "#0891b2",
    violet = "#7c3aed",
}

local scene = vis.scene {theme = "pro_white", camera = { height = 7.2 }}

scene:text {
    text = "Cell  ·  circle to ellipse",
    point = { -5.95, 3.02 },
    align = { 0, 0.5 },
    role = "h2",
}
scene:text {
    text = "x² + y² ≤ r²    to    x²/a² + y²/b² ≤ 1",
    point = { -5.92, 2.53 },
    align = { 0, 0.5 },
    role = "text",
    fill = p.muted,
}

scene:space {
    x = { -3, 3, 1 },
    y = { -2, 2, 1 },
    z = { 0, 0, 1 },
    stroke = p.grid,
    width = 1,
}

local field = scene:space {
    x = { -3, 3, 0.12 },
    y = { -2, 2, 0.12 },
    z = { 0, 0, 1 },
    opacity = 0,
}

local duration = 1.8
field:cell(function(x, y, time)
    local progress = time / duration
    progress = progress * progress * (3 - 2 * progress)
    local a = 1.35 + (2.05 - 1.35) * progress
    local b = 1.35 + (1.05 - 1.35) * progress
    local color = progress < 0.5 and p.blue or p.violet
    return x * x / (a * a) + y * y / (b * b) <= 1 and color or "#00000000"
end, { mode = "padd", padding = 0.045, duration = duration, fps = 24 })

scene:wait(0.45)
return scene
