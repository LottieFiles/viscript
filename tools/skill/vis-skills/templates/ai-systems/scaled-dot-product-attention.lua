-- Source equation: Vaswani et al. (2017), Equation 1 and Figure 2 (left).
-- Question: how does one query row become normalized key weights and a value-weighted output?
-- The numeric row below is illustrative; replace it only with sourced or recomputed values.
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
    text = "Scaled dot-product attention", point = {0, 232}, role = "h2",
    fill = "foreground", layer = LAYER.text, id = "scaled-attention:title",
}
header:text {
    text = "Equation 1 · selected q₂ row · numeric weights are illustrative",
    point = {0, 199}, role = "code", fill = "muted", layer = LAYER.text,
    id = "scaled-attention:subtitle",
}

local function node(id, label, detail, center, size, semantic)
    local group = nodes:group {id = "scaled-attention:node:" .. id}
    local body = group:rectangle {
        center = center, size = size, corner = 6, fill = "surface",
        stroke = semantic or "border", width = semantic and 2.2 or 1.4,
        layer = LAYER.node, id = "scaled-attention:node:" .. id .. ":body",
    }
    -- A list label is a spans row, so an exponent sits raised in glyphs the font has.
    local caption = {
        point = {center[1], center[2] + 11}, role = "text",
        fill = semantic or "foreground", layer = LAYER.text,
        id = "scaled-attention:node:" .. id .. ":label",
    }
    if type(label) == "table" then
        group:spans(vis.merge(label, caption))
    else
        group:text(vis.merge({text = label}, caption))
    end
    group:text {
        text = detail, point = {center[1], center[2] - 16}, role = "code",
        fill = "muted", layer = LAYER.text,
        id = "scaled-attention:node:" .. id .. ":detail",
    }
    return body
end

local q = node("q", "Q", "[T_q,d_k]", {-420, 148}, {92, 64})
local k = node("k", "K", "[T_k,d_k]", {-420, 78}, {92, 64})
local v = node("v", "V", "[T_k,d_v]", {-420, 8}, {92, 64})
local scores = node("scores", {"QK", {"T", rise = 0.35, size = 11}}, "[T_q,T_k]", {-313, 114}, {94, 64}, "accent")
local scale = node("scale", "Scale", "÷ √d_k", {-201, 114}, {88, 64})
local mask = node("mask", "+ mask", "pre-softmax", {-82, 114}, {116, 64}, "focus")
local softmax = node("softmax", "Softmax", "key axis", {64, 114}, {112, 64})
local mix = node("mix", "Weights × V", "weighted sum", {221, 114}, {136, 64}, "accent")
local output = node("output", "O", "[T_q,d_v]", {394, 114}, {94, 64}, "result")

local pipeline_routes = {
    routes:arrow {from = {-374, 148}, to = {-360, 128}, tip = 8, color = "foreground", width = 1.8, layer = LAYER.route, id = "scaled-attention:route:q-scores"},
    routes:arrow {from = {-374, 78}, to = {-360, 101}, tip = 8, color = "foreground", width = 1.8, layer = LAYER.route, id = "scaled-attention:route:k-scores"},
    routes:arrow {from = {-266, 114}, to = {-245, 114}, tip = 9, color = "foreground", width = 2, layer = LAYER.route, id = "scaled-attention:route:scores-scale"},
    routes:arrow {from = {-157, 114}, to = {-140, 114}, tip = 9, color = "foreground", width = 2, layer = LAYER.route, id = "scaled-attention:route:scale-mask"},
    routes:arrow {from = {-24, 114}, to = {8, 114}, tip = 9, color = "foreground", width = 2, layer = LAYER.route, id = "scaled-attention:route:mask-softmax"},
    routes:arrow {from = {120, 114}, to = {153, 114}, tip = 9, color = "foreground", width = 2, layer = LAYER.route, id = "scaled-attention:route:softmax-mix"},
    routes:arrow {from = {289, 114}, to = {347, 114}, tip = 9, color = "result", width = 2.2, layer = LAYER.route, id = "scaled-attention:route:mix-output"},
}
local value_route = routes:route {
    points = {{-374, 8}, {-348, 8}, {-348, 50}, {221, 50}, {221, 82}},
    tip = 9, color = "accent", width = 2, layer = LAYER.route,
    id = "scaled-attention:route:v-mix",
}

