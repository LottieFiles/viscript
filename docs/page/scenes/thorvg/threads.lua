-- Ported from gitpage/static/tmath/gpu-deferred/threadlanes.lua.
-- Trace data and render colors are preserved; structural colors follow the site theme.
local EV = {  -- engine, event, edge, lane (0 = calling thread, n = TaskScheduler worker tid n), ns, shape
  {"SW","canvas.update","B",0,0,-1},
  {"SW","prepare","B",0,3166,0},
  {"SW","request","B",0,13000,0},
  {"SW","prepare","E",0,15000,0},
  {"SW","prepare","B",0,15541,1},
  {"SW","request","B",0,15958,1},
  {"SW","prepare","E",0,17250,1},
  {"SW","prepare","B",0,20916,2},
  {"SW","request","B",0,22875,2},
  {"SW","update","B",2,20916,1},
  {"SW","update","B",1,17333,0},
  {"SW","update","B",3,30083,2},
  {"SW","prepare","E",0,26583,2},
  {"SW","prepare","B",0,34541,3},
  {"SW","request","B",0,42625,3},
  {"SW","prepare","E",0,45041,3},
  {"SW","prepare","B",0,45750,4},
  {"SW","request","B",0,47833,4},
  {"SW","prepare","E",0,48375,4},
  {"SW","prepare","B",0,48833,5},
  {"SW","request","B",0,49291,5},
  {"SW","prepare","E",0,50375,5},
  {"SW","canvas.update","E",0,51208,-1},
  {"SW","canvas.draw","B",0,51958,-1},
  {"SW","update","B",4,60500,3},
  {"SW","update","E",1,78708,0},
  {"SW","update","B",1,80250,4},
  {"SW","update","E",2,68916,1},
  {"SW","update","B",2,82833,5},
  {"SW","wait","B",0,74416,0},
  {"SW","wait","E",0,98250,0},
  {"SW","raster","B",0,101166,0},
  {"SW","update","E",2,105125,5},
  {"SW","update","E",3,128333,2},
  {"SW","update","E",4,146458,3},
  {"SW","raster","E",0,157708,0},
  {"SW","wait","B",0,160208,1},
  {"SW","wait","E",0,160958,1},
  {"SW","raster","B",0,161333,1},
  {"SW","raster","E",0,164916,1},
  {"SW","wait","B",0,165666,2},
  {"SW","wait","E",0,166166,2},
  {"SW","raster","B",0,166541,2},
  {"SW","raster","E",0,176791,2},
  {"SW","wait","B",0,177416,3},
  {"SW","wait","E",0,177916,3},
  {"SW","raster","B",0,178708,3},
  {"SW","raster","E",0,188416,3},
  {"SW","wait","B",0,189833,4},
  {"SW","update","E",1,843791,4},
  {"SW","wait","E",0,850458,4},
  {"SW","raster","B",0,851250,4},
  {"SW","raster","E",0,860250,4},
  {"SW","wait","B",0,861000,5},
  {"SW","wait","E",0,861583,5},
  {"SW","raster","B",0,863125,5},
  {"SW","raster","E",0,894333,5},
  {"SW","canvas.draw","E",0,895833,-1},
  {"SW","canvas.sync","B",0,896541,-1},
  {"SW","canvas.sync","E",0,897000,-1},
  {"GL","canvas.update","B",0,0,-1},
  {"GL","prepare","B",0,5458,0},
  {"GL","prepare","E",0,12708,0},
  {"GL","prepare","B",0,13416,1},
  {"GL","prepare","E",0,14458,1},
  {"GL","prepare","B",0,14875,2},
  {"GL","prepare","E",0,18041,2},
  {"GL","prepare","B",0,18541,3},
  {"GL","prepare","E",0,30166,3},
  {"GL","prepare","B",0,30791,4},
  {"GL","prepare","E",0,124166,4},
  {"GL","prepare","B",0,125916,5},
  {"GL","prepare","E",0,130291,5},
  {"GL","canvas.update","E",0,130916,-1},
  {"GL","canvas.draw","B",0,131291,-1},
  {"GL","record","B",0,8684500,0},
  {"GL","record","E",0,8687083,0},
  {"GL","record","B",0,8693583,1},
  {"GL","record","E",0,8695458,1},
  {"GL","record","B",0,8699541,2},
  {"GL","record","E",0,8700916,2},
  {"GL","record","B",0,8708791,3},
  {"GL","record","E",0,8711000,3},
  {"GL","record","B",0,8718333,4},
  {"GL","record","E",0,8722000,4},
  {"GL","record","B",0,8730250,5},
  {"GL","record","E",0,8741583,5},
  {"GL","canvas.draw","E",0,8763625,-1},
  {"GL","canvas.sync","B",0,8764208,-1},
  {"GL","sync","B",0,8765041,-1},
  {"GL","sync","E",0,19995000,-1},
  {"GL","canvas.sync","E",0,19999875,-1},
}

