// Interactive scenes driven by the retained runtime.
// The page forwards pointer/key events; all bindings, state, and drawing live in Lua.

const HEAD = `if not vis.runtime or not vis.input then
    error("this scene requires a viscript build configured with -Dmodules=interactive")
end
local W, H, U = 960, 540, 90          -- logical pixels, pixels per world unit
local scene = vis.scene {
    width = W, height = H, theme = "pro_black",
    camera = {mode = "fixed", view = "2d", height = H / U},
}
local C = {panel = "#14171f", line = "#3a4152", text = "#e6e8ee", mute = "#7d8596", bar = "#2a3142",
           accent = "#6cb6ff", warn = "#e3b341", danger = "#ff7b72", ok = "#7ee787", alt = "#c8a7ff",
           none = "#00000000"}
local function T(id, s, x, y, size, color, align)
    return scene:text {id = id, text = s, point = {x, y}, size = size or 16, font = "Pretendard",
                       color = color or C.text, align = align or {0.5, 0.5}}
end
-- pointer position (logical px, top-left origin) -> world units
local function world(p) return (p.x - W / 2) / U, (H / 2 - p.y) / U end
local function clicked(p) return p ~= nil and (p.pressed & 1) ~= 0 end
local function held(p) return p ~= nil and (p.buttons & 1) ~= 0 end
`;

const PATH = HEAD + `
T("hint", "click: move the source   ·   hover: trace the path   ·   shift + drag: draw walls", -5.1, 2.5, 13, C.mute, {0, 0.5})
local COLS, ROWS, S = 17, 9, 0.46
local left, top = -COLS * S / 2, 2.1
local cells, wall, N = {}, {}, COLS * ROWS
for r = 1, ROWS do
    for c = 1, COLS do
        local id = (r - 1) * COLS + c
        cells[id] = scene:rectangle {center = {left + (c - 0.5) * S, top - (r - 0.5) * S},
                                     size = {S * 0.9, S * 0.9}, corner = 0.02, fill = C.panel, stroke = C.none}
        wall[id] = false
    end
end
-- two staggered walls with gaps
for r = 1, ROWS do
    if r ~= 7 then wall[(r - 1) * COLS + 6] = true end
    if r ~= 2 then wall[(r - 1) * COLS + 12] = true end
end
local legend = {{C.ok, "source"}, {C.accent, "reached"}, {C.warn, "shortest path"}, {C.line, "wall"}}
for i, l in ipairs(legend) do
    local x = -5.1 + (i - 1) * 1.9
    scene:rectangle {center = {x + 0.1, -2.72}, size = {0.2, 0.2}, fill = l[1], stroke = C.none}
    T("legend-" .. i, l[2], x + 0.3, -2.72, 12, C.mute, {0, 0.5})
end
-- distance shades: accent with decreasing alpha
local SHADE = {"#6cb6ffe0", "#6cb6ffc8", "#6cb6ffb0", "#6cb6ff98", "#6cb6ff84", "#6cb6ff70",
               "#6cb6ff60", "#6cb6ff52", "#6cb6ff46", "#6cb6ff3c", "#6cb6ff34", "#6cb6ff2c"}

local function cell_at(p)
    if not p then return nil end
    local x, y = world(p.position)
    local c = math.floor((x - left) / S) + 1
    local r = math.floor((top - y) / S) + 1
    if c < 1 or c > COLS or r < 1 or r > ROWS then return nil end
    return (r - 1) * COLS + c
end

local src, dist, par = 5 * COLS - COLS + 2, {}, {}
local function bfs()
    dist, par = {}, {}
    if not src then return end
    local q, h = {src}, 1
    dist[src] = 0
    while h <= #q do
        local id = q[h]; h = h + 1
        local r, c = (id - 1) // COLS, (id - 1) % COLS
        local nbs = {}
        if c > 0 then nbs[#nbs + 1] = id - 1 end
        if c < COLS - 1 then nbs[#nbs + 1] = id + 1 end
        if r > 0 then nbs[#nbs + 1] = id - COLS end
        if r < ROWS - 1 then nbs[#nbs + 1] = id + COLS end
        for _, n in ipairs(nbs) do
            if not wall[n] and dist[n] == nil then
                dist[n] = dist[id] + 1; par[n] = id; q[#q + 1] = n
            end
        end
    end
end
bfs()

local wave, hover, drawing, paint = 0, nil, false, true
vis.runtime(scene, {
    fixed_step = 1 / 60, max_steps = 4,
    update = function(ctx, dt)
        local p = ctx:pointer(0)
        local at = cell_at(p)
        local shift = ctx:key("Shift").down
        if clicked(p) and at then
            if shift then
                drawing, paint = true, not wall[at]
            elseif not wall[at] then
                src, wave = at, 0
                bfs()
            end
        end
        if drawing then
            if held(p) then
                if at and at ~= src and wall[at] ~= paint then wall[at] = paint; bfs() end
            else
                drawing = false
            end
        end
        hover = at
        wave = wave + dt * 16
        local path = {}
        if hover and dist[hover] and dist[hover] <= wave then
            local id = hover
            while id do path[id] = true; id = par[id] end
        end
        for id = 1, N do
            local d = dist[id]
            local fill = C.panel
            if wall[id] then fill = C.line
            elseif id == src then fill = C.ok
            elseif path[id] then fill = C.warn
            elseif d and d <= wave then fill = SHADE[math.min(d, #SHADE)] end
            ctx:update(cells[id], {fill = fill})
        end
    end,
})
scene:wait(1)
return scene`;

