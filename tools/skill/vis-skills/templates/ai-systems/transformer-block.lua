-- Question: where do attention, MLP, normalization, and residual paths sit in one decoder block?
-- This is an illustrative pre-norm block. Replace the variant, shapes, head grouping, and equations from the source.
local scene = vis.scene {
    width = 960, height = 540, fps = 30, loop = false,
    theme = "pro_white",
    camera = {mode = "fixed", view = "2d", target = {0, 0}, height = 540},
}

local LAYER = {route = 10, node = 20, text = 30, focus = 40}
local root = scene:group {id = "transformer-block:root"}
local header = root:group {id = "transformer-block:header"}
local nodes = root:group {id = "transformer-block:nodes"}
local routes = root:group {id = "transformer-block:routes"}
local evidence = root:group {id = "transformer-block:evidence"}

header:text {
    text = "Pre-norm Transformer decoder block", point = {0, 232}, role = "h2",
    fill = "foreground", layer = LAYER.text, id = "transformer-block:title",
}
header:text {
    text = "illustrative variant · verify normalization order, head grouping, mask, and MLP form",
    point = {0, 199}, role = "code", fill = "muted", layer = LAYER.text,
    id = "transformer-block:subtitle",
}

local function node(spec)
    local group = nodes:group {id = "transformer-block:node:" .. spec.id}
    local body = group:rectangle {
        center = {spec.x, 66}, size = spec.size, corner = 7,
        fill = "surface", stroke = spec.focal and "accent" or "border",
        width = spec.focal and 2.5 or 1.5, layer = LAYER.node,
        id = "transformer-block:node:" .. spec.id .. ":body",
    }
    group:text {
        text = spec.label, point = {spec.x, 78}, role = "text",
        fill = spec.focal and "accent" or "foreground", layer = LAYER.text,
        id = "transformer-block:node:" .. spec.id .. ":label",
    }
    group:text {
        text = spec.detail, point = {spec.x, 50}, role = "code", fill = "muted",
        layer = LAYER.text, id = "transformer-block:node:" .. spec.id .. ":detail",
    }
    return {body = body, spec = spec}
end

local specs = {
    {id = "input", label = "x", detail = "[B,T,d]", x = -422, size = {70, 70}},
    {id = "norm1", label = "Norm", detail = "pre", x = -316, size = {88, 70}},
    {id = "attention", label = "Attention", detail = "masked", x = -174, size = {116, 78}, focal = true},
    {id = "add1", label = "+", detail = "residual", x = -35, size = {76, 70}},
    {id = "norm2", label = "Norm", detail = "pre", x = 77, size = {88, 70}},
    {id = "mlp", label = "MLP", detail = "source form", x = 218, size = {116, 78}},
    {id = "add2", label = "+", detail = "residual", x = 338, size = {76, 70}},
    {id = "output", label = "x′", detail = "[B,T,d]", x = 426, size = {72, 70}},
}
local handles = {}
for _, spec in ipairs(specs) do handles[#handles + 1] = node(spec) end

local main_routes = {}
for index = 1, #handles - 1 do
    local left = handles[index].spec
    local right = handles[index + 1].spec
    main_routes[#main_routes + 1] = routes:arrow {
        from = {left.x + left.size[1] * 0.5, 66},
        to = {right.x - right.size[1] * 0.5, 66},
        tip = 9, color = "foreground", width = 2, layer = LAYER.route,
        id = "transformer-block:route:" .. left.id .. "-" .. right.id,
    }
end
local residual_routes = {
    routes:route {
        points = {{-422, 101}, {-422, 148}, {-47, 148}, {-47, 101}},
        tip = 9, color = "accent", width = 2, layer = LAYER.route,
        id = "transformer-block:residual:attention",
    },
    routes:route {
        points = {{-23, 101}, {-23, 132}, {338, 132}, {338, 101}},
        tip = 9, color = "accent", width = 2, layer = LAYER.route,
        id = "transformer-block:residual:mlp",
    },
}

evidence:line {
    from = {-174, 27}, to = {-174, -42}, color = "accent", width = 1.5,
    dash = {6, 5}, layer = LAYER.focus, id = "transformer-block:attention-bridge",
}
evidence:spans {
    "Attention(X) = softmax(QK", {"T", rise = 0.35, size = 10}, " / √dₕ + M) V",
    point = {-40, -72}, role = "code", fill = "foreground", layer = LAYER.text,
    id = "transformer-block:attention-equation",
}
evidence:text {
    text = "Q,K,V [B,h,T,dₕ]  ·  scores [B,h,T_q,T_k]  ·  softmax over key axis",
    point = {-40, -108}, role = "code", fill = "muted", layer = LAYER.text,
    id = "transformer-block:attention-shapes",
}
evidence:text {
    text = "MLP(X) = W₂ φ(W₁X)  ·  exact activation/gating follows the source",
    point = {-40, -151}, role = "code", fill = "muted", layer = LAYER.text,
    id = "transformer-block:mlp-equation",
}
local conclusion = root:text {
    text = "Both residual merges preserve the block contract [B,T,d].",
    point = {0, -218}, role = "code", fill = "result", layer = LAYER.text,
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
