// Demo collection: classic algorithm visualizations.
// Each episode is a self-contained Lua scene: shared prelude + algorithm-specific body.
// The page prints each episode's title and blurb, so scenes carry no titles or caption bar:
// t = 0 already shows the subject, motion carries the explanation, and at most one short
// phrase sits beside the action for a beat (say), replacing itself in place.
import {PRELUDE} from "./prelude.js";

// ---------------------------------------------------------------- sorting helpers
const BARS = `
-- each bar is a Group at its slot: body grows up from the baseline, value sits under it
local function bars(vals, x0, x1, base, hmax)
    local n, maxv = #vals, 0
    for _, v in ipairs(vals) do maxv = math.max(maxv, v) end
    local w = (x1 - x0) / n
    local B = {}
    for i, v in ipairs(vals) do
        local h = v / maxv * hmax
        local x = x0 + (i - 0.5) * w
        local g = scene:group {id = "bar:" .. i, matrix = at(x, base)}
        local r = g:rectangle {id = "bar:" .. i .. ":body", center = {0, h / 2}, size = {w * 0.72, h},
                               fill = C.bar, stroke = C.none, layer = L.subject}
        T(g, "bar:" .. i .. ":value", tostring(v), 0, -0.22, 13, C.mute, nil, L.label)
        B[i] = {g = g, r = r, v = v, h = h}
    end
    return B, w
end
local function paint(list, color, d)
    local fns = {}
    for i, b in ipairs(list) do fns[i] = function() scene:fill(b.r, color, {duration = d or 0.14}) end end
    scene:parallel(table.unpack(fns))
end
local function swap(B, i, j, w, d)
    local a, b = B[i], B[j]
    scene:tween({{target = a.g, shift = {(j - i) * w, 0}, arc = -math.pi / 2},
                {target = b.g, shift = {(i - j) * w, 0}, arc = -math.pi / 2}}, {duration = d or 0.32, curve = "smooth"})
    B[i], B[j] = b, a
end
`;

const BUBBLE = PRELUDE + BARS + `
local vals = {5, 2, 8, 3, 9, 1, 6}
local n = #vals
-- trace: every compare, every settle, and the early exit
local TRACE = {}
do
    local a = {table.unpack(vals)}
    local finished = false
    for k = 0, n - 2 do
        local swapped = false
        for i = 2, n - k do
            local sw = a[i - 1] > a[i]
            TRACE[#TRACE + 1] = {kind = "cmp", i = i, a = a[i - 1], b = a[i], swap = sw}
            if sw then a[i - 1], a[i] = a[i], a[i - 1]; swapped = true end
        end
        TRACE[#TRACE + 1] = {kind = "settle", i = n - k}
        if not swapped then TRACE[#TRACE + 1] = {kind = "done", upto = n - k - 1}; finished = true; break end
    end
    if not finished then TRACE[#TRACE + 1] = {kind = "done", upto = 1} end
    for i = 2, n do assert(a[i - 1] <= a[i]) end
end
local x0, x1, base, hmax = -4.6, 4.6, -2.2, 3.6
local B, w = bars(vals, x0, x1, base, hmax)
local X = function(i) return x0 + (i - 0.5) * w end
-- the wall between the unsorted prefix and the settled suffix
local wall = scene:line {id = "wall", from = {x1, base - 0.05}, to = {x1, base + hmax + 0.1},
                         stroke = C.line, width = 2, layer = L.guide}
local wallX = x1
local cueY = base + hmax + 0.45
local seen = {}
for _, e in ipairs(TRACE) do
    if e.kind == "cmp" then
        local first = (e.swap and not seen.swap) or (not e.swap and not seen.keep)
        together(function() paint({B[e.i - 1], B[e.i]}, C.warn) end,
                 function()
                     if first then
                         say(e.swap and string.format("%d > %d  swap", e.a, e.b) or string.format("%d < %d  keep", e.a, e.b),
                             (X(e.i - 1) + X(e.i)) / 2, cueY)
                     else say(nil) end
                 end)
        scene:wait(0.12)
        if e.swap then swap(B, e.i - 1, e.i, w); seen.swap = true else seen.keep = true end
        paint({B[e.i - 1]}, C.bar, 0.1)
    elseif e.kind == "settle" then
        local wx = x0 + (e.i - 1) * w
        together(function() paint({B[e.i]}, C.ok, 0.2) end,
                 function() scene:shift(wall, {wx - wallX, 0}, {duration = 0.3, curve = "smooth"}) end,
                 function()
                     if not seen.settle then say("largest settles", X(e.i), base + B[e.i].h + 0.3, C.ok) else say(nil) end
                 end)
        wallX = wx
        seen.settle = true
    else
        local rest = {}
        for i = 1, e.upto do rest[#rest + 1] = B[i] end
        together(function() paint(rest, C.ok, 0.3) end,
                 function() scene:fade(wall, 0, {duration = 0.3}) end,
                 function() if #rest > 1 then say("no swaps  sorted", X(1) + w * (e.upto - 1) / 2, cueY, C.ok) else say(nil) end end)
    end
end
scene:wait(2.4)
return scene`;

const INSERTION = PRELUDE + BARS + `
local vals = {6, 3, 8, 2, 7, 1, 5}
local n = #vals
-- trace: for each key, the larger neighbours it passes and the slot it lands in
local TRACE = {}
do
    local a = {table.unpack(vals)}
    for i = 2, n do
        local key, pos, shifts = a[i], i, {}
        while pos > 1 and a[pos - 1] > key do
            shifts[#shifts + 1] = {from = pos - 1, v = a[pos - 1]}
            a[pos] = a[pos - 1]; pos = pos - 1
        end
        a[pos] = key
        TRACE[#TRACE + 1] = {i = i, key = key, shifts = shifts, to = pos}
    end
    for i = 2, n do assert(a[i - 1] <= a[i]) end
end
local x0, x1, base, hmax = -4.6, 4.6, -2.2, 3.4
local B, w = bars(vals, x0, x1, base, hmax)
local X = function(i) return x0 + (i - 0.5) * w end
local lift = 0.6
paint({B[1]}, C.ok, 0.25)
for step, e in ipairs(TRACE) do
    local cur = B[e.i]
    together(function() scene:fill(cur.r, C.accent, {duration = 0.15}) end,
             function() scene:shift(cur.g, {0, lift}, {duration = 0.28, curve = "smooth"}) end,
             function() if step == 1 then say("key", X(e.i), base + cur.h + lift + 0.3, C.accent) else say(nil) end end)
    local pos = e.i
    for k, s in ipairs(e.shifts) do
        together(function() scene:shift(B[s.from].g, {w, 0}, {duration = 0.28, curve = "smooth"}) end,
                 function() scene:shift(cur.g, {-w, 0}, {duration = 0.28, curve = "smooth"}) end,
                 function()
                     if step == 1 and k == 1 then say(string.format("%d > %d  shift right", s.v, e.key), X(s.from), base + B[s.from].h + 0.35)
                     elseif step == 1 then say(nil) end
                 end)
        B[pos] = B[s.from]
        pos = s.from
    end
    together(function() scene:shift(cur.g, {0, -lift}, {duration = 0.28, curve = "smooth"}) end,
             function() scene:fill(cur.r, C.ok, {duration = 0.2}) end,
             function() if step == 1 then say(nil) end end)
    B[pos] = cur
    scene:wait(0.1)
end
scene:wait(2.4)
return scene`;

