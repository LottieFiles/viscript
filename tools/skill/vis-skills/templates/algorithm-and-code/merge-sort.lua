-- Question: how does a stable merge choose one run head and emit one value at a time?
-- The tagged duplicate keys 2L and 2R make the left-first equality rule visible.
-- This is the merge kernel, not the recursive split phase.
-- Edit LEFT and RIGHT; the stable left-first trace and output are regenerated below.
local scene = vis.scene {
    width = 960, height = 540, fps = 30, loop = false,
    theme = "adaptive_vscode",
    camera = {mode = "fixed", view = "2d", target = {0, 0}, height = 540},
}

local LEFT = {
    {id = "2L", key = 2, label = "2L"},
    {id = "4L", key = 4, label = "4"},
    {id = "7L", key = 7, label = "7"},
}
local RIGHT = {
    {id = "2R", key = 2, label = "2R"},
    {id = "5R", key = 5, label = "5"},
    {id = "8R", key = 8, label = "8"},
}

local function buildStableMergeTrace(left, right)
    for index = 2, #left do assert(left[index - 1].key <= left[index].key) end
    for index = 2, #right do assert(right[index - 1].key <= right[index].key) end

    local trace, merged, leftHead, rightHead = {}, {}, 1, 1
    while leftHead <= #left or rightHead <= #right do
        local event = {leftIndex = leftHead, rightIndex = rightHead, outputIndex = #merged + 1}
        if leftHead <= #left and rightHead <= #right then
            event.kind = "compare"
            event.left, event.right = left[leftHead], right[rightHead]
            event.side = event.left.key <= event.right.key and "left" or "right"
        elseif leftHead <= #left then
            event.kind, event.side = "drain", "left"
        else
            event.kind, event.side = "drain", "right"
        end

        if event.side == "left" then
            event.chosen = left[leftHead]
            leftHead = leftHead + 1
        else
            event.chosen = right[rightHead]
            rightHead = rightHead + 1
        end
        merged[#merged + 1] = event.chosen
        trace[#trace + 1] = event
    end
    for index = 2, #merged do assert(merged[index - 1].key <= merged[index].key) end
    return trace, merged
end

local TRACE, MERGED = buildStableMergeTrace(LEFT, RIGHT)
local INPUT_X = {-315, -225, -135}
local OUTPUT_X = {-225, -135, -45, 45, 135, 225}
local INPUT_Y, OUTPUT_Y = 70, -90
local LAYER = {slot = 10, token = 20, text = 40}

local function translate(x, y)
    return {1, 0, 0, x, 0, 1, 0, y, 0, 0, 1, 0, 0, 0, 0, 1}
end

local title = scene:text {
    text = "Merge Sort · stable merge kernel", point = {0, 220}, role = "h2",
    fill = "foreground", layer = LAYER.text, id = "scene-title",
}
local leftLabel = scene:text {
    text = "LEFT RUN", point = {-225, 145}, role = "code",
    fill = "accent", layer = LAYER.text, id = "run:left:label",
}
local rightLabel = scene:text {
    text = "RIGHT RUN", point = {225, 145}, role = "code",
    fill = "secondary", layer = LAYER.text, id = "run:right:label",
}
local outputLabel = scene:text {
    text = "OUTPUT BUFFER", point = {0, -155}, role = "code",
    fill = "result", layer = LAYER.text, id = "buffer:label",
}

local slots = scene:group {id = "buffer:slots"}
local outputSlots = {}
for index, x in ipairs(OUTPUT_X) do
    outputSlots[index] = slots:rectangle {
        center = {x, OUTPUT_Y}, size = {76, 70}, corner = 5,
        fill = "surface", stroke = "border", width = 2,
        layer = LAYER.slot, id = "buffer-slot:" .. index,
    }
end

local tokenLayer = scene:group {id = "merge:values"}
local leftTokens, leftBodies, rightTokens, rightBodies = {}, {}, {}, {}
local function makeToken(spec, x, color)
    local token = tokenLayer:group {matrix = translate(x, INPUT_Y), id = "value:" .. spec.id}
    local body = token:rectangle {
        center = {0, 0}, size = {68, 62}, corner = 5,
        fill = "surface", stroke = color, width = 3,
        layer = LAYER.token, id = "value:" .. spec.id .. ":body",
    }
    token:text {
        text = spec.label, point = {0, 0}, role = "h3", fill = color,
        layer = LAYER.text, id = "value:" .. spec.id .. ":label",
    }
    return token, body
end
for index, spec in ipairs(LEFT) do
    leftTokens[index], leftBodies[index] = makeToken(spec, INPUT_X[index], "accent")
end
for index, spec in ipairs(RIGHT) do
    rightTokens[index], rightBodies[index] = makeToken(spec, -INPUT_X[4 - index], "secondary")
end

local status, statusCounter = nil, 0
local function setStatus(message, color)
    statusCounter = statusCounter + 1
    local nextStatus = scene:text {
        text = message, point = {0, -205}, role = "code", fill = color or "focus",
        layer = LAYER.text, id = "status:" .. statusCounter,
    }
    if status then scene:fade_transform(status, nextStatus, 0.16, "gentle")
    else scene:fade_in(nextStatus, {duration = 0.24, curve = "gentle"}) end
    status = nextStatus
end

local leftHead, rightHead, outputHead = 1, 1, 1
local function compareAndEmit(event, message)
    setStatus(message)
    scene:play({
        {target = leftBodies[leftHead], fill = "focus"},
        {target = rightBodies[rightHead], fill = "focus"},
    }, 0.28, "ease_in_out", 0)
    local token, body, sourceX
    if event.side == "left" then
        token, body, sourceX = leftTokens[leftHead], leftBodies[leftHead], INPUT_X[leftHead]
    else
        token, body = rightTokens[rightHead], rightBodies[rightHead]
        sourceX = -INPUT_X[4 - rightHead]
    end
    scene:play({
        {target = token, shift = {OUTPUT_X[outputHead] - sourceX, OUTPUT_Y - INPUT_Y}},
        {target = body, fill = "surface"},
        {target = event.side == "left" and rightBodies[rightHead] or leftBodies[leftHead], fill = "surface"},
        {target = outputSlots[outputHead], stroke = "result"},
    }, 0.55, "ease_out", 0)
    if event.side == "left" then leftHead = leftHead + 1 else rightHead = rightHead + 1 end
    outputHead = outputHead + 1
end

local function drain(event, message)
    setStatus(message, "muted")
    local token, sourceX
    if event.side == "left" then
        token, sourceX = leftTokens[leftHead], INPUT_X[leftHead]
    else
        token, sourceX = rightTokens[rightHead], -INPUT_X[4 - rightHead]
    end
    scene:play({
        {target = token, shift = {OUTPUT_X[outputHead] - sourceX, OUTPUT_Y - INPUT_Y}},
        {target = outputSlots[outputHead], stroke = "result"},
    }, 0.50, "ease_out", 0)
    if event.side == "left" then leftHead = leftHead + 1 else rightHead = rightHead + 1 end
    outputHead = outputHead + 1
end

scene:fade_in(title, {shift = {0, -6}, duration = 0.42, curve = "gentle"})
scene:fade_in(leftLabel, {duration = 0.28, curve = "gentle"})
scene:fade_in(rightLabel, {duration = 0.28, curve = "gentle"})
scene:fade_in(outputLabel, {duration = 0.28, curve = "gentle"})
scene:fade_in(slots, {duration = 0.38, curve = "gentle"})
scene:fade_in(tokenLayer, {shift = {0, 7}, duration = 0.50, curve = "ease_out"})

for _, event in ipairs(TRACE) do
    if event.kind == "compare" then
        local relation = event.left.key == event.right.key and "=="
            or (event.left.key < event.right.key and "<" or ">")
        local message
        if relation == "==" and event.side == "left" then
            message = string.format(
                "%s == %s  →  take LEFT first (stable)", event.left.label, event.right.label
            )
        else
            message = string.format(
                "%s %s %s  →  emit %s",
                event.left.label, relation, event.right.label, event.chosen.label
            )
        end
        compareAndEmit(event, message)
    else
        local exhausted = event.side == "left" and "right" or "left"
        drain(event, string.format(
            "%s run exhausted  →  drain %s", exhausted, event.chosen.label
        ))
    end
end

local mergedLabels = {}
for index, item in ipairs(MERGED) do mergedLabels[index] = item.label end
local result = scene:text {
    text = "merged: [" .. table.concat(mergedLabels, ", ")
        .. "]  ·  equal keys preserve source order",
    point = {0, -238}, role = "text", fill = "result",
    layer = LAYER.text, id = "result:stable-merge",
}
scene:fade_in(result, {shift = {0, 4}, duration = 0.34, curve = "gentle"})
scene:wait(2.2)
return scene
