-- Compare local coordinate numbers with labels scaled by the Space basis.
local scene = vis.scene {
    width = 960, height = 540, fps = 30, loop = false,
    camera = {mode = "fixed", view = "2d", target = {0, 0}, height = 6.2},
}

local range_center, panel_y = {2, 1.5}, -0.1
local point_value, basis_scale = {3, 2}, 0.75
local function matrix_at(scale, center_x)
    return {
        scale, 0, 0, center_x - range_center[1] * scale,
        0, scale, 0, panel_y - range_center[2] * scale,
        0, 0, 1, 0,
        0, 0, 0, 1,
    }
end

-- Both cases own identical local geometry; only the number interpretation differs.
local function coordinate_space(id, center_x, mode, color)
    local space = scene:space {
        id = id .. "-space", x = {0, 4, 1}, y = {0, 3, 1}, numbers = true,
        number_mode = mode, number_color = color, stroke = "border",
        axis_x = color, axis_y = color, width = 1.2, matrix = matrix_at(basis_scale, center_x),
    }
    space:point {
        id = id .. "-point", point = point_value, fill = "result", radius = 7, layer = 10,
    }
    space:text {
        id = id .. "-point-label", text = string.format("p = (%d, %d)", point_value[1], point_value[2]),
        point = {point_value[1] + 0.18, point_value[2] + 0.18}, role = "code",
        fill = "result", align = {0, 0.5}, layer = 40,
    }
    local mode_label = scene:text {
        id = id .. "-mode-label", text = mode, point = {center_x, 2.08},
        role = "code", fill = color, align = {0.5, 0.5}, layer = 40,
    }
    return {space = space, mode_label = mode_label}
end

local fixed = coordinate_space("coordinates-fixed", -2.7, "fixed", "accent")
local relative = coordinate_space("coordinates-relative", 2.7, "relative", "secondary")
local scale_label = scene:text {
    id = "coordinates-scale-label",
    text = string.format("1 local step = %.2f parent units", basis_scale),
    point = {0, -2.35}, role = "code", fill = "foreground", align = {0.5, 0.5}, layer = 40,
}

-- Reveal the same transformed geometry twice so only the number interpretation changes.
scene:create(fixed.space, 0.7, "ease_out")
scene:create(relative.space, 0.7, "ease_out")
scene:fade_in(fixed.mode_label, {shift = {0, -0.08}, duration = 0.24, curve = "gentle"})
scene:fade_in(relative.mode_label, {shift = {0, -0.08}, duration = 0.24, curve = "gentle"})
scene:wait(0.55)
scene:fade_in(scale_label, {shift = {0, -0.08}, duration = 0.3, curve = "gentle"})
scene:wait(1.4)
return scene
