local p = { paper = "#f7f8fb", ink = "#182033", muted = "#687086", blue = "#2563eb" }
local scene = vis.scene {
    fps = 60,
    background = p.paper,
    camera = {
        view = "3d",
        eye = { 5.8, 4.4, 6.4 },
        target = { 0, 0.25, 0 },
        fov = 0.66,
        near = 0.1,
        far = 100,
    },
}
scene:text {
    text = "3D SURFACE PLOT",
    point = { -3.8, 3.05, 0 },
    align = { 0, 0.5 },
    font = "Pretendard",
    size = 25,
    fill = p.ink,
}
scene:text {
    text = "Gaussian density sampled as a surface",
    point = { -3.77, 2.58, 0 },
    align = { 0, 0.5 },
    font = "Pretendard",
    size = 12,
    fill = p.muted,
}
local columns, rows, points = 31, 31, {}
local function gaussian(x, z)
    return 2.35 * math.exp(-(x * x + z * z) / 1.45) - 0.75
end
for row = 0, rows - 1 do
    local z = -2.7 + 5.4 * row / (rows - 1)
    for column = 0, columns - 1 do
        local x = -2.7 + 5.4 * column / (columns - 1)
        points[#points + 1] = { x, gaussian(x, z), z }
    end
end
local surface = scene:surface {
    points = points,
    size = { columns, rows },
    mode = "solid_mesh",
    shading = false,
    fill = p.blue .. "4d",
    stroke = p.blue .. "b8",
    width = 0.75,
    id = "gaussian-surface",
}
scene:create(surface, {duration = 1.0, curve = "gentle"})
scene:look({ eye = { -5.4, 4.8, 6.0 } }, {duration = 1.2, curve = "ease_in_out"})
scene:wait(0.8)
return scene