const QUICK = PRELUDE + BARS + `
local vals = {7, 2, 9, 4, 1, 8, 3, 6}
local x0, x1, base, hmax = -4.8, 1.3, -1.6, 3.4
local maxv = 0
for _, v in ipairs(vals) do maxv = math.max(maxv, v) end
local step = math.min(0.48, 3.6 / #vals)
local B, w = bars(vals, x0, x1, base, hmax)
local X = function(i) return x0 + (i - 0.5) * w end
-- legend: colour plus words, so colour is never the only cue
scene:rectangle {id = "legend:lt", center = {2.05, 2.1}, size = {0.18, 0.18}, fill = C.accent, stroke = C.none, layer = L.subject}
T(scene, "legend:lt:label", "< pivot", 2.25, 2.1, 12, C.mute, {0, 0.5})
scene:rectangle {id = "legend:ge", center = {3.35, 2.1}, size = {0.18, 0.18}, fill = C.warn, stroke = C.none, layer = L.subject}
T(scene, "legend:ge:label", "≥ pivot", 3.55, 2.1, 12, C.mute, {0, 0.5})
-- call stack: each frame is one quick_sort(lo..hi) call: the only extra memory
T(scene, "stack:title", "CALL STACK", 1.95, 1.6, 11, C.mute, {0, 0.5})
local frames, fid = {}, 0
local function push(lo, hi)
    fid = fid + 1
    local y = 1.2 - #frames * step
    if #frames > 0 then scene:stroke(frames[#frames].b, C.line, {duration = 0.1}) end
    local f = token("frame:" .. fid, string.format("quick_sort(%d..%d)", lo - 1, hi - 1), 3.4, y,
                    {size = {2.9, step * 0.84}, width = 1.2, text = 13, layer = L.subject})
    scene:fade_in(f.g, {shift = {0.25, 0}, duration = 0.22})
    frames[#frames + 1] = f
end
local function pop()
    local f = table.remove(frames)
    scene:fade_out(f.g, {shift = {0.25, 0}, duration = 0.2})
    if #frames > 0 then scene:stroke(frames[#frames].b, C.accent, {duration = 0.1}) end
end
-- pivot height line across the active range
local th = scene:rectangle {id = "pivot-line", center = {(x0 + x1) / 2, base}, size = {x1 - x0, 0.03},
                            fill = C.alt, stroke = C.none, layer = L.label - 1}
scene:set({target = th, opacity = 0})
local function threshold(lo, hi, v)
    local cx, y = x0 + (lo - 1 + hi) / 2 * w, base + v / maxv * hmax
    scene:tween({target = th, position = {cx, y}, size = {(hi - lo + 1) * w, 0.03}, opacity = 0.9}, {duration = 0.3, curve = "smooth"})
end
-- scan pointers: i = end of the < pivot zone, j = scanner
local pi = T(scene, "ptr:i", "i", X(1), base - 0.6, 14, C.accent)
local pj = T(scene, "ptr:j", "j", X(1), base - 0.9, 14, C.warn)
scene:set({target = pi, opacity = 0}); scene:set({target = pj, opacity = 0})
local ip, jp = 1, 1
local function mv(t, from, to) if from ~= to then scene:shift(t, {(to - from) * w, 0}, {duration = 0.18, curve = "smooth"}) end end
local function qs(lo, hi)
    push(lo, hi)
    if lo >= hi then
        if lo == hi then paint({B[lo]}, C.ok, 0.2) end
        scene:wait(0.15); pop(); return
    end
    together(function() paint({B[hi]}, C.alt, 0.18) end, function() threshold(lo, hi, vals[hi]) end,
             function() mv(pi, ip, lo) end, function() mv(pj, jp, lo) end,
             function() scene:fade(pi, 1, {duration = 0.2}) end, function() scene:fade(pj, 1, {duration = 0.2}) end)
    ip, jp = lo, lo
    local i = lo
    for j = lo, hi - 1 do
        mv(pj, jp, j); jp = j
        if vals[j] < vals[hi] then
            if i ~= j then swap(B, i, j, w); vals[i], vals[j] = vals[j], vals[i] end
            paint({B[i]}, C.accent, 0.12)
            i = i + 1
            mv(pi, ip, i); ip = i
        else
            paint({B[j]}, C.warn, 0.12)
        end
    end
    if i ~= hi then swap(B, i, hi, w, 0.4); vals[i], vals[hi] = vals[hi], vals[i] end
    local reset = {}
    for k = lo, hi do if k ~= i then reset[#reset + 1] = B[k] end end
    together(function() paint(reset, C.bar, 0.15) end, function() scene:fill(B[i].r, C.ok, {duration = 0.2}) end,
             function() scene:fade(th, 0, {duration = 0.2}) end, function() scene:fade(pi, 0, {duration = 0.2}) end, function() scene:fade(pj, 0, {duration = 0.2}) end)
    qs(lo, i - 1)
    qs(i + 1, hi)
    pop()
end
qs(1, #vals)
for i = 2, #vals do assert(vals[i - 1] <= vals[i]) end
scene:wait(2.4)
return scene`;

const MERGE = PRELUDE + BARS + `
local vals = {6, 2, 7, 4, 8, 1, 5, 3}
local x0, x1, top, low, hmax = -4.6, 4.6, 0.1, -2.5, 1.65
local B, w = bars(vals, x0, x1, top, hmax)
local X = function(i) return x0 + (i - 0.5) * w end
local drop = low - top
T(scene, "region:array", "ARRAY", -5.1, top + hmax + 0.15, 11, C.mute, {0, 0.5})
T(scene, "region:temp", "TEMP", -5.1, low + hmax + 0.15, 11, C.mute, {0, 0.5})
for i = 1, #vals do
    scene:rectangle {id = "temp-slot:" .. i, center = {X(i), low + hmax / 2}, size = {w * 0.8, hmax + 0.1},
                     fill = C.none, stroke = C.line, width = 1, layer = L.region}
end
local function merge(lo, mid, hi)
    local Lr, Rr = {}, {}
    for k = lo, mid do Lr[#Lr + 1] = B[k] end
    for k = mid + 1, hi do Rr[#Rr + 1] = B[k] end
    together(function() paint(Lr, C.alt, 0.15) end, function() paint(Rr, C.accent, 0.15) end)
    local a, b, k, tmp = lo, mid + 1, lo, {}
    local function take(col)
        scene:tween({{target = B[col].g, shift = {(k - col) * w, drop}}, {target = B[col].r, stroke = C.none}}, {duration = 0.3, curve = "smooth"})
        tmp[k] = {b = B[col], v = vals[col]}
        k = k + 1
    end
    while a <= mid and b <= hi do
        scene:tween({{target = B[a].r, stroke = C.text, width = 2}, {target = B[b].r, stroke = C.text, width = 2}}, {duration = 0.15})
        scene:wait(0.12)
        if vals[a] <= vals[b] then
            scene:stroke(B[b].r, C.none, {duration = 0.08}); take(a); a = a + 1
        else
            scene:stroke(B[a].r, C.none, {duration = 0.08}); take(b); b = b + 1
        end
    end
    while a <= mid do take(a); a = a + 1 end
    while b <= hi do take(b); b = b + 1 end
    local up = {}
    for c = lo, hi do
        up[#up + 1] = {target = tmp[c].b.g, shift = {0, -drop}}
        B[c], vals[c] = tmp[c].b, tmp[c].v
    end
    scene:tween(up, {duration = 0.35, curve = "smooth"})
    local run = {}
    for c = lo, hi do run[#run + 1] = B[c] end
    paint(run, (lo == 1 and hi == #vals) and C.ok or C.bar, 0.15)
end
local function ms(lo, hi)
    if lo >= hi then return end
    local mid = math.floor((lo + hi) / 2)
    ms(lo, mid); ms(mid + 1, hi)
    merge(lo, mid, hi)
end
scene:wait(0.4)
ms(1, #vals)
for i = 2, #vals do assert(vals[i - 1] <= vals[i]) end
scene:wait(2.4)
return scene`;

