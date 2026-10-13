-- Question: after detach(), which runtime object receives update(), and what stored relation proves it?
-- Source-derived example: Modern-CPP-Design-Patterns/24_Observer/01_Classic_GoF/Observer.cpp.
-- Edit surface: SPEC, runtime object labels, registry snapshots, and message labels.
-- Preserve: separate structure/scenario views, type-vs-object identity, stable topology, and a settled witness.
local SPEC = {
    question = "After detach(), which runtime object receives update()?",
    source = "Observer.cpp · main → NumberObservable::setVal(20)",
    evidence = "static path derived · output witness observed",
}

local scene = vis.scene {
    width = 1280, height = 720, fps = 30, loop = false,
    theme = "adaptive_vscode",
    camera = {mode = "fixed", view = "2d", target = {0, 0}, height = 720},
}

local LAYER = {boundary = 0, route = 10, body = 20, text = 40, focus = 50}
local root = scene:group {id = "structure-scenario:root"}
local structure = root:group {id = "structure-scenario:structure"}
local scenario = root:group {id = "structure-scenario:scenario"}

local title = root:text {
    text = SPEC.question, point = {0, 318}, role = "h2", fill = "foreground",
    layer = LAYER.text, id = "structure-scenario:title",
}
local subtitle = root:text {
    text = SPEC.source .. " · " .. SPEC.evidence, point = {0, 280},
    role = "code", fill = "muted", layer = LAYER.text,
    id = "structure-scenario:subtitle",
}
local divider = root:line {
    from = {-600, 252}, to = {600, 252}, stroke = "border", width = 1.5,
    layer = LAYER.boundary, id = "structure-scenario:header-divider",
}

local function panel(parent, id, label, center, size)
    local group = parent:group {id = id}
    local body = group:rectangle {
        center = center, size = size, corner = 10, fill = "surface", stroke = "border",
        width = 1.5, opacity = 0.55, layer = LAYER.boundary, id = id .. ":body",
    }
    local heading = group:text {
        text = label, point = {center[1] - size[1] * 0.5 + 20, center[2] + size[2] * 0.5 - 28},
        align = {0, 0.5}, role = "code", fill = "muted", layer = LAYER.text,
        id = id .. ":heading",
    }
    return group, body, heading
end

local structurePanel, structurePanelBody = panel(
    structure, "structure-scenario:structure:panel", "SETTLED RUNTIME STRUCTURE", {-320, -18}, {560, 500}
)
local scenarioPanel, scenarioPanelBody = panel(
    scenario, "structure-scenario:scenario:panel", "REPRESENTATIVE SCENARIO", {304, -18}, {568, 500}
)

local function object_node(parent, id, titleText, detailText, center, size, stroke)
    local group = parent:group {id = id}
    local body = group:rectangle {
        center = center, size = size, corner = 8, fill = "surface", stroke = stroke or "border",
        width = stroke and 2.25 or 1.5, layer = LAYER.body, id = id .. ":body",
    }
    local titleTextHandle = group:text {
        text = titleText, point = {center[1], center[2] + 13}, role = "code",
        fill = stroke or "foreground", layer = LAYER.text, id = id .. ":title",
    }
    local detailTextHandle = group:text {
        text = detailText, point = {center[1], center[2] - 14}, role = "code",
        fill = "muted", layer = LAYER.text, id = id .. ":detail",
    }
    return {group = group, body = body, title = titleTextHandle, detail = detailTextHandle}
end

local numberObject = object_node(
    structurePanel, "obj:numberObservable", "numberObservable", "NumberObservable", {-480, 122}, {210, 78}, "accent"
)
local divObject = object_node(
    structurePanel, "obj:divObserver", "divObserver", "DivObserver{4}", {-410, -18}, {190, 72}
)
local modObject = object_node(
    structurePanel, "obj:modObserver", "modObserver", "ModObserver{3}", {-142, -18}, {190, 72}, "result"
)

local registry = structurePanel:group {id = "field:observers"}
local registryBody = registry:rectangle {
    center = {-195, 122}, size = {282, 104}, corner = 8, fill = "surface", stroke = "border",
    width = 1.5, layer = LAYER.body, id = "field:observers:body",
}
local registryTitle = registry:text {
    text = "observers_ · raw pointer slots", point = {-195, 151}, role = "code",
    fill = "muted", layer = LAYER.text, id = "field:observers:title",
}
local slotCenters = {-239, -151}
local slotBodies = {}
for index, x in ipairs(slotCenters) do
    slotBodies[index] = registry:rectangle {
        center = {x, 111}, size = {76, 40}, corner = 4, fill = "surface", stroke = "border",
        width = 1.25, layer = LAYER.body, id = "field:observers:slot:" .. index .. ":body",
    }
