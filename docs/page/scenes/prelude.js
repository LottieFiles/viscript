// The Lua the algorithm, data-structure and diagram scenes open with (algorithms.js, diagrams.js):
// the scene and its palette, draw-order bands, labels, one phrase beside the action, and tokens.
export const PRELUDE = `local scene = vis.scene {}
local C = {panel = "#14171f", line = "#3a4152", text = "#e6e8ee", mute = "#7d8596", bar = "#2a3142",
           accent = "#6cb6ff", warn = "#e3b341", danger = "#ff7b72", ok = "#7ee787", alt = "#c8a7ff",
           none = "#00000000"}
-- draw-order bands: guides < regions < edges < subjects < tokens < labels < transient focus
local L = {guide = 1, region = 2, edge = 3, subject = 5, token = 6, label = 8, focus = 10}
local function at(x, y) return {1, 0, 0, x, 0, 1, 0, y, 0, 0, 1, 0, 0, 0, 0, 1} end
local function T(parent, id, s, x, y, size, color, align, layer)
    return parent:text {id = id, text = s, point = {x, y}, size = size or 16, font = "Pretendard",
                        color = color or C.text, align = align or {0.5, 0.5}, layer = layer or L.label}
end
-- one short phrase beside the action; the next call fades it out and takes its place, say(nil) fades it
local sayN, sayCur = 0, nil
local function say(s, x, y, color, align)
    if s == nil then
        if sayCur then scene:fade_out(sayCur, {duration = 0.2}); sayCur = nil end
        return
    end
    sayN = sayN + 1
    local nxt = T(scene, "say:" .. sayN, s, x, y, 15, color or C.warn, align, L.focus)
    if sayCur then scene:fade_out(sayCur, {duration = 0.15}) end
    scene:fade_in(nxt, {duration = 0.2})
    sayCur = nxt
end
local function together(...) scene:parallel(...) end
-- a token is one Group (box + label around {0,0}) that moves as a unit
local function token(id, s, x, y, o)
    o = o or {}
    local layer = o.layer or L.token
    local g = scene:group {id = id, matrix = at(x, y)}
    local b
    if o.circle then
        b = g:circle {id = id .. ":box", radius = o.circle, fill = o.fill or C.panel,
                      stroke = o.stroke or C.accent, width = o.width or 2, layer = layer}
    else
        b = g:rectangle {id = id .. ":box", size = o.size or {0.9, 0.48}, corner = 0.04,
                         fill = o.fill or C.panel, stroke = o.stroke or C.accent, width = o.width or 1.5, layer = layer}
    end
    local t = T(g, id .. ":label", s, 0, 0, o.text or 15, o.color or C.text, nil, layer + 2)
    return {g = g, b = b, t = t, x = x, y = y, v = s}
end
-- descriptor that carries a token to (x, y), which it records
local function toward(o, x, y, arc)
    o.x, o.y = x, y
    return {target = o.g, position = {x, y}, arc = arc}
end
`;
