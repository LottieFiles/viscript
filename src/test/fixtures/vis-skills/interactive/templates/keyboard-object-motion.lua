-- Input-only keyboard motion.
-- Requires a viscript build configured with -Dmodules=interactive; no UI Panel is used.
if not vis.input then
    error("keyboard-object-motion.lua requires a viscript build configured with -Dmodules=interactive")
end

local WIDTH, HEIGHT = 960, 540
local scene = vis.scene {
    width = WIDTH, height = HEIGHT, fps = 60, loop = true,
    theme = "adaptive_vscode",
    camera = {mode = "fixed", view = "2d", target = {0, 0}, height = HEIGHT},
}

local LAYER = {field = 10, route = 20, mark = 30, text = 40}

scene:text {
    text = "Keyboard input without a UI control",
    point = {-424, 226}, align = {0, 0.5}, role = "h2",
    layer = LAYER.text, id = "keyboard-motion:title",
}
scene:text {
    text = "Hold Arrow keys or WASD for continuous motion independent of OS key repeat.",
    point = {-424, 187}, align = {0, 0.5}, role = "text", fill = "muted",
    layer = LAYER.text, id = "keyboard-motion:subtitle",
}

scene:rectangle {
    center = {0, -29}, size = {848, 344}, corner = 4,
    fill = "surface", stroke = "border", width = 1.5,
    layer = LAYER.field, id = "keyboard-motion:field",
}
local active_bottom = -145
for x = -384, 384, 48 do
    scene:line {
        from = {x, active_bottom}, to = {x, 143}, color = "border", width = 1,
        layer = LAYER.field, id = "keyboard-motion:grid:x:" .. tostring(x),
    }
end
for y = -125, 115, 48 do
    scene:line {
        from = {-424, y}, to = {424, y}, color = "border", width = 1,
        layer = LAYER.field, id = "keyboard-motion:grid:y:" .. tostring(y),
    }
end
scene:line {
    from = {-424, active_bottom}, to = {424, active_bottom},
    color = "border", width = 1.5, layer = LAYER.route,
    id = "keyboard-motion:contract:separator",
}

scene:text {
    text = "hold → v = ±400 px/s     start ramp = 120 ms     key repeat ignored",
    point = {-402, -174}, align = {0, 0.5}, role = "code", fill = "muted",
    layer = LAYER.text, id = "keyboard-motion:contract",
}

local cursor = scene:group {id = "keyboard-motion:cursor"}
cursor:rectangle {
    center = {0, -29}, size = {38, 38}, corner = 4,
    fill = "accent", stroke = "background", width = 3,
    layer = LAYER.mark, id = "keyboard-motion:cursor:body",
}
cursor:line {
    from = {-11, -29}, to = {11, -29}, color = "background", width = 3,
    layer = LAYER.mark + 1, id = "keyboard-motion:cursor:x",
}
cursor:line {
    from = {0, -40}, to = {0, -18}, color = "background", width = 3,
    layer = LAYER.mark + 1, id = "keyboard-motion:cursor:y",
}

local input = vis.input.controller(scene)
local moves = {
    {key = "ArrowLeft", shift = {-48, 0, 0}},
    {key = "A", shift = {-48, 0, 0}},
    {key = "ArrowRight", shift = {48, 0, 0}},
    {key = "D", shift = {48, 0, 0}},
    {key = "ArrowUp", shift = {0, 48, 0}},
    {key = "W", shift = {0, 48, 0}},
    {key = "ArrowDown", shift = {0, -48, 0}},
    {key = "S", shift = {0, -48, 0}},
}
for _, move in ipairs(moves) do
    input:key_move {
        target = cursor,
        key = move.key,
        shift = move.shift,
        period = 0.12,
    }
end

scene:wait(12)
return scene