-- Source of EV: scratchpad/gpu/threadlanes.log (scratchpad/gpu/threadlanes.cpp, Initializer::init(4),
-- ThorVG 08e80d00 built in scratchpad/gpu/tvg/build_tl with -DTVG_TL_TRACE).
-- x is the rank of each event inside its canvas call (update / draw / sync), not its duration.

local FONT = "CMUTypewriter-Regular"
local scene = vis.scene {width = 800, height = 450, fps = 25, loop = true,
  theme = {text = {h1 = {font = FONT}, h2 = {font = FONT}, h3 = {font = FONT}, text = {font = FONT}, code = {font = FONT}}},
  camera = {height = 4.5}}

local C = {paper = "background", surface = "surface", soft = "border", hair = "border", faint = "muted",
  inksoft = "foreground", ink = "foreground", accent = "accent", light = "accent", wash = "surface"}
-- fill colours of the six paints in threadlanes.cpp buildScene(); S5 is a two-stop LinearGradient
local PAINT = {{"#00c800"}, {"#0000ff"}, {"#ff0000"}, {"#c8c800"}, {"#0078c8"}, {"#ff00ff", "#00ffff"}}

local function W(px, py) return {(px - 400) / 100, (225 - py) / 100} end
local function U(px) return px / 100 end
local LAYER = {track = 1, rule = 2, arrow = 4, block = 6, strip = 7, cursor = 9, text = 10}

-- pixel plan
local X0, X1 = 116, 768
local LANE_PITCH, BLOCK_H = 28, 16
local PANELS = {
  {eng = "SW", name = "SwCanvas", row = 50, lanes = 84},
  {eng = "GL", name = "GlCanvas", row = 244, lanes = 278},
}
local NLANE = 5  -- calling thread + TaskScheduler workers tid 1..4 (Initializer::init(4))
local PHASES = {"canvas.update", "canvas.draw", "canvas.sync"}
local PHASE_LABEL = {"update()", "draw()", "sync()"}
local SYNC_MIN, PHASE_GAP = 64, 10

