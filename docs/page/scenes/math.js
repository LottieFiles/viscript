// Demo collection: the math behind computer graphics — linear algebra and curves.
// Scenes run on viscript's retained runtime: the update callback recomputes every dependent from one state
// each fixed step, plays an autonomous demo, and hands that state to the pointer while the viewer drags.

const HEAD = `if not vis.runtime or not vis.input then
    error("this scene requires a viscript build configured with -Dmodules=interactive")
end
local W, H, U = 960, 540, 90          -- logical pixels, pixels per world unit
local scene = vis.scene {width = W, height = H, theme = "pro_black", camera = {height = H / U}}
local C = {panel = "#14171f", line = "#3a4152", grid = "#1f2430", text = "#e6e8ee", mute = "#7d8596", bar = "#2a3142",
           accent = "#6cb6ff", warn = "#e3b341", danger = "#ff7b72", ok = "#7ee787", alt = "#c8a7ff", none = "#00000000"}
-- layer bands: background < guides < subjects < handles < labels
local L = {bg = 0, guide = 1, subject = 2, handle = 3, label = 4}
local HOME = {}
-- every text has a stable id; its authored point is its home and the runtime moves it by offset
local function T(id, s, x, y, size, color, align, opacity)
    local t = scene:text {id = id, text = s, point = {x, y}, size = size or 16, font = "Pretendard", layer = L.label,
                          color = color or C.text, align = align or {0.5, 0.5}, opacity = opacity or 1}
    HOME[t] = {x, y}
    return t
end
local function place(ctx, o, x, y, opacity)
    local h = HOME[o]
    ctx:update(o, {shift = {x - h[1], y - h[2], 0}, opacity = opacity or 1})
end
local function hint(s, x, y, align) return T("hint", s, x or -5.1, y or -2.75, 12, C.mute, align or {0, 0.5}) end
local function clamp(x, a, b) return math.max(a, math.min(b, x)) end
local function ease(x) x = clamp(x, 0, 1) return x * x * (3 - 2 * x) end
local function phase(t, a, b) return ease((t - a) / (b - a)) end
-- a unit line that the runtime stretches onto any segment
local function seg(color, width, layer, parent)
    return (parent or scene):line {from = {0, 0}, to = {1, 0}, stroke = color, width = width or 2, layer = layer or L.subject}
end
local function put(ctx, l, ax, ay, bx, by, opacity)
    local dx, dy = bx - ax, by - ay
    ctx:update(l, {origin = {0, 0, 0}, scale = {math.max(math.sqrt(dx * dx + dy * dy), 1e-4), 1, 1},
                   rotation = {0, 0, math.atan(dy, dx)}, shift = {ax, ay, 0}, opacity = opacity or 1})
end
-- an arrow = shaft segment + tip triangle (authored pointing +x at the origin)
local function vec(color, width, parent, layer)
    local p = parent or scene
    return {shaft = seg(color, width or 3, layer, p),
            tip = p:polygon {points = {{0, 0}, {-0.2, 0.09}, {-0.2, -0.09}}, fill = color, stroke = C.none, layer = layer or L.subject}}
end
local function putv(ctx, v, ax, ay, bx, by, opacity)
    local dx, dy = bx - ax, by - ay
    local len = math.sqrt(dx * dx + dy * dy)
    local k = len > 0.2 and (len - 0.16) / len or 0
    put(ctx, v.shaft, ax, ay, ax + dx * k, ay + dy * k, opacity)
    ctx:update(v.tip, {origin = {0, 0, 0}, rotation = {0, 0, math.atan(dy, dx)}, shift = {bx, by, 0},
                       scale = len > 0.2 and {1, 1, 1} or {len / 0.2 + 1e-3, len / 0.2 + 1e-3, 1}, opacity = opacity or 1})
end
local function grid(x0, x1, y0, y1, parent, color)
    local p = parent or scene
    for x = x0, x1 do p:line {from = {x, y0}, to = {x, y1}, stroke = color or C.grid, width = 1, layer = L.bg} end
    for y = y0, y1 do p:line {from = {x0, y}, to = {x1, y}, stroke = color or C.grid, width = 1, layer = L.bg} end
end
-- a drag handle ring, moved by the runtime
local function ring(color, r)
    return scene:circle {radius = r or 0.17, fill = C.none, stroke = color, width = 1.5, layer = L.handle}
end
-- pointer in world units (nil when the pointer is off the canvas)
local function pointer(ctx)
    local p = ctx:pointer(0)
    if not p or p.position.x < 0 or p.position.y < 0 then return nil end
    return {x = (p.position.x - W / 2) / U, y = (H / 2 - p.position.y) / U,
            down = (p.buttons & 1) ~= 0, pressed = (p.pressed & 1) ~= 0}
end
-- picks the nearest handle under a press and feeds it the pointer until release
local function dragger(handles, radius)
    local d = {active = nil, hover = nil, idle = 1e9, used = false, p = nil}
    function d.step(ctx, dt)
        local p = pointer(ctx)
        d.p, d.hover, d.idle = p, nil, d.idle + dt
        if not p then d.active = nil return nil end
        local best, bd = nil, (radius or 0.4) ^ 2
        for i, h in ipairs(handles) do
            local hx, hy = h.get()
            local q = (p.x - hx) ^ 2 + (p.y - hy) ^ 2
            if q < bd then best, bd = i, q end
        end
        d.hover = best
        if p.pressed and best then d.active = best end
        if not p.down then d.active = nil end
        if d.active then handles[d.active].set(p.x, p.y); d.idle, d.used = 0, true end
        return d.active
    end
    return d
end
local function ringstate(ctx, d, i, r, x, y, shown)
    local o = (d.active == i or d.hover == i) and 1 or 0.45
    ctx:update(r, {shift = {x, y, 0}, opacity = (shown == false) and 0 or o})
end
`;

