local p = {
    background = "#08111c",
    panel = "#0d1927",
    text = "#f4f7fb",
    muted = "#8fa1b7",
    rule = "#293b50",
    cyan = "#4cc9f0",
    magenta = "#f72585",
    gold = "#ffd166",
    green = "#7bd88f",
    retired = "#60748a",
}

local function regular_polygon(center, radius, sides, rotation)
    local points = {}
    for index = 0, sides - 1 do
        local angle = rotation + index * 2 * math.pi / sides
        points[#points + 1] = {
            center[1] + radius * math.cos(angle),
            center[2] + radius * math.sin(angle),
        }
    end
    return points
end

local function stage(index, title, count, accent, summary)
    local scene = vis.scene {fps = 60, background = p.background, camera = {height = 8}}
    scene:text {
        text = "SEMANTIC SCENE TRANSITION",
        point = { -6.45, 3.25 },
        align = { 0, 0.5 },
        font = "Pretendard",
        size = 13,
        fill = p.muted,
        id = "chrome-kicker",
    }
    scene:text {
        text = index,
        point = { -6.45, 2.75 },
        align = { 0, 0.5 },
        font = "Pretendard",
        size = 13,
        fill = accent,
        id = "stage-index",
    }
    scene:text {
        text = title,
        point = { -5.45, 2.75 },
        align = { 0, 0.5 },
        font = "Pretendard",
        size = 28,
        fill = p.text,
        id = "stage-title",
    }
    scene:rectangle {
        center = { 5.38, 2.86 },
        size = { 2.15, 0.62 },
        corner = 0.14,
        fill = accent .. "20",
        stroke = accent,
        width = 2,
        id = "count-chip",
    }
    scene:text {
        text = count,
        point = { 5.38, 2.86 },
        font = "Pretendard",
        size = 13,
        fill = accent,
        id = "count-label",
    }
    scene:line {
        from = { -6.45, 2.25 },
        to = { 6.45, 2.25 },
        stroke = p.rule,
        width = 2,
        id = "chrome-rule",
    }
    scene:text {
        text = summary,
        point = { 0, -3.13 },
        font = "Pretendard",
        size = 14,
        fill = p.muted,
        id = "stage-summary",
    }
    return scene
end

local function node(scene, spec)
    local shape = scene:polygon {
        points = regular_polygon(spec.center, spec.radius, spec.sides, spec.rotation or 0),
        fill = spec.color .. (spec.fill_alpha or "24"),
        stroke = spec.color,
        width = spec.width or 4,
        id = spec.id,
    }
    scene:text {
        text = spec.label,
        point = spec.center,
        font = "Pretendard",
        size = spec.label_size or 19,
        fill = p.text,
        id = spec.id .. "-label",
    }
    return shape
end

local function link(scene, from, to, color, id, bend)
    local center_x = (from[1] + to[1]) * 0.5
    local center_y = (from[2] + to[2]) * 0.5 + bend
    return scene:curve {
        from = from,
        control1 = { center_x - 0.45, center_y },
        control2 = { center_x + 0.45, center_y },
        to = to,
        samples = 36,
        stroke = color,
        width = 3,
        layer = -1,
        id = id,
    }
end

-- Scene 1: seven candidates. Only A, B, and C have semantic continuity.
local many = stage(
    "01 / 03",
    "DISCOVER / MANY",
    "7 OBJECTS",
    p.cyan,
    "retain A / B / C by id   ·   D / E / F / G leave the next scene"
)
local many_a = { -3.55, 0.45 }
local many_b = { -0.20, 1.02 }
local many_c = { 3.35, 0.30 }
link(many, many_a, many_b, p.cyan, "route-ab", 0.48)
link(many, many_b, many_c, p.magenta, "route-bc", -0.38)
node(many, {
    id = "node-a",
    label = "A",
    center = many_a,
    radius = 0.82,
    sides = 8,
    rotation = math.pi / 8,
    color = p.cyan,
})
node(many, {
    id = "node-b",
    label = "B",
    center = many_b,
    radius = 0.88,
    sides = 4,
    rotation = math.pi / 4,
    color = p.magenta,
})
node(
    many,
    { id = "node-c", label = "C", center = many_c, radius = 0.80, sides = 6, color = p.gold }
)
node(many, {
    id = "candidate-d",
    label = "D",
    center = { -4.70, -1.52 },
    radius = 0.43,
    sides = 5,
    rotation = -0.2,
    color = p.retired,
    width = 2.5,
    label_size = 13,
})
node(many, {
    id = "candidate-e",
    label = "E",
    center = { -2.05, -1.35 },
    radius = 0.43,
    sides = 6,
    color = p.retired,
    width = 2.5,
    label_size = 13,
})
node(many, {
    id = "candidate-f",
    label = "F",
    center = { 1.15, -1.48 },
    radius = 0.43,
    sides = 4,
    rotation = math.pi / 4,
    color = p.retired,
    width = 2.5,
    label_size = 13,
})
node(many, {
    id = "candidate-g",
    label = "G",
    center = { 4.65, -1.42 },
    radius = 0.43,
    sides = 7,
    color = p.retired,
    width = 2.5,
    label_size = 13,
})

