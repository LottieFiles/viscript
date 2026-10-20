local p = {
    background = "#080d14",
    a = "#0d1622",
    b = "#101a27",
    rule = "#253448",
    text = "#f4f7fb",
    muted = "#8fa1b7",
    cyan = "#4cc9f0",
    gold = "#ffd166",
    green = "#7bd88f",
    magenta = "#f72585",
    orange = "#ff9f5a",
}

local function translate(x, y)
    return { 1, 0, 0, x, 0, 1, 0, y, 0, 0, 1, 0, 0, 0, 0, 1 }
end

local function vector_to_point(origin, target, clearance)
    local dx, dy = target[1] - origin[1], target[2] - origin[2]
    local length = math.sqrt(dx * dx + dy * dy)
    local scale = (length - clearance) / length
    return { dx * scale, dy * scale }
end

local function panel(index, title, background)
    local child = vis.scene {
        width = 320,
        height = 270,
        fps = 60,
        background = background,
        camera = { height = 6.4 },
    }
    child:text {
        text = index,
        point = { -3.42, 2.70 },
        align = { 0, 0.5 },
        font = "Pretendard",
        size = 10,
        fill = p.muted,
        id = "panel-index-" .. index,
    }
    child:text {
        text = title,
        point = { -2.78, 2.70 },
        align = { 0, 0.5 },
        font = "Pretendard",
        size = 17,
        fill = p.text,
        id = "panel-title-" .. index,
    }
    child:line {
        from = { -3.42, 2.25 },
        to = { 3.42, 2.25 },
        stroke = p.rule,
        width = 1.5,
        id = "panel-rule-" .. index,
    }
    child:wait(0.28)
    return child
end

