-- Project one sample onto a gradient direction and reuse t for every color channel.
local scene = vis.scene {camera = {height = 6.2}}

local start_rgb, end_rgb = {76, 201, 240}, {255, 107, 107}
local stop_t = {0, 1}
local columns, sample_index = 32, 11
local sample_t = sample_index / (columns - 1)
local function color_at(t)
    local channels = {}
    for i = 1, 3 do
        channels[i] = math.floor(start_rgb[i] + (end_rgb[i] - start_rgb[i]) * t + 0.5)
    end
    return string.format("#%02x%02x%02x", channels[1], channels[2], channels[3])
end

local left, right, arrow_y = -4.8, 4.8, 1.25
local sample_x = left + (right - left) * sample_t
local start_color, end_color, sample_color = color_at(stop_t[1]), color_at(stop_t[2]), color_at(sample_t)
local direction = scene:arrow {
    id = "gradient-direction", from = {left, arrow_y}, to = {right, arrow_y},
    stroke = "foreground", width = 1.5, tail = 7, tip = 7, trim_distance = {0, 0.16}, layer = 20,
}
local stops = {
    scene:point {id = "gradient-stop-0", point = {left, arrow_y}, fill = start_color, radius = 8, layer = 30},
    scene:point {id = "gradient-stop-1", point = {right, arrow_y}, fill = end_color, radius = 8, layer = 30},
}
local sample = scene:point {id = "gradient-sample", point = {sample_x, arrow_y}, fill = sample_color, radius = 8, layer = 30}
local guide = scene:line {
    id = "gradient-sample-guide", from = {sample_x, arrow_y - 0.16}, to = {sample_x, -0.38},
    stroke = sample_color, width = 1, layer = 10,
}

-- Cell centers map exactly to the arrow endpoints, including the selected column, which takes an ink
-- outline because a stroke in its own color would vanish against it.
local cell_scale = (right - left) / (columns - 1)
local pixel_space = scene:space {
    x = {0, columns, 1}, y = {0, 4, 1}, opacity = 0,
    matrix = {cell_scale, 0, 0, left - 0.5 * cell_scale, 0, 0.28, 0, -1.58, 0, 0, 1, 0, 0, 0, 0, 1},
}
local patches = {}
for x = 0, columns - 1 do
    patches[#patches + 1] = {region = {x, 0, 1, 4}, color = color_at(x / (columns - 1))}
end
local gradient = pixel_space:cell {
    id = "gradient-ramp", origin = {0, 0}, size = {columns, 4},
    mode = "full", color = "surface", patches = patches, layer = 0,
}
local selected = pixel_space:rectangle {
    id = "gradient-selected-cell", center = {sample_index + 0.5, 2}, size = {1, 4},
    stroke = "foreground", width = 2, layer = 20,
}

local labels = {
    scene:text {id = "gradient-stop-0-label", text = string.format("t = %.0f", stop_t[1]), point = {left, 0.83}, role = "code", fill = start_color, align = {0, 0.5}, layer = 40},
    scene:text {id = "gradient-sample-label", text = string.format("t = %.2f", sample_t), point = {sample_x + 0.12, 0.52}, role = "code", fill = sample_color, align = {0, 0.5}, layer = 40},
    scene:text {id = "gradient-stop-1-label", text = string.format("t = %.0f", stop_t[2]), point = {right, 0.83}, role = "code", fill = end_color, align = {1, 0.5}, layer = 40},
    scene:text {id = "gradient-formula", text = "color(t) = mix(c0, c1, t)", point = {0, -2.35}, role = "code", fill = "foreground", align = {0.5, 0.5}, layer = 40},
}

-- Establish direction and stops before connecting the projected sample to its ramp cell.
scene:create(direction, {duration = 0.55, curve = "ease_out"})
scene:grow_from_center(stops[1], {duration = 0.18, curve = "ease_out"})
scene:grow_from_center(stops[2], {duration = 0.18, curve = "ease_out"})
scene:fade_in(labels[1], {shift = {0, 0.08}, duration = 0.22, curve = "gentle"})
scene:fade_in(labels[3], {shift = {0, 0.08}, duration = 0.22, curve = "gentle"})
scene:grow_from_center(sample, {duration = 0.22, curve = "ease_out"})
scene:create(guide, {duration = 0.36, curve = "ease_out"})
scene:fade_in(labels[2], {shift = {0, 0.08}, duration = 0.22, curve = "gentle"})
scene:create(gradient, {duration = 0.95, curve = "linear"})
scene:create(selected, {duration = 0.28, curve = "ease_out"})
scene:fade_in(labels[4], {shift = {0, -0.08}, duration = 0.28, curve = "gentle"})
scene:wait(1.2)
return scene
