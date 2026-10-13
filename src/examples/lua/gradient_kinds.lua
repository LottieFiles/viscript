-- Linear, radial and conic gradients side by side.
-- Stops resolve Theme color roles or hex colors; geometry is in local coordinates.
local scene = vis.scene {
    width = 960,
    height = 540,
    fps = 30,
    antialiasing = true,
    background = "#0b111a",
    camera = { mode = "fixed", view = "2d", height = 7.4 },
}
local space = scene:space {
    x = { -6.5, 6.5, 1 },
    y = { -3.5, 3.5, 1 },
    opacity = 0,
}

local title = space:text {
    text = "Gradient kinds",
    point = { 0, 2.95 },
    font = "Pretendard",
    size = 30,
    fill = "#f4f7fb",
}

local linear = space:rectangle {
    id = "linear",
    center = { -4.2, 0.2 },
    size = { 3.4, 3.4 },
    corner = 0.12,
    stroke = "#00000000",
    fill = "#ffffff",
    gradient = {
        type = "linear",
        stops = { { 0, "#4cc9f0" }, { 0.5, "#f4f7fb" }, { 1, "#ff6b6b" } },
        from = { -5.9, 0.2 },
        to = { -2.5, 0.2 },
    },
}
local radial = space:circle {
    id = "radial",
    center = { 0, 0.2 },
    radius = 1.7,
    stroke = "#00000000",
    fill = "#ffffff",
    gradient = {
        type = "radial",
        stops = { "#ffd166", "#ff6b6b", "#25314a" },
        focal = { -0.45, 0.65 },
    },
}
local conic = space:circle {
    id = "conic",
    center = { 4.2, 0.2 },
    radius = 1.7,
    stroke = "#00000000",
    fill = "#ffffff",
    gradient = {
        type = "conic",
        stops = { "#4cc9f0", "#7bd88f", "#ffd166", "#ff6b6b", "#4cc9f0" },
        angle = -90,
    },
}

local labels = {}
for i, item in ipairs({ { -4.2, "linear · from → to" }, { 0, "radial · center, radius, focal" }, { 4.2, "conic · center, angle" } }) do
    labels[i] = space:text {
        text = item[2],
        point = { item[1], -2.15 },
        font = "Pretendard",
        size = 16,
        fill = "#94a3b8",
    }
end

scene:create(title, 0.4, "ease_out")
scene:create({ linear, radial, conic }, 0.8, "ease_out", 0.1)
scene:create(labels, 0.4, "ease_out", 0.05)
scene:wait(0.6)
return scene