local weights = {0.10, 0.20, 0.70, 0.00}
local selected_row = 3
local x0, y0, cw, ch = -205, -60, 47, 31
for column = 1, 4 do
    matrix:text {
        text = "k" .. tostring(column - 1), point = {x0 + (column - 1) * cw, y0 + 27},
        role = "code", fill = "muted", layer = LAYER.text,
        id = "scaled-attention:key-label:" .. column,
    }
end
for row = 1, 4 do
    matrix:text {
        text = "q" .. tostring(row - 1), point = {x0 - 39, y0 - (row - 1) * ch},
        role = "code", fill = row == selected_row and "accent" or "muted",
        layer = LAYER.text, id = "scaled-attention:query-label:" .. row,
    }
    for column = 1, 4 do
        local is_selected = row == selected_row
        local is_masked = column > row
        local fill = "surface"
        if is_selected and is_masked then fill = "#fecdd3" end
        if is_selected and not is_masked then
            local value = weights[column]
            if value >= 0.6 then fill = "#93c5fd" elseif value >= 0.15 then fill = "#bfdbfe" else fill = "#dbeafe" end
        end
        matrix:rectangle {
            center = {x0 + (column - 1) * cw, y0 - (row - 1) * ch},
            size = {45, 27}, corner = 2, fill = fill,
            stroke = is_selected and "accent" or "border", width = is_selected and 1.6 or 1,
            layer = LAYER.cell, id = "scaled-attention:cell:" .. row .. ":" .. column,
        }
        if is_selected then
            matrix:text {
                text = is_masked and "mask" or string.format("%.2f", weights[column]),
                point = {x0 + (column - 1) * cw, y0 - (row - 1) * ch}, role = "code",
                fill = is_masked and "danger" or "foreground", layer = LAYER.text,
                id = "scaled-attention:value:" .. column,
            }
        end
    end
end
matrix:text {
    text = "post-softmax weights", point = {-135, -188}, role = "code",
    fill = "muted", layer = LAYER.text, id = "scaled-attention:matrix-label",
}
local axis = axis_group:arrow {
    from = {-205, -202}, to = {-64, -202}, tip = 8, color = "accent", width = 1.8,
    layer = LAYER.focus, id = "scaled-attention:key-axis",
}
local axis_label = axis_group:text {
    text = "normalize over keys", point = {-135, -218}, role = "code",
    fill = "accent", layer = LAYER.text, id = "scaled-attention:key-axis-label",
}

result:rectangle {
    center = {238, -113}, size = {296, 106}, corner = 8, fill = "surface",
    stroke = "result", width = 2, layer = LAYER.node,
    id = "scaled-attention:selected-output:body",
}
result:text {
    text = "selected output row o₂", point = {238, -83}, role = "text",
    fill = "result", layer = LAYER.text, id = "scaled-attention:selected-output:title",
}
result:text {
    text = "0.10 V₀ + 0.20 V₁ + 0.70 V₂", point = {238, -119}, role = "code",
    fill = "foreground", layer = LAYER.text, id = "scaled-attention:selected-output:formula",
}
result:text {
    text = "k₃ masked → contribution 0", point = {238, -149}, role = "code",
    fill = "muted", layer = LAYER.text, id = "scaled-attention:selected-output:mask-note",
}
local conclusion = root:spans {
    "Attention(Q,K,V) = softmax(QK", {"T", rise = 0.35, size = 10}, " / √d_k + mask) V",
    point = {0, -246}, role = "code", fill = "result", layer = LAYER.text,
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
