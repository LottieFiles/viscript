-- README "Scenes as code, written with AI": a prompt becomes a scene script; the script is a
-- retained object tree; animation changes properties; the agent checks layout and renders.
local scene = vis.scene {width = 1200, height = 420, fps = 20, theme = "pro_white",
  camera = {target = {0, 0.1}, height = 7.0}}

-- Text is set in world units (plane) so it scales with the camera moves below; sizes are
-- authored in points at the opening camera and converted here.
local PT = (4 / 3) * 7.0 / 420
local function plane(o) o.orientation = "plane"; o.size = (o.size or 14) * PT; return o end
local function TEXT(o) return scene:text(plane(o)) end
local function CODE(o) return scene:code(plane(o)) end
local INK = "foreground"

local function panel(x, w, title, id)
  local box = scene:rectangle {center = {x, 0}, size = {w, 5.2}, fill = "#ffffff", stroke = INK, width = 2.5, layer = 0, id = id}
  local t = TEXT {text = title, point = {x - w / 2, 2.95}, align = {0, 0.5}, size = 12, role = "code", fill = INK, id = id .. ":title"}
  return {box, t}
end

-- 1. Agent.
local AX, AW = -7.25, 5.0
local agentP = panel(AX, AW, "AGENT", "agent")
local function bubble(y, text, id, me)
  local box = scene:rectangle {center = {AX + (me and 0.2 or -0.2), y}, size = {AW - 0.8, 0.62}, fill = me and "surface" or "#ffffff", stroke = INK, width = 1.5, layer = 1, id = id}
  local t = TEXT {text = text, point = {AX + (me and 0.2 or -0.2) - (AW - 0.8) / 2 + 0.18, y}, align = {0, 0.5}, size = 11, fill = INK, layer = 2, id = id .. ":text"}
  return box, t
end
local p1, p1t = bubble(1.95, "Animate a ball rolling right.", "prompt1", true)
local r1, r1t = bubble(1.15, "Drafting scene.lua …", "reply1", false)
local p2, p2t = bubble(0.15, "Make it orange.", "prompt2", true)
local r2, r2t = bubble(-0.65, "Updated fill.", "reply2", false)

-- Agent loop: draft → check → render.
local STEPS = {"draft", "check", "render"}
local pills = {}
for i, s in ipairs(STEPS) do
  local x = AX - 1.6 + (i - 1) * 1.6
  pills[i] = scene:rectangle {center = {x, -2.0}, size = {1.4, 0.5}, fill = "#ffffff", stroke = INK, width = 2, layer = 1, id = "pill-" .. s}
  TEXT {text = s, point = {x, -2.0}, size = 11, role = "code", fill = INK, layer = 2, id = "pill-" .. s .. ":t"}
end

-- 2. Script.
local CX, CW = -0.9, 6.6
local codeP = panel(CX, CW, "scene.lua", "code")
local SRC = "local ball = scene:circle {\n  center = {-2, 0}, radius = 0.5,\n  fill = \"accent\",\n}\nscene:shift(ball, {4, 0}, 1)"
local listing = CODE {text = SRC, language = "lua", size = 10, id = "src", layer = 2}
listing:move_to({CX - 0.2, 1.0})

-- 3. Retained tree + preview.
local TX, TW = 6.2, 6.2
local sceneP = panel(TX, TW, "SCENE", "scene")
local root = scene:rectangle {center = {TX - 2.3, 1.95}, size = {0.2, 0.2}, fill = "#ffffff", stroke = INK, width = 2, layer = 2}
TEXT {text = "scene", point = {TX - 2.1, 1.95}, align = {0, 0.5}, size = 11, role = "code", fill = INK, layer = 2, id = "t-scene"}
scene:line {from = {TX - 2.3, 1.85}, to = {TX - 2.3, 1.45}, stroke = INK, width = 1.5, layer = 1}
scene:line {from = {TX - 2.3, 1.45}, to = {TX - 1.95, 1.45}, stroke = INK, width = 1.5, layer = 1}
local leaf = scene:rectangle {center = {TX - 1.85, 1.45}, size = {0.2, 0.2}, fill = "#ffffff", stroke = INK, width = 2, layer = 2, id = "leaf"}
TEXT {text = "circle  ball", point = {TX - 1.65, 1.45}, align = {0, 0.5}, size = 11, role = "code", fill = INK, layer = 2, id = "t-ball"}
local center = TEXT {text = "center = {-2, 0}", point = {TX - 1.65, 1.0}, align = {0, 0.5}, size = 11, role = "code", fill = "muted", layer = 2, id = "prop-center"}
local fill = TEXT {text = "fill   = accent", point = {TX - 1.65, 0.6}, align = {0, 0.5}, size = 11, role = "code", fill = "muted", layer = 2, id = "prop-fill"}
local PY = -1.3
scene:rectangle {center = {TX, PY}, size = {TW - 0.6, 2.2}, fill = "surface", stroke = INK, width = 1.5, layer = 1, id = "preview"}
local ball = scene:circle {center = {TX - 1.6, PY}, radius = 0.42, fill = "accent", stroke = INK, width = 2, layer = 3, id = "ball", opacity = 0}
local okText = TEXT {text = "layout ok", point = {TX + TW / 2 - 0.45, PY + 0.85}, align = {1, 0.5}, size = 10, role = "code", fill = "success", layer = 3, opacity = 0, id = "audit"}