const STEPPER = HEAD + `
T("hint", "space / → / click: one comparison   ·   A: autoplay   ·   R: reshuffle", -5.1, 2.5, 13, C.mute, {0, 0.5})
local vals = {6, 2, 9, 4, 7, 1, 8, 3, 10, 5}
local n, w, x0, base = #vals, 0.9, -4.5, -1.75
local bars, labels = {}, {}
for i, v in ipairs(vals) do
    local x, h = x0 + (i - 0.5) * w, v * 0.36
    bars[i] = scene:rectangle {center = {x, base + h / 2}, size = {w * 0.7, h}, fill = C.bar, stroke = C.none}
    labels[i] = T("value-" .. i, tostring(v), x, base - 0.25, 14, C.mute)
end
local marker = scene:rectangle {center = {x0 + w, base - 0.55}, size = {w * 1.7, 0.05}, fill = C.warn, stroke = C.none}
local auto_on = T("autoplay", "AUTOPLAY", 5.1, 2.5, 13, C.accent, {1, 0.5})
local done_txt = T("sorted", "sorted", x0 + n * w / 2, base - 0.65, 14, C.ok)

local order, pos = {}, {}       -- order[slot] = bar, pos[bar] = drawn slot (smoothed)
local seed = 7
local function rnd(k) seed = (seed * 1103515245 + 12345) % 2147483648; return seed % k + 1 end
local j, limit, swapped, done, cmp
local function reset(shuffle)
    for i = 1, n do order[i] = i end
    if shuffle then
        for i = n, 2, -1 do local k = rnd(i); order[i], order[k] = order[k], order[i] end
    end
    j, limit, swapped, done, cmp = 1, n, false, false, nil
end
reset(false)
for i = 1, n do pos[i] = i end

local function step()
    if done then return end
    local a, b = order[j], order[j + 1]
    cmp = {a, b}
    if vals[a] > vals[b] then order[j], order[j + 1] = b, a; swapped = true end
    j = j + 1
    if j >= limit then
        limit = limit - 1
        if not swapped or limit <= 1 then done = true; cmp = nil; limit = 0 end
        j, swapped = 1, false
    end
end

local auto, timer, mslot = false, 0, 1.5
vis.runtime(scene, {
    fixed_step = 1 / 60, max_steps = 4,
    update = function(ctx, dt)
        if ctx:key("Space").pressed or ctx:key("ArrowRight").pressed or clicked(ctx:pointer(0)) then step() end
        if ctx:key("A").pressed then auto = not auto; timer = 0 end
        if ctx:key("R").pressed then reset(true) end
        if auto then
            timer = timer + dt
            if timer > 0.28 then timer = 0; step() end
        end
        local k = math.min(1, dt * 14)
        for slot = 1, n do
            local b = order[slot]
            pos[b] = pos[b] + (slot - pos[b]) * k
            local fill = C.bar
            if done or slot > limit then fill = C.ok
            elseif cmp and (b == cmp[1] or b == cmp[2]) then fill = C.warn end
            local dx = (pos[b] - b) * w
            ctx:update(bars[b], {shift = {dx, 0, 0}, fill = fill})
            ctx:update(labels[b], {shift = {dx, 0, 0}})
        end
        local target = done and mslot or (j + 0.5)
        mslot = mslot + (target - mslot) * k
        ctx:update(marker, {shift = {(mslot - 1.5) * w, 0, 0}, opacity = done and 0 or 1})
        ctx:update(auto_on, {opacity = (auto and not done) and 1 or 0})
        ctx:update(done_txt, {opacity = done and 1 or 0})
    end,
})
scene:wait(1)
return scene`;

export const INTERACTIVE = {
  id: "interactive",
  title: "Interactive",
  tags: "pointer · keyboard · retained runtime",
  summary: "Scenes that respond to pointer and keyboard input through a retained Lua runtime.",
  episodes: [
    {id: "shortest-path", title: "Shortest path (BFS)", category: "Pointer", code: PATH, interactive: true,
     blurb: "Click a cell to move the source and watch the BFS wave spread.\nHover any reached cell to trace its shortest path back.\nHold Shift and drag to draw or erase walls."},
    {id: "sort-stepper", title: "Bubble sort stepper", category: "Keyboard", code: STEPPER, interactive: true,
     blurb: "Click the canvas, then press Space or → for one comparison at a time.\nA toggles autoplay, R reshuffles."},
  ],
};