const VECTORS = HEAD + `
grid(-5, 5, -3, 3)
local O = {-1.4, -1.3}
local u, v = {2.4, 0.6}, {0.6, 1.8}
local U1, V1, V2 = vec(C.accent), vec(C.alt), vec(C.alt)
local S, K = vec(C.warn, 4), vec(C.ok, 4)
local lu = T("vec-u", "u", O[1] + 1.4, O[2] + 0.05, 18, C.accent)
local lv = T("vec-v", "v", O[1] - 0.05, O[2] + 0.95, 18, C.alt)
local ls = T("vec-sum", "u + v", O[1] + 3.3, O[2] + 2.75, 18, C.warn)
local lk = T("vec-ku", "k·u", O[1] + 3.2, O[2] + 0.4, 18, C.ok)
local hn = hint("drag the tips of u and v")
local ru, rv = ring(C.accent), ring(C.alt)
-- keep each tip and the sum's tip on screen
local function setter(w, o) return function(x, y)
    w[1] = clamp(clamp(x, -4.9, 4.9) - O[1], -4.9 - O[1] - o[1], 4.9 - O[1] - o[1])
    w[2] = clamp(clamp(y, -2.7, 2.7) - O[2], -2.7 - O[2] - o[2], 2.7 - O[2] - o[2])
end end
local drag = dragger {
    {get = function() return O[1] + u[1], O[2] + u[2] end, set = setter(u, v)},
    {get = function() return O[1] + v[1], O[2] + v[2] end, set = setter(v, u)},
}
-- label a vector beside its middle, on the side given by s (+1 left of the direction, -1 right)
local function side(ctx, lab, ax, ay, dx, dy, s, opacity)
    local len = math.max(math.sqrt(dx * dx + dy * dy), 1e-3)
    place(ctx, lab, ax + dx * 0.5 - dy / len * 0.32 * s, ay + dy * 0.5 + dx / len * 0.32 * s, opacity)
end
local function cross(a, b) return a[1] * b[2] - a[2] * b[1] end
local PERIOD, clock = 11, 0
vis.runtime(scene, {fixed_step = 1 / 60, max_steps = 4, update = function(ctx, dt, elapsed)
    if drag.step(ctx, dt) then
        if clock < 4.6 or clock > 6.4 then clock = 4.6 end   -- hold the sum while dragging
    else
        clock = (clock + dt) % PERIOD
    end
    local t = clock
    local slide = phase(t, 1.2, 2.6) * (1 - phase(t, 10.2, 10.9))
    local sum = phase(t, 3.0, 4.2)
    local fade = 1 - phase(t, 6.4, 7.0)
    local scaling = t > 7.0 and t < 10.2
    local k = scaling and (1 + 1.6 * math.sin((t - 7.0) / 3.2 * 2 * math.pi)) or 1
    local ox, oy = O[1], O[2]
    local su = cross(u, v) >= 0 and -1 or 1       -- u's label goes on the side away from v
    putv(ctx, U1, ox, oy, ox + u[1], oy + u[2], scaling and 0.35 or 1)
    putv(ctx, V1, ox, oy, ox + v[1], oy + v[2], 1 - 0.6 * slide)
    local sx, sy = ox + u[1] * slide, oy + u[2] * slide
    putv(ctx, V2, sx, sy, sx + v[1], sy + v[2], slide > 0.01 and slide * fade or 0)
    putv(ctx, S, ox, oy, ox + (u[1] + v[1]) * sum, oy + (u[2] + v[2]) * sum, sum > 0.01 and fade or 0)
    putv(ctx, K, ox, oy, ox + u[1] * k, oy + u[2] * k, scaling and 1 or 0)
    side(ctx, lu, ox, oy, u[1], u[2], su, scaling and 0.35 or 1)
    side(ctx, lv, ox, oy, v[1], v[2], -su, 1 - 0.6 * slide)
    local sl = math.max(math.sqrt((u[1] + v[1]) ^ 2 + (u[2] + v[2]) ^ 2), 1e-3)
    -- u + v is labelled just past its tip, leaning away from u (where v's copy runs)
    local sx1, sy1 = (u[1] + v[1]) / sl, (u[2] + v[2]) / sl
    local nx, ny = -sy1, sx1
    if nx * u[1] + ny * u[2] > 0 then nx, ny = -nx, -ny end
    place(ctx, ls, clamp(ox + u[1] + v[1] + sx1 * 0.2 + nx * 0.5, -4.7, 4.7),
          clamp(oy + u[2] + v[2] + sy1 * 0.2 + ny * 0.5, -2.75, 2.75), sum * fade)
    local ul = math.max(math.sqrt(u[1] ^ 2 + u[2] ^ 2), 1e-3)
    local kk = k + (k >= 0 and 0.45 or -0.45) / ul
    place(ctx, lk, ox + u[1] * kk, oy + u[2] * kk + 0.12, scaling and 1 or 0)
    ringstate(ctx, drag, 1, ru, ox + u[1], oy + u[2], not scaling)
    ringstate(ctx, drag, 2, rv, ox + v[1], oy + v[2])
    ctx:update(hn, {opacity = drag.used and 0 or 1})
end})
scene:wait(1)
return scene`;

