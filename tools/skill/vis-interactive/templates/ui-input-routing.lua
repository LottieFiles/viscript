-- Combined UI and Input routing template.
-- Requires a viscript build configured with -Dmodules=interactive.
if not vis.ui then
    error("ui-input-routing.lua requires a viscript build configured with -Dmodules=interactive")
end
if not vis.input then
    error("ui-input-routing.lua requires a viscript build configured with -Dmodules=interactive")
end

local WIDTH, HEIGHT = 960, 540
local scene = vis.scene {
    width = WIDTH, height = HEIGHT, fps = 30, loop = true,
    theme = "adaptive",
    camera = {mode = "fixed", view = "2d", target = {0, 0}, height = HEIGHT},
}

local LAYER = {surface = 10, grid = 12, data = 20, mark = 30, text = 40, control = 50}
local UI_RAIL = {left = -420, right = -188, bottom = -142, top = 148}
local FIELD = {left = -142, right = 418, bottom = -142, top = 148}

local function pixel_region(left, top, right, bottom)
    return {
        x = left + WIDTH / 2,
        y = HEIGHT / 2 - top,
        width = right - left,
        height = top - bottom,
    }
end

local function translate(x, y)
    return {1, 0, 0, x, 0, 1, 0, y, 0, 0, 1, 0, 0, 0, 0, 1}
end
local function text(spec) scene:text(spec) end

for _, spec in ipairs {
    {text = "One Canvas, two independent systems", point = {-424, 226}, align = {0, 0.5}, role = "h2", layer = LAYER.text, id = "routing:title"},
    {text = "UI owns captured gestures. Only unhandled pointer motion continues to Input.", point = {-424, 187}, align = {0, 0.5}, role = "text", fill = "muted", layer = LAYER.text, id = "routing:subtitle"},
} do text(spec) end

scene:rectangle {
    center = {(UI_RAIL.left + UI_RAIL.right) / 2, 3},
    size = {UI_RAIL.right - UI_RAIL.left, UI_RAIL.top - UI_RAIL.bottom}, corner = 4,
    fill = "surface", stroke = "border", width = 1.5,
    layer = LAYER.surface, id = "routing:ui:surface",
}
for _, spec in ipairs {
    {text = "UI / captured", point = {UI_RAIL.left + 24, 116}, align = {0, 0.5}, role = "h3", layer = LAYER.text, id = "routing:ui:title"},
    {text = "Drag gain, keep holding.", point = {UI_RAIL.left + 24, 72}, align = {0, 0.5}, role = "text", fill = "muted", layer = LAYER.text, id = "routing:ui:instruction:drag"},
    {text = "Move into the field.", point = {UI_RAIL.left + 24, 43}, align = {0, 0.5}, role = "text", fill = "muted", layer = LAYER.text, id = "routing:ui:instruction:move"},
} do text(spec) end

local track_parent = scene:group {
    matrix = translate(UI_RAIL.left + 34, -15), id = "routing:ui:track-parent",
}
local track = track_parent:group {id = "routing:ui:track"}
track:line {
    from = {0, 0}, to = {164, 0}, color = "border", width = 5,
    layer = LAYER.control, id = "routing:ui:track:shaft",
}
track:line {
    from = {0, -9}, to = {0, 9}, color = "border", width = 1.5,
    layer = LAYER.control, id = "routing:ui:track:min",
}
track:line {
    from = {164, -9}, to = {164, 9}, color = "border", width = 1.5,
    layer = LAYER.control, id = "routing:ui:track:max",
}
local handle = track_parent:circle {
    center = {0, 0}, radius = 10, fill = "focus", stroke = "background", width = 3,
    layer = LAYER.control + 1, id = "routing:ui:handle",
}

