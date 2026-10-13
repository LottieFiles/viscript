local p = {
    background = "#0b111a",
    panel_a = "#101925",
    panel_b = "#0f1722",
    rule = "#263447",
    text = "#f4f7fb",
    muted = "#94a3b8",
    cyan = "#4cc9f0",
    magenta = "#f72585",
    gold = "#ffd166",
    green = "#7bd88f",
    red = "#ff6b6b",
}

local function make_panel(background, prefix, index, title, caption)
    local panel = vis.scene {
        width = 320,
        height = 270,
        fps = 30,
        background = background,
        camera = { mode = "fixed", view = "2d", target = { 0, 0 }, height = 6.4 },
    }
    panel:text {
        text = index,
        point = { -3.45, 2.72 },
        align = { 0, 0.5 },
        font = "Pretendard",
        size = 11,
        fill = p.muted,
        id = prefix .. "-step",
    }
    panel:text {
        text = title,
        point = { -2.75, 2.72 },
        align = { 0, 0.5 },
        font = "Pretendard",
        size = 18,
        fill = p.text,
        id = prefix .. "-title",
    }
    panel:line {
        from = { -3.45, 2.28 },
        to = { 3.45, 2.28 },
        stroke = p.rule,
        width = 1.5,
        id = prefix .. "-rule",
    }
    panel:text {
        text = caption,
        point = { 0, -2.68 },
        align = { 0.5, 0.5 },
        font = "Pretendard",
        size = 11,
        fill = p.muted,
        id = prefix .. "-caption",
    }
    return panel
end

local function direction_panel(background, index, title, direction, color)
    local panel = vis.scene {
        width = 160,
        height = 270,
        fps = 30,
        background = background,
        camera = { mode = "fixed", view = "2d", target = { 0, 0 }, height = 6.4 },
    }
    panel:text {
        text = index,
        point = { -1.68, 2.72 },
        align = { 0, 0.5 },
        font = "Pretendard",
        size = 10,
        fill = p.muted,
        id = "direction-" .. direction .. "-step",
    }
    panel:text {
        text = title,
        point = { -0.90, 2.72 },
        align = { 0, 0.5 },
        font = "Pretendard",
        size = 12,
        fill = p.text,
        id = "direction-" .. direction .. "-title",
    }
    panel:line {
        from = { -1.68, 2.28 },
        to = { 1.68, 2.28 },
        stroke = p.rule,
        width = 1.5,
        id = "direction-" .. direction .. "-rule",
    }
    panel:circle {
        center = { 0, -0.12 },
        radius = 1.05,
        fill = "#00000000",
        stroke = p.rule,
        width = 2,
        id = "direction-" .. direction .. "-guide",
    }
    local path = panel:circle {
        center = { 0, -0.12 },
        radius = 1.05,
        fill = "#00000000",
        stroke = color,
        width = 5,
        id = "direction-" .. direction .. "-path",
    }
    panel:point {
        point = { 1.05, -0.12 },
        fill = p.gold,
        radius = 5,
        layer = 6,
        id = "direction-" .. direction .. "-start",
    }
    local tangentFrom = direction == "counterclockwise" and { 1.38, -0.46 } or { 1.38, 0.22 }
    local tangentTo = direction == "counterclockwise" and { 1.38, 0.22 } or { 1.38, -0.46 }
    panel:arrow {
        from = tangentFrom,
        to = tangentTo,
        color = color,
        width = 3,
        tip = 9,
        id = "direction-" .. direction .. "-tangent",
    }
    panel:text {
        text = direction == "counterclockwise" and "CCW" or "CW",
        point = { 0, -1.62 },
        font = "Pretendard",
        size = 17,
        fill = color,
        id = "direction-" .. direction .. "-label",
    }
    panel:text {
        text = "CREATE / UNCREATE",
        point = { 0, -2.68 },
        align = { 0.5, 0.5 },
        font = "Pretendard",
        size = 10,
        fill = p.muted,
        id = "direction-" .. direction .. "-caption",
    }

    panel:wait(0.35)
    panel:create(path, 1.20, "linear", 0, direction)
    panel:wait(0.35)
    panel:uncreate(path, 1.00, "linear", 0, direction)
    panel:wait(0.55)
    return panel
end

local direction = vis.scene {
    width = 320,
    height = 270,
    fps = 30,
    background = p.panel_a,
    camera = { mode = "fixed", view = "2d", target = { 0, 0 }, height = 6.4 },
}
local ccw = direction_panel(p.panel_a, "01A", "CCW PATH", "counterclockwise", p.cyan)
local cw = direction_panel(p.panel_b, "01B", "CW PATH", "clockwise", p.magenta)
direction:viewport(ccw, { x = 0, y = 0, width = 0.5, height = 1 })
direction:viewport(cw, { x = 0.5, y = 0, width = 0.5, height = 1 })