const BINARY = PRELUDE + `
local arr = {2, 5, 8, 12, 16, 23, 38, 45, 56, 72, 91}
local target = 56
-- trace: every probe with its window and verdict
local TRACE = {}
do
    local lo, hi = 1, #arr
    while lo <= hi do
        local mid = (lo + hi) // 2
        local verdict = arr[mid] == target and "hit" or (arr[mid] < target and "right" or "left")
        TRACE[#TRACE + 1] = {lo = lo, hi = hi, mid = mid, verdict = verdict}
        if verdict == "hit" then break elseif verdict == "right" then lo = mid + 1 else hi = mid - 1 end
    end
end
local w = math.min(0.8, 9.2 / #arr)
local x0, y = -w * #arr / 2, 0.1
local X = function(i) return x0 + (i - 0.5) * w end
local cells, box = {}, {}
for i, v in ipairs(arr) do
    cells[i] = scene:group {id = "cell:" .. i}
    box[i] = cells[i]:rectangle {id = "cell:" .. i .. ":box", center = {X(i), y}, size = {w * 0.9, 0.6}, corner = 0.03,
                        fill = C.panel, stroke = C.line, width = 1.2, layer = L.subject}
    T(cells[i], "cell:" .. i .. ":value", tostring(v), X(i), y, 15, C.text)
    T(scene, "cell:" .. i .. ":index", tostring(i - 1), X(i), y - 0.5, 10, C.mute)
end
T(scene, "target", "target  " .. target, x0, 1.75, 16, C.warn, {0, 0.5})
-- the live window [lo, hi]: one outline that shrinks
local winW = function(lo, hi) return (hi - lo + 1) * w end
local win = scene:rectangle {id = "window", center = {(X(1) + X(#arr)) / 2, y}, size = {winW(1, #arr) + 0.08, 0.8},
                             corner = 0.06, fill = C.none, stroke = C.accent, width = 2, layer = L.edge}
local wlo, whi = 1, #arr
local plo = T(scene, "ptr:lo", "lo", X(1), y - 0.95, 13, C.accent)
local phi = T(scene, "ptr:hi", "hi", X(#arr), y - 0.95, 13, C.accent)
local pmid = T(scene, "ptr:mid", "mid", X(1), y + 0.65, 13, C.warn)
scene:set({target = pmid, opacity = 0})
scene:wait(0.5)
for _, e in ipairs(TRACE) do
    together(function() scene:tween({target = pmid, position = {X(e.mid), y + 0.65}, opacity = 1}, {duration = 0.35, curve = "smooth"}) end,
             function() scene:tween({target = box[e.mid], stroke = C.warn}, {duration = 0.25}) end)
    local v = arr[e.mid]
    local rel = e.verdict == "hit" and "=" or (e.verdict == "right" and "<" or ">")
    if e.verdict == "hit" then
        together(function() scene:tween({target = box[e.mid], fill = "#7ee78733", stroke = C.ok}, {duration = 0.35}) end,
                 function() say(string.format("%d = %d  found at index %d", v, target, e.mid - 1), X(e.mid), y + 1.1, C.ok) end)
        scene:indicate(box[e.mid], {color = C.ok, scale = 1.12, duration = 0.6})
        break
    end
    say(string.format("%d %s %d", v, rel, target), X(e.mid), y + 1.1)
    scene:wait(0.35)
    local nlo, nhi = e.lo, e.hi
    local dim = {}
    if e.verdict == "right" then
        for k = e.lo, e.mid do dim[#dim + 1] = k end
        nlo = e.mid + 1
    else
        for k = e.mid, e.hi do dim[#dim + 1] = k end
        nhi = e.mid - 1
    end
    local ncx = (X(nlo) + X(nhi)) / 2
    local fns = {
        function() scene:tween({target = win, position = {ncx, y}, size = {winW(nlo, nhi) + 0.08, 0.8}}, {duration = 0.45, curve = "smooth"}) end,
        function() if nlo ~= wlo then scene:shift(plo, {(nlo - wlo) * w, 0}, {duration = 0.45, curve = "smooth"}) end end,
        function() if nhi ~= whi then scene:shift(phi, {(nhi - whi) * w, 0}, {duration = 0.45, curve = "smooth"}) end end,
        function() scene:stroke(box[e.mid], C.line, {duration = 0.2}) end,
    }
    for _, k in ipairs(dim) do fns[#fns + 1] = function() scene:fade(cells[k], 0.25, {duration = 0.3}) end end
    scene:parallel(table.unpack(fns))
    wlo, whi = nlo, nhi
end
scene:wait(2.4)
return scene`;

