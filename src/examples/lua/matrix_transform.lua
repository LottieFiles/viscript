local scene = vis.scene {camera = { mode = "interactive", height = 7 }}

local world = scene:space {
    x = { -6, 6, 1 },
    y = { -4, 4, 1 },
    z = { 0, 0, 1 },
    color = "#28323d",
    id = "global-space",
}
local space = scene:space {
    x = { -3, 3, 1 },
    y = { -3, 3, 1 },
    z = { 0, 0, 1 },
    color = "#665c99",
    axis_x = "#ff8a80",
    axis_y = "#9ccc65",
    id = "local-space",
}
local square = space:polygon {
    points = { { 0, 0 }, { 2, 0 }, { 2, 2 }, { 0, 2 } },
    fill = "#4cc9f044",
    stroke = "#4cc9f0",
    width = 4,
    id = "unit-square",
}
local basis = space:vector {
    value = { 2, 0 },
    color = "#ffd166",
    width = 5,
    id = "basis-x",
}

scene:create({ world, space, square, basis }, {duration = 0.8, curve = "ease_out", lag = 0.12})
scene:transform(space, {
    1.0,
    0.65,
    0,
    0,
    0.35,
    1.0,
    0,
    0,
    0,
    0,
    1,
    0,
    0,
    0,
    0,
    1,
}, {duration = 1.4, curve = "ease_in_out"})
scene:wait(0.5)
return scene