local fade = make_panel(p.panel_b, "fade", "02", "FADE IN / OUT", "opacity + shift + scale")
local fadeCard = fade:group { id = "fade-card" }
fadeCard:rectangle {
    center = { 0, -0.12 },
    size = { 3.45, 1.48 },
    corner = 0.18,
    fill = "#4cc9f02e",
    stroke = p.cyan,
    width = 3,
    id = "fade-card-body",
}
fadeCard:text {
    text = "OPACITY",
    point = { 0, 0.06 },
    font = "Pretendard",
    size = 22,
    fill = p.text,
    id = "fade-card-label",
}
fadeCard:text {
    text = "0  ->  1  ->  0",
    point = { 0, -0.48 },
    font = "Pretendard",
    size = 14,
    fill = p.cyan,
    id = "fade-opacity-caption",
}
fade:wait(0.35)
fade:fade_in(fadeCard, {
    shift = { 0, -0.42 },
    scale = 0.86,
    duration = 1.20,
    easing = "ease_out",
})
fade:wait(0.35)
fade:fade_out(fadeCard, {
    shift = { 0, 0.42 },
    scale = 0.86,
    duration = 1.00,
    easing = "ease_in",
})
fade:wait(0.55)

local grow = make_panel(p.panel_a, "grow", "03", "GROW / SHRINK", "fixed center anchor")
local growSubject = grow:group { id = "grow-subject" }
growSubject:circle {
    center = { 0, -0.12 },
    radius = 1.02,
    fill = "#f7258533",
    stroke = p.magenta,
    width = 4,
    id = "grow-subject-body",
}
growSubject:text {
    text = "CENTER",
    point = { 0, -0.58 },
    font = "Pretendard",
    size = 13,
    fill = p.magenta,
    id = "grow-subject-label",
}
growSubject:point {
    point = { 0, -0.12 },
    fill = p.gold,
    radius = 4,
    layer = 8,
    id = "grow-anchor",
}
grow:wait(0.35)
grow:grow_from_center(growSubject, 1.20, "ease_out")
grow:wait(0.35)
grow:shrink_to_center(growSubject, 1.00, "ease_in")
grow:wait(0.55)

