-- README "Browser": the JS example from the README, playing on a <canvas> in a page.
local scene = vis.scene {width = 1000, height = 400, fps = 20, theme = "pro_white",
  camera = {height = 6.4}}

-- Browser window chrome.
local WX, WY, WW, WH = 0, -0.1, 11.2, 5.8
scene:rectangle {center = {WX, WY}, size = {WW, WH}, corner = 0.18, fill = "#ffffff", stroke = "border", width = 2, layer = 0}
scene:rectangle {center = {WX, WY + WH / 2 - 0.35}, size = {WW - 0.04, 0.68}, corner = 0.16, fill = "surface", stroke = "#00000000", layer = 1}
for k, c in ipairs({"#ff5f57", "#febc2e", "#28c840"}) do
  scene:circle {center = {WX - WW / 2 + 0.3 + (k - 1) * 0.3, WY + WH / 2 - 0.35}, radius = 0.09, fill = c, stroke = c, layer = 2}
end
scene:rectangle {center = {WX + 0.3, WY + WH / 2 - 0.35}, size = {6.0, 0.42}, corner = 0.21, fill = "#ffffff", stroke = "border", width = 1.5, layer = 2}
scene:text {text = "localhost:8000/index.html", point = {WX + 0.3, WY + WH / 2 - 0.35}, size = 13, role = "code", fill = "muted", layer = 3, id = "url"}

local BAR_Y = WY + WH / 2 - 0.71
local loading = scene:line {from = {WX - WW / 2 + 0.02, BAR_Y}, to = {WX + WW / 2 - 0.02, BAR_Y}, stroke = "accent", width = 3, layer = 3, progress = 0}

-- The page's <canvas>: a dashed outline, and the time draw(canvas, t) is asked for.
local CW, CH, CY = 7.6, 4.0, WY - 0.4
local canvas = scene:rectangle {center = {WX, CY}, size = {CW, CH}, corner = 0.06, stroke = "border", width = 1.5, dash = {6, 5}, layer = 1, opacity = 0}
local tag = scene:text {text = "<canvas>", point = {WX - CW / 2 + 0.1, CY + CH / 2 + 0.2}, align = {0, 0.5}, size = 11, role = "code", fill = "muted", layer = 2, id = "canvas-tag"}
local clock = scene:number {value = 0, decimals = 2, prefix = "draw(canvas, ", suffix = ")", point = {WX + CW / 2 - 0.1, CY - CH / 2 + 0.25},
  align = {1, 0.5}, size = 11, role = "code", fill = "muted", layer = 3, tabular = true, id = "clock"}

-- The README scene, drawn into the canvas (same coordinates, scaled into the window).
local S, OY = 0.62, CY + 0.15
local page = scene:group {matrix = {S,0,0,0, 0,S,0,OY, 0,0,1,0, 0,0,0,1}, layer = 4}
local title = page:text {text = "Hello, viscript", point = {0, 1.2}, size = 40, align = {0.5, 0.5}, id = "title"}
local circle = page:circle {center = {-2, -0.8}, radius = 0.6, fill = "accent", stroke = "accent", id = "circle"}
local shadow = page:rectangle {center = {-2, -1.5}, size = {1.0, 0.12}, corner = 0.06, fill = "#00000022", stroke = "#00000000", id = "shadow"}
scene:set({target = title, opacity = 0})
scene:set({target = circle, opacity = 0})
scene:set({target = shadow, opacity = 0})
scene:set({target = tag, opacity = 0})
scene:set({target = clock, opacity = 0})

-- Load the page.
scene:wait(0.2)
scene:tween({target = loading, progress = 1}, {duration = 0.6, curve = "ease_out"})
scene:tween({{target = loading, opacity = 0}, {target = canvas, opacity = 1}, {target = tag, opacity = 1}, {target = clock, opacity = 1}}, {duration = 0.25})

-- The scene plays; the clock reports the sampled time.
local T0 = scene:time()
local PLAY = 0.1 + 1 + 0.5 + 1.8 + 0.6
scene:at(T0, function() scene:tween({target = clock, value = PLAY}, {duration = PLAY, curve = "linear"}) end)
scene:wait(0.1)
scene:set({target = title, opacity = 1})
scene:write(title, {duration = 1})
scene:set({target = circle, opacity = 1})
scene:parallel(
  function() scene:grow_from_center(circle, {duration = 0.5, curve = {preset = "back", strength = 1.4}}) end,
  function() scene:tween({target = shadow, opacity = 1}, {duration = 0.5}) end)
-- Three hops to the right, each with a squash on landing.
for _ = 1, 3 do
  scene:parallel(
    function() scene:tween({target = circle, shift = {4 / 3, 0}, arc = -2.2}, {duration = 0.45, curve = "ease_in_out"}) end,
    function() scene:tween({target = shadow, shift = {4 / 3, 0}}, {duration = 0.45, curve = "ease_in_out"}) end)
  scene:scale(circle, {1.18, 0.82, 1}, {duration = 0.08, curve = "ease_out"})
  scene:scale(circle, {1 / 1.18, 1 / 0.82, 1}, {duration = 0.12, curve = "ease_out"})
end
scene:indicate(title, {duration = 0.6})
scene:wait(1.0)
return scene
