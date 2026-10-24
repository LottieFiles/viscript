-- README "Render": one scene.lua, sampled by time into a PNG frame, a GIF and an MP4.
local scene = vis.scene {width = 1000, height = 360, fps = 20, theme = "pro_white",
  camera = {target = {-0.55, 0}, height = 6}}

local DUR, STILL = 5.0, 3.8      -- scene length and the `--time 3.8` frame
local THUMB = 0.8                -- wave scale inside an output card
local function wave_point(cx, cy, u) return {cx + (u - 0.5) * 2.0 * THUMB, cy + 0.45 * THUMB * math.sin(u * 2 * math.pi)} end

-- Source card: file tab, code, timeline.
local SX, SW, SH = -5.3, 4.4, 4.2
scene:rectangle {center = {SX, 0}, size = {SW, SH}, corner = 0.16, fill = "surface", stroke = "border", width = 2, layer = 0}
scene:text {text = "scene.lua", point = {SX - SW / 2 + 0.3, SH / 2 - 0.4}, align = {0, 0.5}, size = 15, role = "code", fill = "muted", layer = 2, id = "src-name"}
scene:line {from = {SX - SW / 2, SH / 2 - 0.75}, to = {SX + SW / 2, SH / 2 - 0.75}, stroke = "border", width = 2, layer = 1}
local listing = scene:code {text = "local wave = axes:plot {...}\nscene:create(wave)\nscene:move_along(dot, wave)", language = "lua", size = 11, id = "src-code", layer = 2}
listing:move_to({SX, 0.45})
local TL0, TL1, TLY = SX - 1.75, SX + 1.75, -1.35
local function tx(t) return TL0 + (TL1 - TL0) * t / DUR end
scene:line {from = {TL0, TLY}, to = {TL1, TLY}, stroke = "border", width = 4, layer = 1}
local played = scene:line {from = {TL0, TLY}, to = {TL1, TLY}, stroke = "accent", width = 4, layer = 2, progress = 0}
scene:text {text = "0s", point = {TL0, TLY - 0.38}, size = 11, fill = "muted", id = "t0"}
scene:text {text = "5s", point = {TL1, TLY - 0.38}, size = 11, fill = "muted", id = "t1"}
local mark = scene:line {from = {tx(STILL), TLY - 0.2}, to = {tx(STILL), TLY + 0.2}, stroke = "warning", width = 3, layer = 3}
scene:text {text = "3.8", point = {tx(STILL), TLY - 0.38}, size = 11, fill = "warning", id = "still-label"}
local head = scene:circle {center = {TL0, TLY}, radius = 0.11, fill = "accent", stroke = "accent", layer = 4, id = "playhead"}

-- Command chip.
local CX = -1.45
scene:arrow {from = {SX + SW / 2 + 0.1, 0}, to = {CX - 1.05, 0}, stroke = "border", width = 2.5, tip = 9, layer = 1}
local chip = scene:rectangle {center = {CX, 0}, size = {1.95, 0.6}, corner = 0.3, fill = "foreground", stroke = "foreground", layer = 2}
scene:text {text = "render", point = {CX, 0}, size = 14, role = "code", fill = "background", layer = 3, id = "cmd"}

-- Outputs.
local OX = 2.75
local OUT = {
  {name = "frame.png", note = "--time 3.8", y = 1.5, color = "warning"},
  {name = "scene.gif", note = "--fps 20", y = 0, color = "accent"},
  {name = "scene.mp4", note = "--fps 30", y = -1.5, color = "secondary"},
}
for k, o in ipairs(OUT) do
  scene:route {points = {{CX + 1.0, 0}, {CX + 1.5, 0}, {CX + 1.5, o.y}, {OX - 1.95, o.y}}, stroke = "border", width = 2, tip = 9, layer = 1}
  local tx0 = OX - 0.75
  o.card = scene:rectangle {center = {OX + 0.5, o.y}, size = {4.7, 1.25}, corner = 0.12, fill = "#ffffff", stroke = "border", width = 2, layer = 2}
  o.frame = scene:rectangle {center = {tx0, o.y}, size = {2.0, 0.95}, corner = 0.08, fill = "surface", stroke = "#00000000", layer = 3}
  scene:text {text = o.name, point = {OX + 0.55, o.y + 0.2}, align = {0, 0.5}, size = 15, role = "code", fill = "foreground", layer = 3, id = "out" .. k}
  scene:text {text = o.note, point = {OX + 0.55, o.y - 0.22}, align = {0, 0.5}, size = 11, role = "code", fill = "muted", layer = 3, id = "note" .. k}
  local last = (k == 1) and STILL / DUR or 1
  local pts = {}
  for i = 0, 48 do pts[#pts + 1] = wave_point(tx0, o.y, i / 48 * last) end
  o.plot = scene:plot {points = pts, stroke = o.color, width = 3, layer = 4, progress = (k == 1) and 1 or 0, opacity = (k == 1) and 0 or 1}
  o.dot = scene:circle {center = wave_point(tx0, o.y, (k == 1) and last or 0), radius = 0.075, fill = o.color, stroke = o.color, layer = 5, opacity = (k == 1) and 0 or 1}
end
local bar = scene:line {from = {OX - 1.75, -1.5 - 0.47}, to = {OX + 0.25, -1.5 - 0.47}, stroke = "secondary", width = 3, layer = 4, progress = 0}
local shutter = scene:rectangle {center = {OX - 0.75, 1.5}, size = {2.0, 0.95}, corner = 0.08, fill = "#ffffff", stroke = "#00000000", layer = 6, opacity = 0}

-- Timeline: the playhead runs; GIF and MP4 keep every frame; the PNG keeps one instant.
local linear = {duration = DUR, curve = "linear"}
scene:wait(0.4)
scene:parallel(
  function() scene:shift(head, {TL1 - TL0, 0}, linear) end,
  function() scene:tween({target = played, progress = 1}, {duration = DUR, curve = "linear"}) end,
  function() scene:create(OUT[2].plot, linear) end,
  function() scene:create(OUT[3].plot, linear) end,
  function() scene:move_along(OUT[2].dot, OUT[2].plot, linear) end,
  function() scene:move_along(OUT[3].dot, OUT[3].plot, linear) end,
  function() scene:tween({target = bar, progress = 1}, {duration = DUR, curve = "linear"}) end,
  function()
    scene:wait(STILL)
    scene:tween({{target = shutter, opacity = 1}, {target = OUT[1].card, stroke = "warning"}, {target = mark, width = 5}}, {duration = 0.06, curve = "linear"})
    scene:tween({{target = OUT[1].plot, opacity = 1}, {target = OUT[1].dot, opacity = 1}}, {duration = 0.01})
    scene:tween({target = shutter, opacity = 0}, {duration = 0.4, curve = "ease_out"})
    scene:tween({{target = OUT[1].card, stroke = "border"}, {target = mark, width = 3}}, {duration = 0.3})
  end)
scene:wait(1.2)
return scene