// ---------------------------------------------------------------- graph helpers
const GRAPH = `
local P = {A = {-3.6, 1.2}, B = {-4.7, -0.1}, C = {-2.5, -0.1}, D = {-5.0, -1.5}, E = {-3.8, -1.5}, F = {-1.9, -1.5}, G = {-0.6, 0.4}}
local ADJ = {A = {"B", "C"}, B = {"A", "D", "E"}, C = {"A", "F", "G"}, D = {"B"}, E = {"B"}, F = {"C"}, G = {"C"}}
for _, k in ipairs({"A", "B", "C", "D", "E", "F", "G"}) do local p = P[k]; P[k] = {p[1] * 1.15 + 3.2, p[2] * 1.1 + 0.25} end
local ORDER = {"A", "B", "C", "D", "E", "F", "G"}
local N, E = {}, {}
local function ekey(a, b) return a < b and a .. b or b .. a end
for _, a in ipairs(ORDER) do
    for _, b in ipairs(ADJ[a]) do
        if a < b then E[ekey(a, b)] = scene:line {id = "edge:" .. ekey(a, b), from = P[a], to = P[b], stroke = C.line, width = 2, layer = L.edge} end
    end
end
-- opaque node bodies keep the labels readable over the edges
for _, k in ipairs(ORDER) do
    N[k] = scene:circle {id = "node:" .. k, center = P[k], radius = 0.3, fill = C.panel, stroke = C.line, width = 2, layer = L.subject}
    T(scene, "node:" .. k .. ":label", k, P[k][1], P[k][2], 15, C.text)
end
-- a horizontal strip of tokens (queue / stack) along the bottom
local function strip(id, label, x0, y)
    T(scene, id .. ":title", label, x0, y + 0.42, 11, C.mute, {0, 0.5})
    local items, w, n = {}, 0.5, 0
    local S = {}
    function S.push(k)
        n = n + 1
        local o = token(id .. ":" .. n, k, x0 + (#items + 0.5) * w, y, {size = {w * 0.86, 0.42}, width = 1.2, text = 13})
        scene:fade_in(o.g, {shift = {0, 0.2}, duration = 0.25})
        items[#items + 1] = o
    end
    function S.pop_front()
        local o = table.remove(items, 1)
        local d = {}
        for _, q in ipairs(items) do d[#d + 1] = toward(q, q.x - w, q.y) end
        scene:parallel(function() scene:fade_out(o.g, {shift = {-0.3, 0}, duration = 0.25}) end,
                       function() if #d > 0 then scene:tween(d, {duration = 0.25, curve = "smooth"}) end end)
    end
    function S.pop_back()
        local o = table.remove(items)
        scene:fade_out(o.g, {shift = {0, 0.2}, duration = 0.25})
    end
    return S
end
`;

const BFS = PRELUDE + GRAPH + `
local Q = strip("queue", "QUEUE", -5.1, -2.45)
local queue = {"A"}
local seen = {A = true}
scene:wait(0.3)
scene:tween({target = N.A, stroke = C.warn}, {duration = 0.2})
Q.push("A")
while #queue > 0 do
    local node = table.remove(queue, 1)
    together(function() Q.pop_front() end,
             function() scene:tween({target = N[node], fill = "#1b3350", stroke = C.accent}, {duration = 0.25}) end)
    for _, nb in ipairs(ADJ[node]) do
        scene:stroke(E[ekey(node, nb)], C.warn, {duration = 0.15})
        if not seen[nb] then
            seen[nb] = true
            together(function() scene:tween({target = N[nb], stroke = C.warn}, {duration = 0.2}) end,
                     function() scene:tween({target = E[ekey(node, nb)], stroke = C.accent, width = 4}, {duration = 0.2}) end)
            Q.push(nb)
            queue[#queue + 1] = nb
        else
            scene:stroke(E[ekey(node, nb)], C.line, {duration = 0.12})
        end
    end
    scene:tween({target = N[node], fill = "#1c3a26", stroke = C.ok}, {duration = 0.2})
end
scene:wait(2.4)
return scene`;

const DFS = PRELUDE + GRAPH + `
local S = strip("stack", "CALL STACK", -5.1, -2.45)
local seen = {}
local function dfs(node, parent)
    S.push(node)
    seen[node] = true
    scene:tween({target = N[node], fill = "#1b3350", stroke = C.accent}, {duration = 0.25})
    for _, nb in ipairs(ADJ[node]) do
        if nb ~= parent then scene:stroke(E[ekey(node, nb)], C.warn, {duration = 0.15}) end
        if not seen[nb] then
            scene:tween({target = E[ekey(node, nb)], stroke = C.accent, width = 4}, {duration = 0.2})
            dfs(nb, node)
        elseif nb ~= parent then
            scene:stroke(E[ekey(node, nb)], C.line, {duration = 0.12})
        end
    end
    scene:tween({target = N[node], fill = "#1c3a26", stroke = C.ok}, {duration = 0.2})
    S.pop_back()
end
scene:wait(0.3)
dfs("A", nil)
scene:wait(2.4)
return scene`;

const DIAMETER = PRELUDE + `
-- an unweighted tree; ADJ[i] lists the neighbours of node i
local EDGES = {{1, 2}, {1, 3}, {2, 4}, {2, 5}, {3, 6}, {4, 7}, {4, 8}, {6, 9}, {9, 10}}
local n = 10
local ADJ = {}
for i = 1, n do ADJ[i] = {} end
for _, e in ipairs(EDGES) do
    ADJ[e[1]][#ADJ[e[1]] + 1] = e[2]
    ADJ[e[2]][#ADJ[e[2]] + 1] = e[1]
end

-- hang the tree from root: depth sets the row, leaves take columns left to right,
-- a parent sits over the middle of its children
local TOP, ROW, COL = 2.55, 0.68, 1.05
local function hang(root)
    local depth, parent, x, leaves = {}, {}, {}, 0
    local function walk(v, p, d)
        depth[v], parent[v] = d, p
        local xs = {}
        for _, w in ipairs(ADJ[v]) do
            if w ~= p then walk(w, v, d + 1); xs[#xs + 1] = x[w] end
        end
        if #xs == 0 then leaves = leaves + 1; x[v] = leaves
        else x[v] = (xs[1] + xs[#xs]) / 2 end
    end
    walk(root, nil, 0)
    local pos = {}
    for v = 1, n do pos[v] = {(x[v] - (leaves + 1) / 2) * COL - 0.6, TOP - depth[v] * ROW} end
    return pos, depth, parent
end

local pos = hang(3)
local node = {}
for v = 1, n do node[v] = token("node:" .. v, tostring(v), pos[v][1], pos[v][2], {circle = 0.24, stroke = C.line, text = 13}) end
local link = {}
for i, e in ipairs(EDGES) do
    link[i] = scene:connector {id = "edge:" .. i, from = node[e[1]].g, to = node[e[2]].g,
                               stroke = C.line, width = 2, tip = 0, layer = L.edge}
end
-- the hand holding the tree: a string from the top of the frame to the root
local hand = scene:line {id = "hand", from = {pos[3][1], 3.2}, to = {pos[3][1], pos[3][2] + 0.24},
                         stroke = C.mute, width = 2, layer = L.guide}

-- light the tree one depth at a time; return the deepest node
local function sweep(depth, color)
    local deepest, rows = 1, 0
    for v = 1, n do
        if depth[v] > depth[deepest] then deepest = v end
        rows = math.max(rows, depth[v])
    end
    for d = 0, rows do
        local row = {}
        for v = 1, n do if depth[v] == d then row[#row + 1] = {target = node[v].b, stroke = color} end end
        scene:tween(row, {duration = 0.18})
    end
    return deepest
end
local function reset()
    local all = {}
    for v = 1, n do all[#all + 1] = {target = node[v].b, stroke = C.line, fill = C.panel} end
    scene:tween(all, {duration = 0.25})
end

scene:wait(0.4)
say("hold any node — the rest hangs below", 0, -2.75, C.mute, {0.5, 0.5})
local _, depth = hang(3)
local a = sweep(depth, C.accent)
scene:tween({target = node[a].b, stroke = C.danger, fill = "#3a1d1d"}, {duration = 0.3})
say("the lowest node is one end of the diameter", 0, -2.75, C.danger, {0.5, 0.5})
scene:wait(0.8)

-- pick that node up: every node falls into its new row at once
reset()
scene:tween({target = node[a].b, stroke = C.danger, fill = "#3a1d1d"}, {duration = 0.2})
local pos2, depth2, parent2 = hang(a)
local moves = {}
for v = 1, n do moves[#moves + 1] = toward(node[v], pos2[v][1], pos2[v][2]) end
moves[#moves + 1] = {target = hand, from = {pos2[a][1], 3.2}, to = {pos2[a][1], pos2[a][2] + 0.24}}
say("pick it up", 0, -2.75, C.danger, {0.5, 0.5})
scene:tween(moves, {duration = 1.4, curve = "ease_in_out"})
scene:wait(0.3)

local b = sweep(depth2, C.accent)
scene:tween({target = node[b].b, stroke = C.ok, fill = "#1c3a26"}, {duration = 0.3})

-- the path from the bottom back up to the hand is the diameter
local path = {}
local v = b
while parent2[v] do
    for i, e in ipairs(EDGES) do
        if (e[1] == v and e[2] == parent2[v]) or (e[2] == v and e[1] == parent2[v]) then
            path[#path + 1] = {target = link[i], stroke = C.warn, width = 4}
        end
    end
    v = parent2[v]
end
scene:tween(path, {duration = 0.5})

-- measure top to bottom
local mx = 4.6
local ruler = scene:arrow {id = "ruler", from = {mx, pos2[a][2]}, to = {mx, pos2[b][2]}, stroke = C.warn, width = 2, tip = 10, layer = L.label}
local ticks = {}
ticks[1] = scene:line {id = "tick:top", from = {mx - 0.2, pos2[a][2]}, to = {mx + 0.2, pos2[a][2]}, stroke = C.warn, width = 2, layer = L.label}
ticks[2] = scene:line {id = "tick:bottom", from = {mx - 0.2, pos2[b][2]}, to = {mx + 0.2, pos2[b][2]}, stroke = C.warn, width = 2, layer = L.label}
scene:create(ticks, {duration = 0.25})
scene:create(ruler, {duration = 0.6})
say("diameter = " .. depth2[b] .. " edges", mx - 0.35, (pos2[a][2] + pos2[b][2]) / 2, C.warn, {1, 0.5})
scene:wait(2.4)
return scene`;

