-- README "Embed it where you need it": one Lua scene and one renderer, playing the same frames
-- in a native app, a web page and VS Code.
local scene = vis.scene {width = 1200, height = 400, fps = 20, theme = "pro_white",
  camera = {target = {0.3, -0.3}, height = 6.67}}

-- Plane text scales with the camera. This factor includes the clip's existing 4/3 label
-- enlargement as well as its reference-height mapping; it is not just a points conversion.
local PLANE_SIZE_SCALE = (4 / 3) * 6.67 / 400
local function plane(o) o.orientation = "plane"; o.size = (o.size or 14) * PLANE_SIZE_SCALE; return o end
local function TEXT(o) return scene:text(plane(o)) end
local function CODE(o) return scene:code(plane(o)) end
local INK = "foreground"

local function box(x, y, w, h, title, sub, id)
  scene:rectangle {center = {x, y}, size = {w, h}, fill = "#ffffff", stroke = INK, width = 2.5, layer = 1, id = id}
  TEXT {text = title, point = {x, y + 0.15}, size = 13, role = "code", fill = INK, layer = 2, id = id .. ":t"}
  TEXT {text = sub, point = {x, y - 0.22}, size = 10, fill = "muted", layer = 2, id = id .. ":s"}
end

-- Source and renderer.
local SX = -7.3
box(SX, 1.1, 2.8, 1.0, "scene.lua", "one scene", "src")
box(SX, -1.1, 2.8, 1.0, "ThorVG", "one renderer", "tvg")
local a1 = scene:arrow {from = {SX, 0.6}, to = {SX, -0.6}, stroke = INK, width = 2, tip = 9, layer = 1, id = "a-src"}

-- Hosts.
local HW, HH, HY = 4.3, 3.7, 0.15
local HOSTS = {{x = -2.75, name = "Native app", sub = "C library"}, {x = 1.85, name = "Web page", sub = "WebAssembly"}, {x = 6.45, name = "VS Code", sub = "preview"}}
local FX = SX + 1.4 + 0.55
local routes = {}
for i, h in ipairs(HOSTS) do
  routes[i] = scene:route {points = {{SX + 1.4, -1.1}, {FX, -1.1}, {FX, -2.8}, {h.x, -2.8}, {h.x, HY - HH / 2}}, stroke = INK, width = 2, tip = 9, layer = 1, id = "r" .. i}
end
local frames = {}
for i, h in ipairs(HOSTS) do
  local x, top = h.x, HY + HH / 2
  scene:rectangle {center = {x, HY}, size = {HW, HH}, fill = "#ffffff", stroke = INK, width = 2.5, layer = 1, id = "host" .. i}
  scene:rectangle {center = {x, top - 0.25}, size = {HW, 0.5}, fill = "surface", stroke = INK, width = 2.5, layer = 2}
  local cx, cw = x, HW - 0.4      -- the region the scene draws into
  if i == 1 then
    for k = 0, 2 do scene:rectangle {center = {x - HW / 2 + 0.25 + k * 0.22, top - 0.25}, size = {0.12, 0.12}, fill = INK, stroke = INK, layer = 3} end
    TEXT {text = "my-app", point = {x, top - 0.25}, size = 10, role = "code", fill = INK, layer = 3, id = "chrome1"}
  elseif i == 2 then
    scene:rectangle {center = {x, top - 0.25}, size = {HW - 1.0, 0.3}, fill = "#ffffff", stroke = INK, width = 1.5, layer = 3}
    TEXT {text = "localhost:8000", point = {x, top - 0.25}, size = 10, role = "code", fill = "muted", layer = 4, id = "chrome2"}
  else
    TEXT {text = "scene.lua", point = {x - HW / 2 + 0.2, top - 0.25}, align = {0, 0.5}, size = 10, role = "code", fill = INK, layer = 3, id = "chrome3"}
    scene:line {from = {x - 0.55, top - 0.5}, to = {x - 0.55, HY - HH / 2}, stroke = INK, width = 1.5, layer = 2}
    for k = 0, 5 do
      scene:rectangle {center = {x - HW / 2 + 0.25 + (0.25 + (k % 3) * 0.15), top - 0.85 - k * 0.32}, size = {0.5 + (k % 3) * 0.3, 0.1}, fill = "border", stroke = "#00000000", layer = 2}
    end
    cx, cw = x + 1.05, 1.95
  end
  frames[i] = {cx = cx, cw = cw}
  TEXT {text = h.name, point = {x - HW / 2, HY - HH / 2 - 0.3}, align = {0, 0.5}, size = 11, role = "code", fill = INK, id = "host" .. i .. ":name"}
  TEXT {text = h.sub, point = {x + HW / 2, HY - HH / 2 - 0.3}, align = {1, 0.5}, size = 10, fill = "muted", id = "host" .. i .. ":sub"}
end

-- The same scene in each host: a wave drawn and a dot riding it.
local waves, dots = {}, {}
for i, f in ipairs(frames) do
  local w, cy = f.cw * 0.42, HY - 0.25
  local x0, x1 = f.cx - w, f.cx + w
  local amp = 0.55 * (f.cw / (HW - 0.4))
  waves[i] = scene:plot {fn = function(t) return cy + amp * math.sin((t - x0) / (x1 - x0) * 2 * math.pi) end, x_range = {x0, x1, 0.01},
    stroke = "accent", width = 3.5, layer = 3, progress = 0, id = "wave" .. i}
  dots[i] = scene:circle {center = {x0, cy}, radius = 0.1 * f.cw / 3.9 + 0.04, fill = "warning", stroke = "warning", layer = 4, opacity = 0, id = "dot" .. i}
end

-- Timeline: frames leave the renderer for all three hosts; the hosts play in lockstep.
local srcBox, tvgBox = scene:find("src"), scene:find("tvg")
scene:focus({srcBox, tvgBox}, {padding = 0.1, duration = 0.01})
scene:wait(0.5)
scene:passing_flash(a1, {duration = 0.4})
scene:parallel(
  function() scene:focus(nil, {duration = 0.9, curve = "ease_in_out"}) end,
  table.unpack((function()
    local fs = {}
    for i = 1, #routes do fs[i] = function() scene:wait(0.2); scene:passing_flash(routes[i], {duration = 0.7}) end end
    return fs
  end)()))
local fs = {}
for i = 1, #waves do
  fs[#fs + 1] = function() scene:create(waves[i], {duration = 1.0, curve = "ease_out"}) end
end
scene:parallel(table.unpack(fs))
local on = {}
for i = 1, #dots do on[i] = {target = dots[i], opacity = 1} end
scene:tween(on, 0.2)
local ride = {}
for i = 1, #dots do ride[i] = function() scene:move_along(dots[i], waves[i], {duration = 1.8, curve = "ease_in_out"}) end end
scene:parallel(table.unpack(ride))
-- Push in on the VS Code preview, then back out.
scene:focus(scene:find("host3"), {padding = 0.18, duration = 0.6, curve = "ease_in_out"})
local back = {}
for i = 1, #dots do back[i] = function() scene:move_along(dots[i], waves[i], {duration = 1.8, curve = {preset = "ease_in_out", reverse = true}}) end end
scene:parallel(table.unpack(back))
scene:focus(nil, {duration = 0.7, curve = "ease_in_out"})
scene:wait(0.6)
return scene