-- Scene 2: only the reusable semantic core remains and is rearranged.
local few = stage(
    "02 / 03",
    "FOCUS / FEW",
    "3 REUSED",
    p.magenta,
    "same ids, new geometry   ·   matched contours move and morph instead of redrawing"
)
local few_a = { -2.85, -0.15 }
local few_b = { 0, 0.72 }
local few_c = { 2.85, -0.15 }
link(few, few_a, few_b, p.cyan, "route-ab", 0.62)
link(few, few_b, few_c, p.magenta, "route-bc", -0.52)
node(few, {
    id = "node-a",
    label = "A",
    center = few_a,
    radius = 1.02,
    sides = 6,
    rotation = math.pi / 6,
    color = p.cyan,
})
node(few, {
    id = "node-b",
    label = "B",
    center = few_b,
    radius = 1.05,
    sides = 12,
    rotation = math.pi / 12,
    color = p.magenta,
})
node(few, {
    id = "node-c",
    label = "C",
    center = few_c,
    radius = 1.02,
    sides = 5,
    rotation = -math.pi / 2,
    color = p.gold,
})

-- Scene 3: the core is reused once more; six unmatched objects are introduced.
local expanded = stage(
    "03 / 03",
    "EXPAND / MORE",
    "9 OBJECTS",
    p.green,
    "morph A / B / C   ·   create six objects that have no predecessor"
)
local expanded_a = { -3.55, 0.75 }
local expanded_b = { 0, 1.18 }
local expanded_c = { 3.55, 0.75 }
link(expanded, expanded_a, expanded_b, p.cyan, "route-ab", 0.35)
link(expanded, expanded_b, expanded_c, p.magenta, "route-bc", -0.30)
node(expanded, {
    id = "node-a",
    label = "A",
    center = expanded_a,
    radius = 0.78,
    sides = 5,
    rotation = -math.pi / 2,
    color = p.cyan,
})
node(expanded, {
    id = "node-b",
    label = "B",
    center = expanded_b,
    radius = 0.82,
    sides = 8,
    rotation = math.pi / 8,
    color = p.magenta,
})
node(expanded, {
    id = "node-c",
    label = "C",
    center = expanded_c,
    radius = 0.78,
    sides = 4,
    rotation = math.pi / 4,
    color = p.gold,
})

local additions = {
    {
        id = "new-d",
        label = "D",
        center = { -4.85, -1.28 },
        color = p.green,
        sides = 6,
        parent = expanded_a,
    },
    {
        id = "new-e",
        label = "E",
        center = { -2.65, -1.65 },
        color = p.green,
        sides = 4,
        parent = expanded_a,
    },
    {
        id = "new-f",
        label = "F",
        center = { -0.95, -1.25 },
        color = p.cyan,
        sides = 5,
        parent = expanded_b,
    },
    {
        id = "new-g",
        label = "G",
        center = { 0.95, -1.25 },
        color = p.magenta,
        sides = 7,
        parent = expanded_b,
    },
    {
        id = "new-h",
        label = "H",
        center = { 2.65, -1.65 },
        color = p.gold,
        sides = 4,
        parent = expanded_c,
    },
    {
        id = "new-i",
        label = "I",
        center = { 4.85, -1.28 },
        color = p.gold,
        sides = 6,
        parent = expanded_c,
    },
}
for index, item in ipairs(additions) do
    link(expanded, item.parent, item.center, item.color, "new-route-" .. index, -0.08)
    node(expanded, {
        id = item.id,
        label = item.label,
        center = item.center,
        radius = 0.46,
        sides = item.sides,
        rotation = math.pi / item.sides,
        color = item.color,
        width = 2.5,
        label_size = 13,
    })
end

local scene = vis.scene {fps = 60, background = p.background, camera = {height = 8}}
scene:rectangle {
    center = { 0, 0 },
    size = { 13.75, 7.55 },
    corner = 0.10,
    stroke = p.rule,
    width = 1,
    id = "outer-frame",
}
scene:scene_transition({ many, few, expanded }, {
    live = false,
    duration = 1.4,
    hold = 0.9,
    curve = { preset = "smooth", strength = 1.0 },
})
return scene