const BST = PRELUDE + `
local keys = {50, 30, 70, 20, 40, 60, 80, 35, 65}
local R = 0.3
-- trace: each key's comparison path and its parent slot
local TRACE, maxd = {}, 0
do
    local t = nil
    for _, k in ipairs(keys) do
        local path, n, parent, side = {}, t, nil, nil
        while n do
            path[#path + 1] = {node = n.v, go = k < n.v and "left" or "right"}
            parent, side = n, (k < n.v) and "l" or "r"
            n = n[side]
        end
        local leaf = {v = k}
        if parent then parent[side] = leaf else t = leaf end
        TRACE[#TRACE + 1] = {key = k, path = path}
        maxd = math.max(maxd, #path)
    end
end
local dy = math.min(1.1, 3.4 / math.max(maxd, 1))
-- the keys still to insert wait in a row along the top
local rgap = 0.7
local rx0 = -(#keys - 1) * rgap / 2
local row = {}
for i, k in ipairs(keys) do
    row[i] = token("key:" .. k, tostring(k), rx0 + (i - 1) * rgap, 2.55, {circle = R, stroke = C.mute, text = 14})
end
-- the invariant every comparison relies on
T(scene, "bst:rule", "left subtree  <  node  <  right subtree", -5.1, -2.8, 13, C.mute, {0, 0.5})
local count = T(scene, "bst:count", "0 keys · 0 comparisons", 5.1, -2.8, 13, C.mute, {1, 0.5})
-- comparison label: text on an opaque plate so tree edges never cut through it
local cmpN, cmpCur = 0, nil
local function compare(s, x, y, color)
    cmpN = cmpN + 1
    local g = scene:group {id = "cmp:" .. cmpN, matrix = at(x, y), opacity = 0}
    g:rectangle {id = "cmp:" .. cmpN .. ":plate", center = {0, 0}, size = {#s * 0.085 + 0.3, 0.34}, corner = 0.06,
                 fill = "#000000", stroke = C.none, layer = L.focus - 1}
    T(g, "cmp:" .. cmpN .. ":text", s, 0, 0, 15, color, nil, L.focus)
    if cmpCur then scene:fade_out(cmpCur, {duration = 0.15}) end
    scene:tween({target = g, opacity = 1}, {duration = 0.2})
    cmpCur = g
end
local function compare_clear()
    if cmpCur then scene:fade_out(cmpCur, {duration = 0.15}); cmpCur = nil end
end
local placed = {}   -- key -> {x, y, o}
local total = 0
for i, e in ipairs(TRACE) do
    local probe = row[i]
    say(nil)
    scene:tween({target = probe.b, stroke = C.warn}, {duration = 0.15})
    local x, y, span = 0, 1.45, 2.2
    if #e.path == 0 then say("empty tree: " .. e.key .. " becomes the root", 0, 0.75) end
    for _, p in ipairs(e.path) do
        local nd = placed[p.node]
        -- stand beside the node on the side the key is about to take
        local side = p.go == "left" and -1 or 1
        scene:tween(toward(probe, nd.x + side * 0.8, nd.y + 0.05), {duration = 0.32, curve = "smooth"})
        local lt = p.go == "left"
        say(nil)
        compare(string.format("%d %s %d  →  %s", e.key, lt and "<" or ">", p.node, p.go),
                nd.x + side * 0.8, nd.y + 0.62, lt and C.accent or C.alt)
        scene:indicate(nd.o.b, {color = C.warn, scale = 1.1, duration = 0.45})
        scene:wait(0.35)
        x = p.go == "left" and nd.x - span or nd.x + span
        y = nd.y - dy
        span = span / 2
    end
    local parent = e.path[#e.path]
    compare_clear()
    scene:tween(toward(probe, x, y), {duration = 0.35, curve = "smooth"})
    if parent then
        local msg = string.format("empty slot: %s child of %d", parent.go, parent.node)
        say(msg, x, y - 0.52, C.ok)
    end
    total = total + #e.path
    local nc = T(scene, "bst:count:" .. i, string.format("%d keys · %d comparison%s", i, total, total == 1 and "" or "s"), 5.1, -2.8, 13, C.mute, {1, 0.5})
    scene:fade_out(count, {duration = 0.12}); scene:fade_in(nc, {duration = 0.15})
    count = nc
    scene:stroke(probe.b, C.accent, {duration = 0.15})
    placed[e.key] = {x = x, y = y, o = probe}
    if parent then
        local pn = placed[parent.node]
        local ddx, ddy = x - pn.x, y - pn.y
        local len = math.sqrt(ddx * ddx + ddy * ddy)
        local edge = scene:line {id = "edge:" .. parent.node .. "-" .. e.key,
                                 from = {pn.x + ddx / len * R, pn.y + ddy / len * R},
                                 to = {x - ddx / len * R, y - ddy / len * R}, stroke = C.line, width = 2, layer = L.edge}
        scene:create(edge, {duration = 0.25})
    end
    scene:wait(0.25)
end
say(nil)
scene:wait(2.4)
return scene`;

