local p = {
    muted = "#667085",
    grid = "#d8dadd66",
    blue = "#2563eb",
    violet = "#7c3aed",
}

local page = vis.scene {
    width = 960,
    height = 540,
    theme = "pro_white",
    camera = { view = "2d", height = 8 },
}

page:text {
    text = "Voxel  ·  sphere to ellipsoid",
    point = { -6.55, 3.42 },
    align = { 0, 0.5 },
    role = "h2",
}
page:text {
    text = "x² + y² + z² ≤ r²    to    x²/a² + y²/b² + z²/c² ≤ 1",
    point = { -6.52, 2.91 },
    align = { 0, 0.5 },
    role = "text",
    fill = p.muted,
}

local view = vis.scene {
    width = 900,
    height = 400,
    theme = "pro_white",
    camera = {
        view = "3d",
        eye = { 5.2, 3.8, 6.0 },
        fov = 0.66,
        near = 0.1,
        far = 100,
    },
}

local field = view:space {
    x = { -2.1, 2.1, 0.35 },
    y = { -2.1, 2.1, 0.35 },
    z = { -2.1, 2.1, 0.35 },
    stroke = p.grid,
    width = 0.8,
}

local duration = 1.8
field:voxel(function(x, y, z, time)
    local progress = time / duration
    progress = progress * progress * (3 - 2 * progress)
    local a = 1.5 + (1.9 - 1.5) * progress
    local b = 1.5 + (1.15 - 1.5) * progress
    local color = progress < 0.5 and p.blue or p.violet
    local value = x * x / (a * a) + y * y / (b * b) + z * z / (1.5 * 1.5)
    return value <= 1 and color or "#00000000"
end, { mode = "padd", padding = 0.055, duration = duration, fps = 24 })

view:wait(0.35)

page:viewport(view, { x = 0.03, y = 0.18, width = 0.94, height = 0.78 })
return page
