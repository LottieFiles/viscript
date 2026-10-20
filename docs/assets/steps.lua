-- README "Show every step": four algorithms, each showing its intermediate states.
local scene = vis.scene {width = 1200, height = 380, fps = 20, theme = "pro_white",
  camera = {target = {0, 0.15}, height = 6.33}}

-- Plane text scales with the camera. This factor includes the clip's existing 4/3 label
-- enlargement as well as its reference-height mapping; it is not just a points conversion.
local PLANE_SIZE_SCALE = (4 / 3) * 6.33 / 380
local function plane(o) o.orientation = "plane"; o.size = (o.size or 14) * PLANE_SIZE_SCALE; return o end
local function TEXT(o) return scene:text(plane(o)) end
local function CODE(o) return scene:code(plane(o)) end

local INK, PANEL = "foreground", 4.4
local CX = {-7.35, -2.45, 2.45, 7.35}
local frames, titles = {}, {}
local function panel(x, title, id)
  frames[#frames + 1] = scene:rectangle {center = {x, -0.2}, size = {PANEL, PANEL}, fill = "#ffffff", stroke = INK, width = 2.5, layer = 0, id = id}
  titles[#titles + 1] = TEXT {text = title, point = {x - PANEL / 2, 2.35}, align = {0, 0.5}, size = 12, role = "code", fill = INK, id = id .. ":title"}
  return TEXT {text = "step 0", point = {x + PANEL / 2, 2.35}, align = {1, 0.5}, size = 11, role = "code", fill = "muted", id = id .. ":step"}
end
local function counter(h, n) scene:tween({target = h, text = "step " .. n}, 0.01) end

-- A. Sorting pass (bubble sort trace).
local stepA = panel(CX[1], "SORTING PASS", "sort")
local DATA = {5, 2, 6, 1, 4, 3}
local trace, arr = {}, {table.unpack(DATA)}
for i = 1, #arr - 1 do
  for j = 1, #arr - i do
    local sw = arr[j] > arr[j + 1]
    trace[#trace + 1] = {j = j, swap = sw, done = (j == #arr - i) and (#arr - i + 1) or nil}
    if sw then arr[j], arr[j + 1] = arr[j + 1], arr[j] end
  end
end
local SLOT, BASE, UNIT = 0.62, -2.0, 0.55
local ax0 = CX[1] - (#DATA - 1) * SLOT / 2
local bars, rects, order = {}, {}, {}
for k, v in ipairs(DATA) do
  local g = scene:group {id = "bar" .. k, layer = 2}
  rects[k] = g:rectangle {center = {ax0 + (k - 1) * SLOT, BASE + v * UNIT / 2}, size = {0.46, v * UNIT}, fill = "#ffffff", stroke = INK, width = 2, id = "rect" .. k}
  bars[k], order[k] = g, k
end

-- B. Graph search (BFS frontier by level).
local stepB = panel(CX[2], "GRAPH SEARCH", "bfs")
local NODES = {{0, 1.3}, {-1.2, 0.3}, {1.2, 0.3}, {-1.6, -0.9}, {-0.4, -0.9}, {0.6, -0.9}, {1.6, -0.9}, {-1.0, -1.9}, {1.1, -1.9}}
local EDGES = {{1, 2}, {1, 3}, {2, 4}, {2, 5}, {3, 6}, {3, 7}, {5, 8}, {6, 9}, {4, 8}, {7, 9}}
local LEVEL = {}
do  -- BFS from node 1
  local adj = {}
  for _, e in ipairs(EDGES) do
    adj[e[1]] = adj[e[1]] or {}; adj[e[2]] = adj[e[2]] or {}
    table.insert(adj[e[1]], e[2]); table.insert(adj[e[2]], e[1])
  end
  local q, dist = {1}, {[1] = 0}
  while #q > 0 do
    local u = table.remove(q, 1)
    for _, v in ipairs(adj[u]) do if not dist[v] then dist[v] = dist[u] + 1; q[#q + 1] = v end end
  end
  LEVEL = dist
end
local nodes, edges = {}, {}
for i, e in ipairs(EDGES) do
  local a, b = NODES[e[1]], NODES[e[2]]
  edges[i] = scene:line {from = {CX[2] + a[1], -0.2 + a[2]}, to = {CX[2] + b[1], -0.2 + b[2]}, stroke = "border", width = 2, layer = 1, id = "edge" .. i}
end
for i, p in ipairs(NODES) do
  nodes[i] = scene:rectangle {center = {CX[2] + p[1], -0.2 + p[2]}, size = {0.36, 0.36}, fill = "#ffffff", stroke = INK, width = 2, layer = 2, id = "node" .. i}
end

-- Shared pixel grid helper for C and D.
local N, CELL = 10, 0.38
local function grid(cx, id)
  local cells = {}
  local x0, y0 = cx - (N - 1) * CELL / 2, -0.2 - (N - 1) * CELL / 2
  for r = 1, N do
    cells[r] = {}
    for c = 1, N do
      local p = {x0 + (c - 1) * CELL, y0 + (N - r) * CELL}
      cells[r][c] = {p = p, h = scene:rectangle {center = p, size = {CELL, CELL}, fill = "#ffffff", stroke = "border", width = 1, layer = 1, id = id .. r .. "_" .. c}}
    end
  end
  return cells
end

-- C. Pixel sampling: one sample per pixel against a circle.
local stepC = panel(CX[3], "PIXEL SAMPLING", "sample")
local cellsC = grid(CX[3], "c")
local CR = 1.55
scene:circle {center = {CX[3], -0.2}, radius = CR, stroke = "accent", width = 2.5, layer = 3, id = "circle"}
local dotsC = {}
for r = 1, N do
  dotsC[r] = {}
  for c = 1, N do
    dotsC[r][c] = scene:circle {center = cellsC[r][c].p, radius = 0.045, fill = INK, stroke = INK, layer = 4, opacity = 0}
  end
end

-- D. Rasterization: scanline spans between triangle edges.
local stepD = panel(CX[4], "RASTERIZATION", "raster")
local cellsD = grid(CX[4], "d")
local TRI = {{CX[4] - 1.6, -1.9}, {CX[4] + 1.7, -1.3}, {CX[4] - 0.2, 1.6}}
scene:polygon {points = TRI, fill = "#00000000", stroke = "accent", width = 2.5, layer = 3, id = "tri"}
local function inside(p, a, b, c)
  local function s(p1, p2, p3) return (p1[1] - p3[1]) * (p2[2] - p3[2]) - (p2[1] - p3[1]) * (p1[2] - p3[2]) end
  local d1, d2, d3 = s(p, a, b), s(p, b, c), s(p, c, a)
  local neg, pos = d1 < 0 or d2 < 0 or d3 < 0, d1 > 0 or d2 > 0 or d3 > 0
  return not (neg and pos)
end
local scan = scene:rectangle {center = {CX[4], cellsD[N][1].p[2]}, size = {N * CELL, CELL}, stroke = "warning", width = 2.5, layer = 5, opacity = 0, id = "scanline"}

-- Camera: open wide, push in on each panel in turn, pull back for the finale.
local function camera()
  local CAM = {duration = 0.7, curve = "ease_in_out", padding = 0.1}
  scene:wait(0.2)
  for i = 1, #frames do
    scene:focus({frames[i], titles[i]}, CAM)
    scene:wait(i == 1 and 1.0 or 0.75)
  end
  scene:focus(nil, {duration = 0.9, curve = "ease_in_out"})
end

-- Timeline: all four run side by side, each with its own step counter.
local function lit(h, on) return {target = h, stroke = on and "warning" or INK, width = on and 3 or 2} end
scene:wait(0.5)
scene:parallel(
  camera,
  function()  -- A
    local S = 0.27
    for n, s in ipairs(trace) do
      local L, R = order[s.j], order[s.j + 1]
      counter(stepA, n)
      scene:tween({lit(rects[L], true), lit(rects[R], true)}, S * 0.5, "smooth")
      if s.swap then
        scene:tween({{target = bars[L], shift = {SLOT, 0}, arc = -1.2}, {target = bars[R], shift = {-SLOT, 0}, arc = -1.2}}, S, "ease_in_out")
        order[s.j], order[s.j + 1] = R, L
      end
      scene:tween({lit(rects[order[s.j]], false), lit(rects[order[s.j + 1]], false)}, S * 0.3, "smooth")
      if s.done then scene:tween({target = rects[order[s.done]], fill = "accent"}, 0.12) end
    end
    scene:tween({target = rects[order[1]], fill = "accent"}, 0.12)
  end,
  function()  -- B
    local maxl = 0
    for _, l in ipairs(LEVEL) do maxl = math.max(maxl, l) end
    for l = 0, maxl do
      counter(stepB, l + 1)
      local frontier, tree = {}, {}
      for i, d in ipairs(LEVEL) do if d == l then frontier[#frontier + 1] = {target = nodes[i], fill = "warning", stroke = "warning"} end end
      for i, e in ipairs(EDGES) do
        local a, b = LEVEL[e[1]], LEVEL[e[2]]
        if math.max(a, b) == l and math.abs(a - b) == 1 then tree[#tree + 1] = {target = edges[i], stroke = INK, width = 3} end
      end
      if #tree > 0 then scene:tween(tree, 0.4, "ease_out") end
      scene:tween(frontier, {duration = 0.35, curve = "ease_out", lag = 0.05})
      scene:wait(0.5)
      local settle = {}
      for i, d in ipairs(LEVEL) do if d == l then settle[#settle + 1] = {target = nodes[i], fill = "accent", stroke = INK} end end
      scene:tween(settle, 0.25)
    end
  end,
  function()  -- C
    for r = 1, N do
      counter(stepC, r)
      local show, fill = {}, {}
      for c = 1, N do
        local p = cellsC[r][c].p
        show[#show + 1] = {target = dotsC[r][c], opacity = 1}
        local d = math.sqrt((p[1] - CX[3]) ^ 2 + (p[2] + 0.2) ^ 2)
        if d <= CR then fill[#fill + 1] = {target = cellsC[r][c].h, fill = "accent"} end
      end
      scene:tween(show, {duration = 0.18, lag = 0.012})
      if #fill > 0 then scene:tween(fill, 0.15) end
      scene:wait(0.12)
    end
  end,
  function()  -- D
    scene:tween({target = scan, opacity = 1}, 0.15)
    for r = N, 1, -1 do
      counter(stepD, N - r + 1)
      local fill = {}
      for c = 1, N do
        if inside(cellsD[r][c].p, TRI[1], TRI[2], TRI[3]) then fill[#fill + 1] = {target = cellsD[r][c].h, fill = "accent"} end
      end
      if #fill > 0 then scene:tween(fill, {duration = 0.25, lag = 0.03, curve = "ease_out"}) else scene:wait(0.25) end
      if r > 1 then scene:tween({target = scan, shift = {0, CELL}}, 0.18, "ease_in_out") end
    end
    scene:tween({target = scan, opacity = 0}, 0.2)
  end)
scene:wait(1.2)
return scene