const FIB = PRELUDE + `
local n = 9
local dp = {[0] = 0, [1] = 1}
for i = 2, n do dp[i] = dp[i - 1] + dp[i - 2] end
local x0, w, y = -4.5, 0.9, -0.4
local X = function(i) return x0 + (i + 0.5) * w end
local cells = {}
for i = 0, n do
    cells[i] = scene:rectangle {id = "dp:" .. i, center = {X(i), y}, size = {w * 0.9, 0.62}, corner = 0.03,
                                fill = C.panel, stroke = C.line, width = 1.2, layer = L.subject}
    T(scene, "dp:" .. i .. ":index", tostring(i), X(i), y - 0.55, 10, C.mute)
end
T(scene, "dp:title", "dp", x0 - 0.05, y + 0.6, 12, C.mute, {0, 0.5})
local function put(i, color)
    local t = T(scene, "dp:" .. i .. ":value", tostring(dp[i]), X(i), y, 15, color or C.text)
    scene:fade_in(t, {scale = 0.7, duration = 0.25})
end
scene:wait(0.3)
put(0, C.mute)
put(1)
-- two read arcs, i-1 (low) and i-2 (high), slide right one cell per step
local a1 = scene:curve {id = "read:1", from = {X(1), y + 0.32}, control1 = {X(1), y + 0.8}, control2 = {X(2) - 0.12, y + 0.8},
                        to = {X(2) - 0.12, y + 0.34}, stroke = C.accent, width = 2, layer = L.edge}
local a2 = scene:curve {id = "read:2", from = {X(0), y + 0.32}, control1 = {X(0), y + 1.35}, control2 = {X(2) + 0.12, y + 1.35},
                        to = {X(2) + 0.12, y + 0.34}, stroke = C.alt, width = 2, layer = L.edge}
for i = 2, n do
    local reads = function()
        scene:tween({{target = cells[i - 1], stroke = C.accent}, {target = cells[i - 2], stroke = C.alt}}, {duration = 0.2})
    end
    if i == 2 then
        together(reads, function() scene:create({a1, a2}, {duration = 0.4, curve = "smooth"}) end)
    else
        together(reads, function() scene:tween({{target = a1, shift = {w, 0}}, {target = a2, shift = {w, 0}}}, {duration = 0.35, curve = "smooth"}) end)
    end
    put(i)
    scene:tween({{target = cells[i - 1], stroke = C.line}, {target = cells[i - 2], stroke = C.line},
                {target = cells[i], stroke = C.ok}}, {duration = 0.2})
end
scene:uncreate({a1, a2}, {duration = 0.3})
say("dp[" .. n .. "] = " .. dp[n], X(n), y - 1.05, C.ok)
scene:indicate(cells[n], {color = C.ok, scale = 1.1, duration = 0.6})
scene:wait(2.4)
return scene`;

const REVERSE = PRELUDE + `
local vals = {1, 2, 3, 4, 5}
local x0, gap, y = -2.7, 1.35, 0.4
local X = function(i) return x0 + (i - 1) * gap end
for i, v in ipairs(vals) do
    scene:rectangle {id = "node:" .. i, center = {X(i), y}, size = {0.6, 0.5}, corner = 0.04,
                     fill = C.panel, stroke = C.accent, width = 1.5, layer = L.subject}
    T(scene, "node:" .. i .. ":label", tostring(v), X(i), y, 15)
end
T(scene, "none:left", "None", X(0) + 0.1, y, 12, C.mute)
T(scene, "none:right", "None", X(#vals + 1) - 0.1, y, 12, C.mute)
local nextArrow = {}
for i = 1, #vals do
    nextArrow[i] = scene:arrow {id = "next:" .. i, from = {X(i) + 0.32, y}, to = {X(i + 1) - 0.36, y},
                                stroke = C.line, width = 2, tip = 8, layer = L.edge}
end
local function ptr(id, label, i, color, dy)
    return {t = T(scene, id, label, X(i), y + dy, 13, color), i = i}
end
local function move(p, i) scene:shift(p.t, {(i - p.i) * gap, 0}, {duration = 0.3, curve = "smooth"}); p.i = i end
local prev = ptr("ptr:prev", "prev", 0, C.alt, -0.75)
local head = ptr("ptr:head", "head", 1, C.accent, 0.72)
local nxt = ptr("ptr:next", "next", 1, C.warn, -1.15)
scene:set({target = nxt.t, opacity = 0})
scene:wait(0.4)
for i = 1, #vals do
    together(function() move(nxt, i + 1) end, function() scene:fade(nxt.t, 1, {duration = 0.2}) end,
             function() scene:stroke(nextArrow[i], C.warn, {duration = 0.2}) end)
    -- the same pointer swings round its node: it now points back at prev
    together(function() scene:rotate(nextArrow[i], -math.pi, {about = {X(i), y}, duration = 0.55, curve = "smooth"}) end,
             function() scene:stroke(nextArrow[i], C.ok, {duration = 0.55}) end)
    move(prev, i)
    together(function() move(head, i + 1) end, function() scene:fade(nxt.t, 0, {duration = 0.2}) end)
end
say("new head", X(#vals), y + 0.72, C.ok)
scene:indicate(prev.t, {color = C.ok, scale = 1.3, duration = 0.6})
scene:wait(2.4)
return scene`;


// ---------------------------------------------------------------- data structures
const STACK = PRELUDE + `
local start = {3, 7}
local OPS = {{"push", 1}, {"push", 9}, {"pop"}, {"pop"}, {"push", 4}, {"push", 8}, {"pop"}}
local x, y0, h = 0, -2.0, 0.58
scene:line {id = "wall:left", from = {x - 0.62, y0 - 0.34}, to = {x - 0.62, 1.9}, stroke = C.line, width = 2, layer = L.region}
scene:line {id = "wall:right", from = {x + 0.62, y0 - 0.34}, to = {x + 0.62, 1.9}, stroke = C.line, width = 2, layer = L.region}
scene:line {id = "wall:floor", from = {x - 0.62, y0 - 0.34}, to = {x + 0.62, y0 - 0.34}, stroke = C.line, width = 2, layer = L.region}
local S, n = {}, 0
local function item(v)
    n = n + 1
    return token("item:" .. n, tostring(v), x, y0 + #S * h)
end
for _, v in ipairs(start) do S[#S + 1] = item(v) end
local tpos = y0 + (#S - 1) * h
local top = T(scene, "ptr:top", "top", x - 0.85, tpos, 14, C.warn, {1, 0.5})
local function point()
    local y = y0 + math.max(#S - 1, 0) * h
    if y ~= tpos then scene:shift(top, {0, y - tpos}, {duration = 0.22, curve = "smooth"}); tpos = y end
end
scene:wait(0.5)
for _, op in ipairs(OPS) do
    if op[1] == "push" then
        local o = item(op[2])
        scene:fade_in(o.g, {shift = {0, 1.2}, duration = 0.3})
        S[#S + 1] = o
        point()
    else
        local o = table.remove(S)
        scene:tween({target = o.b, stroke = C.warn}, {duration = 0.15})
        scene:parallel(function() scene:fade_out(o.g, {shift = {2.2, 0.6}, duration = 0.3}) end, point)
    end
    scene:wait(0.15)
end
scene:wait(2.4)
return scene`;

