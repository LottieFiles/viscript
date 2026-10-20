-- Interactive state template.
-- Requires a viscript build configured with -Dmodules=interactive.
-- Replace the two state labels, descriptions, target positions, and Button labels together.
if not vis.ui then
    error("button-state-motion.lua requires a viscript build configured with -Dmodules=interactive")
end

local WIDTH, HEIGHT = 960, 540
local scene = vis.scene {
    width = WIDTH, height = HEIGHT, fps = 30, loop = true,
    theme = "adaptive",
    camera = {mode = "fixed", view = "2d", target = {0, 0}, height = HEIGHT},
}

local LAYER = {route = 10, body = 20, focus = 30, text = 40, control = 50}
local BUTTON = {width = 190, height = 52, stroke = 2}

local function pixel_region(cx, cy, width, height)
    return {
        x = cx - width / 2 + WIDTH / 2,
        y = HEIGHT / 2 - (cy + height / 2),
        width = width,
        height = height,
    }
end

scene:text {
    text = "One process, two explicit states",
    point = {-424, 226}, align = {0, 0.5}, role = "h2",
    layer = LAYER.text, id = "state-demo:title",
}
scene:text {
    text = "Select a state. The persistent indicator moves with a timed transition.",
    point = {-424, 187}, align = {0, 0.5}, role = "text", fill = "muted",
    layer = LAYER.text, id = "state-demo:subtitle",
}

local state_y = 45
local queued_x, running_x = -230, 230

scene:rectangle {
    center = {queued_x, state_y}, size = {250, 124}, corner = 4,
    fill = "surface", stroke = "border", width = 1.5,
    layer = LAYER.body, id = "state-demo:queued:body",
}
scene:text {
    text = "Queued", point = {queued_x, state_y + 18}, role = "h3",
    layer = LAYER.text, id = "state-demo:queued:title",
}
scene:text {
    text = "waiting for a worker", point = {queued_x, state_y - 25},
    role = "text", fill = "muted", layer = LAYER.text,
    id = "state-demo:queued:description",
}

scene:rectangle {
    center = {running_x, state_y}, size = {250, 124}, corner = 4,
    fill = "surface", stroke = "border", width = 1.5,
    layer = LAYER.body, id = "state-demo:running:body",
}
scene:text {
    text = "Running", point = {running_x, state_y + 18}, role = "h3",
    layer = LAYER.text, id = "state-demo:running:title",
}
scene:text {
    text = "task is executing", point = {running_x, state_y - 25},
    role = "text", fill = "muted", layer = LAYER.text,
    id = "state-demo:running:description",
}

scene:arrow {
    from = {-94, state_y}, to = {94, state_y}, tip = 11,
    color = "muted", width = 2, layer = LAYER.route,
    id = "state-demo:transition:route",
}
scene:text {
    text = "dispatch", point = {0, state_y + 28}, role = "code", fill = "muted",
    layer = LAYER.text, id = "state-demo:transition:label",
}

-- One semantic target persists across both states; the Buttons only select its transform.
local indicator = scene:group {id = "state-demo:indicator"}
indicator:rectangle {
    center = {queued_x, state_y}, size = {270, 144}, corner = 6,
    fill = "#00000000", stroke = "focus", width = 3,
    layer = LAYER.focus, id = "state-demo:indicator:outline",
}
indicator:circle {
    center = {queued_x - 101, state_y + 47}, radius = 7,
    fill = "focus", stroke = "focus", layer = LAYER.focus,
    id = "state-demo:indicator:dot",
}

scene:text {
    text = "Choose the process state",
    point = {0, -112}, role = "text", fill = "muted",
    layer = LAYER.text, id = "state-demo:controls:label",
}

local function button_faces(id, label, center)
    scene:text {
        text = label, point = center, role = "code", fill = "foreground",
        layer = LAYER.control + 1, id = "state-demo:button:" .. id .. ":label",
    }

    local function face(state, stroke, width)
        return scene:rectangle {
            center = center, size = {BUTTON.width, BUTTON.height}, corner = 4,
            fill = "surface", stroke = stroke, width = width,
            layer = LAYER.control, id = "state-demo:button:" .. id .. ":" .. state .. ":body",
        }
    end

    return {
        idle = face("idle", "border", BUTTON.stroke),
        hover = face("hover", "accent", BUTTON.stroke),
        pressed = face("pressed", "focus", BUTTON.stroke),
    }
end

local queued_button = button_faces("queued", "Set queued", {-145, -198})
local running_button = button_faces("running", "Set running", {145, -198})

local panel = vis.ui.panel(scene)
panel:button {
    visual = queued_button.idle,
    hover_visual = queued_button.hover,
    pressed_visual = queued_button.pressed,
    region = pixel_region(-145, -198, BUTTON.width + BUTTON.stroke, BUTTON.height + BUTTON.stroke),
    target = indicator,
    transform = {},
    duration = 0.48,
}
panel:button {
    visual = running_button.idle,
    hover_visual = running_button.hover,
    pressed_visual = running_button.pressed,
    region = pixel_region(145, -198, BUTTON.width + BUTTON.stroke, BUTTON.height + BUTTON.stroke),
    target = indicator,
    transform = {shift = {running_x - queued_x, 0, 0}},
    duration = 0.48,
}

scene:wait(12)
return scene
