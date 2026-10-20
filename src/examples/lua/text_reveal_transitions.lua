local p = {
    background = "#090d13",
    panel = "#0f1722",
    panel_alt = "#111a26",
    line = "#263547",
    text = "#f4f7fb",
    muted = "#8fa1b7",
    cyan = "#4cc9f0",
    gold = "#ffd166",
    green = "#7bd88f",
}

local scene = vis.scene {fps = 60, background = p.background, camera = { height = 7 }}

local function arrow_points(from, to, shaft, head, head_length)
    local dx, dy = to[1] - from[1], to[2] - from[2]
    local length = math.sqrt(dx * dx + dy * dy)
    local ux, uy = dx / length, dy / length
    local nx, ny = -uy, ux
    local joint = { to[1] - ux * head_length, to[2] - uy * head_length }
    return {
        { from[1] + nx * shaft, from[2] + ny * shaft },
        { joint[1] + nx * shaft, joint[2] + ny * shaft },
        { joint[1] + nx * head, joint[2] + ny * head },
        { to[1], to[2] },
        { joint[1] - nx * head, joint[2] - ny * head },
        { joint[1] - nx * shaft, joint[2] - ny * shaft },
        { from[1] - nx * shaft, from[2] - ny * shaft },
        { from[1] - nx * shaft, from[2] + ny * shaft },
    }
end