const QUEUE = PRELUDE + `
local start = {4, 8}
local OPS = {{"enq", 2}, {"enq", 6}, {"deq"}, {"deq"}, {"enq", 5}, {"enq", 9}, {"deq"}}
local x0, gap, y = -3.6, 1.05, 0
scene:line {id = "rail:top", from = {x0 - 0.6, y + 0.4}, to = {x0 + 7 * gap, y + 0.4}, stroke = C.line, width = 2, layer = L.region}
scene:line {id = "rail:bottom", from = {x0 - 0.6, y - 0.4}, to = {x0 + 7 * gap, y - 0.4}, stroke = C.line, width = 2, layer = L.region}
local Q, n = {}, 0
local function item(v)
    n = n + 1
    return token("item:" .. n, tostring(v), x0 + #Q * gap, y)
end
for _, v in ipairs(start) do Q[#Q + 1] = item(v) end
local front = T(scene, "ptr:front", "front", x0, y - 0.75, 13, C.accent)
local rpos = #Q - 1
local rear = T(scene, "ptr:rear", "rear", x0 + rpos * gap, y + 0.75, 13, C.warn)
scene:wait(0.5)
for _, op in ipairs(OPS) do
    if op[1] == "enq" then
        local i = #Q
        local o = item(op[2])
        scene:fade_in(o.g, {shift = {1.0, 0}, duration = 0.3})
        Q[#Q + 1] = o
        if i ~= rpos then scene:shift(rear, {(i - rpos) * gap, 0}, {duration = 0.2, curve = "smooth"}) end
        rpos = i
    else
        local o = table.remove(Q, 1)
        scene:tween({target = o.b, stroke = C.accent}, {duration = 0.12})
        local m = {}
        for _, q in ipairs(Q) do m[#m + 1] = toward(q, q.x - gap, q.y) end
        m[#m + 1] = {target = rear, shift = {-gap, 0}}
        rpos = rpos - 1
        scene:parallel(function() scene:fade_out(o.g, {shift = {-1.2, 0}, duration = 0.3}) end,
                       function() scene:tween(m, {duration = 0.3, curve = "smooth"}) end)
    end
    scene:wait(0.15)
end
scene:wait(2.4)
return scene`;

const HEAP = PRELUDE + `
local input = {7, 5, 9, 3, 6, 8, 1}
local PRE = 3   -- the first pushes are already settled at t = 0
-- trace: for each push, the chain of compares up toward the root (1-based: parent of i is i // 2)
local TRACE, settled = {}, nil
do
    local h = {}
    for k, v in ipairs(input) do
        h[#h + 1] = v
        local i, steps = #h, {}
        while i > 1 do
            local p = i // 2
            steps[#steps + 1] = {i = i, p = p, swap = h[p] > h[i]}
            if h[p] <= h[i] then break end
            h[p], h[i] = h[i], h[p]; i = p
        end
        TRACE[#TRACE + 1] = {v = v, at = #h, steps = steps}
        if k == PRE then settled = {table.unpack(h)} end
    end
    for i = 2, #h do assert(h[i // 2] <= h[i]) end
end
local function pos(i)
    local d = math.floor(math.log(i, 2) + 1e-9)
    local k = i - 2 ^ d
    return -4.4 + (k + 0.5) / 2 ^ d * 8.8, 1.75 - d * 1.2
end
local R = 0.3
local ax0, aw, ay = -2.2, 0.75, -2.35
local AX = function(i) return ax0 + (i - 1) * aw end
T(scene, "array:title", "ARRAY", ax0 - 0.55, ay, 11, C.mute, {1, 0.5})
for i = 1, #input do
    scene:rectangle {id = "slot:" .. i, center = {AX(i), ay}, size = {aw * 0.9, 0.5}, corner = 0.03,
                     fill = C.none, stroke = C.line, width = 1, layer = L.region}
    T(scene, "slot:" .. i .. ":index", tostring(i - 1), AX(i), ay - 0.42, 10, C.mute)
end
local N, A = {}, {}
local function edge(i)
    local x, y = pos(i)
    local px, py = pos(i // 2)
    local dx, dy = x - px, y - py
    local l = math.sqrt(dx * dx + dy * dy)
    return scene:line {id = "edge:" .. i, from = {px + dx / l * R, py + dy / l * R}, to = {x - dx / l * R, y - dy / l * R},
                       stroke = C.line, width = 2, layer = L.edge}
end
local function node(i, v)
    local x, y = pos(i)
    N[i] = token("node:" .. v, tostring(v), x, y, {circle = R})
    A[i] = T(scene, "arr:" .. v, tostring(v), AX(i), ay, 15)
end
-- tree and array views of the settled heap, visible from the first frame
for i, v in ipairs(settled) do
    node(i, v)
    if i > 1 then edge(i) end
end
-- arc angle that lifts a value 'rise' above the row while it travels d slots
local function hop(d, rise) return 4 * math.atan(2 * rise / (d * aw)) end
local function swap(i, j)
    local xi, yi = pos(i)
    local xj, yj = pos(j)
    -- tree tokens pass each other on opposite arcs; in the array both values hop, one higher
    scene:tween({toward(N[i], xj, yj, 0.9), toward(N[j], xi, yi, 0.9),
                {target = A[i], shift = {(j - i) * aw, 0}, arc = hop(i - j, 0.74)},
                {target = A[j], shift = {(i - j) * aw, 0}, arc = -hop(i - j, 0.36)}}, {duration = 0.45, curve = "smooth"})
    N[i], N[j] = N[j], N[i]; A[i], A[j] = A[j], A[i]
end
scene:wait(0.5)
for k = PRE + 1, #TRACE do
    local e = TRACE[k]
    -- append: the value lands in the next free slot of both views at once
    node(e.at, e.v)
    local ed = edge(e.at)
    together(function() scene:fade_in(N[e.at].g, {scale = 0.6, duration = 0.3}) end,
             function() scene:fade_in(A[e.at], {shift = {0, 0.3}, duration = 0.3}) end,
             function() scene:create(ed, {duration = 0.3}) end)
    for _, s in ipairs(e.steps) do
        scene:tween({{target = N[s.i].b, stroke = C.warn}, {target = N[s.p].b, stroke = C.warn}}, {duration = 0.15})
        if s.swap then swap(s.i, s.p) else scene:wait(0.15) end
        scene:tween({{target = N[s.i].b, stroke = C.accent}, {target = N[s.p].b, stroke = C.accent}}, {duration = 0.12})
    end
    scene:wait(0.15)
end
scene:tween({target = N[1].b, stroke = C.ok, fill = "#1c3a26"}, {duration = 0.3})
local rx, ry = pos(1)
say("min", rx + 0.5, ry, C.ok, {0, 0.5})
scene:indicate(N[1].b, {color = C.ok, scale = 1.15, duration = 0.6})
scene:wait(2.4)
return scene`;

