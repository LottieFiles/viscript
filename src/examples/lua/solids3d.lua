local scene = vis.scene {
    camera = {
        mode = "interactive",
        view = "3d",
        eye = { 8, 6, 10 },
        fov = 0.72,
        near = 0.1,
        far = 100,
    },
}
local space = scene:space {
    x = { -5, 5, 1 },
    y = { -3, 3, 1 },
    z = { -4, 4, 1 },
    color = "#26313b",
}
local shapes = {}
local function edge(a, b, color)
    shapes[#shapes + 1] = space:line { from = a, to = b, color = color, width = 2.5 }
end

local cube = {
    { -3.8, -1, -1 },
    { -1.8, -1, -1 },
    { -1.8, 1, -1 },
    { -3.8, 1, -1 },
    { -3.8, -1, 1 },
    { -1.8, -1, 1 },
    { -1.8, 1, 1 },
    { -3.8, 1, 1 },
}
local cubeEdges = {
    { 1, 2 },
    { 2, 3 },
    { 3, 4 },
    { 4, 1 },
    { 5, 6 },
    { 6, 7 },
    { 7, 8 },
    { 8, 5 },
    { 1, 5 },
    {
        2,
        6,
    },
    {
        3,
        7,
    },
    {
        4,
        8,
    },
}
for i = 1, #cubeEdges do
    edge(cube[cubeEdges[i][1]], cube[cubeEdges[i][2]], "#4cc9f0")
end

local base = { { -0.8, -1, -1 }, { 1.2, -1, -1 }, { 1.2, -1, 1 }, { -0.8, -1, 1 } }
for i = 1, 4 do
    edge(base[i], base[i % 4 + 1], "#ffd166")
    edge(base[i], { 0.2, 1.4, 0 }, "#ffd166")
end

for ring = 0, 2 do
    local points = {}
    local y = -1 + ring
    for i = 0, 48 do
        local angle = i * 6.28318530718 / 48
        points[#points + 1] = { 3.2 + math.cos(angle), y, math.sin(angle) }
    end
    shapes[#shapes + 1] = space:plot { points = points, color = "#7bd88f", width = 2.5 }
end
for i = 0, 3 do
    local angle = i * 1.57079632679
    edge(
        { 3.2 + math.cos(angle), -1, math.sin(angle) },
        { 3.2 + math.cos(angle), 1, math.sin(angle) },
        "#7bd88f"
    )
end
local cube_label = space:text {
    text = "cube",
    point = { -4.4, 2.15, 0 },
    align = { 0, 0.5 },
    font = "Pretendard",
    size = 22,
    fill = "#4cc9f0",
}
local pyramid_label = space:text {
    text = "pyramid",
    point = { -0.95, 2.15, 0 },
    align = { 0, 0.5 },
    font = "Pretendard",
    size = 22,
    fill = "#ffd166",
}
local cylinder_label = space:text {
    text = "cylinder",
    point = { 2.45, 2.15, 0 },
    align = { 0, 0.5 },
    font = "Pretendard",
    size = 22,
    fill = "#7bd88f",
}

scene:create(shapes, {duration = 1.2, curve = "ease_out", lag = 0.025})
scene:create({ cube_label, pyramid_label, cylinder_label }, {duration = 0.45, curve = "ease_out", lag = 0.08})
scene:wait(0.7)
return scene
