-- README demo: a command typed in a terminal produces a result on the right.
-- One loop, ink-on-white, camera opens on the cause and widens to the effect.
local scene = vis.scene {width = 1200, height = 420, fps = 20, loop = true, theme = "pro_white",
  camera = {target = {-4.6, 0.25}, height = 5}}
local INK = "foreground"

-- The configured camera is the opening close-up on the terminal; focus() widens to the
-- whole stage and focus(nil) returns, so the loop starts and ends on the same frame.
-- Plane text keeps its size relative to the frame while the camera moves.
-- Sizes are points at the wide shot (height 7 over 420 px).
local PT = (4 / 3) * 7 / 420
local function plane(o) o.orientation = "plane"; o.size = (o.size or 14) * PT; return o end
local function TEXT(o) return scene:text(plane(o)) end

-- Cause: a dark terminal panel with a mono caps label above it.
local TX, TW, TH = -4.6, 7.6, 3.8
local term = scene:rectangle {center = {TX, 0}, size = {TW, TH}, fill = INK, stroke = INK, width = 2.5, layer = 0, id = "terminal"}
local termTitle = TEXT {text = "TERMINAL", point = {TX - TW / 2, TH / 2 + 0.3}, align = {0, 0.5}, size = 12, role = "code", fill = INK, id = "terminal:title"}
local cmd = TEXT {text = "$ viscript render scene.lua", point = {TX - TW / 2 + 0.35, 1.1}, align = {0, 0.5},
  size = 11, role = "code", fill = "background", layer = 2, opacity = 0, id = "cmd"}
local ok = TEXT {text = "wrote scene.gif", point = {TX - TW / 2 + 0.35, 0.4}, align = {0, 0.5},
  size = 11, role = "code", fill = "success", layer = 2, opacity = 0, id = "ok"}

-- Effect: a white panel with thick ink border; the subject inside moves, not the text.
local RX, RW = 4.4, 4.6
local out = scene:rectangle {center = {RX, 0}, size = {RW, TH}, fill = "#ffffff", stroke = INK, width = 2.5, layer = 0, id = "output"}
local outTitle = TEXT {text = "SCENE.GIF", point = {RX - RW / 2, TH / 2 + 0.3}, align = {0, 0.5}, size = 12, role = "code", fill = INK, id = "output:title"}
local slot = scene:rectangle {center = {RX, 0}, size = {RW - 0.8, TH - 0.8}, stroke = "border", width = 1.5, dash = {6, 5}, layer = 1, id = "slot"}
local dot = scene:circle {center = {RX - 1.2, -0.6}, radius = 0.4, fill = "accent", stroke = INK, width = 2.5, layer = 2, opacity = 0, id = "dot"}
local wire = scene:arrow {from = {TX + TW / 2 + 0.15, 0}, to = {RX - RW / 2 - 0.15, 0}, stroke = INK, width = 2, tip = 10, layer = 1, progress = 0, id = "wire"}

-- The effect side waits off-frame while the camera is on the terminal; hide it so
-- nothing is clipped at the frame edge.
local effect = {out, outTitle, slot, wire}
for _, h in ipairs(effect) do scene:set({target = h, opacity = 0}) end
local function fade(hs, v, d)
  local t = {}
  for k, h in ipairs(hs) do t[k] = {target = h, opacity = v} end
  scene:tween(t, d)
end

local CAM = {duration = 0.7, curve = "ease_in_out"}
scene:wait(0.4)
scene:set({target = cmd, opacity = 1})
scene:write(cmd, 1.0)
scene:parallel(function() scene:focus({term, termTitle, out, outTitle}, {padding = 0.06, duration = 0.7, curve = "ease_in_out"}) end, function() scene:wait(0.3); fade({out, outTitle, slot}, 1, 0.4) end)
scene:set({target = wire, opacity = 1})
scene:create(wire, {duration = 0.4, curve = "ease_out"})
scene:set({target = dot, opacity = 1})
scene:grow_from_center(dot, {duration = 0.4, curve = {preset = "back", strength = 1.4}})
scene:tween({target = dot, shift = {2.4, 0}, arc = -2}, 0.6, "ease_in_out")
scene:set({target = ok, opacity = 1})
scene:write(ok, 0.4)
scene:wait(1.0)
-- Return to the opening pose so the loop seam is invisible.
fade({cmd, ok, dot, wire, out, outTitle, slot}, 0, 0.3)
scene:set({target = dot, shift = {-2.4, 0}})
scene:set({target = wire, progress = 0})
scene:focus(nil, CAM)
return scene
