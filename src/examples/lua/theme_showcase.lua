local function custom_theme(gradient)
    return {
        preset = "pro_black",
        background = "#171225",
        text = {
            h1 = { font = "Pretendard", size = 38, color = "#f8fafc" },
            h2 = { font = "Pretendard", size = 29, color = "#c4b5fd" },
            h3 = { font = "Pretendard", size = 22, color = "#67e8f9" },
            text = { font = "Pretendard", size = 16, color = "#d8cff0" },
            code = { font = "JetBrainsMono", size = 15, color = "#fbbf24" },
        },
        objects = {
            "#8b5cf6",
            "#22d3ee",
            "#f472b6",
            "#fbbf24",
            "#34d399",
            "#60a5fa",
        },
        object_width = 3.0,
        gradient = gradient,
        end_gradient_stop = "#fb7185",
        axis = {
            x = "#f472b6",
            y = "#34d399",
            z = "#60a5fa",
            grid = "#493f63",
            label = "#bdb4d7",
        },
    }
end

local custom = custom_theme(false)
local custom_gradient = custom_theme(true)
local gradient_pro_white = { preset = "pro_white", gradient = true }
local gradient_pro_black = { preset = "pro_black", gradient = true }

local function translation(x, y)
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

local function vector_to_point(origin, target, clearance)
    local dx, dy = target[1] - origin[1], target[2] - origin[2]
    local length = math.sqrt(dx * dx + dy * dy)
    local scale = (length - clearance) / length
    return { dx * scale, dy * scale }
end

local function panel(theme, name, heading)
    local scene = vis.scene {
        width = 480,
        height = 500,
        fps = 30,
        loop = false,
        theme = theme,
        camera = { mode = "fixed", view = "2d", target = { 0, 0 }, height = 10 },
    }

    local content = scene:group { id = "content" }
    local typography = content:group { id = "typography" }
    typography:text {
        text = name,
        point = { -3.45, 4.20 },
        align = { 0, 0.5 },
        role = "h1",
        id = "title",
    }
    typography:text {
        text = heading,
        point = { -3.35, 3.40 },
        align = { 0, 0.5 },
        role = "h2",
        id = "heading",
    }
    typography:text {
        text = "Semantic typography",
        point = { -3.35, 2.70 },
        align = { 0, 0.5 },
        role = "h3",
        id = "subheading",
    }
    typography:text {
        text = "Text · labels · annotations",
        point = { -3.35, 2.02 },
        align = { 0, 0.5 },
        role = "text",
        id = "body",
    }
    typography:text {
        text = "f(x) = sin(x) + 1",
        point = { -3.35, 1.45 },
        align = { 0, 0.5 },
        role = "code",
        id = "formula",
    }

    local space = content:space {
        x = { -3, 3, 1 },
        y = { -1.5, 1.5, 0.75 },
        z = { 0, 0, 1 },
        matrix = translation(0, -1.55),
        numbers = true,
        number_size = 11,
        id = "space",
    }
    local vectorOrigin, pointPosition = { -0.55, 0.10 }, { 0.70, 0.92 }
    local vector = space:vector {
        origin = vectorOrigin,
        value = vector_to_point(vectorOrigin, pointPosition, 0.34),
        tip = 13,
        id = "vector",
    }
    local shapes = space:group { id = "shapes" }
    shapes:point {
        point = pointPosition,
        radius = 6,
        id = "point",
    }
    shapes:circle {
        center = { -2.30, 0.72 },
        radius = 0.34,
        id = "circle",
    }
    shapes:rectangle {
        center = { -1.45, -0.72 },
        size = { 0.72, 0.56 },
        corner = 0.08,
        id = "rectangle",
    }
    shapes:polygon {
        points = { { -0.45, -1.02 }, { 0.12, -0.38 }, { 0.72, -1.02 } },
        id = "polygon",
    }
    local curve = space:curve {
        from = { 1.15, 0.52 },
        control1 = { 1.48, 1.25 },
        control2 = { 2.18, 1.25 },
        to = { 2.52, 0.52 },
        id = "curve",
    }
    shapes:surface {
        points = {
            { 1.42, -1.02 },
            { 2.62, -1.02 },
            { 1.42, -0.42 },
            { 2.62, -0.42 },
        },
        size = { 2, 2 },
        mode = "solid",
        shading = false,
        id = "filled-shape",
    }

    scene:fade_in(content, {
        shift = { 0, 0.16 },
        scale = 0.985,
        duration = 0.62,
        curve = "gentle",
    })
    scene:wait(0.55)
    return scene
end

local page = vis.scene {
    width = 1440,
    height = 1000,
    fps = 30,
    loop = false,
    background = "#111827",
}

local columns = {
    { "pro_white", gradient_pro_white, "PRO WHITE" },
    { "pro_black", gradient_pro_black, "PRO BLACK" },
    { custom, custom_gradient, "CUSTOM" },
}
for index, column in ipairs(columns) do
    local x = (index - 1) / #columns
    page:viewport(panel(column[1], column[3], "VISUAL SYSTEM"), {
        x = x,
        y = 0,
        width = 1 / #columns,
        height = 0.5,
    })
    page:viewport(panel(column[2], column[3], "GRADIENT"), {
        x = x,
        y = 0.5,
        width = 1 / #columns,
        height = 0.5,
    })
end
return page