local scenePanel = panel("01", "SCENE / VIEWPORT", p.a)
local viewportCards = {}
for index, spec in ipairs {
    { -2.18, 0.15, p.cyan, "A" },
    { 0, 0.15, p.gold, "B" },
    { 2.18, 0.15, p.green, "C" },
} do
    local card = scenePanel:group { id = "viewport-card-" .. index }
    card:rectangle {
        center = { spec[1], spec[2] },
        size = { 1.72, 1.62 },
        corner = 0.12,
        fill = spec[3] .. "18",
        stroke = spec[3],
        width = 3,
        id = "viewport-body-" .. index,
    }
    card:text {
        text = spec[4],
        point = { spec[1], spec[2] + 0.15 },
        font = "Pretendard",
        size = 23,
        fill = spec[3],
        id = "viewport-label-" .. index,
    }
    card:text {
        text = "timeline",
        point = { spec[1], spec[2] - 0.43 },
        font = "Pretendard",
        size = 10,
        fill = p.muted,
        id = "viewport-timeline-" .. index,
    }
    viewportCards[#viewportCards + 1] = card
end
for _, card in ipairs(viewportCards) do
    scenePanel:fade_in(
        card,
        { shift = { 0, 0.20 }, scale = 0.94, duration = 0.22, curve = "snappy" }
    )
end
scenePanel:wait(0.42)

local shapePanel = panel("02", "SHAPES / PATHS", p.b)
local shapeCircle = shapePanel:circle {
    center = { -2.30, 0.82 },
    radius = 0.52,
    fill = p.cyan,
    stroke = p.cyan,
    width = 2,
    id = "shape-circle",
}
local shapeRectangle = shapePanel:rectangle {
    center = { 0, 0.82 },
    size = { 1.18, 0.94 },
    corner = 0.14,
    fill = p.gold,
    stroke = p.gold,
    width = 2,
    id = "shape-rectangle",
}
local shapePolygon = shapePanel:polygon {
    points = { { 1.76, 0.35 }, { 2.30, 1.34 }, { 2.84, 0.35 } },
    fill = p.green,
    stroke = p.green,
    width = 2,
    id = "shape-polygon",
}
local shapeCurve = shapePanel:curve {
    from = { -3.05, -1.22 },
    control1 = { -2.42, 0.12 },
    control2 = { -1.28, -2.32 },
    to = { -0.45, -0.92 },
    stroke = p.magenta,
    width = 4,
    id = "shape-curve",
}
local shapePath = shapePanel:path {
    commands = {
        { type = "move", to = { 0.48, -1.72 } },
        { type = "line", to = { 0.92, -0.58 } },
        { type = "quadratic", control = { 1.72, 0.06 }, to = { 2.34, -0.68 } },
        {
            type = "cubic",
            control1 = { 3.05, -1.24 },
            control2 = { 2.62, -2.04 },
            to = {
                1.78,
                -1.70,
            },
        },
        { type = "close" },
    },
    fill = p.orange .. "30",
    stroke = p.orange,
    width = 3,
    id = "shape-path",
}
shapePanel:text {
    text = "CIRCLE",
    point = { -2.30, 0.08 },
    font = "Pretendard",
    size = 9,
    fill = p.muted,
    id = "circle-label",
}
shapePanel:text {
    text = "RECTANGLE",
    point = { 0, 0.08 },
    font = "Pretendard",
    size = 9,
    fill = p.muted,
    id = "rectangle-label",
}
shapePanel:text {
    text = "POLYGON",
    point = { 2.30, 0.08 },
    font = "Pretendard",
    size = 9,
    fill = p.muted,
    id = "polygon-label",
}
shapePanel:text {
    text = "CURVE",
    point = { -1.74, -2.12 },
    font = "Pretendard",
    size = 9,
    fill = p.magenta,
    id = "curve-label",
}
shapePanel:text {
    text = "PATH",
    point = { 1.78, -2.12 },
    font = "Pretendard",
    size = 9,
    fill = p.orange,
    id = "path-label",
}
shapePanel:grow_from_center(shapeCircle, 0.34, { preset = "back", strength = 0.50 })
shapePanel:grow_from_center(shapeRectangle, 0.34, { preset = "back", strength = 0.50 })
shapePanel:grow_from_center(shapePolygon, 0.34, { preset = "back", strength = 0.50 })
shapePanel:create(shapeCurve, 0.42, "ease_out")
shapePanel:draw_border_then_fill(shapePath, 0.72, "ease_in_out")
shapePanel:wait(0.42)

local vectorPanel = panel("03", "LINE / VECTOR / POINT", p.a)
local line = vectorPanel:line {
    from = { -2.85, 0.72 },
    to = { -0.95, 0.72 },
    stroke = p.muted,
    width = 4,
    id = "line",
}
local arrow = vectorPanel:arrow {
    from = { -2.85, -0.18 },
    to = { -0.95, -0.18 },
    color = p.gold,
    width = 4,
    tip = 14,
    id = "arrow",
}
local vectorOrigin, pointPosition = { 0.35, -0.55 }, { 2.50, 0.80 }
local vector = vectorPanel:vector {
    origin = vectorOrigin,
    value = vector_to_point(vectorOrigin, pointPosition, 0.28),
    color = p.cyan,
    width = 5,
    tip = 16,
    id = "vector",
}
local point = vectorPanel:point {
    point = pointPosition,
    fill = p.magenta,
    radius = 7,
    id = "point",
}
vectorPanel:text {
    text = "LINE",
    point = { -1.90, 1.16 },
    font = "Pretendard",
    size = 10,
    fill = p.muted,
    id = "line-label",
}
vectorPanel:text {
    text = "ARROW",
    point = { -1.90, -0.66 },
    font = "Pretendard",
    size = 10,
    fill = p.gold,
    id = "arrow-label",
}
vectorPanel:text {
    text = "VECTOR + POINT",
    point = { 1.45, -1.12 },
    font = "Pretendard",
    size = 10,
    fill = p.cyan,
    id = "vector-label",
}
vectorPanel:create(line, 0.28, "linear")
vectorPanel:create(arrow, 0.34, "linear")
vectorPanel:create(vector, 0.42, "linear")
vectorPanel:grow_from_center(point, 0.24, { preset = "back", strength = 0.55 })
vectorPanel:wait(0.42)

local textPanel = panel("04", "TEXT", p.b)
local textLeft = textPanel:text {
    text = "LEFT",
    point = { -2.65, 0.84 },
    align = { 0, 0.5 },
    font = "Pretendard",
    size = 14,
    fill = p.cyan,
    id = "text-left",
}
local textCenter = textPanel:text {
    text = "Type",
    point = { 0, 0.10 },
    font = "Pretendard",
    size = 32,
    fill = p.text,
    id = "text-center",
}
local textRight = textPanel:text {
    text = "RIGHT",
    point = { 2.65, -0.72 },
    align = { 1, 0.5 },
    font = "Pretendard",
    size = 14,
    fill = p.gold,
    id = "text-right",
}
textPanel:fade_in(textLeft, { shift = { -0.22, 0 }, duration = 0.24, curve = "snappy" })
textPanel:fade_in(textCenter, { scale = 0.90, duration = 0.30, curve = "gentle" })
textPanel:fade_in(textRight, { shift = { 0.22, 0 }, duration = 0.24, curve = "snappy" })
textPanel:wait(0.42)

local spacePanel = panel("05", "SPACE / GRID", p.a)
local space = spacePanel:space {
    x = { -3, 3, 1 },
    y = { -1.4, 1.4, 0.7 },
    color = p.rule,
    axis_x = p.orange,
    axis_y = p.green,
    numbers = false,
    id = "space",
}
local spaceOrigin, spacePointPosition = { 0, 0 }, { 1.8, 0.9 }
local spaceVector = space:vector {
    origin = spaceOrigin,
    value = vector_to_point(spaceOrigin, spacePointPosition, 0.26),
    color = p.cyan,
    width = 5,
    tip = 15,
    id = "space-vector",
}
local spacePoint =
    space:point { point = spacePointPosition, fill = p.gold, radius = 6, id = "space-point" }
spacePanel:create(space, 0.46, "linear")
spacePanel:create(spaceVector, 0.38, "linear")
spacePanel:grow_from_center(spacePoint, 0.24, { preset = "back", strength = 0.55 })
spacePanel:wait(0.42)

local groupPanel = panel("06", "GROUP / CONNECTOR", p.b)
local nodes = groupPanel:group { id = "node-group" }
local nodeItems = {}
for index, spec in ipairs {
    { -1.85, 0.62, p.cyan, "A" },
    { 0, -0.35, p.gold, "B" },
    {
        1.85,
        0.62,
        p.green,
        "C",
    },
} do
    local node = nodes:group { matrix = translate(spec[1], spec[2]), id = "node-" .. index }
    node:circle {
        radius = 0.48,
        fill = spec[3],
        stroke = spec[3],
        width = 2,
        id = "node-body-" .. index,
    }
    node:text {
        text = spec[4],
        point = { 0, 0 },
        font = "Pretendard",
        size = 18,
        fill = p.background,
        id = "node-label-" .. index,
    }
    nodeItems[#nodeItems + 1] = node
end
local leftEdge = groupPanel:connector {
    from = nodeItems[1],
    to = nodeItems[2],
    padding = 0.08,
    stroke = p.rule,
    width = 3,
    id = "edge-a-b",
}
local rightEdge = groupPanel:connector {
    from = nodeItems[2],
    to = nodeItems[3],
    padding = 0.08,
    stroke = p.rule,
    width = 3,
    id = "edge-b-c",
}
for _, node in ipairs(nodeItems) do
    groupPanel:grow_from_center(node, 0.24, { preset = "back", strength = 0.50 })
end
groupPanel:create({ leftEdge, rightEdge }, 0.34, "linear", 0.08)
groupPanel:shift(nodes, { 0, 0.18 }, 0.40, "ease_in_out")
groupPanel:wait(0.42)

local gallery = vis.scene {
    width = 960,
    height = 540,
    fps = 60,
    background = p.background,
    camera = { height = 6.4 },
}
local panels = { scenePanel, shapePanel, vectorPanel, textPanel, spacePanel, groupPanel }
for index, child in ipairs(panels) do
    gallery:viewport(child, {
        x = ((index - 1) % 3) / 3,
        y = math.floor((index - 1) / 3) / 2,
        width = 1 / 3,
        height = 1 / 2,
    })
end

return gallery
