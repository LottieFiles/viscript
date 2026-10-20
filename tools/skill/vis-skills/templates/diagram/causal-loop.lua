-- Question: which event moves a process between scheduler states, and where does execution repeat?
-- This is an operational state lifecycle; model guard/choice-heavy branching with explicit choice nodes.
-- Replace state_specs and transition labels; preserve the Ready -> Running -> Waiting -> Ready cycle.
-- Look: ink states and transitions; accent marks execution (Running and the dispatch into it), and the
-- final state is dimmed context (`muted`). The caption above the rule states what the figure shows.
local scene = vis.scene {
    width = 960, height = 540, fps = 30, loop = false,
    camera = {mode = "fixed", view = "2d", target = {0, 0}, height = 540},
}

local LAYER = {guide = 0, route = 10, state = 20, text = 30}
local root = scene:group {id = "process-lifecycle:root"}
local header = root:group {id = "process-lifecycle:header"}
local states = root:group {id = "process-lifecycle:states"}
local routes = root:group {id = "process-lifecycle:routes"}
local routeLabels = root:group {id = "process-lifecycle:route-labels"}

header:text {
    text = "Scheduler and I/O events drive the repeating execution cycle",
    point = {-430, 184}, align = {0, 0.5}, role = "code", fill = "muted",
    layer = LAYER.text, id = "process-lifecycle:subtitle",
}
header:line {
    from = {-430, 158}, to = {430, 158}, color = "border", width = 1,
    layer = LAYER.guide, id = "process-lifecycle:header-rule",
}

local state_specs = {
    {id = "new", label = "New", detail = "created", center = {-370, 60}, size = {120, 76}},
    {id = "ready", label = "Ready", detail = "run queue", center = {-160, 60}, size = {150, 80}},
    {id = "running", label = "Running", detail = "on CPU", center = {104, 60}, size = {160, 80}, color = "accent", focal = true},
    {id = "terminated", label = "Terminated", detail = "exit status", center = {350, 60}, size = {160, 80}, color = "muted"},
    {id = "waiting", label = "Waiting", detail = "sleeping / blocked", center = {104, -150}, size = {224, 80}},
}

local stateHandles = {}
for _, spec in ipairs(state_specs) do
    local group = states:group {id = "process-lifecycle:state:" .. spec.id}
    local prefix = "process-lifecycle:state:" .. spec.id
    group:rectangle {
        center = spec.center, size = spec.size, fill = "#00000000",
        stroke = spec.color or "foreground", width = spec.focal and 1.5 or 1.25,
        layer = LAYER.state, id = prefix .. ":body",
    }
    group:line {
        from = {spec.center[1] - spec.size[1] * 0.5, spec.center[2] + 4},
        to = {spec.center[1] + spec.size[1] * 0.5, spec.center[2] + 4},
        color = "border", width = 1, opacity = 0.55,
        layer = LAYER.state, id = prefix .. ":divider",
    }
    group:text {
        text = spec.label, point = {spec.center[1], spec.center[2] + 20},
        role = "text", fill = spec.color or "foreground",
        layer = LAYER.text, id = prefix .. ":label",
    }
    group:text {
        text = spec.detail, point = {spec.center[1], spec.center[2] - 17},
        role = "code", fill = spec.color or "muted",
        layer = LAYER.text, id = prefix .. ":detail",
    }
    stateHandles[spec.id] = group
end

local transitionHandles = {}
local function transition(spec)
    local prefix = "process-lifecycle:transition:" .. spec.id
    local geometry
    if #spec.points == 2 then
        geometry = routes:arrow {
            from = spec.points[1], to = spec.points[2], tip = 7,
            color = spec.color or "foreground", width = 1,
            dash = spec.dash, layer = LAYER.route, id = prefix .. ":route",
        }
    else
        geometry = routes:route {
            points = spec.points, tip = 7, color = spec.color or "foreground",
            width = 1, dash = spec.dash, layer = LAYER.route,
            id = prefix .. ":route",
        }
    end
    local label = routeLabels:text {
        text = spec.label, point = spec.label_point, align = spec.align,
        role = "code", fill = spec.color or "muted",
        layer = LAYER.text, id = prefix .. ":label",
    }
    transitionHandles[#transitionHandles + 1] = {geometry = geometry, label = label}
end

-- The horizontal spine is admission/completion; the two return corridors form the operational loop.
transition {
    id = "new-ready", label = "admit",
    points = {{-310, 60}, {-235, 60}}, label_point = {-272, 88},
}
transition {
    id = "ready-running", label = "dispatch",
    points = {{-85, 60}, {24, 60}}, label_point = {-30, 88}, color = "accent",
}
transition {
    id = "running-terminated", label = "exit",
    points = {{184, 60}, {270, 60}}, label_point = {227, 88},
}
transition {
    id = "running-ready", label = "timer expired / preempt",
    points = {{104, 100}, {104, 120}, {-160, 120}, {-160, 100}},
    label_point = {-28, 138},
}
transition {
    id = "running-waiting", label = "I/O request / sleep",
    points = {{104, 20}, {104, -110}}, label_point = {122, -45},
    align = {0, 0.5},
}
transition {
    id = "waiting-ready", label = "I/O complete / wake",
    points = {{-8, -150}, {-160, -150}, {-160, 20}},
    label_point = {-24, -172}, align = {1, 0.5},
}

scene:fade_in(header, {duration = 0.4, curve = "gentle"})
scene:fade_in(states, {shift = {0, -6}, duration = 0.65, curve = "gentle"})
for _, item in ipairs(transitionHandles) do
    scene:create(item.geometry, 0.42, "ease_out")
    scene:fade_in(item.label, {shift = {0, -4}, duration = 0.18, curve = "gentle"})
end

-- One truthful trace: queued, scheduled, blocked, then woken back into the run queue.
scene:indicate(stateHandles.ready, {scale = 1.035, duration = 0.38, curve = "ease_in_out"})
scene:indicate(stateHandles.running, {scale = 1.035, duration = 0.38, curve = "ease_in_out"})
scene:indicate(stateHandles.waiting, {scale = 1.035, duration = 0.38, curve = "ease_in_out"})
scene:indicate(stateHandles.ready, {scale = 1.035, duration = 0.38, curve = "ease_in_out"})
scene:wait(2.5)
return scene
