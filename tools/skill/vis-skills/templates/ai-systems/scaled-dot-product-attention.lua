-- Source equation: Vaswani et al. (2017), Equation 1 and Figure 2 (left).
-- Question: how does one query row become normalized key weights and a value-weighted output?
-- The numeric row below is illustrative; replace it only with sourced or recomputed values.
-- Look: ink operators and routes; the selected q₂ row carries `accent`, its output o₂ carries `result`,
-- and the masked key is a dashed cell. Node widths fit their measured labels with at least 6 px inset.
local scene = vis.scene {
    width = 960, height = 540, fps = 30, loop = false,
    theme = "pro_white",
    camera = {mode = "fixed", view = "2d", target = {0, 0}, height = 540},
}

local LAYER = {guide = 0, route = 10, cell = 20, node = 22, text = 30, focus = 40}
local root = scene:group {id = "scaled-attention:root"}
local header = root:group {id = "scaled-attention:header"}
local nodes = root:group {id = "scaled-attention:nodes"}
local routes = root:group {id = "scaled-attention:routes"}
local matrix = root:group {id = "scaled-attention:matrix"}
local axis_group = root:group {id = "scaled-attention:axis-group"}
local result = root:group {id = "scaled-attention:result"}

header:text {
    text = "Equation 1 · selected q₂ row · numeric weights are illustrative",
    point = {0, 228}, role = "code", fill = "muted", layer = LAYER.text,
    id = "scaled-attention:subtitle",
}

local function node(id, label, detail, center, size)
    local group = nodes:group {id = "scaled-attention:node:" .. id}
    local body = group:rectangle {
        center = center, size = size, fill = "#00000000",
        stroke = "foreground", width = 1.25,
        layer = LAYER.node, id = "scaled-attention:node:" .. id .. ":body",
    }
    -- A list label is a spans row, so an exponent sits raised in glyphs the font has.
    local caption = {
        point = {center[1], center[2] + 11}, role = "text",
        fill = "foreground", layer = LAYER.text,
        id = "scaled-attention:node:" .. id .. ":label",
    }
    if type(label) == "table" then
        group:spans(vis.merge(label, caption))
    else
        group:text(vis.merge({text = label}, caption))
    end
    group:text {
        text = detail, point = {center[1], center[2] - 16}, role = "code", size = 12,
        fill = "muted", layer = LAYER.text,
        id = "scaled-attention:node:" .. id .. ":detail",
    }
    return body
end

local q = node("q", "Q", "[T_q,d_k]", {-404, 165}, {100, 64})
local k = node("k", "K", "[T_k,d_k]", {-404, 95}, {100, 64})
local v = node("v", "V", "[T_k,d_v]", {-404, 25}, {100, 64})
local scores = node("scores", {"QK", {"T", rise = 0.35, size = 11}}, "[T_q,T_k]", {-258, 130}, {100, 64})
local scale = node("scale", "Scale", "÷ √d_k", {-143, 130}, {74, 64})
local mask = node("mask", "+ mask", "pre-softmax", {-18, 130}, {120, 64})
local softmax = node("softmax", "Softmax", "key axis", {117, 130}, {94, 64})
local mix = node("mix", "Weights × V", "weighted sum", {257, 130}, {130, 64})
local output = node("output", "O", "[T_q,d_v]", {400, 130}, {100, 64})

local pipeline_routes = {
    routes:arrow {from = {-352, 163}, to = {-311, 144}, tip = 7, color = "foreground", width = 1, layer = LAYER.route, id = "scaled-attention:route:q-scores"},
    routes:arrow {from = {-352, 97}, to = {-311, 116}, tip = 7, color = "foreground", width = 1, layer = LAYER.route, id = "scaled-attention:route:k-scores"},
    routes:arrow {from = {-206, 130}, to = {-182, 130}, tip = 7, color = "foreground", width = 1, layer = LAYER.route, id = "scaled-attention:route:scores-scale"},
    routes:arrow {from = {-104, 130}, to = {-80, 130}, tip = 7, color = "foreground", width = 1, layer = LAYER.route, id = "scaled-attention:route:scale-mask"},
    routes:arrow {from = {44, 130}, to = {68, 130}, tip = 7, color = "foreground", width = 1, layer = LAYER.route, id = "scaled-attention:route:mask-softmax"},
    routes:arrow {from = {166, 130}, to = {190, 130}, tip = 7, color = "foreground", width = 1, layer = LAYER.route, id = "scaled-attention:route:softmax-mix"},
    routes:arrow {from = {324, 130}, to = {348, 130}, tip = 7, color = "foreground", width = 1, layer = LAYER.route, id = "scaled-attention:route:mix-output"},
}
local value_route = routes:route {
    points = {{-352, 25}, {-331, 25}, {-331, 72}, {257, 72}, {257, 96}},
    tip = 7, color = "foreground", width = 1, layer = LAYER.route,
    id = "scaled-attention:route:v-mix",
}