const DOT = HEAD + `
grid(-5, 1, -3, 3)
local O = {-2.4, 0}
local LU = 3
local U1 = vec(C.accent, 4)
local V1 = vec(C.alt, 3)
local projP = seg(C.ok, 6, L.guide); local projN = seg(C.danger, 6, L.guide)
local drop = scene:line {from = {0, 0}, to = {1, 0}, stroke = "#e6e8ee66", width = 1.5, dash = {5, 5}, layer = L.guide}
local sq1, sq2 = seg(C.mute, 1.5, L.guide), seg(C.mute, 1.5, L.guide)
T("dot-u", "u", O[1] + LU + 0.3, O[2] + 0.02, 18, C.accent)
local lv = T("dot-v", "v", O[1] + 1.6, O[2] + 1.7, 18, C.alt)
-- gauge: the bar is v's signed shadow on u, so its height is u·v / |u|
local gx, gy = 3.0, 0
scene:line {from = {gx - 0.6, gy}, to = {gx + 0.6, gy}, stroke = C.line, width = 1.5, layer = L.guide}
local bar = scene:rectangle {center = {gx, gy + 0.5}, size = {0.7, 1}, fill = C.ok, stroke = C.none, layer = L.subject}
T("dot-gauge", "u · v", gx, 2.62, 15, C.mute)
local sg = {T("dot-pos", "u · v > 0", gx + 0.7, gy + 0.3, 15, C.ok, {0, 0.5}),
            T("dot-zero", "u · v = 0", gx + 0.7, gy - 0.1, 15, C.text, {0, 0.5}),
            T("dot-neg", "u · v < 0", gx + 0.7, gy - 0.5, 15, C.danger, {0, 0.5})}
local hn = hint("drag the tip of v", 5.1, -2.75, {1, 0.5})
local rv = ring(C.alt)
local th, LV = 0.9, 2.2
local drag = dragger {{get = function() return O[1] + LV * math.cos(th), O[2] + LV * math.sin(th) end,
                       set = function(x, y)
                           local dx, dy = x - O[1], y - O[2]
                           th, LV = math.atan(dy, dx), clamp(math.sqrt(dx * dx + dy * dy), 0.8, 2.4)
                       end}}
vis.runtime(scene, {fixed_step = 1 / 60, max_steps = 4, update = function(ctx, dt, elapsed)
    if not drag.step(ctx, dt) and drag.idle > 2.5 then th = th + dt * 0.55 end
    local vx, vy = LV * math.cos(th), LV * math.sin(th)
    local ox, oy = O[1], O[2]
    local d = LU * vx                     -- u · v with u = (LU, 0)
    local proj = d / LU                   -- signed length of v's shadow on u
    putv(ctx, U1, ox, oy, ox + LU, oy)
    putv(ctx, V1, ox, oy, ox + vx, oy + vy)
    put(ctx, projP, ox, oy, ox + math.max(proj, 1e-3), oy, proj > 0 and 1 or 0)
    put(ctx, projN, ox, oy, ox + math.min(proj, -1e-3), oy, proj < 0 and 1 or 0)
    put(ctx, drop, ox + vx, oy + vy, ox + proj, oy + (math.abs(vy) < 1e-3 and 1e-3 or 0))
    -- right-angle mark at the foot of the drop
    local m, mk = 0.16, (math.abs(vx) > 0.3 and math.abs(vy) > 0.3) and 0.8 or 0
    local fx, fy, sx, sy = ox + proj, oy, vx > 0 and -1 or 1, vy > 0 and 1 or -1
    put(ctx, sq1, fx + sx * m, fy, fx + sx * m, fy + sy * m, mk)
    put(ctx, sq2, fx + sx * m, fy + sy * m, fx, fy + sy * m, mk)
    local lr = (LV + 0.42) / LV
    place(ctx, lv, ox + vx * lr, oy + vy * lr)
    ctx:update(bar, {origin = {gx, gy, 0}, scale = {1, proj + (proj >= 0 and 1e-3 or -1e-3), 1},
                     fill = proj >= 0 and C.ok or C.danger})
    local k = math.abs(vx / LV) < 0.06 and 2 or (vx > 0 and 1 or 3)
    for i, s in ipairs(sg) do place(ctx, s, gx + 0.7, gy + clamp(proj, -2.2, 2.2) * 0.5, i == k and 1 or 0) end
    ringstate(ctx, drag, 1, rv, ox + vx, oy + vy)
    ctx:update(hn, {opacity = drag.used and 0 or 1})
end})
scene:wait(1)
return scene`;