end
local slotInitial = registry:group {id = "field:observers:snapshot:initial"}
slotInitial:text {
    text = "&div", point = {slotCenters[1], 111}, role = "code", fill = "foreground",
    layer = LAYER.text, id = "field:observers:slot:1:initial",
}
slotInitial:text {
    text = "&mod", point = {slotCenters[2], 111}, role = "code", fill = "foreground",
    layer = LAYER.text, id = "field:observers:slot:2:initial",
}

local stateBody = structurePanel:rectangle {
    center = {-480, -120}, size = {210, 62}, corner = 7, fill = "surface", stroke = "border",
    width = 1.5, layer = LAYER.body, id = "field:val:body",
}
local stateValue = structurePanel:text {
    text = "val_ = 14", point = {-480, -120}, role = "code", fill = "foreground",
    layer = LAYER.text, id = "field:val:value:14",
}
local sourceAnchor = structurePanel:text {
    text = "detach · setVal · notify", point = {-210, -120}, role = "code", fill = "muted",
    layer = LAYER.text, id = "structure-scenario:structure:anchor",
}

local objectToRegistry = structurePanel:arrow {
    from = {-375, 122}, to = {-336, 122}, tip = 10, stroke = "accent", width = 2,
    layer = LAYER.route, id = "relation:numberObservable:stores:observers",
}
local divRelation = structurePanel:route {
    points = {{slotCenters[1], 90}, {slotCenters[1], 58}, {-410, 58}, {-410, 18}}, tip = 9,
    stroke = "foreground", width = 2, layer = LAYER.route,
    id = "relation:slot-1:borrows:divObserver",
}
local modRelationInitial = structurePanel:route {
    points = {{slotCenters[2], 90}, {slotCenters[2], 58}, {-142, 58}, {-142, 18}}, tip = 9,
    stroke = "foreground", width = 2, layer = LAYER.route,
    id = "relation:slot-2:borrows:modObserver",
}
local relationLabel = structurePanel:text {
    text = "stored raw pointers", point = {-276, 32},
    role = "code", fill = "muted", layer = LAYER.text,
    id = "relation:observers:label",
}
local ownershipNote = structurePanel:text {
    text = "ownership not claimed", point = {-210, -151}, role = "code", fill = "muted",
    layer = LAYER.text, id = "relation:observers:ownership-note",
}

local actors = {
    {id = "main", label = "main", x = 105, width = 130},
    {id = "number", label = "numberObservable", x = 300, width = 200},
    {id = "mod", label = "modObserver", x = 505, width = 160},
}
local actorHandles = {}
for _, actor in ipairs(actors) do
    local group = scenarioPanel:group {id = "scenario:actor:" .. actor.id}
    local body = group:rectangle {
        center = {actor.x, 158}, size = {actor.width, 50}, corner = 7, fill = "surface",
        stroke = actor.id == "mod" and "result" or "border", width = 1.5,
        layer = LAYER.body, id = "scenario:actor:" .. actor.id .. ":body",
    }
    local label = group:text {
        text = actor.label, point = {actor.x, 158}, role = "code",
        fill = actor.id == "mod" and "result" or "foreground",
        layer = LAYER.text, id = "scenario:actor:" .. actor.id .. ":label",
    }
    local lifeline = scenarioPanel:line {
        from = {actor.x, 132}, to = {actor.x, -176}, stroke = "border", width = 1.4,
        dash = {7, 7}, layer = LAYER.boundary, id = "scenario:lifeline:" .. actor.id,
    }
    actorHandles[actor.id] = {group = group, body = body, label = label, lifeline = lifeline}
end

local function message(id, fromX, toX, y, label, color, dashed)
    local route = scenarioPanel:arrow {
        from = {fromX, y}, to = {toX, y}, tip = 10, stroke = color, width = 2.25,
        dash = dashed and {8, 6} or nil, layer = LAYER.route, id = "scenario:message:" .. id,
    }
    local textHandle = scenarioPanel:text {
        text = label, point = {(fromX + toX) * 0.5, y + 17}, role = "code",
        fill = color, layer = LAYER.text, id = "scenario:message:" .. id .. ":label",
    }
    return {route = route, label = textHandle}
end

local detachMessage = message("detach", 105, 300, 82, "detach(&divObserver)", "foreground", false)
local setValueMessage = message("set-value", 105, 300, 4, "setVal(20)", "accent", false)
local notifyMessage = message("notify", 300, 505, -104, "update(this)", "result", false)

