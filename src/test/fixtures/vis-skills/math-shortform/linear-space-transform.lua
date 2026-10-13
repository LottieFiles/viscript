-- One owning Space carries its complete geometric family through real matrices.
local scene = vis.scene {
    width = 960, height = 540, fps = 30, loop = false,
    camera = {mode = "fixed", view = "2d", target = {0, 0}, height = 7.4},
}

local identity = {1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1}
local matrices = {
    {1, 0.62, 0, 0.15, 0.18, 1, 0, -0.12, 0, 0, 1, 0, 0, 0, 0, 1},
    {0.58, -0.82, 0, -0.1, 0.82, 0.78, 0, 0.08, 0, 0, 1, 0, 0, 0, 0, 1},
    identity,
}
local world = scene:space {
    id = "shortform-reference-space", x = {-6, 6, 1}, y = {-3.4, 3.4, 0.5}, numbers = false,
    stroke = "border", axis_x = "border", axis_y = "border", width = 1, opacity = 0.45,
}
local localSpace = scene:space {
    id = "shortform-transform-space", x = {-4.5, 4.5, 0.5}, y = {-2.7, 2.7, 0.5}, numbers = false,
    stroke = "accent", axis_x = "accent", axis_y = "secondary", width = 1.3, opacity = 0.62,
}

-- The two basis vectors span the area; their sum is its opposite corner.
local basisX, basisY = {1.55, 0}, {0, 1.55}
local areaCorner = {basisX[1] + basisY[1], basisX[2] + basisY[2]}
local region = localSpace:polygon {
    id = "shortform-transform-area", points = {{0, 0}, basisX, areaCorner, basisY},
    fill = "result", stroke = "result", opacity = 0.22, width = 3, layer = 5,
}
local basis = {
    localSpace:vector {id = "shortform-basis-x", origin = {0, 0}, value = basisX, stroke = "accent", width = 5, tip = 13, layer = 20},
    localSpace:vector {id = "shortform-basis-y", origin = {0, 0}, value = basisY, stroke = "secondary", width = 5, tip = 13, layer = 20},
}
local sample = localSpace:point {id = "shortform-transform-point", point = areaCorner, fill = "result", radius = 8, layer = 30}

scene:create(world, 0.5, "ease_out")
scene:create(localSpace, 0.5, "ease_out")
scene:draw_border_then_fill(region, 0.45, "ease_out")
scene:create({basis[1], basis[2], sample}, 0.45, "ease_out", 0.05)
scene:wait(0.45)

-- Contrasting targets expose shear, rotation/scale, then exact return.
scene:transform(localSpace, matrices[1], 1.25, "ease_in_out")
scene:wait(0.42)
scene:transform(localSpace, matrices[2], 1.25, "ease_in_out")
scene:wait(0.42)
scene:transform(localSpace, matrices[3], 1.25, {preset = "ease_in_out", reverse = true})

local conclusion = scene:text {
    id = "shortform-transform-conclusion", text = "grid · basis · point · area  —  one matrix",
    point = {0, -3.12}, role = "code", fill = "result", align = {0.5, 0.5}, layer = 40,
}
scene:fade_in(conclusion, {shift = {0, 0.08}, duration = 0.34, curve = "gentle"})
scene:wait(1.7)
return scene
