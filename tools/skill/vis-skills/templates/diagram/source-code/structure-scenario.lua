-- Question: after detach(), which runtime object receives update(), and what stored relation proves it?
-- Source-derived example: the classic GoF Observer, a NumberObservable with a DivObserver and a ModObserver.
-- Edit surface: SPEC, runtime object labels, registry snapshots, and message labels.
-- Preserve: separate structure/scenario views, type-vs-object identity, stable topology, and a settled witness.
-- Look: a hairline separates the two views and structure stays ink. `result` arrives with the evidence:
-- the surviving slot and route at detach, then the callback, its receiver, and the caption at notify.
-- The val_ write is only a transient `focus` pulse.
local SPEC = {
    question = "After detach(), which runtime object receives update()?",
    scenario = "main → detach(&divObserver) → setVal(20)",
}

local scene = vis.scene {
    width = 1280, height = 720, fps = 30, loop = false,
    theme = "adaptive",
    camera = {mode = "fixed", view = "2d", target = {0, 0}, height = 720},
}

local LAYER = {boundary = 0, route = 10, body = 20, text = 40, focus = 50}
local root = scene:group {id = "structure-scenario:root"}
local structure = root:group {id = "structure-scenario:structure"}
local scenario = root:group {id = "structure-scenario:scenario"}

local title = root:text {
    text = SPEC.question, point = {-600, 296}, align = {0, 0.5}, role = "h3", fill = "foreground",
    layer = LAYER.text, id = "structure-scenario:title",
}
local subtitle = root:text {
    text = SPEC.scenario, point = {-600, 262}, align = {0, 0.5},
    role = "code", fill = "muted", layer = LAYER.text,
    id = "structure-scenario:subtitle",
}
local divider = root:line {
    from = {-600, 236}, to = {600, 236}, stroke = "border", width = 1,
    layer = LAYER.boundary, id = "structure-scenario:header-divider",
}
local viewDivider = root:line {
    from = {-8, 214}, to = {-8, -296}, stroke = "border", width = 1,
    layer = LAYER.boundary, id = "structure-scenario:view-divider",
}

-- A view is a Group with a heading; the hairline above, not a card, separates the views.
local function panel(parent, id, label, left, top)
    local group = parent:group {id = id}
    local heading = group:text {
        text = label, point = {left, top}, align = {0, 0.5}, role = "code", fill = "muted",
        layer = LAYER.text, id = id .. ":heading",
    }
    return group, heading
end

local structurePanel = panel(
    structure, "structure-scenario:structure:panel", "SETTLED RUNTIME STRUCTURE", -600, 204
)
local scenarioPanel = panel(
    scenario, "structure-scenario:scenario:panel", "REPRESENTATIVE SCENARIO", 22, 204
)

local function object_node(parent, id, titleText, detailText, center, size)
    local group = parent:group {id = id}
    local body = group:rectangle {
        center = center, size = size, fill = "#00000000", stroke = "foreground",
        width = 1.25, layer = LAYER.body, id = id .. ":body",
    }
    local titleTextHandle = group:text {
        text = titleText, point = {center[1], center[2] + 13}, role = "code",
        fill = "foreground", layer = LAYER.text, id = id .. ":title",
    }
    local detailTextHandle = group:text {
        text = detailText, point = {center[1], center[2] - 14}, role = "code",
        fill = "muted", layer = LAYER.text, id = id .. ":detail",
    }
    return {group = group, body = body, title = titleTextHandle, detail = detailTextHandle}
end

local numberObject = object_node(
    structurePanel, "obj:numberObservable", "numberObservable", "NumberObservable", {-502, 132}, {196, 72}
)
local divObject = object_node(
    structurePanel, "obj:divObserver", "divObserver", "DivObserver{4}", {-470, -44}, {180, 72}
)
local modObject = object_node(
    structurePanel, "obj:modObserver", "modObserver", "ModObserver{3}", {-170, -44}, {180, 72}
)

-- The field box holds the slots; its name sits above it as a label.
local registry = structurePanel:group {id = "field:observers"}
local registryBody = registry:rectangle {
    center = {-272, 122}, size = {200, 64}, fill = "#00000000", stroke = "foreground",
    width = 1.25, layer = LAYER.body, id = "field:observers:body",
}
local registryTitle = registry:text {
    text = "observers_ · raw pointer slots", point = {-372, 172}, align = {0, 0.5}, role = "code",
    fill = "muted", layer = LAYER.text, id = "field:observers:title",
}
local slotCenters = {-322, -222}
local slotBodies = {}
for index, x in ipairs(slotCenters) do
    slotBodies[index] = registry:rectangle {
        center = {x, 122}, size = {76, 40}, fill = "#00000000", stroke = "border",
        width = 1, layer = LAYER.body, id = "field:observers:slot:" .. index .. ":body",
    }