local scenarioStateBody = scenarioPanel:rectangle {
    center = {300, -48}, size = {126, 34}, corner = 5, fill = "surface", stroke = "accent",
    width = 1.5, layer = LAYER.body, id = "scenario:state-write:body",
}
local scenarioStateLabel = scenarioPanel:text {
    text = "val_ ← 20", point = {300, -48}, role = "code", fill = "accent",
    layer = LAYER.text, id = "scenario:state-write:label",
}
local resultBody = scenarioPanel:rectangle {
    center = {304, -210}, size = {500, 48}, corner = 7, fill = "surface", stroke = "result",
    width = 2, layer = LAYER.body, id = "scenario:result:body",
}
local resultLabel = scenarioPanel:text {
    text = "observed: ModObserver receives the only callback", point = {304, -210},
    role = "code", fill = "result", layer = LAYER.text, id = "scenario:result:label",
}

scene:fade_in(title, {shift = {0, -6}, duration = 0.42, curve = "gentle"})
scene:fade_in(subtitle, {shift = {0, -4}, duration = 0.32, curve = "gentle"})
scene:create(divider, 0.28, "ease_out")
scene:fade_in(structurePanel, {shift = {0, 5}, duration = 0.50, curve = "gentle"})
scene:fade_in(scenarioPanel, {shift = {0, 5}, duration = 0.50, curve = "gentle"})
scene:create({objectToRegistry, divRelation, modRelationInitial}, 0.62, "ease_out", 0.08, "forward")
scene:wait(0.55)

scene:create(detachMessage.route, 0.46, "ease_out")
scene:fade_in(detachMessage.label, {shift = {0, -3}, duration = 0.24, curve = "gentle"})
local modRelationFinal = structurePanel:route {
    points = {{slotCenters[1], 90}, {slotCenters[1], 58}, {-142, 58}, {-142, 18}}, tip = 9,
    stroke = "result", width = 2.25, layer = LAYER.route,
    id = "relation:slot-1:borrows:modObserver:after-detach",
}
local detachedBadgeBody = structurePanel:rectangle {
    center = {-410, -70}, size = {112, 28}, corner = 4, fill = "surface", stroke = "muted",
    width = 1.25, layer = LAYER.body, id = "obj:divObserver:detached:body",
}
local detachedBadgeLabel = structurePanel:text {
    text = "DETACHED", point = {-410, -70}, role = "code", fill = "muted",
    layer = LAYER.text, id = "obj:divObserver:detached:label",
}
local slotFinal = registry:group {id = "field:observers:snapshot:after-detach"}
slotFinal:text {
    text = "&mod", point = {slotCenters[1], 111}, role = "code", fill = "result",
    layer = LAYER.text, id = "field:observers:slot:1:final",
}
slotFinal:text {
    text = "empty", point = {slotCenters[2], 111}, role = "code", fill = "muted",
    layer = LAYER.text, id = "field:observers:slot:2:final",
}
scene:fade_transform(slotInitial, slotFinal, 0.38, "ease_in_out")
scene:play({
    {target = divRelation, opacity = 0},
    {target = modRelationInitial, opacity = 0},
}, 0.36, "ease_in_out", 0)
scene:create(modRelationFinal, 0.40, "ease_out")
scene:play({
    {target = divObject.body, opacity = 0.34},
    {target = divObject.title, opacity = 0.34},
    {target = divObject.detail, opacity = 0.34},
}, 0.28, "ease_in_out", 0)
scene:fade_in(detachedBadgeBody, {duration = 0.22, curve = "gentle"})
scene:fade_in(detachedBadgeLabel, {duration = 0.18, curve = "gentle"})
scene:wait(0.55)

scene:create(setValueMessage.route, 0.42, "ease_out")
scene:fade_in(setValueMessage.label, {shift = {0, -3}, duration = 0.22, curve = "gentle"})
scene:fade_in(scenarioStateBody, {scale = 0.96, duration = 0.24, curve = "gentle"})
scene:fade_in(scenarioStateLabel, {duration = 0.18, curve = "gentle"})
local stateValueFinal = structurePanel:text {
    text = "val_ = 20", point = {-480, -120}, role = "code", fill = "result",
    layer = LAYER.text, id = "field:val:value:20",
}
scene:fade_transform(stateValue, stateValueFinal, 0.30, "gentle")
stateValue = stateValueFinal
scene:play({{target = stateBody, stroke = "result"}}, 0.24, "ease_in_out", 0)
scene:wait(0.50)

scene:create(notifyMessage.route, 0.48, "ease_out")
scene:fade_in(notifyMessage.label, {shift = {0, -3}, duration = 0.22, curve = "gentle"})
scene:indicate(modObject.body, {color = "result", scale = 1.035, duration = 0.38, curve = "ease_in_out"})
scene:indicate(actorHandles.mod.body, {color = "result", scale = 1.035, duration = 0.34, curve = "ease_in_out"})
scene:fade_in(resultBody, {scale = 0.98, duration = 0.32, curve = "gentle"})
scene:fade_in(resultLabel, {duration = 0.24, curve = "gentle"})
scene:wait(2.8)
return scene
