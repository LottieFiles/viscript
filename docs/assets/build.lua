-- README "Build": optional modules plug into the core at configure time, then compile.
-- Module names follow meson_options.txt; interactive is one option over four internal modules.
local scene = vis.scene {width = 1200, height = 420, fps = 20, theme = "pro_white",
  camera = {target = {0, 0.1}, height = 7.6}}
local INK = "foreground"

-- Text is set in world units (plane) so it scales with the camera moves below.
local PT = (4 / 3) * 7 / 420
local function plane(o) o.orientation = "plane"; o.size = (o.size or 14) * PT; return o end
local function TEXT(o) return scene:text(plane(o)) end

-- Terminal.
local TX, TW, TH = -5.2, 9.0, 4.6
local term = scene:rectangle {center = {TX, 0}, size = {TW, TH}, fill = INK, stroke = INK, width = 2.5, layer = 0, id = "terminal"}
local termTitle = TEXT {text = "TERMINAL", point = {TX - TW / 2, TH / 2 + 0.3}, align = {0, 0.5}, size = 12, role = "code", fill = INK, id = "terminal:title"}
local LX = TX - TW / 2 + 0.35
local function line(y, s, id, color)
  return TEXT {text = s, point = {LX, y}, align = {0, 0.5}, size = 11, role = "code", fill = color or "background", layer = 2, id = id}
end
local cmd1 = line(1.5, "$ meson setup build -Dmodules=diagram,tex", "cmd1")
local cmd2 = line(0.8, "$ meson compile -C build", "cmd2")
local BAR_Y, BAR_W = 0.05, TW - 0.7
scene:rectangle {center = {TX, BAR_Y}, size = {BAR_W, 0.32}, stroke = "muted", width = 1.5, layer = 1}
local bar = scene:rectangle {center = {LX + 0.01, BAR_Y}, size = {0.02, 0.32}, fill = "success", stroke = "#00000000", layer = 2, opacity = 0, id = "bar"}
local done = line(-0.7, "build/viscript", "done", "success")
for _, h in ipairs({cmd1, cmd2, done}) do scene:set({target = h, opacity = 0}) end

-- Core and modules.
local CX, CY = 5.3, 0
local core = scene:rectangle {center = {CX, CY}, size = {3.8, 1.3}, fill = "#ffffff", stroke = INK, width = 3, layer = 2, id = "core"}
TEXT {text = "core", point = {CX, CY + 0.2}, size = 15, role = "code", fill = INK, layer = 3, id = "core:t"}
TEXT {text = "Scene · Lua · ThorVG · PNG", point = {CX, CY - 0.25}, size = 10, fill = "muted", layer = 3, id = "core:s"}
TEXT {text = "MODULES", point = {CX - 4.25, 3.1}, align = {0, 0.5}, size = 12, role = "code", fill = INK, id = "modules:title"}
local MODS = {"diagram", "tex", "interactive", "audio"}
local SUBS = {[3] = "ui · input · states · runtime"}
local SLOTS = {{-1.55, 1.95}, {1.55, 1.95}, {-1.75, -1.95}, {2.35, -1.95}}
local WIDTHS = {2.6, 2.6, 5.0, 2.0}
local tiles, wires, labels, subs = {}, {}, {}, {}
for i, name in ipairs(MODS) do
  local x, y = CX + SLOTS[i][1], CY + SLOTS[i][2]
  local w, h = WIDTHS[i], SUBS[i] and 0.95 or 0.7
  local edge = (y > CY) and (CY + 0.65) or (CY - 0.65)
  local tip = (y > CY) and (y - h / 2) or (y + h / 2)
  wires[i] = scene:line {from = {x * 0.35 + CX * 0.65, edge}, to = {x, tip}, stroke = INK, width = 2, layer = 1, progress = 0, id = "wire-" .. name}
  scene:rectangle {center = {x, y}, size = {w, h}, fill = "#ffffff", stroke = "border", width = 2, dash = {5, 4}, layer = 2, id = "slot-" .. name}
  tiles[i] = scene:rectangle {center = {x, y}, size = {w, h}, fill = "#e8f0fb", stroke = INK, width = 2.5, layer = 2, opacity = 0, id = "mod-" .. name}
  local ly = SUBS[i] and (y + 0.17) or y
  labels[i] = TEXT {text = name, point = {x, ly}, size = 13, role = "code", fill = "muted", layer = 3, id = "mod-" .. name .. ":t"}
  if SUBS[i] then
    subs[i] = TEXT {text = SUBS[i], point = {x, y - 0.22}, size = 9, role = "code", fill = "muted", layer = 3, id = "mod-" .. name .. ":s"}
  end
end
local function attach(i, d)
  return function()
    scene:parallel(
      function() scene:create(wires[i], {duration = d, curve = "ease_out"}) end,
      function()
        local t = {{target = tiles[i], opacity = 1}, {target = labels[i], fill = INK}}
        if subs[i] then t[#t + 1] = {target = subs[i], fill = INK} end
        scene:play(t, d, "ease_out")
      end)
  end
end

-- Camera opens on the terminal.
local CAM = {duration = 0.7, curve = "ease_in_out"}
scene:focus({term, termTitle}, {padding = 0.2, duration = 0.01})
scene:wait(0.4)
scene:set({target = cmd1, opacity = 1})
scene:write(cmd1, 1.0)
-- Configure: the named modules attach.
scene:focus({core, tiles[1], tiles[2], tiles[3], tiles[4]}, {padding = 0.08, duration = 0.7, curve = "ease_in_out"})
scene:parallel(attach(1, 0.45), function() scene:wait(0.12); attach(2, 0.45)() end)
scene:wait(0.4)
-- Reconfigure with all modules: the rest attach in a wave.
scene:focus(nil, CAM)
scene:play({target = cmd1, text = "$ meson setup build -Dmodules=all", retype = true}, 0.7)
local wave = {}
for k, i in ipairs({3, 4}) do wave[k] = function() scene:wait((k - 1) * 0.1); attach(i, 0.4)() end end
scene:parallel(table.unpack(wave))
scene:wait(0.3)
-- Compile.
scene:set({target = cmd2, opacity = 1})
scene:write(cmd2, 0.6)
scene:play({target = bar, opacity = 1}, 0.1)
scene:parallel(
  function() scene:play({target = bar, size = {BAR_W, 0.32}, shift = {BAR_W / 2, 0}}, 1.2, "ease_in_out") end,
  function() scene:wait(0.9); scene:play({target = core, stroke = "success", width = 3.5}, 0.3, "ease_out") end)
scene:set({target = done, opacity = 1})
scene:write(done, 0.4)
scene:play({target = core, stroke = INK, width = 3}, 0.4)
scene:wait(1.2)
return scene