const MATRIX = HEAD + `
grid(-6, 6, -4, 4, nil, "#151922")
-- nested groups: rotation(φ) · scale(σ1, σ2) · rotation(θ)  =  any 2x2 matrix (its SVD)
local g1 = scene:group {}
local g2 = g1:group {}
local g3 = g2:group {}
for x = -6, 6 do g3:line {from = {x, -6}, to = {x, 6}, stroke = x == 0 and C.mute or "#2a3a55", width = x == 0 and 1.5 or 1, layer = L.guide} end
for y = -6, 6 do g3:line {from = {-6, y}, to = {6, y}, stroke = y == 0 and C.mute or "#2a3a55", width = y == 0 and 1.5 or 1, layer = L.guide} end
local square = g3:rectangle {center = {0.5, 0.5}, size = {1, 1}, fill = "#e3b34140", stroke = C.warn, width = 1.5, layer = L.subject}
local I1 = vec(C.ok, 4); local J1 = vec(C.danger, 4)
local li = T("mat-i", "î", 1.2, -0.25, 18, C.ok)
local lj = T("mat-j", "ĵ", 1.0, 0.6, 18, C.danger)
local ri, rj = ring(C.ok), ring(C.danger)
local M = {
    {"rotation 30°", 0.866, -0.5, 0.5, 0.866},
    {"shear", 1, 1, 0, 1},
    {"scale 1.6 × 0.6", 1.6, 0, 0, 0.6},
    {"reflection", -1, 0, 0, 1},
    {"det = 0: the plane collapses to a line", 1, 0.5, 0.8, 0.4},
}
-- opaque plates keep the moving grid out of the labels
scene:rectangle {center = {-2.0, 2.6}, size = {6.4, 0.5}, fill = "#000000", stroke = C.none, layer = L.handle}
scene:rectangle {center = {-3.55, -2.6}, size = {3.3, 0.5}, fill = "#000000", stroke = C.none, layer = L.handle}
local labels = {}
for i, m in ipairs(M) do
    labels[i] = T("mat-preset-" .. i, string.format("%s   [%g  %g ; %g  %g]   det = %.2f", m[1], m[2], m[3], m[4], m[5],
                  m[2] * m[5] - m[3] * m[4]), -5.1, 2.6 - (i - 1) * 0.32, 14, C.accent, {0, 0.5})
end
local dl = {T("mat-det-pos", "det > 0: orientation kept", -5.1, 1.0, 14, C.warn, {0, 0.5}),
            T("mat-det-zero", "det = 0: the plane collapses", -5.1, 0.68, 14, C.text, {0, 0.5}),
            T("mat-det-neg", "det < 0: the plane is flipped", -5.1, 0.36, 14, C.danger, {0, 0.5})}
local hn = T("mat-hint", "drag î or ĵ  ·  square area = |det|", -5.1, -2.6, 12, C.mute, {0, 0.5})
local function svd(a, b, c, d)
    local E, F, G, Hh = (a + d) / 2, (a - d) / 2, (c + b) / 2, (c - b) / 2
    local Q, R = math.sqrt(E * E + Hh * Hh), math.sqrt(F * F + G * G)
    local a1, a2 = math.atan(G, F), math.atan(Hh, E)
    return (a2 + a1) / 2, Q + R, Q - R, (a2 - a1) / 2
end
-- columns: î -> (a, c), ĵ -> (b, d)
local man = {1, 0, 0, 1}
local cur = {1, 0, 0, 1}
local function snap(x) local s = math.floor(x * 2 + 0.5) / 2 return math.abs(x - s) < 0.1 and s or x end
local drag = dragger {
    {get = function() return cur[1], cur[3] end, set = function(x, y) man[1], man[3] = snap(clamp(x, -4.5, 4.5)), snap(clamp(y, -2.2, 2.2)) end},
    {get = function() return cur[2], cur[4] end, set = function(x, y) man[2], man[4] = snap(clamp(x, -4.5, 4.5)), snap(clamp(y, -2.2, 2.2)) end},
}
local STEP, clock, w = 4.4, 0, 0
vis.runtime(scene, {fixed_step = 1 / 60, max_steps = 4, update = function(ctx, dt, elapsed)
    if drag.step(ctx, dt) then
        if w < 1 then man = {cur[1], cur[2], cur[3], cur[4]} end
        w, clock = 1, 0
    elseif drag.idle > 4 then
        w = math.max(0, w - dt)         -- ease back to the identity, then resume the presets
    end
    local i, s = 1, 0
    if w == 0 then
        clock = clock + dt
        i = math.floor(clock / STEP) % #M + 1
        local t = clock % STEP
        s = phase(t, 0.3, 1.6) * (1 - phase(t, 3.2, 4.2))
    end
    local m, e = M[i], ease(w)
    local auto = {1 + (m[2] - 1) * s, m[3] * s, m[4] * s, 1 + (m[5] - 1) * s}
    for k = 1, 4 do
        local idk = (k == 1 or k == 4) and 1 or 0
        cur[k] = w > 0 and (idk + (man[k] - idk) * e) or auto[k]
    end
    local a, b, c, d = cur[1], cur[2], cur[3], cur[4]
    local phi, s1, s2, th = svd(a, b, c, d)
    if math.abs(s2) < 0.01 then s2 = s2 < 0 and -0.01 or 0.01 end
    ctx:update(g1, {origin = {0, 0, 0}, rotation = {0, 0, phi}})
    ctx:update(g2, {origin = {0, 0, 0}, scale = {s1, s2, 1}})
    ctx:update(g3, {origin = {0, 0, 0}, rotation = {0, 0, th}})
    local det = a * d - b * c
    ctx:update(square, {fill = det < -0.02 and "#ff7b7240" or "#e3b34140"})
    putv(ctx, I1, 0, 0, a, c); putv(ctx, J1, 0, 0, b, d)
    local function tiplabel(lab, x, y)
        local len = math.max(math.sqrt(x * x + y * y), 1e-3)
        place(ctx, lab, x + x / len * 0.32, y + y / len * 0.32)
    end
    tiplabel(li, a, c); tiplabel(lj, b, d)
    ringstate(ctx, drag, 1, ri, a, c); ringstate(ctx, drag, 2, rj, b, d)
    for k = 1, #M do place(ctx, labels[k], -5.1, 2.6, (w == 0 and k == i) and 1 or 0) end
    local dk = math.abs(det) < 0.02 and 2 or (det > 0 and 1 or 3)
    for k = 1, 3 do place(ctx, dl[k], -5.1, 2.6, (w > 0 and k == dk) and 1 or 0) end
end})
scene:wait(1)
return scene`;