end
local slotInitial = registry:group {id = "field:observers:snapshot:initial"}
slotInitial:text {
    text = "&div", point = {slotCenters[1], 122}, role = "code", fill = "foreground",
    layer = LAYER.text, id = "field:observers:slot:1:initial",
}
slotInitial:text {
    text = "&mod", point = {slotCenters[2], 122}, role = "code", fill = "foreground",
    layer = LAYER.text, id = "field:observers:slot:2:initial",
}

local stateBody = structurePanel:rectangle {
    center = {-502, -204}, size = {196, 56}, fill = "#00000000", stroke = "foreground",
    width = 1.25, layer = LAYER.body, id = "field:val:body",
}
local stateValue = structurePanel:text {
    text = "val_ = 14", point = {-502, -204}, role = "code", fill = "foreground",
    layer = LAYER.text, id = "field:val:value:14",
}
local sourceAnchor = structurePanel:text {
    text = "detach · setVal · notify", point = {-226, -188}, role = "code", fill = "muted",
    layer = LAYER.text, id = "structure-scenario:structure:anchor",
}

local objectToRegistry = structurePanel:arrow {
    from = {-402, 132}, to = {-374, 132}, tip = 7, stroke = "foreground", width = 1,
    layer = LAYER.route, id = "relation:numberObservable:stores:observers",
}
local divRelation = structurePanel:route {
    points = {{slotCenters[1], 102}, {slotCenters[1], 56}, {-470, 56}, {-470, -8}}, tip = 7,
    stroke = "foreground", width = 1, layer = LAYER.route,
    id = "relation:slot-1:borrows:divObserver",
}
local modRelationInitial = structurePanel:route {
    points = {{slotCenters[2], 102}, {slotCenters[2], 56}, {-170, 56}, {-170, -8}}, tip = 7,
    stroke = "foreground", width = 1, layer = LAYER.route,
    id = "relation:slot-2:borrows:modObserver",
}
local relationLabel = structurePanel:text {
    text = "stored raw pointers", point = {-320, 26},
    role = "code", fill = "muted", layer = LAYER.text,
    id = "relation:observers:label",
}
local ownershipNote = structurePanel:text {
    text = "ownership not claimed", point = {-226, -220}, role = "code", fill = "muted",
    layer = LAYER.text, id = "relation:observers:ownership-note",
}

-- Lifelines are spaced so every message label fits between the two lifelines it joins.
local actors = {
    {id = "main", label = "main", x = 72, width = 100},
    {id = "number", label = "numberObservable", x = 322, width = 200},
    {id = "mod", label = "modObserver", x = 512, width = 146},
}
local actorHandles = {}
for _, actor in ipairs(actors) do
    local group = scenarioPanel:group {id = "scenario:actor:" .. actor.id}
    local body = group:rectangle {
        center = {actor.x, 154}, size = {actor.width, 44}, fill = "#00000000", stroke = "foreground",
        width = 1.25, layer = LAYER.body, id = "scenario:actor:" .. actor.id .. ":body",
    }
    local label = group:text {
        text = actor.label, point = {actor.x, 154}, role = "code", fill = "foreground",
        layer = LAYER.text, id = "scenario:actor:" .. actor.id .. ":label",
    }
    local lifeline = scenarioPanel:line {
        from = {actor.x, 132}, to = {actor.x, -232}, stroke = "border", width = 1,
        layer = LAYER.boundary, id = "scenario:lifeline:" .. actor.id,
    }
    actorHandles[actor.id] = {group = group, body = body, label = label, lifeline = lifeline}
end

local function message(id, fromX, toX, y, label, color, dashed)
    local route = scenarioPanel:arrow {
        from = {fromX, y}, to = {toX, y}, tip = 7, stroke = color,
        width = color == "result" and 1.5 or 1,
        dash = dashed and {8, 6} or nil, layer = LAYER.route, id = "scenario:message:" .. id,
    }
    local textHandle = scenarioPanel:text {
        text = label, point = {(fromX + toX) * 0.5, y + 17}, role = "code",
        fill = color, layer = LAYER.text, id = "scenario:message:" .. id .. ":label",
    }
    return {route = route, label = textHandle}
end

local detachMessage = message("detach", 72, 322, 80, "detach(&divObserver)", "foreground", false)
local setValueMessage = message("set-value", 72, 322, -10, "setVal(20)", "foreground", false)
local notifyMessage = message("notify", 322, 512, -170, "update(this)", "result", false)

