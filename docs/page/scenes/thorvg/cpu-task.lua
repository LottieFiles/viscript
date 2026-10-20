-- Ported from gitpage/static/tmath/gpu-deferred/task-sw.lua.
-- Trace data and render colors are preserved; structural colors follow the site theme.
-- One SwShapeTask through update() / draw() / sync() (cpu_engine/tvgSwRenderer.cpp, tvgTaskScheduler.h).
-- The worker turns the path into shape.rle; the caller copies the spans into the surface.


local FONT = "CMUTypewriter-Regular"
local scene = vis.scene {width = 800, height = 450, fps = 25, loop = true,
  theme = {preset = "pro_black",
    text = {h1 = {font = FONT}, h2 = {font = FONT}, h3 = {font = FONT}, text = {font = FONT}, code = {font = FONT}}},
  camera = {height = 4.5}}

local C = {paper = "background", surface = "surface", soft = "border", hair = "border", faint = "muted",
  inksoft = "foreground", ink = "foreground", accent = "accent", light = "accent", wash = "surface"}

local function W(px, py) return {(px - 400) / 100, (225 - py) / 100} end
local function U(px) return px / 100 end
local TINY = 0.0001
local function M(s, xl) local xw = U(xl - 400); return {s, 0, 0, xw * (1 - s), 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1} end
local function T(dx, dy) return {1, 0, 0, U(dx), 0, 1, 0, -U(dy), 0, 0, 1, 0, 0, 0, 0, 1} end

local textId = 0
local function txt(s, x, y, o)
  textId = textId + 1
  o = o or {}
  return (o.parent or scene):text {id = "task-sw/text-" .. textId, text = s, point = W(x, y), align = o.align or {0, 0.5}, size = o.size or 14,
    font = FONT, fill = o.fill or C.ink, opacity = o.opacity, layer = o.layer or 10}
end
local rectId = 0
local function rect(x, y, w, h, o)
  rectId = rectId + 1
  o = o or {}
  return (o.parent or scene):rectangle {id = "task-sw/rect-" .. rectId, center = W(x + w / 2, y + h / 2), size = {U(w), U(h)}, fill = o.fill or C.paper,
    stroke = o.stroke or C.inksoft, width = o.width or 1, opacity = o.opacity, layer = o.layer or 3}
end
-- A lane block that grows from its left edge.
local function block(label, xa, xb, y, o)
  o = o or {}
  local h = o.h or 22
  local g = scene:group {matrix = M(TINY, xa)}
  local r = g:rectangle {center = W((xa + xb) / 2, y), size = {U(xb - xa), U(h)}, fill = o.fill or C.wash,
    stroke = C.accent, width = o.width or 1, opacity = 0, layer = 6}
  local t = label and txt(label, (xa + xb) / 2, o.h == 3 and y - 18 or y, {align = {0.5, 0.5}, size = 12, opacity = 0, layer = 8})
  return {g = g, r = r, t = t, xa = xa}