local level_parent = scene:group {
    matrix = translate(UI_RAIL.left + 34, -79), id = "routing:ui:level-parent",
}
level_parent:line {
    from = {0, 0}, to = {164, 0}, color = "border", width = 10,
    layer = LAYER.data, id = "routing:ui:level:base",
}
local level = level_parent:line {
    from = {0, 0}, to = {164, 0}, color = "accent", width = 10,
    layer = LAYER.data + 1, id = "routing:ui:level:value",
}
text {text = "drag remains captured", point = {UI_RAIL.left + 24, -119}, align = {0, 0.5}, role = "code", fill = "muted", layer = LAYER.text, id = "routing:ui:capture-note"}

scene:rectangle {
    center = {(FIELD.left + FIELD.right) / 2, 3},
    size = {FIELD.right - FIELD.left, FIELD.top - FIELD.bottom}, corner = 4,
    fill = "#00000000", stroke = "border", width = 1.5,
    layer = LAYER.surface, id = "routing:input:field",
}
for x = FIELD.left + 56, FIELD.right - 1, 56 do
    scene:line {
        from = {x, FIELD.bottom}, to = {x, FIELD.top}, color = "#FFFFFF10", width = 1,
        layer = LAYER.grid, id = "routing:input:grid:x:" .. tostring(x),
    }
end
for y = FIELD.bottom + 58, FIELD.top - 1, 58 do
    scene:line {
        from = {FIELD.left, y}, to = {FIELD.right, y}, color = "#FFFFFF10", width = 1,
        layer = LAYER.grid, id = "routing:input:grid:y:" .. tostring(y),
    }
end
for _, spec in ipairs {
    {text = "INPUT / unhandled", point = {FIELD.left + 24, 116}, align = {0, 0.5}, role = "h3", layer = LAYER.text, id = "routing:input:title"},
    {text = "Move the pointer here after releasing the slider.", point = {FIELD.left + 24, 80}, align = {0, 0.5}, role = "text", fill = "muted", layer = LAYER.text, id = "routing:input:instruction"},
} do text(spec) end

local follower = scene:group {id = "routing:input:follower"}
follower:circle {
    center = {0, 0}, radius = 17, fill = "accent", stroke = "background", width = 3,
    layer = LAYER.mark, id = "routing:input:follower:body",
}
follower:line {
    from = {0, 0}, to = {34, 0}, color = "focus", width = 4,
    layer = LAYER.mark + 1, id = "routing:input:follower:heading",
}
follower:circle {
    center = {0, 0}, radius = 4, fill = "background", stroke = "background",
    layer = LAYER.mark + 2, id = "routing:input:follower:pivot",
}

for _, spec in ipairs {
    {text = "UI handled → stop", point = {-414, -184}, align = {0, 0.5}, role = "code", fill = "focus", layer = LAYER.text, id = "routing:ledger:ui"},
    {text = "UI unhandled → Input", point = {-80, -184}, align = {0, 0.5}, role = "code", fill = "accent", layer = LAYER.text, id = "routing:ledger:input"},
} do text(spec) end
scene:line {
    from = {-184, -184}, to = {-112, -184}, color = "border", width = 2,
    layer = LAYER.data, id = "routing:ledger:separator",
}

local panel = vis.ui.panel(scene)
panel:slider {
    visual = track,
    region = pixel_region(UI_RAIL.left + 22, 12, UI_RAIL.right - 22, -42),
    axis = "horizontal",
    value = 0.35,
    bindings = {
        {target = handle, from = {}, to = {shift = {164, 0, 0}}},
        {target = level, from = {progress = 0.08}, to = {progress = 1}},
    },
}

local input = vis.input.controller(scene)
input:pointer_follow {
    target = follower,
    region = pixel_region(FIELD.left, FIELD.top, FIELD.right, FIELD.bottom),
    target_origin = {0, 0, 0},
    map_origin = {FIELD.left, FIELD.top, 0},
    map_x = {FIELD.right - FIELD.left, 0, 0},
    map_y = {0, FIELD.bottom - FIELD.top, 0},
    period = 0.16,
    rotate = true,
    reset_on_leave = false,
}

scene:wait(12)
return scene