local function ellipse_points(cx, cy, rx, ry)
    local points = {}
    for index = 0, 7 do
        local angle = 2 * math.pi * index / 8
        points[#points + 1] = { cx + rx * math.cos(angle), cy + ry * math.sin(angle) }
    end
    return points
end

local function box_points(cx, cy, width, height)
    local x0, x1 = cx - width * 0.5, cx + width * 0.5
    local y0, y1 = cy - height * 0.5, cy + height * 0.5
    return {
        { x0, y0 },
        { cx, y0 },
        { x1, y0 },
        { x1, cy },
        { x1, y1 },
        { cx, y1 },
        { x0, y1 },
        { x0, cy },
    }
end

local title = scene:text {
    text = "OBJECT / SHAPE / TEXT",
    point = { -5.55, 3.05 },
    align = { 0, 0.5 },
    font = "Pretendard",
    size = 29,
    fill = p.text,
    id = "title",
}
local subtitle = scene:text {
    text = "Keep the original. Consume only its solid copy.",
    point = { -5.53, 2.56 },
    align = { 0, 0.5 },
    font = "Pretendard",
    size = 14,
    fill = p.muted,
    id = "subtitle",
}
local divider = scene:line {
    from = { -5.55, 2.25 },
    to = { 5.55, 2.25 },
    stroke = p.line,
    width = 2,
    id = "divider",
}

local defaultPanel = scene:rectangle {
    center = { 0, 1.36 },
    size = { 11.1, 1.25 },
    corner = 0.16,
    fill = p.panel,
    stroke = p.line,
    width = 2,
    layer = -5,
    id = "morph-fade-panel",
}
local bulletPanel = scene:rectangle {
    center = { 0, -0.05 },
    size = { 11.1, 1.25 },
    corner = 0.16,
    fill = p.panel_alt,
    stroke = p.line,
    width = 2,
    layer = -5,
    id = "bullet-panel",
}
local cursorPanel = scene:rectangle {
    center = { 0, -1.46 },
    size = { 11.1, 1.25 },
    corner = 0.16,
    fill = p.panel,
    stroke = p.line,
    width = 2,
    layer = -5,
    id = "cursor-panel",
}
local defaultCaption = scene:text {
    text = "01  MORPH FADE  ·  DEFAULT",
    point = { -5.18, 1.75 },
    align = { 0, 0.5 },
    font = "Pretendard",
    size = 12,
    fill = p.muted,
    id = "morph-fade-caption",
}
local bulletCaption = scene:text {
    text = "02  BULLET REVEAL",
    point = { -5.18, 0.34 },
    align = { 0, 0.5 },
    font = "Pretendard",
    size = 12,
    fill = p.muted,
    id = "bullet-caption",
}
local cursorCaption = scene:text {
    text = "03  CURSOR WRITE",
    point = { -5.18, -1.07 },
    align = { 0, 0.5 },
    font = "Pretendard",
    size = 12,
    fill = p.muted,
    id = "cursor-caption",
}

local handoffPoint = scene:point {
    point = { -4.45, 1.25 },
    fill = p.green,
    stroke = p.green,
    width = 1,
    radius = 9,
    id = "morph-fade-source",
}
local handoffLabel = scene:text {
    text = "SOURCE",
    point = { -4.45, 0.94 },
    font = "Pretendard",
    size = 11,
    fill = p.green,
    id = "morph-fade-source-label",
}

local vectorFrom, vectorTo = { -4.85, -0.25 }, { -3.38, 0.08 }
local vector = scene:vector {
    origin = vectorFrom,
    value = { vectorTo[1] - vectorFrom[1], vectorTo[2] - vectorFrom[2] },
    color = p.cyan,
    width = 7,
    tip = 18,
    id = "original-vector",
}
local vectorLabel = scene:text {
    text = "ORIGINAL",
    point = { -4.12, -0.51 },
    font = "Pretendard",
    size = 11,
    fill = p.cyan,
    id = "original-vector-label",
}

local point = scene:point {
    point = { -4.45, -1.50 },
    fill = p.gold,
    radius = 9,
    id = "original-point",
}
local pointLabel = scene:text {
    text = "ORIGINAL",
    point = { -4.45, -1.92 },
    font = "Pretendard",
    size = 11,
    fill = p.gold,
    id = "original-point-label",
}

scene:fade_in(title, { shift = { 0, 0.14 }, duration = 0.24, curve = "snappy" })
scene:fade_in(subtitle, { shift = { 0, 0.10 }, duration = 0.18, curve = "gentle" })
scene:fade_in(defaultPanel, { shift = { -0.18, 0 }, duration = 0.25, curve = "snappy" })
scene:fade_in(bulletPanel, { shift = { 0.18, 0 }, duration = 0.25, curve = "snappy" })
scene:fade_in(cursorPanel, { shift = { -0.18, 0 }, duration = 0.25, curve = "snappy" })
scene:fade_in(defaultCaption, { duration = 0.12, curve = "gentle" })
scene:fade_in(bulletCaption, { duration = 0.12, curve = "gentle" })
scene:fade_in(cursorCaption, { duration = 0.12, curve = "gentle" })
scene:create(divider, 0.24, "linear")
scene:grow_from_center(handoffPoint, 0.24, { preset = "back", strength = 0.45 })
scene:create(vector, 0.36, "linear")
scene:grow_from_center(point, 0.28, { preset = "back", strength = 0.55 })
scene:fade_in(handoffLabel, { duration = 0.12, curve = "gentle" })
scene:fade_in(vectorLabel, { duration = 0.12, curve = "gentle" })
scene:fade_in(pointLabel, { duration = 0.12, curve = "gentle" })
scene:wait(0.20)

-- Default copy-from treatment: arrive as a same-color seed, then cross-fade into Text.
local handoffCopy = scene:polygon {
    points = ellipse_points(-4.45, 1.25, 0.16, 0.16),
    fill = p.green,
    stroke = p.green,
    width = 1,
    layer = 7,
    id = "morph-fade-solid-copy",
}
local handoffSeed = scene:polygon {
    points = ellipse_points(-1.38, 1.25, 0.13, 0.18),
    fill = p.green,
    stroke = p.green,
    width = 1,
    layer = 7,
    id = "morph-fade-seed",
}
scene:morph(handoffCopy, handoffSeed, 0.58, "ease_in_out")
local handoffText = scene:text {
    text = "normal vector",
    point = { -1.50, 1.25 },
    align = { 0, 0.5 },
    font = "Pretendard",
    size = 21,
    fill = p.green,
    layer = 8,
    id = "morph-fade-text",
}
scene:fade_transform(handoffSeed, handoffText, 0.22, "gentle")
scene:wait(0.18)

-- A solid duplicate is consumed by the morph; the authored vector remains visible.
local vectorCopy = scene:polygon {
    points = arrow_points(vectorFrom, vectorTo, 0.055, 0.18, 0.34),
    fill = p.cyan,
    stroke = p.cyan,
    width = 1,
    layer = 4,
    id = "vector-solid-copy",
}
local bullet = scene:polygon {
    points = ellipse_points(-2.18, -0.16, 0.12, 0.12),
    fill = p.cyan,
    stroke = p.cyan,
    width = 1,
    layer = 4,
    id = "vector-bullet-morph-proxy",
}
scene:morph(vectorCopy, bullet, 0.62, { preset = "back", strength = 0.45 })
local bulletCircle = scene:circle {
    center = { -2.18, -0.16 },
    radius = 0.12,
    fill = p.cyan,
    stroke = p.cyan,
    width = 1,
    layer = 4,
    id = "vector-bullet",
}
scene:remove(bullet)

local bulletWords = {
    scene:text {
        text = "vector",
        point = { -1.78, -0.16 },
        align = { 0, 0.5 },
        font = "Pretendard",
        size = 19,
        fill = p.cyan,
        id = "bullet-word-vector",
    },
    scene:text {
        text = "=",
        point = { -0.62, -0.16 },
        font = "Pretendard",
        size = 18,
        fill = p.muted,
        id = "bullet-word-equals",
    },
    scene:text {
        text = "magnitude",
        point = { -0.28, -0.16 },
        align = { 0, 0.5 },
        font = "Pretendard",
        size = 19,
        fill = p.text,
        id = "bullet-word-magnitude",
    },
    scene:text {
        text = "+",
        point = { 1.77, -0.16 },
        font = "Pretendard",
        size = 18,
        fill = p.muted,
        id = "bullet-word-plus",
    },
    scene:text {
        text = "direction",
        point = { 2.15, -0.16 },
        align = { 0, 0.5 },
        font = "Pretendard",
        size = 19,
        fill = p.text,
        id = "bullet-word-direction",
    },
}
scene:create(bulletWords, 0.16, "ease_out", 0.38)
scene:wait(0.24)

-- A second solid copy narrows into a cursor, then moves with each revealed character.
local pointCopy = scene:polygon {
    points = ellipse_points(-4.45, -1.50, 0.16, 0.16),
    fill = p.gold,
    stroke = p.gold,
    width = 1,
    layer = 5,
    id = "point-solid-copy",
}
local cursor = scene:polygon {
    points = box_points(-1.40, -1.50, 0.075, 0.54),
    fill = p.gold,
    stroke = p.gold,
    width = 1,
    layer = 8,
    id = "text-cursor",
}
scene:morph(pointCopy, cursor, 0.58, { preset = "snappy", strength = 0.86 })

local prefixData = {
    { "dot(", 0.947 },
    { "dot(u", 1.159 },
    { "dot(u, ", 1.329 },
    { "dot(u, v", 1.515 },
    { "dot(u, v)", 1.649 },
    { "dot(u, v) = ", 2.050 },
    { "dot(u, v) = u", 2.261 },
    { "dot(u, v) = u · ", 2.552 },
    { "dot(u, v) = u · v", 2.738 },
}
local typedFormula = scene:group { id = "typed-formula" }
local prefixes = {}
for index, prefix in ipairs(prefixData) do
    prefixes[index] = typedFormula:text {
        text = prefix[1],
        point = { -1.20, -1.50 },
        align = { 0, 0.5 },
        font = "Pretendard",
        size = 21,
        fill = p.text,
        opacity = 0,
        id = "typed-prefix-" .. index,
    }
end
local cursorPosition = 0
for index, prefix in ipairs(prefixData) do
    local delta = prefix[2] - cursorPosition
    if index > 1 then
        scene:tween({
            { target = prefixes[index - 1], opacity = 0 },
            { target = cursor, shift = { delta / 3, 0 } },
        }, 0.025, "linear", 0)
        cursorPosition = cursorPosition + delta / 3
    end
    scene:tween({
        { target = prefixes[index], opacity = 1 },
        { target = cursor, shift = { prefix[2] - cursorPosition, 0 } },
    }, index > 1 and 0.05 or 0.075, "linear", 0)
    cursorPosition = prefix[2]
end
scene:fade(cursor, 0, 0.16, "ease_out")
scene:indicate(typedFormula, { color = p.gold, scale = 1.035, duration = 0.42, curve = "gentle" })
scene:wait(0.65)

return scene