const HASH = PRELUDE + `
local keys = {"cat", "dog", "ant", "owl", "bee", "fox", "emu"}
local q = "emu"
local M = 5
local function h(k) local s = 0 for i = 1, #k do s = s + string.byte(k, i) end return s end
local bx, by0, bh = -4.4, 1.25, 0.78
local B, chains = {}, {}
for i = 0, M - 1 do
    local y = by0 - i * bh
    B[i] = scene:rectangle {id = "bucket:" .. i, center = {bx, y}, size = {0.7, 0.56}, corner = 0.03,
                            fill = C.panel, stroke = C.line, width = 1.5, layer = L.subject}
    T(scene, "bucket:" .. i .. ":index", tostring(i), bx, y, 14, C.mute)
    chains[i] = {}
end
-- keys waiting to be inserted, along the top
local pend = {}
for k, key in ipairs(keys) do pend[k] = token("key:" .. key, key, -1.6 + (k - 1) * 1.0, 2.55, {size = {0.86, 0.46}, stroke = C.mute, text = 14}) end
local sx, sy = 3.4, 0.5   -- where a key is hashed
local function hashAt(o, key)
    local hv = h(key)
    scene:tween(toward(o, sx, sy), {duration = 0.4, curve = "smooth"})
    say(string.format("h(%s) = %d,  %d mod %d = %d", key, hv, hv, M, hv % M), sx, sy - 0.55)
    return hv % M
end
scene:wait(0.4)
for k, key in ipairs(keys) do
    local o = pend[k]
    local i = hashAt(o, key)
    scene:stroke(B[i], C.warn, {duration = 0.2})
    local x, y = bx + 1.25 + #chains[i] * 1.35, by0 - i * bh
    local fx = #chains[i] == 0 and bx + 0.37 or x - 1.35 + 0.45
    say(nil)
    scene:tween({toward(o, x, y), {target = o.b, stroke = C.accent}}, {duration = 0.45, curve = "smooth"})
    local a = scene:arrow {id = "link:" .. key, from = {fx, y}, to = {x - 0.45, y}, stroke = C.line, width = 1.5, tip = 7, layer = L.edge}
    scene:parallel(function() scene:create(a, {duration = 0.2}) end, function() scene:stroke(B[i], C.line, {duration = 0.2}) end)
    chains[i][#chains[i] + 1] = {o = o, k = key}
end
-- lookup: hash once, then walk one short chain
local probe = token("query", q .. "?", sx, 2.55, {size = {0.86, 0.46}, stroke = C.warn, text = 14})
scene:fade_in(probe.g, {shift = {0, -0.3}, duration = 0.3})
local i = hashAt(probe, q)
scene:stroke(B[i], C.warn, {duration = 0.2})
local hit = false
for _, c in ipairs(chains[i]) do
    scene:tween({target = c.o.b, stroke = C.warn}, {duration = 0.2})
    if c.k == q then
        hit = true
        together(function() scene:tween({target = c.o.b, stroke = C.ok, fill = "#1c3a26"}, {duration = 0.3}) end,
                 function() scene:stroke(probe.b, C.ok, {duration = 0.3}) end,
                 function() say("found in bucket " .. i, sx, sy - 0.55, C.ok) end)
        break
    end
    scene:tween({target = c.o.b, stroke = C.accent}, {duration = 0.15})
end
if not hit then
    together(function() scene:tween({target = B[i], stroke = C.danger}, {duration = 0.3}) end,
             function() say("not in the table", sx, sy - 0.55, C.danger) end)
end
scene:wait(2.4)
return scene`;

export const ALGORITHMS = {
  id: "algorithms",
  title: "Algorithms",
  episodes: [
    {id: "bubble-sort", title: "Bubble sort", code: BUBBLE,
     blurb: "Adjacent pairs are compared and swapped; after each pass the largest remaining value settles behind the wall at the end."},
    {id: "insertion-sort", title: "Insertion sort", code: INSERTION,
     blurb: "Each value is lifted out and slid left past every larger value until it fits."},
    {id: "quick-sort", title: "Quick sort", code: QUICK,
     blurb: "Partition around the last element in place.\nThe violet line marks the pivot height; i and j scan the range, and the call stack on the right is the only extra memory."},
    {id: "merge-sort", title: "Merge sort", code: MERGE,
     blurb: "Runs are merged through a temp buffer: the smaller head drops into the next free slot below, then the merged run is copied back up."},
    {id: "binary-search", title: "Binary search", code: BINARY,
     blurb: "Each probe compares the middle with the target and the window outline shrinks to the half that can still hold it."},
    {id: "bfs", title: "Breadth-first search", code: BFS,
     blurb: "Visit nodes level by level using a FIFO queue. Highlighted edges form the BFS tree."},
    {id: "dfs", title: "Depth-first search", code: DFS,
     blurb: "Go as deep as possible before backtracking. The call stack grows and shrinks along the bottom."},
    {id: "tree-diameter", title: "Tree diameter", code: DIAMETER,
     blurb: "Hold any node and let the tree hang: the lowest node is one end of the longest path.\nPick that node up, and the new lowest node is the other end — two searches, O(n)."},
    {id: "fibonacci-dp", title: "Fibonacci (DP)", code: FIB,
     blurb: "Every cell is the sum of the two before it — computed once, left to right."},
  ],
};

export const DATA_STRUCTURES = {
  id: "data-structures",
  title: "Data structures",
  episodes: [
    {id: "stack", title: "Stack", code: STACK,
     blurb: "Items enter and leave from the same end; the top pointer follows the last push."},
    {id: "queue", title: "Queue", code: QUEUE,
     blurb: "Items join at the rear and leave from the front; everything behind slides forward."},
    {id: "reverse-list", title: "Reverse linked list", code: REVERSE,
     blurb: "Walk the list once, swinging each next pointer round to point back at the previous node."},
    {id: "bst-insert", title: "BST insert", code: BST,
     blurb: "Each waiting key walks down from the root, turning left or right at every comparison until it finds an empty slot."},
    {id: "binary-heap", title: "Binary heap", code: HEAP,
     blurb: "The heap is a tree stored in an array. A new value is appended, then swapped up while it is smaller than its parent."},
    {id: "hash-table", title: "Hash table", code: HASH,
     blurb: "Each key is hashed to a bucket; collisions extend that bucket's chain. Lookup hashes once and walks one short chain."},
  ],
};

export const COLLECTIONS = [ALGORITHMS, DATA_STRUCTURES];