-- The write box masks the lifeline it sits on.
local scenarioStateBody = scenarioPanel:rectangle {
    center = {322, -92}, size = {126, 34}, fill = "background", stroke = "foreground",
    width = 1, layer = LAYER.body, id = "scenario:state-write:body",
}
local scenarioStateLabel = scenarioPanel:text {
    text = "val_ ← 20", point = {322, -92}, role = "code", fill = "foreground",
    layer = LAYER.text, id = "scenario:state-write:label",
}
local resultLabel = scenarioPanel:text {
    text = "observed: ModObserver receives the only callback", point = {22, -272}, align = {0, 0.5},
    role = "text", fill = "result", layer = LAYER.text, id = "scenario:result:label",
}

scene:fade_in(title, {shift = {0, -6}, duration = 0.42, curve = "gentle"})
scene:fade_in(subtitle, {shift = {0, -4}, duration = 0.32, curve = "gentle"})
scene:create({divider, viewDivider}, 0.28, "ease_out")
scene:fade_in(structurePanel, {shift = {0, 5}, duration = 0.50, curve = "gentle"})
scene:fade_in(scenarioPanel, {shift = {0, 5}, duration = 0.50, curve = "gentle"})
scene:create({objectToRegistry, divRelation, modRelationInitial}, 0.62, "ease_out", 0.08, "forward")
scene:wait(0.55)

scene:create(detachMessage.route, 0.46, "ease_out")
scene:fade_in(detachMessage.label, {shift = {0, -3}, duration = 0.24, curve = "gentle"})
local modRelationFinal = structurePanel:route {
    points = {{slotCenters[1], 102}, {slotCenters[1], 56}, {-170, 56}, {-170, -8}}, tip = 7,
    stroke = "result", width = 1.5, layer = LAYER.route,
    id = "relation:slot-1:borrows:modObserver:after-detach",
}
local detachedBadgeLabel = structurePanel:text {
    text = "DETACHED", point = {-470, -102}, role = "code", fill = "muted",
    layer = LAYER.text, id = "obj:divObserver:detached:label",
}
local slotFinal = registry:group {id = "field:observers:snapshot:after-detach"}
slotFinal:text {
    text = "&mod", point = {slotCenters[1], 122}, role = "code", fill = "result",
    layer = LAYER.text, id = "field:observers:slot:1:final",
}
slotFinal:text {
    text = "empty", point = {slotCenters[2], 122}, role = "code", fill = "muted",
    layer = LAYER.text, id = "field:observers:slot:2:final",
}
scene:fade_transform(slotInitial, slotFinal, 0.38, "ease_in_out")
scene:play({
    {target = divRelation, opacity = 0},
    {target = modRelationInitial, opacity = 0},
}, 0.36, "ease_in_out", 0)
scene:create(modRelationFinal, 0.40, "ease_out")
scene:play({
    {target = divObject.body, opacity = 0.4},
    {target = divObject.title, opacity = 0.4},
    {target = divObject.detail, opacity = 0.4},
}, 0.28, "ease_in_out", 0)
scene:fade_in(detachedBadgeLabel, {duration = 0.18, curve = "gentle"})
scene:wait(0.55)

scene:create(setValueMessage.route, 0.42, "ease_out")
scene:fade_in(setValueMessage.label, {shift = {0, -3}, duration = 0.22, curve = "gentle"})
scene:fade_in(scenarioStateBody, {scale = 0.96, duration = 0.24, curve = "gentle"})
scene:fade_in(scenarioStateLabel, {duration = 0.18, curve = "gentle"})
local stateValueFinal = structurePanel:text {
    text = "val_ = 20", point = {-502, -204}, role = "code", fill = "foreground",
    layer = LAYER.text, id = "field:val:value:20",
}
scene:fade_transform(stateValue, stateValueFinal, 0.30, "gentle")
stateValue = stateValueFinal
scene:indicate(stateBody, {color = "focus", scale = 1.03, duration = 0.30, curve = "ease_in_out"})
scene:wait(0.50)

scene:create(notifyMessage.route, 0.48, "ease_out")
scene:fade_in(notifyMessage.label, {shift = {0, -3}, duration = 0.22, curve = "gentle"})
-- The receiver takes the result hue only when the callback reaches it.
scene:play({
    {target = modObject.body, stroke = "result", width = 1.5},
    {target = modObject.title, fill = "result"},
    {target = actorHandles.mod.body, stroke = "result", width = 1.5},
    {target = actorHandles.mod.label, fill = "result"},
}, 0.30, "ease_in_out", 0)
scene:indicate(modObject.body, {color = "result", scale = 1.035, duration = 0.38, curve = "ease_in_out"})
scene:indicate(actorHandles.mod.body, {color = "result", scale = 1.035, duration = 0.34, curve = "ease_in_out"})
scene:fade_in(resultLabel, {shift = {0, -4}, duration = 0.32, curve = "gentle"})
scene:wait(2.8)
return scene