local function radial_points(radius, square, count)
    local points = {}
    for i = 0, count - 1 do
        local angle = 6.28318530718 * i / count
        local x = math.cos(angle)
        local y = math.sin(angle)
        local scale = radius
        if square then
            scale = radius / math.max(math.abs(x), math.abs(y))
        end
        points[#points + 1] = { x * scale, y * scale - 0.12 }
    end
    return points
end

local function star_points(outer, inner, count)
    local points = {}
    for i = 0, count - 1 do
        local angle = 6.28318530718 * i / count
        local radius = i % 2 == 0 and outer or inner
        points[#points + 1] = { math.cos(angle) * radius, math.sin(angle) * radius - 0.12 }
    end
    return points
end

local morph = make_panel(p.panel_b, "morph", "04", "MORPH", "single contour / target identity")
local morphSubject = morph:polygon {
    points = radial_points(1.02, false, 32),
    fill = "#4cc9f033",
    stroke = p.cyan,
    width = 4,
    id = "morph-subject",
}
morph:text {
    text = "A",
    point = { -1.05, -1.73 },
    font = "Pretendard",
    size = 16,
    fill = p.cyan,
    id = "morph-id-a0",
}
morph:arrow {
    from = { -0.68, -1.70 },
    to = { -0.30, -1.70 },
    color = p.muted,
    width = 2,
    tip = 7,
    id = "morph-flow-ab",
}
morph:text {
    text = "B",
    point = { 0, -1.73 },
    font = "Pretendard",
    size = 16,
    fill = p.green,
    id = "morph-id-b",
}
morph:arrow {
    from = { 0.30, -1.70 },
    to = { 0.68, -1.70 },
    color = p.muted,
    width = 2,
    tip = 7,
    id = "morph-flow-ba",
}
morph:text {
    text = "A",
    point = { 1.05, -1.73 },
    font = "Pretendard",
    size = 16,
    fill = p.cyan,
    id = "morph-id-a1",
}
morph:wait(0.35)
local morphSquare = morph:polygon {
    points = radial_points(1.02, true, 32),
    fill = "#7bd88f33",
    stroke = p.green,
    width = 4,
    id = "morph-square-template",
}
morph:morph(morphSubject, morphSquare, 1.20, "ease_in_out")
morph:wait(0.35)
local morphOrigin = morph:polygon {
    points = radial_points(1.02, false, 32),
    fill = "#4cc9f033",
    stroke = p.cyan,
    width = 4,
    id = "morph-origin-template",
}
morph:morph(morphSquare, morphOrigin, 1.00, "ease_in_out")
morph:wait(0.55)

local replacement =
    make_panel(p.panel_a, "replacement", "05", "REPLACEMENT", "A leaves / B owns the next beat")
local replacementSource = replacement:polygon {
    points = radial_points(0.98, false, 16),
    fill = "#ff6b6b33",
    stroke = p.red,
    width = 4,
    id = "replacement-source",
}
replacement:text {
    text = "A / SOURCE",
    point = { -1.65, -1.92 },
    align = { 0.5, 0.5 },
    font = "Pretendard",
    size = 13,
    fill = p.red,
    id = "replacement-source-badge",
}
replacement:arrow {
    from = { -0.48, -1.90 },
    to = { 0.48, -1.90 },
    color = p.muted,
    width = 2,
    tip = 8,
    id = "replacement-flow",
}
replacement:text {
    text = "B / TARGET",
    point = { 1.65, -1.92 },
    align = { 0.5, 0.5 },
    font = "Pretendard",
    size = 13,
    fill = p.green,
    id = "replacement-target-badge",
}
replacement:wait(0.35)
local replacementTarget = replacement:polygon {
    points = star_points(1.08, 0.62, 16),
    fill = "#7bd88f33",
    stroke = p.green,
    width = 4,
    id = "replacement-target",
}
replacement:replacement_transform(replacementSource, replacementTarget, 1.20, "ease_in_out")
replacement:wait(0.35)
replacement:indicate(replacementTarget, {
    color = p.gold,
    scale = 1.16,
    duration = 1.00,
    easing = "ease_in_out",
})
replacement:wait(0.55)

local indicate =
    make_panel(p.panel_b, "indicate", "06", "INDICATE", "temporary emphasis / exact restore")
local function translate(x, y)
    return {
        1,
        0,
        0,
        x,
        0,
        1,
        0,
        y,
        0,
        0,
        1,
        0,
        0,
        0,
        0,
        1,
    }
end
local function node(scene, name, x, label, color)
    local item = scene:group { matrix = translate(x, -0.12), id = name }
    item:circle {
        center = { 0, 0 },
        radius = 0.56,
        fill = color .. "33",
        stroke = color,
        width = 3,
        id = name .. "-body",
    }
    item:text {
        text = label,
        point = { 0, 0 },
        font = "Pretendard",
        size = 18,
        fill = p.text,
        id = name .. "-label",
    }
    return item
end
local indicateLeft = node(indicate, "indicate-left", -1.75, "1", p.muted)
local indicateSubject = node(indicate, "indicate-subject", 0, "2", p.cyan)
local indicateResult = node(indicate, "indicate-result", 1.75, "3", p.green)
indicate:connector {
    from = indicateLeft,
    to = indicateSubject,
    padding = 0.08,
    stroke = p.rule,
    width = 2,
    id = "indicate-edge-left",
}
indicate:connector {
    from = indicateSubject,
    to = indicateResult,
    padding = 0.08,
    stroke = p.rule,
    width = 2,
    id = "indicate-edge-right",
}
indicate:wait(0.35)
indicate:indicate(indicateSubject, {
    color = p.gold,
    scale = 1.16,
    duration = 1.20,
    easing = "ease_in_out",
})
indicate:wait(0.35)
indicate:indicate(indicateResult, {
    color = p.gold,
    scale = 1.12,
    duration = 1.00,
    easing = "ease_in_out",
})
indicate:wait(0.55)

local gallery = vis.scene {
    width = 960,
    height = 540,
    fps = 30,
    background = p.background,
    camera = { mode = "fixed", view = "2d", target = { 0, 0 }, height = 6.4 },
}
gallery:viewport(direction, { x = 0, y = 0, width = 1 / 3, height = 0.5 })
gallery:viewport(fade, { x = 1 / 3, y = 0, width = 1 / 3, height = 0.5 })
gallery:viewport(grow, { x = 2 / 3, y = 0, width = 1 / 3, height = 0.5 })
gallery:viewport(morph, { x = 0, y = 0.5, width = 1 / 3, height = 0.5 })
gallery:viewport(replacement, { x = 1 / 3, y = 0.5, width = 1 / 3, height = 0.5 })
gallery:viewport(indicate, { x = 2 / 3, y = 0.5, width = 1 / 3, height = 0.5 })
return gallery