end
local function grow(b) local d = {{target = b.g, transform = M(1, b.xa)}, {target = b.r, opacity = 1}}
  if b.t then d[#d + 1] = {target = b.t, opacity = 1} end return d end
local function shrink(b) local d = {{target = b.g, transform = M(TINY, b.xa)}, {target = b.r, opacity = 0, fill = C.wash, stroke = C.accent}}
  if b.t then d[#d + 1] = {target = b.t, opacity = 0} end return d end
local function settle(b) return {{target = b.r, fill = C.surface, stroke = C.inksoft}} end
local function arrow(x1, y1, x2, y2)
  return scene:arrow {from = W(x1, y1), to = W(x2, y2), stroke = C.accent, color = C.accent, width = 1, tip = 6, opacity = 0, layer = 4}
end
local function cat(...) local r = {} for _, l in ipairs({...}) do for _, x in ipairs(l) do r[#r + 1] = x end end return r end
local function poly(pts, o)
  local q = {}
  for i, p in ipairs(pts) do q[i] = W(p[1], p[2]) end
  return (o.parent or scene):polygon {points = q, fill = o.fill or "#00000000", stroke = o.stroke or "#00000000",
    width = o.width or 0.01, opacity = o.opacity, layer = o.layer or 5}
end
local PAINT = "#0078c8"
-- The example Shape: a five-point star, ten path points, FillRule::NonZero.
local function star(cx, cy)
  local q = {}
  for k = 0, 9 do
    local a = math.rad(-90 + k * 36)
    local r = k % 2 == 0 and 66 or 28
    q[#q + 1] = {cx + r * math.cos(a), cy + r * math.sin(a)}
  end
  return q
end
-- Spans of a polygon on scanline y (non-zero winding).
local function spans(pts, y)
  local xs = {}
  for i = 1, #pts do
    local a, b = pts[i], pts[i % #pts + 1]
    if (a[2] <= y and b[2] > y) or (b[2] <= y and a[2] > y) then
      xs[#xs + 1] = {a[1] + (y - a[2]) * (b[1] - a[1]) / (b[2] - a[2]), b[2] > a[2] and 1 or -1}
    end
  end
  table.sort(xs, function(p, q) return p[1] < q[1] end)
  local out, w, x0 = {}, 0, 0
  for _, c in ipairs(xs) do
    local nw = w + c[2]
    if w == 0 and nw ~= 0 then x0 = c[1] end
    if w ~= 0 and nw == 0 then out[#out + 1] = {x0, c[1]} end
    w = nw
  end
  return out
end
-- A panel: a frame with a name in its top-left corner.
local function panel(name, x, y, w, h)
  rect(x, y, w, h, {stroke = C.soft, layer = 1})
  return txt(name, x + 10, y + 14, {size = 12, fill = C.faint})
end

-- phases and lanes
local PH = {{"update()", 140, 330}, {"draw()", 350, 644}, {"sync()", 656, 768}}
local phaseText = {}
for i, p in ipairs(PH) do
  scene:line {from = W(p[2], 52), to = W(p[3], 52), stroke = C.hair, width = 1, layer = 2}
  scene:line {from = W(p[2], 52), to = W(p[2], 57), stroke = C.hair, width = 1, layer = 2}
  scene:line {from = W(p[3], 52), to = W(p[3], 57), stroke = C.hair, width = 1, layer = 2}
  phaseText[i] = txt(p[1], (p[2] + p[3]) / 2, 40, {align = {0.5, 0.5}, fill = C.inksoft})
end
txt("SwCanvas", 32, 40, {size = 15})
local LANE = {caller = 96, tid = 150}
for _, l in ipairs({{"caller", LANE.caller, C.ink}, {"tid 1", LANE.tid, C.faint}}) do
  scene:line {from = W(140, l[2]), to = W(768, l[2]), stroke = C.soft, width = 1, layer = 1}
  txt(l[1], 128, l[2], {align = {1, 0.5}, fill = l[3]})
end

-- panels: the path, the RLE the worker builds, the surface the caller writes
local PY, PHH = 196, 188
local CX = {147, 400, 653}
panel("rshape->path", 32, PY, 230, PHH)
panel("task->shape.rle", 285, PY, 230, PHH)
panel("surface", 538, PY, 230, PHH)
local CY = 302
local path = star(CX[1], CY)
poly(path, {stroke = C.inksoft, width = 1.2})
for i, p in ipairs(path) do scene:circle {center = W(p[1], p[2]), radius = 0.025, fill = C.inksoft, stroke = C.inksoft, width = 0.01, layer = 6} end
local outline = poly(star(CX[2], CY), {stroke = C.accent, width = 1.2, opacity = 0})
-- surface pixel rows
for y = PY + 30, PY + PHH - 8, 6 do
  scene:line {from = W(548, y), to = W(758, y), stroke = C.surface, width = 0.6, layer = 1}
end
local CHUNK = 4
local rleChunk, pixChunk = {}, {}
for k = 1, CHUNK do rleChunk[k] = scene:group {opacity = 0}; pixChunk[k] = scene:group {opacity = 0} end
local rows = {}
for y = CY - 63, CY + 60, 6 do rows[#rows + 1] = y end
for ri, y in ipairs(rows) do
  local k = math.floor((ri - 1) * CHUNK / #rows) + 1
  for _, s in ipairs(spans(path, y)) do
    local dx = CX[2] - CX[1]
    rect(s[1] + dx, y - 1.5, s[2] - s[1], 3, {parent = rleChunk[k], fill = C.accent, stroke = C.accent, width = 0.01, layer = 7})
    local px = CX[3] - CX[1]
    rect(math.floor(s[1] + px), y - 3, math.ceil(s[2] - s[1]), 6, {parent = pixChunk[k], fill = PAINT, stroke = PAINT, width = 0.01, layer = 7})
  end
end
local spanLabel = txt("span", 505, PY + 14, {align = {1, 0.5}, size = 12, fill = C.accent, opacity = 0})

-- the running call
local function ph(i) local d = {} for j, t in ipairs(phaseText) do d[#d + 1] = {target = t, fill = j == i and C.accent or C.inksoft} end return d end

-- lane blocks
local bPrep = block("prepare()", 150, 240, LANE.caller)
local bRun = block("run(tid)", 258, 516, LANE.tid)
local bWait = block("done()", 360, 516, LANE.caller, {h = 3, fill = C.accent})
local bRas = block("rasterShape()", 522, 642, LANE.caller)
local bSync = block("sync()", 664, 758, LANE.caller)
local aReq = arrow(240, 107, 258, 139)
local aDone = arrow(516, 139, 522, 107)
local function to(b, xb, x) return {target = b.g, transform = M((x - b.xa) / (xb - b.xa), b.xa)} end
local function on(b) local d = {{target = b.r, opacity = 1}}
  if b == bWait then d[#d + 1] = {target = b.t, opacity = 1} end
  return d end

local function run(d, t, c) scene:play(d, t, c or "smooth") end
scene:wait(0.5)
-- update(): prepare() stores the inputs and calls TaskScheduler::request(task).
run(cat(ph(1), grow(bPrep)), 0.8)
run(cat(settle(bPrep), {{target = aReq, opacity = 1}}), 0.4)
run(cat(on(bRun), {to(bRun, 516, 300), {target = outline, opacity = 1}}), 0.7, "linear")
run(cat({to(bRun, 516, 345), {target = rleChunk[1], opacity = 1}}), 0.6, "linear")
-- draw(): the caller blocks in done() while the worker finishes the spans
run(cat(ph(2), on(bWait), {to(bRun, 516, 405), to(bWait, 516, 405), {target = rleChunk[2], opacity = 1}}), 0.6, "linear")
run({to(bRun, 516, 460), to(bWait, 516, 460), {target = rleChunk[3], opacity = 1}}, 0.6, "linear")
run({to(bRun, 516, 516), to(bWait, 516, 516), {target = rleChunk[4], opacity = 1}, {target = spanLabel, opacity = 1}}, 0.6, "linear")
run(cat(settle(bRun), {{target = bRun.t, opacity = 1}, {target = aDone, opacity = 1}}), 0.4)
run(cat(grow(bRas)), 0.5)
for k = 1, CHUNK do run({{target = pixChunk[k], opacity = 1}}, 0.35) end
run(settle(bRas), 0.3)
-- sync(): only the list is cleared
run(cat(ph(3), grow(bSync)), 0.7)
run(settle(bSync), 0.3)
scene:wait(1.8)
local r = cat(ph(0), shrink(bPrep), shrink(bRun), shrink(bWait), shrink(bRas), shrink(bSync),
  {{target = aReq, opacity = 0}, {target = aDone, opacity = 0}, {target = outline, opacity = 0}, {target = spanLabel, opacity = 0}})
for k = 1, CHUNK do r[#r + 1] = {target = rleChunk[k], opacity = 0}; r[#r + 1] = {target = pixChunk[k], opacity = 0} end
run(r, 0.7)
scene:wait(0.4)
return scene