-- phase bounds and ranked events per engine
local bound, evs = {}, {}
for _, p in ipairs(PANELS) do bound[p.eng] = {}; evs[p.eng] = {} end
for _, e in ipairs(EV) do
  local eng, k, edge, lane, ns, s = e[1], e[2], e[3], e[4], e[5], e[6]
  local ph = nil
  for i, name in ipairs(PHASES) do if k == name then ph = i end end
  if ph then
    bound[eng][ph] = bound[eng][ph] or {}
    bound[eng][ph][edge] = ns
  else
    local t = evs[eng]; t[#t + 1] = {k = k, edge = edge, lane = lane, ns = ns, s = s}
  end
end
for _, p in ipairs(PANELS) do
  local list = evs[p.eng]
  table.sort(list, function(a, b) return a.ns < b.ns end)
  for _, e in ipairs(list) do
    for i = 1, 3 do
      local b = bound[p.eng][i]
      if e.ns >= b.B and e.ns <= b.E then e.ph = i end
    end
    assert(e.ph, "event outside canvas calls")
  end
end
-- phase widths: slots = largest event count of either engine in that phase
local count = {}
for i = 1, 3 do
  count[i] = 0
  for _, p in ipairs(PANELS) do
    local n = 0
    for _, e in ipairs(evs[p.eng]) do if e.ph == i then n = n + 1 end end
    if n > count[i] then count[i] = n end
  end
end
local avail = (X1 - X0) - 2 * PHASE_GAP - SYNC_MIN
local px0 = {}
local pw = {count[1] + 1, count[2] + 1}
local tot = pw[1] + pw[2]
local phx, phw = {}, {}
phx[1] = X0; phw[1] = avail * pw[1] / tot
phx[2] = phx[1] + phw[1] + PHASE_GAP; phw[2] = avail * pw[2] / tot
phx[3] = phx[2] + phw[2] + PHASE_GAP; phw[3] = SYNC_MIN
-- event x: rank inside its phase, spread over the phase width
for _, p in ipairs(PANELS) do
  local rank, n = {0, 0, 0}, {0, 0, 0}
  for _, e in ipairs(evs[p.eng]) do n[e.ph] = n[e.ph] + 1 end
  for _, e in ipairs(evs[p.eng]) do
    rank[e.ph] = rank[e.ph] + 1
    e.x = phx[e.ph] + phw[e.ph] * (rank[e.ph] - 0.5) / n[e.ph]
  end
end

local function laneY(p, lane) return p.lanes + lane * LANE_PITCH end

-- static structure: lane tracks, labels, phase rules, cursor
for pi, p in ipairs(PANELS) do
  scene:text {id = "tl:" .. p.eng .. ":name", text = p.name, point = W(32, p.row), align = {0, 0.5},
    size = 15, font = FONT, fill = C.ink, layer = LAYER.text}
  for lane = 0, NLANE - 1 do
    local y = laneY(p, lane)
    scene:line {from = W(X0, y), to = W(X1, y), stroke = C.soft, width = 1, layer = LAYER.track}
    scene:text {id = "tl:" .. p.eng .. ":lane" .. lane, text = lane == 0 and "caller" or ("tid " .. lane),
      point = W(104, y), align = {1, 0.5}, size = 14, font = FONT,
      fill = lane == 0 and C.ink or C.faint, layer = LAYER.text}
  end
  for i = 1, 3 do
    local xa, xb, y = phx[i], phx[i] + phw[i], p.row + 12
    scene:line {from = W(xa, y), to = W(xb, y), stroke = C.hair, width = 1, layer = LAYER.rule}
    scene:line {from = W(xa, y), to = W(xa, y + 5), stroke = C.hair, width = 1, layer = LAYER.rule}
    scene:line {from = W(xb, y), to = W(xb, y + 5), stroke = C.hair, width = 1, layer = LAYER.rule}
    scene:text {id = "tl:" .. p.eng .. ":phase" .. i, text = PHASE_LABEL[i], point = W((xa + xb) / 2, p.row),
      align = {0.5, 0.5}, size = 14, font = FONT, fill = C.inksoft, layer = LAYER.text}
  end
end

-- blocks: pair B/E per (event, lane, shape)
local blocks = {}
local function M(s, xl) local xw = U(xl - 400); return {s, 0, 0, xw * (1 - s), 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1} end
local TINY = 0.0001
local function addBlock(p, xa, xb, lane, s, kind)
  local y = laneY(p, lane)
  local b = {xa = xa, xb = xb, kind = kind}
  local h = kind == "wait" and 2 or BLOCK_H
  b.g = scene:group {matrix = M(TINY, xa)}
  b.r = b.g:rectangle {center = W((xa + xb) / 2, y), size = {U(xb - xa), U(h)},
    fill = kind == "wait" and C.accent or C.wash, stroke = kind == "wait" and C.accent or C.accent,
    width = kind == "wait" and 0.01 or 1, opacity = 0, layer = LAYER.block}
  b.strips = {}
  if s >= 0 and kind ~= "wait" then
    local cols = PAINT[s + 1]
    for ci, col in ipairs(cols) do
      local hh = (BLOCK_H - 4) / #cols
      local cy = y - (BLOCK_H - 4) / 2 + hh * (ci - 0.5)
      b.strips[#b.strips + 1] = scene:rectangle {center = W(xa + 4, cy), size = {U(4), U(hh)},
        fill = col, stroke = col, width = 0.01, opacity = 0, layer = LAYER.strip}
    end
  end
  blocks[#blocks + 1] = b
  return b
end

local arrows = {}
local function addArrow(x1, y1, x2, y2, appear)
  local a = scene:arrow {from = W(x1, y1), to = W(x2, y2), stroke = C.accent, color = C.accent,
    width = 1, tip = 6, opacity = 0, layer = LAYER.arrow}
  arrows[#arrows + 1] = {a = a, x = appear}
end

for _, p in ipairs(PANELS) do
  local open, byShape = {}, {}
  for _, e in ipairs(evs[p.eng]) do
    local key = e.k .. ":" .. e.lane .. ":" .. e.s
    if e.edge == "B" then open[key] = e
    else
      local b = open[key]; open[key] = nil
      if b then
        byShape[e.k .. ":" .. e.s] = {B = b, E = e}
      end
    end
  end
  for _, e in ipairs(evs[p.eng]) do
    if e.edge == "E" then
      local pair = byShape[e.k .. ":" .. e.s]
      if pair and pair.E == e then
        if e.k == "wait" then
          -- done() blocked only when the shape's update ended after the wait began
          local u = byShape["update:" .. e.s]
          if u and u.E.ns > pair.B.ns then addBlock(p, pair.B.x, e.x, 0, e.s, "wait") end
        elseif e.k ~= "request" then
          addBlock(p, pair.B.x, e.x, e.lane, e.s, e.k)
        end
      end
    end
  end
  if p.eng == "SW" then
    for s = 0, 5 do
      local req, upd, ras
      for _, e in ipairs(evs.SW) do
        if e.s == s and e.k == "request" then req = e end
      end
      upd = byShape["update:" .. s]; ras = byShape["raster:" .. s]
      local wy = laneY(p, upd.B.lane)
      -- TaskScheduler::request -> SwShapeTask::run on worker tid
      addArrow(req.x, laneY(p, 0) + BLOCK_H / 2, upd.B.x, wy - BLOCK_H / 2, upd.B.x)
      -- run() end -> task->done() returns, rasterShape on the calling thread
      addArrow(upd.E.x, wy - BLOCK_H / 2, ras.B.x, laneY(p, 0) + BLOCK_H / 2, ras.B.x)
    end
  end
end

-- cursors
local cursors = {}
for _, p in ipairs(PANELS) do
  cursors[#cursors + 1] = scene:line {from = W(X0, p.row + 18), to = W(X0, laneY(p, NLANE - 1) + 12),
    stroke = C.accent, width = 1.5, layer = LAYER.cursor}
end

-- breakpoints
local xs = {X0, X1}
for _, b in ipairs(blocks) do xs[#xs + 1] = b.xa; xs[#xs + 1] = b.xb end
for _, a in ipairs(arrows) do xs[#xs + 1] = a.x end
table.sort(xs)
local bp = {}
for _, x in ipairs(xs) do if #bp == 0 or x - bp[#bp] > 0.5 then bp[#bp + 1] = x end end
local function near(a, b) return math.abs(a - b) <= 0.5 end

local SPEED = (X1 - X0) / 7.0
local SETTLE = 40
scene:wait(0.6)
-- forward sweep
for i = 1, #bp - 1 do
  local xa, xb = bp[i], bp[i + 1]
  local d = {}
  for _, c in ipairs(cursors) do d[#d + 1] = {target = c, shift = {U(xb - xa), 0}} end
  for _, b in ipairs(blocks) do
    if b.xa <= xa + 0.5 and b.xb >= xb - 0.5 then
      local s = (xb - b.xa) / (b.xb - b.xa)
      d[#d + 1] = {target = b.g, transform = M(s, b.xa)}
      if near(b.xa, xa) then
        d[#d + 1] = {target = b.r, opacity = 1}
        for _, st in ipairs(b.strips) do d[#d + 1] = {target = st, opacity = 1} end
      end
    elseif near(b.xb, xa) then
      if b.kind == "wait" then d[#d + 1] = {target = b.r, fill = C.light, stroke = C.light}
      else d[#d + 1] = {target = b.r, fill = C.surface, stroke = C.inksoft} end
    end
  end
  for _, a in ipairs(arrows) do
    if near(a.x, xa) then d[#d + 1] = {target = a.a, opacity = 1}
    elseif a.x < xa and not a.settled and xa >= a.x + SETTLE then
      a.settled = true
      d[#d + 1] = {target = a.a, stroke = C.hair}
    end
  end
  scene:tween(d, {duration = (xb - xa) / SPEED, curve = "linear"})
end
local rest = {}
for _, a in ipairs(arrows) do if not a.settled then a.settled = true; rest[#rest + 1] = {target = a.a, stroke = C.hair} end end
if #rest > 0 then scene:tween(rest, {duration = 0.4, curve = "smooth"}) end
scene:wait(1.0)
-- return: cursor moves back to X0; each block shrinks to its start and each arrow fades as the cursor passes it
local BACK = (X1 - X0) / 1.6
for i = #bp, 2, -1 do
  local xb, xa = bp[i], bp[i - 1]
  local d = {}
  for _, c in ipairs(cursors) do d[#d + 1] = {target = c, shift = {-U(xb - xa), 0}} end
  for _, b in ipairs(blocks) do
    if b.xa <= xa + 0.5 and b.xb >= xb - 0.5 then
      if near(b.xa, xa) then
        d[#d + 1] = {target = b.g, transform = M(TINY, b.xa)}
        local f = b.kind == "wait" and C.accent or C.wash
        d[#d + 1] = {target = b.r, opacity = 0, fill = f, stroke = C.accent}
        for _, st in ipairs(b.strips) do d[#d + 1] = {target = st, opacity = 0} end
      else
        d[#d + 1] = {target = b.g, transform = M((xa - b.xa) / (b.xb - b.xa), b.xa)}
      end
    end
  end
  for _, a in ipairs(arrows) do
    if near(a.x, xa) then d[#d + 1] = {target = a.a, opacity = 0, stroke = C.accent} end
  end
  scene:tween(d, {duration = (xb - xa) / BACK, curve = "linear"})
end
scene:wait(0.4)
return scene
