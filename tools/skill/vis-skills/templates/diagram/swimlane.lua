-- Question: which role owns each step, and where does responsibility cross a lane?
-- Replace step_specs and handoff_specs; keep roles, stages, and routing corridors separate.
-- Look: hairline grid, ink steps and handoffs, and one hue (accent) for the focal decision.
local scene = vis.scene {camera = {height = 540}}

local LAYER = {guide = 0, route = 10, node = 20, text = 30}
local root = scene:group {id = "release-handoff:root"}
local frame = root:group {id = "release-handoff:frame"}
local nodes = root:group {id = "release-handoff:steps"}

local ROLE_RIGHT = -300
local PROCESS_RIGHT = 450
local TOP = 170
local BOTTOM = -160
local lane_centers = {author = 115, reviewer = 5, operations = -105}
local stage_centers = {-220, -70, 80, 230, 380}

-- The frame is a process grid, not a set of decorative containers.
frame:line {
    from = {-450, TOP}, to = {PROCESS_RIGHT, TOP}, color = "border", width = 1,
    layer = LAYER.guide, id = "release-handoff:header-rule",
}
frame:line {
    from = {ROLE_RIGHT, 220}, to = {ROLE_RIGHT, BOTTOM}, color = "border", width = 1,
    layer = LAYER.guide, id = "release-handoff:role-divider",
}
for index, y in ipairs({60, -50, BOTTOM}) do
    frame:line {
        from = {-450, y}, to = {PROCESS_RIGHT, y}, color = "border", width = 1,
        opacity = index == 3 and 0.8 or 0.45, layer = LAYER.guide,
        id = "release-handoff:lane-rule:" .. index,
    }
end
for index, x in ipairs({-145, 5, 155, 305, PROCESS_RIGHT}) do
    frame:line {
        from = {x, TOP}, to = {x, BOTTOM}, color = "border", width = 1,
        opacity = index == 5 and 0.8 or 0.25, layer = LAYER.guide,
        id = "release-handoff:stage-rule:" .. index,
    }
end

local roles = {
    {id = "author", label = "AUTHOR"},
    {id = "reviewer", label = "REVIEWER"},
    {id = "operations", label = "OPERATIONS"},
}
for _, role in ipairs(roles) do
    frame:text {
        text = role.label, point = {-430, lane_centers[role.id]}, align = {0, 0.5},
        role = "code", fill = "muted", layer = LAYER.text,
        id = "release-handoff:role:" .. role.id,
    }
end

local stage_names = {"SUBMIT", "TRIAGE", "DECIDE", "DEPLOY", "REPORT"}
for index, label in ipairs(stage_names) do
    frame:text {
        text = string.format("%02d", index), point = {stage_centers[index], 208},
        role = "code", fill = "muted", layer = LAYER.text,
        id = "release-handoff:stage:" .. index .. ":number",
    }
    frame:text {
        text = label, point = {stage_centers[index], 184},
        role = "code", fill = "foreground", layer = LAYER.text,
        id = "release-handoff:stage:" .. index .. ":label",
    }
end

-- Copy/paste edit surface. Each step belongs to exactly one lane and one stage.
local step_specs = {
    {id = "submit", label = "Submit", detail = "AUTHOR", stage = 1, lane = "author"},
    {id = "triage", label = "Triage", detail = "REVIEWER", stage = 2, lane = "reviewer"},
    {id = "approve", label = "Approve", detail = "POLICY", stage = 3, lane = "reviewer", focal = true},
    {id = "deploy", label = "Deploy", detail = "OPERATIONS", stage = 4, lane = "operations"},
    {id = "notify", label = "Notify", detail = "RESULT", stage = 5, lane = "author"},
}

local STEP_W = 128
local STEP_H = 50
for _, step in ipairs(step_specs) do
    local center = {stage_centers[step.stage], lane_centers[step.lane]}
    local prefix = "release-handoff:step:" .. step.id
    nodes:rectangle {
        center = center, size = {STEP_W, STEP_H},
        stroke = step.focal and "accent" or "foreground", width = step.focal and 1.5 or 1.25,
        layer = LAYER.node, id = prefix .. ":body",
    }
    nodes:text {
        text = step.label, point = {center[1], center[2] + 11}, role = "text",
        fill = step.focal and "accent" or "foreground", layer = LAYER.text,
        id = prefix .. ":label",
    }
    nodes:text {
        text = step.detail, point = {center[1], center[2] - 14}, role = "code",
        fill = "muted", layer = LAYER.text, id = prefix .. ":detail",
    }
end

-- Explicit corridors keep handoffs out of node bodies and make lane crossings auditable.
local handoff_specs = {
    {
        id = "submit-triage", from = "submit", to = "triage", label = "HANDOFF",
        points = {{-220, 90}, {-220, 42}, {-70, 42}, {-70, 30}},
        label_point = {-145, 56},
    },
    {
        id = "triage-approve", from = "triage", to = "approve", label = "REVIEW",
        points = {{-6, 5}, {16, 5}}, label_point = {5, 47},
    },
    {
        id = "approve-deploy", from = "approve", to = "deploy", label = "APPROVED",
        points = {{80, -20}, {80, -40}, {230, -40}, {230, -80}},
        label_point = {155, -58},
    },
    {
        id = "deploy-notify", from = "deploy", to = "notify", label = "RELEASED",
        points = {{294, -105}, {380, -105}, {380, 90}},
        label_point = {365, 5}, label_align = {1, 0.5},
    },
}

local handoffs = {}
for _, spec in ipairs(handoff_specs) do
    local prefix = "release-handoff:handoff:" .. spec.id
    local route = root:route {
        points = spec.points, tip = 7, color = "foreground", width = 1,
        layer = LAYER.route, id = prefix .. ":route",
    }
    local label = root:text {
        text = spec.label, point = spec.label_point, align = spec.label_align,
        role = "code", fill = "muted",
        layer = LAYER.text, id = prefix .. ":label",
    }
    handoffs[#handoffs + 1] = {route = route, label = label}
end

-- The answer caption is counted from the specs: a handoff crosses a lane when its steps' lanes differ.
local lane_of, crossings = {}, 0
for _, step in ipairs(step_specs) do lane_of[step.id] = step.lane end
for _, spec in ipairs(handoff_specs) do
    if lane_of[spec.from] ~= lane_of[spec.to] then crossings = crossings + 1 end
end
local caption = root:text {
    text = string.format("%d of %d handoffs cross a lane", crossings, #handoff_specs),
    point = {-430, BOTTOM - 38}, align = {0, 0.5}, role = "text", fill = "foreground",
    layer = LAYER.text, id = "release-handoff:caption",
}

-- The grid and steps settle together; handoffs then trace in process order.
scene:fade_in(frame, {duration = 0.5, curve = "gentle"})
scene:fade_in(nodes, {shift = {0, -6}, duration = 0.6, curve = "gentle"})
for _, handoff in ipairs(handoffs) do
    scene:create(handoff.route, {duration = 0.5, curve = "ease_out"})
    scene:fade_in(handoff.label, {shift = {0, -4}, duration = 0.18, curve = "gentle"})
end
scene:fade_in(caption, {duration = 0.3, curve = "gentle"})
scene:wait(2.4)
return scene
