-- README "Quick Start": someone edits scene.lua and the Scene re-renders beside it — type a line,
-- tweak a value, add an animation, retime it, add more, delete it again, swap the animation.
local scene = vis.scene {width = 960, height = 540, fps = 20, theme = "pro_white",
  camera = {view = "2d", height = 6}}

local L = {card = 0, guide = 1, body = 2, mark = 3, text = 4}
local U = 6 / 540                       -- world units per pixel
local ADV, PITCH = 9.19 * U, 19.36 * U  -- size-12 code advance and line pitch
local function at(x, y) return {1, 0, 0, x, 0, 1, 0, y, 0, 0, 1, 0, 0, 0, 0, 1} end

-- Every version of scene.lua, in the order it is typed.
local LINE = {
  wave = function(w) return string.format("wave = axes:plot {width = %d}", w) end,
  label = 'scene:text {text = "y = sin x"}',
  dot = "dot = axes:circle {r = 0.2}",
  anim = function(verb, d) return string.format("scene:%s(wave, %.1f)", verb, d) end,
  move = "scene:move_along(dot, wave, 2.4)",
}
local VERSIONS = {
  {LINE.wave(4)},
  {LINE.wave(12)},
  {LINE.wave(6)},
  {LINE.wave(6), LINE.anim("create", 1.2)},
  {LINE.wave(6), LINE.anim("create", 2.0)},
  {LINE.wave(6), LINE.label, LINE.dot, LINE.anim("create", 2.0), LINE.move},
  {LINE.wave(6), LINE.dot, LINE.anim("create", 2.0), LINE.move},
  {LINE.wave(6), LINE.dot, LINE.anim("fade_in", 2.0), LINE.move},
}