local weights = {0.10, 0.20, 0.70, 0.00}
local selected_row = 3
local x0, y0, cw, ch = -230, 0, 58, 32
for column = 1, 4 do
    matrix:text {
        text = "k" .. tostring(column - 1), point = {x0 + (column - 1) * cw, y0 + 30},
        role = "code", fill = "muted", layer = LAYER.text,
        id = "scaled-attention:key-label:" .. column,
    }
end
for row = 1, 4 do
    matrix:text {
        text = "q" .. tostring(row - 1), point = {x0 - 50, y0 - (row - 1) * ch},
        role = "code", fill = row == selected_row and "accent" or "muted",
        layer = LAYER.text, id = "scaled-attention:query-label:" .. row,
    }
    for column = 1, 4 do
        local is_selected = row == selected_row
        local is_masked = column > row
        -- No fills: the printed weights carry the values, the selected row its hue, a mask its dash.
        matrix:rectangle {
            center = {x0 + (column - 1) * cw, y0 - (row - 1) * ch},
            size = {54, 28}, fill = "#00000000",
            stroke = is_selected and "accent" or "border", width = is_selected and 1.25 or 1,
            dash = is_selected and is_masked and {3, 3} or nil,
            layer = LAYER.cell, id = "scaled-attention:cell:" .. row .. ":" .. column,
        }
        if is_selected then
            matrix:text {
                text = is_masked and "mask" or string.format("%.2f", weights[column]),
                point = {x0 + (column - 1) * cw, y0 - (row - 1) * ch}, role = "code", size = 12,
                fill = is_masked and "muted" or "foreground", layer = LAYER.text,
                id = "scaled-attention:value:" .. column,
            }
        end
    end
end
matrix:text {
    text = "post-softmax weights", point = {x0 + 1.5 * cw, -136}, role = "code",
    fill = "muted", layer = LAYER.text, id = "scaled-attention:matrix-label",
}
local axis = axis_group:arrow {
    from = {x0 - 27, -158}, to = {x0 + 3 * cw + 27, -158}, tip = 7, color = "muted", width = 1,
    layer = LAYER.focus, id = "scaled-attention:key-axis",
}
local axis_label = axis_group:text {
    text = "normalize over keys", point = {x0 + 1.5 * cw, -178}, role = "code",
    fill = "muted", layer = LAYER.text, id = "scaled-attention:key-axis-label",
}

result:rectangle {
    center = {232, -50}, size = {336, 112}, fill = "#00000000",
    stroke = "result", width = 1.25, layer = LAYER.node,
    id = "scaled-attention:selected-output:body",
}
result:text {
    text = "selected output row o₂", point = {232, -20}, role = "text",
    fill = "result", layer = LAYER.text, id = "scaled-attention:selected-output:title",
}
result:text {
    text = "0.10 V₀ + 0.20 V₁ + 0.70 V₂", point = {232, -53}, role = "code",
    fill = "foreground", layer = LAYER.text, id = "scaled-attention:selected-output:formula",
}
result:text {
    text = "k₃ masked → contribution 0", point = {232, -84}, role = "code",
    fill = "muted", layer = LAYER.text, id = "scaled-attention:selected-output:mask-note",
}
local conclusion = root:spans {
    "Attention(Q,K,V) = softmax(QK", {"T", rise = 0.35, size = 10}, " / √d_k + mask) V",
    point = {0, -222}, role = "code", fill = "foreground", layer = LAYER.text,
    id = "scaled-attention:conclusion",
}

scene:fade_in(header, {shift = {0, -6}, duration = 0.55, curve = "gentle"})
scene:fade_in(nodes, {shift = {0, -6}, duration = 0.7, curve = "ease_out"})
scene:create(pipeline_routes, 0.75, "ease_out", 0.05, "forward")
scene:create(value_route, 0.45, "ease_out")
scene:fade_in(matrix, {shift = {0, -5}, duration = 0.65, curve = "gentle"})
scene:create(axis, 0.35, "linear")
scene:fade_in(axis_label, {duration = 0.25, curve = "gentle"})
scene:fade_in(result, {shift = {0, -5}, duration = 0.55, curve = "gentle"})
scene:indicate(mask, {color = "focus", scale = 1.025, duration = 0.4, curve = "ease_in_out"})
scene:indicate(output, {color = "result", scale = 1.025, duration = 0.4, curve = "ease_in_out"})
scene:fade_in(conclusion, {duration = 0.35, curve = "gentle"})
scene:wait(2.5)
return scene