-- Hide everything that the story reveals.
for _, h in ipairs({r1, r1t, p2, p2t, r2, r2t}) do scene:set({target = h, opacity = 0}) end
scene:set({target = listing, opacity = 0})
local function pill(i, on) return {target = pills[i], fill = on and "foreground" or "#ffffff"} end
local pillText = {}
for i, s in ipairs(STEPS) do pillText[i] = scene:find("pill-" .. s .. ":t") end
local function pillOn(i, on) return {{target = pills[i], fill = on and INK or "#ffffff"}, {target = pillText[i], fill = on and "background" or INK}} end
local function seq(t) local o = {} for _, l in ipairs(t) do for _, d in ipairs(l) do o[#o + 1] = d end end return o end

-- Camera opens on the agent.
scene:focus(agentP, {padding = 0.08, duration = 0.01})
-- Beat 1: prompt → draft.
scene:wait(0.5)
scene:play(seq({{{target = r1, opacity = 1}, {target = r1t, opacity = 1}}, pillOn(1, true)}), 0.3, "ease_out")
scene:focus({agentP[1], agentP[2], codeP[1], codeP[2]}, {padding = 0.08, duration = 0.6, curve = "ease_in_out"})
scene:set({target = listing, opacity = 1})
scene:write(listing, 1.4)
scene:play({{target = leaf, fill = "accent"}, {target = ball, opacity = 1}}, 0.3, "ease_out")
scene:focus({codeP[1], codeP[2], sceneP[1], sceneP[2]}, {padding = 0.08, duration = 0.6, curve = "ease_in_out"})
-- Beat 2: check.
scene:play(seq({pillOn(1, false), pillOn(2, true)}), 0.25)
scene:play({target = okText, opacity = 1}, 0.25, "ease_out")
scene:wait(0.3)
-- Beat 3: render — changing a property animates it; push in on the scene.
scene:play(seq({pillOn(2, false), pillOn(3, true)}), 0.25)
scene:focus(sceneP, {padding = 0.08, duration = 0.6, curve = "ease_in_out"})
scene:parallel(
  function() scene:shift(ball, {3.2, 0}, 1.0, "ease_in_out") end,
  function() scene:play({target = center, text = "center = { 2, 0}", fill = "accent"}, 1.0, "ease_in_out") end)
scene:play({{target = center, fill = "muted"}}, 0.2)
scene:play(seq({pillOn(3, false)}), 0.2)
-- Beat 4: refine; pull back to see the whole loop.
scene:focus(nil, {duration = 0.8, curve = "ease_in_out"})
scene:play({{target = p2, opacity = 1}, {target = p2t, opacity = 1}}, 0.3, "ease_out")
local edited = scene:edit(listing, {replace = "\"accent\"", with = "\"#ff6a00\""})
scene:play(seq({{{target = r2, opacity = 1}, {target = r2t, opacity = 1}}, pillOn(3, true)}), 0.25)
scene:play({{target = ball, fill = "#ff6a00"}, {target = fill, text = "fill   = #ff6a00", fill = "#ff6a00"}}, 0.4, "ease_out")
scene:play(seq({pillOn(3, false), {{target = fill, fill = "muted"}}}), 0.3)
scene:wait(1.2)
return scene