-- Code card. A code block centres on its origin, so each version gets a group that pins its
-- top-left corner to the same place.
local CX, CY, CW, CH = -2.85, 0.95, 4.6, 2.9
scene:rectangle {center = {CX, CY}, size = {CW, CH}, corner = 0.14, fill = "surface", stroke = "border", width = 2, layer = L.card}
scene:text {text = "scene.lua", point = {CX - CW / 2 + 0.25, CY + CH / 2 - 0.3}, align = {0, 0.5}, size = 13, role = "code", fill = "muted", layer = L.text, id = "code-name"}
scene:line {from = {CX - CW / 2, CY + CH / 2 - 0.6}, to = {CX + CW / 2, CY + CH / 2 - 0.6}, stroke = "border", width = 2, layer = L.guide}
local LEFT, TOP = CX - CW / 2 + 0.3, CY + CH / 2 - 0.82
for k, v in ipairs(VERSIONS) do
  local w = 0
  for _, s in ipairs(v) do w = math.max(w, #s) end
  v.text = table.concat(v, "\n")
  v.slot = scene:group {matrix = at(LEFT + w * ADV / 2, TOP - #v * PITCH / 2), id = "code-slot" .. k}
end

-- Scene frame: a 2D camera of height 6 drawn at half scale.
local FX, FY, FW, FH = 2.75, 0.95, 4.6, 2.9
local S = FH / 6
scene:text {text = "Scene", point = {FX - FW / 2 + 0.25, FY + FH / 2 + 0.25}, align = {0, 0.5}, size = 13, role = "code", fill = "muted", layer = L.text, id = "frame-name"}
scene:rectangle {center = {FX, FY}, size = {FW, FH}, corner = 0.14, fill = "#ffffff", stroke = "border", width = 2, layer = L.card}
local run = scene:arrow {from = {CX + CW / 2 + 0.1, CY}, to = {FX - FW / 2 - 0.1, FY}, stroke = "border", width = 2.5, tip = 9, layer = L.guide, id = "run"}

local function wpt(x) return {FX + x * S, FY - 0.15 + math.sin(x) * S} end
local X0, X1 = -4, 4
local pts = {}
for i = 0, 96 do pts[#pts + 1] = wpt(X0 + (X1 - X0) * i / 96) end
local wave = scene:plot {points = pts, stroke = "accent", width = 4, layer = L.body, progress = 0, id = "wave"}
local dot = scene:circle {center = wpt(X0), radius = 0.2 * S, fill = "warning", stroke = "warning", layer = L.mark, opacity = 0, id = "dot"}
local label = scene:text {text = "y = sin x", point = {FX, FY + FH / 2 - 0.3}, size = 14, fill = "foreground", layer = L.text, opacity = 0, id = "label"}

-- Timeline, 0 to 5 seconds.
local T_MAX = 5
local TL0, TL1, TY, TH = -5.0, 5.0, -1.35, 0.42
local function tx(t) return TL0 + (TL1 - TL0) * t / T_MAX end
scene:line {from = {TL0, TY - TH / 2 - 0.12}, to = {TL1, TY - TH / 2 - 0.12}, stroke = "border", width = 2, layer = L.guide}
for s = 0, T_MAX do
  scene:text {text = s .. "s", point = {tx(s), TY - 0.62}, size = 11, fill = "muted", id = "tick" .. s}
end
local function clip(name, color, t0, d)
  local c = {t0 = t0, d = d}
  c.box = scene:rectangle {center = {(tx(t0) + tx(t0 + d)) / 2, TY}, size = {tx(d) - tx(0) - 0.06, TH}, corner = 0.08, fill = "surface", stroke = color, width = 2, layer = L.body, opacity = 0, id = "clip-" .. name}
  c.label = scene:text {text = name, point = {(tx(t0) + tx(t0 + d)) / 2, TY}, size = 11, role = "code", fill = "foreground", layer = L.text, opacity = 0, id = "clip-label-" .. name}
  return c
end
local function stretch(x, k) return {k, 0, 0, x * (1 - k), 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1} end
local anim = clip("create", "accent", 0, 1.2)
local move = clip("move_along", "warning", 2.0, 2.4)
local cursor = scene:group {id = "cursor", opacity = 0}
cursor:line {from = {tx(0), TY - TH / 2 - 0.05}, to = {tx(0), TY + TH / 2 + 0.05}, stroke = "foreground", width = 3, layer = L.text}
cursor:text {text = "cursor", point = {tx(0), TY + TH / 2 + 0.22}, size = 10, role = "code", fill = "foreground", layer = L.text, id = "cursor-label"}
local cursor_t = 0
local function cursor_to(t, d) scene:shift(cursor, {tx(t) - tx(cursor_t), 0}, {duration = d or 0.35, curve = "smooth"}); cursor_t = t end

local head = scene:group {id = "head", opacity = 0}
head:line {from = {tx(0), TY - TH / 2 - 0.12}, to = {tx(0), TY + TH / 2 + 0.12}, stroke = "danger", width = 3, layer = L.text}

-- Caption.
local caption = scene:text {text = "Write a line, see it render", point = {0, -2.5}, size = 16, fill = "foreground", id = "caption"}
local function say(s) scene:play({target = caption, text = s}, 0.35, "smooth") end

local code = VERSIONS[1].slot:code {text = VERSIONS[1].text, language = "lua", size = 12, id = "code1", layer = L.text, opacity = 0}

-- Editing: each version is created in its slot right before the old one turns into it.
-- hit names the changed value; it punches (grows, recolours, flashes) while react re-renders the Scene.
local function edit(k, hit, react, d)
  local v = VERSIONS[k]
  local next_code = v.slot:code {text = v.text, language = "lua", size = 12, id = "code" .. k, layer = L.text}
  scene:match_transform(code, next_code, d or 0.5, "smooth", {diff = true})
  code = next_code
  local tokens = hit and next_code:search(hit, {match = "last"})
  scene:parallel(
    function() if tokens then scene:indicate(tokens, {scale = 1.9, color = "danger", duration = 0.5, curve = "ease_out"}) end end,
    function() if tokens then scene:flash(tokens[1], {color = "danger", size = 0.2, lines = 8, width = 2.5, duration = 0.4}) end end,
    function() scene:wait(0.1); scene:passing_flash(run, {duration = 0.35}) end,
    function() if react then scene:wait(0.15); react() end end)
end

-- Preview: the host re-renders from t = 0 with the current timeline.
local function preview(st)
  local total = st.d + (st.move and move.d or 0)
  local reset = {{target = head, opacity = 1}}
  if st.verb == "create" then
    reset[#reset + 1] = {target = wave, progress = 0, opacity = 1}
  else
    reset[#reset + 1] = {target = wave, progress = 1, opacity = 0}
  end
  scene:play(reset, 0.15)
  if st.move then scene:save(dot) end
  scene:parallel(
    function() scene:shift(head, {tx(total) - tx(0), 0}, {duration = total, curve = "linear"}) end,
    function()
      if st.verb == "create" then scene:play({target = wave, progress = 1}, st.d, "smooth")
      else scene:play({target = wave, opacity = 1}, st.d, "smooth") end
    end,
    function()
      if not st.move then return end
      scene:wait(st.d)
      scene:move_along(dot, wave, {duration = move.d, curve = "ease_in_out"})
    end)
  scene:wait(0.5)
  scene:play({target = head, opacity = 0}, 0.25)
  scene:shift(head, {tx(0) - tx(total), 0}, 0.01)
  if st.move then scene:restore(dot, 0.3) end
end

-- 1. Type the first line; the wave renders.
scene:wait(0.4)
scene:play({target = code, opacity = 1}, 0.01)
scene:write(code, 0.9)
scene:passing_flash(run, {duration = 0.4})
scene:play({target = wave, progress = 1}, 0.5, "ease_out")
scene:wait(0.6)

-- 2. Tweak a value: width is in pixels.
say("Tweak a value")
edit(2, "12", function() scene:play({target = wave, width = 12}, 0.4, "ease_out") end)
scene:wait(0.6)
edit(3, "6", function() scene:play({target = wave, width = 6}, 0.4, "ease_out") end)
scene:wait(0.5)

-- 3. Add an animation; it lands on the timeline cursor.
say("Add an animation")
scene:fade_in(cursor, {duration = 0.25})
edit(4, "create", function() scene:play({{target = anim.box, opacity = 1}, {target = anim.label, opacity = 1}}, 0.3, "ease_out") end)
cursor_to(1.2)
preview {verb = "create", d = 1.2}

-- 4. Retime it.
say("Retime it")
edit(5, "2.0", function() scene:parallel(
  function() scene:play({{target = anim.box, transform = stretch(TL0, (tx(2.0) - tx(0) - 0.06) / (tx(1.2) - tx(0) - 0.06))},
    {target = anim.label, shift = {(tx(2.0) - tx(1.2)) / 2, 0}}}, 0.45, "smooth") end,
  function() cursor_to(2.0, 0.45) end) end)
preview {verb = "create", d = 2.0}

-- 5. Add more to the Scene.
say("Add more to the Scene")
edit(6, nil, nil, 0.8)
scene:play({{target = label, opacity = 1}, {target = dot, opacity = 1}}, 0.35, "ease_out")
scene:play({{target = move.box, opacity = 1}, {target = move.label, opacity = 1}}, 0.3, "ease_out")
cursor_to(4.4)
preview {verb = "create", d = 2.0, move = true}

-- 6. Remove what you don't need.
say("Delete what you don't need")
edit(7)
scene:play({target = label, opacity = 0}, 0.35, "smooth")
scene:wait(0.4)

-- 7. Swap the animation call.
say("Swap the animation")
edit(8, "fade_in", function() scene:play({target = anim.label, text = "fade_in"}, 0.35, "smooth") end)
preview {verb = "fade_in", d = 2.0, move = true}

say("Lua → Scene")
scene:play({target = cursor, opacity = 0}, 0.3)
scene:wait(1.5)
return scene
