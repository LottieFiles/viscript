-- Question: where do attention, MLP, normalization, and residual paths sit in one decoder block?
-- This is an illustrative pre-norm block. Replace the variant, shapes, head grouping, and equations from the source.
-- Look: ink operators and main path; the residual paths and the contract they keep share one hue (accent).
-- The focal sublayer is told apart by a heavier outline, not a color. Box widths fit the measured labels.
local scene = vis.scene {theme = "pro_white", camera = {height = 540}}

local LAYER = {route = 10, node = 20, text = 30, focus = 40}
local root = scene:group {id = "transformer-block:root"}
local header = root:group {id = "transformer-block:header"}
local nodes = root:group {id = "transformer-block:nodes"}
local routes = root:group {id = "transformer-block:routes"}
local evidence = root:group {id = "transformer-block:evidence"}

header:text {
    text = "pre-norm decoder block · illustrative variant",
    point = {0, 203}, role = "code", fill = "muted", layer = LAYER.text,
    id = "transformer-block:subtitle",
}

local NODE_Y = 72
local function node(spec)
    local group = nodes:group {id = "transformer-block:node:" .. spec.id}
    local body = group:rectangle {
        center = {spec.x, NODE_Y}, size = spec.size,
        stroke = "foreground",
        width = spec.focal and 1.5 or 1.25, layer = LAYER.node,
        id = "transformer-block:node:" .. spec.id .. ":body",
    }
    group:text {
        text = spec.label, point = {spec.x, NODE_Y + 12}, role = "text",
        fill = "foreground", layer = LAYER.text,
        id = "transformer-block:node:" .. spec.id .. ":label",
    }
    group:text {
        text = spec.detail, point = {spec.x, NODE_Y - 16}, role = "code", size = 12, fill = "muted",
        layer = LAYER.text, id = "transformer-block:node:" .. spec.id .. ":detail",
    }
    return {body = body, spec = spec}
end

local specs = {
    {id = "input", label = "x", detail = "[B,T,d]", x = -414, size = {82, 70}},
    {id = "norm1", label = "Norm", detail = "pre", x = -310, size = {68, 70}},
    {id = "attention", label = "Attention", detail = "masked", x = -196, size = {102, 78}, focal = true},
    {id = "add1", label = "+", detail = "residual", x = -70, size = {92, 70}},
    {id = "norm2", label = "Norm", detail = "pre", x = 39, size = {68, 70}},
    {id = "mlp", label = "MLP", detail = "source form", x = 162, size = {120, 78}},
    {id = "add2", label = "+", detail = "residual", x = 297, size = {92, 70}},
    {id = "output", label = "x′", detail = "[B,T,d]", x = 413, size = {82, 70}},
}
local handles = {}
for _, spec in ipairs(specs) do handles[#handles + 1] = node(spec) end

local main_routes = {}
for index = 1, #handles - 1 do
    local left = handles[index].spec
    local right = handles[index + 1].spec
    main_routes[#main_routes + 1] = routes:arrow {
        from = {left.x + left.size[1] * 0.5, NODE_Y},
        to = {right.x - right.size[1] * 0.5, NODE_Y},
        tip = 7, color = "foreground", width = 1, layer = LAYER.route,
        id = "transformer-block:route:" .. left.id .. "-" .. right.id,
    }
end
local residual_routes = {
    routes:route {
        points = {{-414, 107}, {-414, 150}, {-82, 150}, {-82, 107}},
        tip = 7, color = "accent", width = 1.5, layer = LAYER.route,
        id = "transformer-block:residual:attention",
    },
    routes:route {
        points = {{-58, 107}, {-58, 132}, {297, 132}, {297, 107}},
        tip = 7, color = "accent", width = 1.5, layer = LAYER.route,
        id = "transformer-block:residual:mlp",
    },
}

-- A dashed leader ties the focal sublayer to its equation: an implied relation, not a data path.
evidence:line {
    from = {-196, 29}, to = {-196, -9}, color = "muted", width = 1,
    dash = {6, 5}, layer = LAYER.focus, id = "transformer-block:attention-bridge",
}
evidence:spans {
    "Attention(X) = softmax(QK", {"T", rise = 0.35, size = 10}, " / √d_h + M) V",
    point = {-40, -25}, role = "code", fill = "foreground", layer = LAYER.text,
    id = "transformer-block:attention-equation",
}
evidence:text {
    text = "Q,K,V [B,h,T,d_h]  ·  scores [B,h,T_q,T_k]  ·  softmax over key axis",
    point = {-40, -62}, role = "code", fill = "muted", layer = LAYER.text,
    id = "transformer-block:attention-shapes",
}
evidence:text {
    text = "MLP(X) = W₂ φ(W₁X)  ·  exact activation/gating follows the source",
    point = {-40, -108}, role = "code", fill = "muted", layer = LAYER.text,
    id = "transformer-block:mlp-equation",
}
local conclusion = root:text {
    text = "Both residual merges preserve the block contract [B,T,d].",
    point = {0, -190}, role = "code", fill = "accent", layer = LAYER.text,
    id = "transformer-block:conclusion",
}

scene:fade_in(header, {shift = {0, -6}, duration = 0.55, curve = "gentle"})
scene:fade_in(nodes, {shift = {0, -7}, duration = 0.7, curve = "ease_out"})
scene:create(main_routes, 0.85, "ease_out", 0.06, "forward")
scene:create(residual_routes, 0.7, "ease_out", 0.08, "forward")
scene:fade_in(evidence, {shift = {0, -6}, duration = 0.7, curve = "gentle"})
scene:indicate(handles[3].body, {color = "focus", scale = 1.025, duration = 0.42, curve = "ease_in_out"})
scene:fade_in(conclusion, {shift = {0, -5}, duration = 0.35, curve = "gentle"})
scene:wait(2.3)
return scene
