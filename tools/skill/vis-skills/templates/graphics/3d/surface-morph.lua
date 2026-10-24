local pi = math.pi
local r, polar, az = 8, 65 * pi / 180, -60 * pi / 180
local s = vis.scene {theme = "pro_black", camera = {
    view = "3d",
    eye = {r * math.sin(polar) * math.cos(az), r * math.sin(polar) * math.sin(az), r * math.cos(polar)},
    up = {0, 0, 1}, fov = 42 * pi / 180, near = 0.1, far = 100,
}}
local label_size, tick_length = 18, 8
local label_gap = 2 * label_size * 4 / 3
local axes = s:space {x = {-3, 3, 1}, y = {-3, 3, 1}, z = {-2, 2, 1},
    stroke = "#00000000", axis_width = 2, tick_length = tick_length, layer = 0, id = "axes", opacity = 0}
axes:labels {placement = "screen", size = label_size, color = "#ffffff", gap = label_gap + tick_length,
    x = {{-3, "x"}, {3, "x"}}}
axes:labels {placement = "screen", size = label_size, color = "#ffffff", gap = label_gap,
    y = {{-3, "y"}, {3, "y"}}, z = {{-2, "z"}, {2, "z"}}}
local function height(x, y, k)
    return (1 - k) * (0.25 * (x * x + y * y) - 1) + k * 0.25 * (x * x - y * y)
end
local function points(k)
    local p = {}
    for j = 0, 80 do
        local y = -2 + j / 20
        for i = 0, 80 do
            local x = -2 + i / 20
            p[#p + 1] = {x, y, height(x, y, k)}
        end
    end
    return p
end
local function surface(k, id, opacity)
    return s:surface {points = points(k), size = {81, 81}, mode = "solid_mesh", mesh = {9, 9},
        colors = {map = "viridis", range = {-1, 1}}, stroke = "#ffffff80", width = 1,
        light = {direction = {-1, 1, 2}, ambient = 0.65}, layer = 0, id = id, opacity = opacity}
end
local bowl = surface(0, "bowl", 0)
local sphere = {}
for j = 0, 12 do
    local p = pi * j / 12
    for i = 0, 24 do
        local a = 2 * pi * i / 24
        sphere[#sphere + 1] = {1 + 0.07 * math.sin(p) * math.cos(a),
            1 + 0.07 * math.sin(p) * math.sin(a), height(1, 1, 0) + 0.07 * math.cos(p)}
    end
end
local marker = s:surface {points = sphere, size = {25, 13}, fill = "#ff3030", mode = "solid",
    light = {direction = {-1, 1, 2}, ambient = 0.65}, id = "marker", layer = 0, opacity = 0}
local k = s:tracker(0)
s:bind(marker, k, function(v) return {shift = {0, 0, height(1, 1, v) - height(1, 1, 0)}} end)
s:fade(axes, 1, {duration = 1})
s:tween({{target = bowl, opacity = 1}, {target = marker, opacity = 1}}, {duration = 1})
s:wait(0.5)
local saddle = surface(1, "saddle", 1)
s:parallel(function() s:morph(bowl, saddle, {duration = 2.5, curve = "smooth"}) end,
    function() s:track(k, 1, {duration = 2.5, curve = "smooth"}) end)
s:orbit(pi / 2, {axis = {0, 0, 1}, duration = 4, curve = "linear"})
s:wait(1)
return s