const EIGEN = HEAD + `
local N, R = 24, 0.8
local O = {0, -0.1}
-- A = [2 1 ; 1 2]: eigenvectors (1,1) with λ = 3 and (1,−1) with λ = 1
local A = {2, 1, 1, 2}
scene:line {id = "eig-dir-1", from = {O[1] - 3.2, O[2] - 2.6}, to = {O[1] + 2.6, O[2] + 2.6}, stroke = "#e3b34155", width = 1.5, dash = {6, 6}, layer = L.guide}
scene:line {id = "eig-dir-2", from = {O[1] - 2.6, O[2] + 2.6}, to = {O[1] + 2.6, O[2] - 2.6}, stroke = "#e3b34155", width = 1.5, dash = {6, 6}, layer = L.guide}
scene:circle {center = O, radius = R, fill = C.none, stroke = C.line, width = 1, layer = L.guide}
local vs, eig = {}, {}
for i = 1, N do
    eig[i] = (i - 1) % 6 == 3
    vs[i] = vec(eig[i] and C.warn or "#6cb6ffaa", eig[i] and 3.5 or 2)
end
T("eig-l3", "λ = 3", O[1] + 3.3, O[2] + 2.45, 15, C.warn)
T("eig-l1", "λ = 1", O[1] - 3.3, O[2] + 2.45, 15, C.warn)
T("eig-a", "A = [2 1 ; 1 2]", 5.1, 2.62, 15, C.mute, {1, 0.5})
-- probe: the viewer points at a direction x, and A x is drawn from the same origin
local px = vec(C.text, 3, nil, L.handle)
local pAx = {vec(C.ok, 3.5, nil, L.subject), vec(C.warn, 3.5, nil, L.subject)}   -- turning / kept direction
local lx = T("eig-x", "x", 0.75, -0.1, 16, C.text)
local lax = {T("eig-ax", "Ax", 2.1, -0.1, 16, C.ok), T("eig-ax-3", "Ax = 3x", 2.1, -0.5, 16, C.warn),
             T("eig-ax-1", "Ax = x", 2.1, -0.9, 16, C.warn)}
local hn = hint("point anywhere to test a direction")
local PERIOD, clock, idle = 7, 1, 1e9
vis.runtime(scene, {fixed_step = 1 / 60, max_steps = 4, update = function(ctx, dt, elapsed)
    local p = pointer(ctx)
    local probing = p ~= nil and math.abs(p.x - O[1]) + math.abs(p.y - O[2]) > 0.15
    idle = probing and 0 or idle + dt
    clock = (clock + dt) % PERIOD
    local s = phase(clock, 1, 3) * (1 - phase(clock, 6, 6.9))
    local field = probing and 0.25 or 1
    for i = 1, N do
        local a = (i - 1) / N * 2 * math.pi
        local x, y = R * math.cos(a), R * math.sin(a)
        local mx, my = A[1] * x + A[2] * y, A[3] * x + A[4] * y
        putv(ctx, vs[i], O[1], O[2], O[1] + x + (mx - x) * s, O[2] + y + (my - y) * s, eig[i] and (probing and 0.6 or 1) or field)
    end
    local op = probing and 1 or 0
    local a = probing and math.atan(p.y - O[2], p.x - O[1]) or 0
    -- a gentle snap so the exact eigen-directions are easy to land on
    for k = 0, 3 do
        local e = math.pi / 4 + k * math.pi / 2
        local dd = math.atan(math.sin(a - e), math.cos(a - e))
        if math.abs(dd) < 0.05 then a = e end
    end
    local x, y = R * math.cos(a), R * math.sin(a)
    local mx, my = A[1] * x + A[2] * y, A[3] * x + A[4] * y
    local ml = math.sqrt(mx * mx + my * my)
    local fixed = math.abs(x * my - y * mx) / (R * ml) < 0.01
    putv(ctx, pAx[1], O[1], O[2], O[1] + mx, O[2] + my, fixed and 0 or op)
    putv(ctx, pAx[2], O[1], O[2], O[1] + mx, O[2] + my, fixed and op or 0)
    putv(ctx, px, O[1], O[2], O[1] + x, O[2] + y, op)
    local nx, ny = -math.sin(a) * 0.28, math.cos(a) * 0.28
    place(ctx, lx, O[1] + x * 0.55 + nx, O[2] + y * 0.55 + ny, op)
    local which = not fixed and 1 or (ml > 2 * R and 2 or 3)
    local lr = 1 + (which == 1 and 0.38 or 0.62) / ml
    for k = 1, 3 do place(ctx, lax[k], O[1] + mx * lr, O[2] + my * lr, k == which and op or 0) end
    ctx:update(hn, {opacity = idle > 6 and 1 or 0.35})
end})
scene:wait(1)
return scene`;

const BEZIER = HEAD + `
local P = {{-4, -1.6}, {-2.4, 1.9}, {1.6, 1.9}, {3.6, -1.5}}
local NS = 64
local function lerp(a, b, t) return {a[1] + (b[1] - a[1]) * t, a[2] + (b[2] - a[2]) * t} end
local function decas(t)
    local l1 = {lerp(P[1], P[2], t), lerp(P[2], P[3], t), lerp(P[3], P[4], t)}
    local l2 = {lerp(l1[1], l1[2], t), lerp(l1[2], l1[3], t)}
    return l1, l2, lerp(l2[1], l2[2], t)
end
local hull, ghost, ink = {}, {}, {}
for i = 1, 3 do hull[i] = seg(C.line, 1.5, L.guide) end
for i = 1, NS do ghost[i] = seg(C.bar, 2, L.guide) end
for i = 1, NS do ink[i] = scene:line {from = {0, 0}, to = {1, 0}, stroke = C.accent, width = 4, layer = L.subject} end
local s1 = {seg("#c8a7ffcc", 2), seg("#c8a7ffcc", 2)}
local s2 = seg(C.ok, 2.5)
local d1, d2 = {}, {}
for i = 1, 3 do d1[i] = scene:circle {center = {0, 0}, radius = 0.07, fill = C.alt, stroke = C.none, layer = L.subject} end
for i = 1, 2 do d2[i] = scene:circle {center = {0, 0}, radius = 0.08, fill = C.ok, stroke = C.none, layer = L.subject} end
local pen = scene:circle {center = {0, 0}, radius = 0.11, fill = C.warn, stroke = C.none, layer = L.handle}
local cp, lp, rp = {}, {}, {}
for i, p in ipairs(P) do
    cp[i] = scene:circle {center = {0, 0}, radius = 0.1, fill = C.text, stroke = C.none, layer = L.handle}
    rp[i] = ring(C.text, 0.2)
    lp[i] = T("bez-p" .. (i - 1), "P" .. (i - 1), p[1], p[2] + (p[2] > 0 and 0.34 or -0.34), 13, C.mute)
end
scene:line {from = {-4, -2.6}, to = {4, -2.6}, stroke = C.line, width = 2, layer = L.guide}
local tbar = scene:line {from = {-4, -2.6}, to = {4, -2.6}, stroke = C.warn, width = 3, layer = L.subject}
T("bez-t", "t", -4.3, -2.6, 14, C.mute)
local hn = hint("drag P0–P3", 5.1, -2.6, {1, 0.5})
local handles = {}
for i = 1, 4 do
    handles[i] = {get = function() return P[i][1], P[i][2] end,
                  set = function(x, y) P[i] = {clamp(x, -4.9, 4.9), clamp(y, -1.95, 2.5)} end}
end
local drag = dragger(handles, 0.35)
local PERIOD, clock = 5, 0
vis.runtime(scene, {fixed_step = 1 / 60, max_steps = 4, update = function(ctx, dt, elapsed)
    drag.step(ctx, dt)
    clock = (clock + dt) % PERIOD
    local t = clamp((clock - 0.4) / 3.8, 0, 1)
    local prev = P[1]
    for i = 1, NS do
        local _, _, q = decas(i / NS)
        put(ctx, ghost[i], prev[1], prev[2], q[1], q[2])
        local a = clamp(t * NS - (i - 1), 0, 1)        -- the ink grows exactly to the pen
        put(ctx, ink[i], prev[1], prev[2], prev[1] + (q[1] - prev[1]) * a, prev[2] + (q[2] - prev[2]) * a, a > 0 and 1 or 0)
        prev = q
    end
    for i = 1, 3 do put(ctx, hull[i], P[i][1], P[i][2], P[i + 1][1], P[i + 1][2]) end
    for i = 1, 4 do
        ctx:update(cp[i], {shift = {P[i][1], P[i][2], 0}})
        ringstate(ctx, drag, i, rp[i], P[i][1], P[i][2])
        -- label above or below the point, away from the curve's interior
        local above = (i == 1 or i == 4) and P[i][2] > (P[i == 1 and 2 or 3][2]) or (i == 2 or i == 3) and
                      P[i][2] >= (P[i == 2 and 1 or 4][2])
        place(ctx, lp[i], P[i][1], P[i][2] + (above and 0.36 or -0.36))
    end
    local l1, l2, p = decas(t)
    for i = 1, 3 do ctx:update(d1[i], {shift = {l1[i][1], l1[i][2], 0}}) end
    for i = 1, 2 do
        put(ctx, s1[i], l1[i][1], l1[i][2], l1[i + 1][1], l1[i + 1][2])
        ctx:update(d2[i], {shift = {l2[i][1], l2[i][2], 0}})
    end
    put(ctx, s2, l2[1][1], l2[1][2], l2[2][1], l2[2][2])
    ctx:update(pen, {shift = {p[1], p[2], 0}})
    ctx:update(tbar, {origin = {-4, -2.6, 0}, scale = {math.max(t, 0.001), 1, 1}})
    ctx:update(hn, {opacity = drag.used and 0 or 1})
end})
scene:wait(1)
return scene`;

export const MATH = {
  id: "math",
  title: "Math",
  episodes: [
    {id: "vectors", title: "Vectors", code: VECTORS, interactive: true,
     blurb: "Addition is tip-to-tail; scaling stretches a vector and, for negative k, flips it."},
    {id: "dot-product", title: "Dot product", code: DOT, interactive: true,
     blurb: "As v turns, its shadow on u is the dot product (up to |u|): positive, zero at 90°, negative beyond."},
    {id: "matrix-transform", title: "Matrices as transformations", code: MATRIX, interactive: true,
     blurb: "The grid is carried by a 2×2 matrix: î and ĵ land on its columns. The yellow square's area is |det|."},
    {id: "eigenvectors", title: "Eigenvectors", code: EIGEN, interactive: true,
     blurb: "Apply A to vectors on a circle. Almost all of them change direction — except along the two dashed eigen-directions."},
    {id: "bezier", title: "Bézier curves", code: BEZIER, interactive: true,
     blurb: "de Casteljau's algorithm: interpolate between control points, then between those points, until one remains."},
  ],
};
