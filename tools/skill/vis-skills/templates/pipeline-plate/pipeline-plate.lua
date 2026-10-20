-- Question: why do sprites added after load() never reach the screen?
-- Answer: load() copies items.size() into count once; update(dt) grows items every frame;
-- draw() still loops while i < count, so the later sprites are never drawn.
-- Format: pipeline plate. The stage spine runs down the left; each section on the right explains
-- one stage or a group of stages, aligned with its nodes. Every section is complete at t = 0.
-- MODE "loops": each section is a child Scene that loops on its own clock and returns to its t = 0 pose.
-- MODE "sequence": the sections replay one after another while the spine marks the active stage.
-- The camera never moves. Structure is ink; `danger` carries the one finding; `focus` is transient.
-- Scenario (illustrative): a sprite batch whose load() stores the sprite count once, whose
-- update(dt) appends spawned sprites, and whose draw() loops up to the stored count. A VS Code
-- Scene cites its real source through the manifest's links, never in comments.
-- Edit LOADED, SPAWNS, or FRAMES: every row, ruler, label, and caption is derived from the trace.
local MODE = "loops"
local THEME = "adaptive"

-- Trace ------------------------------------------------------------------------------------------
local LOADED = 3          -- sprites in the level when load() runs
local SPAWNS = {2, 4}     -- frames whose update(dt) appends one sprite
local FRAMES = 6          -- frames shown on the update(dt) axis

local count = LOADED      -- load(): count = items.size(), never written again
local sizes = {[0] = LOADED}
for frame = 1, FRAMES do
    sizes[frame] = sizes[frame - 1]
    for _, spawn in ipairs(SPAWNS) do
        if spawn == frame then sizes[frame] = sizes[frame] + 1 end
    end
end
local SIZE = sizes[FRAMES]
local DRAWN = math.min(count, SIZE)  -- draw(): for (i = 0; i < count; i++)
assert(DRAWN < SIZE, "the trace must leave sprites undrawn")

-- Pixel plan: 1 world unit = 1 px, top-left origin through P() in the root and in every section ----
local W, MARGIN, GUTTER = 1280, 48, 28
local HEADER = {title = MARGIN + 11, answer = MARGIN + 45}  -- text centres; the title starts at the margin
local TOP = MARGIN + 83                           -- below the title and the answer line
local NODE = {x = MARGIN + 140, w = 232, h = 76}  -- spine column: node centre x and size, with
                                                 -- room for the repeat route inside the margin
local BODY = 376                                  -- sections start here
local LABEL_X, X0 = BODY + 12, BODY + 112         -- row labels, first slot
local SLOT = {w = 44, pitch = 56}
local SECTIONS = {
    {id = "load", stages = {1}, h = 196},
    {id = "update", stages = {2}, h = 220},
    -- draw() and the screen share one band: rows are the centres of their own evidence rows,
    -- and the band ends at the screen frame so the bottom margin matches the others.
    {id = "draw", stages = {3, 4}, h = 272, rows = {75, 225}},
}
local H = TOP + MARGIN
for k, section in ipairs(SECTIONS) do
    section.top = H - MARGIN
    H = H + section.h + (k < #SECTIONS and GUTTER or 0)
end

local LAYER = {rule = 4, slot = 8, body = 12, mark = 16, head = 20, text = 30}

local function P(x, y) return {x - W / 2, H / 2 - y} end
local function M(x, y) return {1, 0, 0, x - W / 2, 0, 1, 0, H / 2 - y, 0, 0, 1, 0, 0, 0, 0, 1} end
local function slotX(i) return X0 + SLOT.w / 2 + i * SLOT.pitch end  -- zero-based slot centre
local function rulerX(i) return slotX(i) - SLOT.pitch / 2 end         -- boundary before slot i

local function text(parent, id, value, x, y, options)
    options = options or {}
    return parent:text {
        id = id, text = value, point = P(x, y), align = options.align or {0, 0.5},
        role = options.role or "text", size = options.size, fill = options.fill or "foreground",
        opacity = options.opacity or 1, layer = LAYER.text,
    }
end

-- One glyph per sprite identity, authored around {0, 0} inside a Group placed at its slot.
local GLYPHS = {"circle", "triangle", "square", "diamond", "hexagon"}
local function polygon(sides, radius, turn)
    local points = {}
    for k = 0, sides - 1 do
        local a = turn + 2 * math.pi * k / sides
        points[#points + 1] = {radius * math.cos(a), radius * math.sin(a)}
    end
    return points
end
local function sprite(parent, id, k, x, y, radius, style)
    local g = parent:group {id = id, matrix = M(x, y), opacity = style.opacity or 1}
    local paint = {fill = style.fill or "#00000000", stroke = style.stroke or "#00000000",
        width = style.width or 0, dash = style.dash, layer = LAYER.body}
    local kind = GLYPHS[(k - 1) % #GLYPHS + 1]
    local shape
    if kind == "circle" then
        shape = vis.merge(paint, {center = {0, 0}, radius = radius})
        g:circle(vis.merge(shape, {id = id .. ":glyph"}))
    elseif kind == "square" then
        shape = vis.merge(paint, {center = {0, 0}, size = {1.6 * radius, 1.6 * radius}})
        g:rectangle(vis.merge(shape, {id = id .. ":glyph"}))
    else
        local sides = ({triangle = 3, diamond = 4, hexagon = 6})[kind]
        local turn = ({triangle = math.pi / 2, diamond = math.pi / 2, hexagon = 0})[kind]
        g:polygon(vis.merge(paint, {id = id .. ":glyph", points = polygon(sides, radius * 1.15, turn)}))
    end
    return g
end

local function slots(parent, prefix, n, y)
    for i = 0, n - 1 do
        parent:rectangle {
            id = prefix .. ":slot:" .. i, center = P(slotX(i), y), size = {SLOT.w, SLOT.w},
            stroke = "border", width = 1, layer = LAYER.slot,
        }
        text(parent, prefix .. ":index:" .. i, tostring(i), slotX(i), y + SLOT.w / 2 + 14,
            {align = {0.5, 0.5}, size = 11, fill = "muted"})
    end
end

-- A code fragment in ink; only the decisive identifier takes the finding's color.
local function fragment(parent, id, source, x, y, decisive)
    local block = parent:code {id = id, text = source, language = "plain", layer = LAYER.text}
    local bounds = parent:bounds(block)
    block:move_to(P(x + bounds.width / 2, y))
    if decisive then
        for _, token in ipairs(block:search(decisive, {match = "all", whole = true})) do
            parent:set {target = token, fill = "danger"}
        end
    end
    return block
end

-- Root: the claim and the spine --------------------------------------------------------------------
local root = vis.scene {
    width = W, height = H, loop = true, theme = THEME,
    camera = {height = H},
}
text(root, "title", "Why sprites added after load() never appear", MARGIN, HEADER.title, {role = "h3"})
text(root, "answer", "load() fixes count once; update(dt) grows items every frame; draw() still stops at count.",
    MARGIN, HEADER.answer, {fill = "muted"})

local STAGES = {
    {id = "load", name = "load()", when = "once, on load"},
    {id = "update", name = "update(dt)", when = "every frame"},
    {id = "draw", name = "draw()", when = "every frame"},
    {id = "screen", name = "screen", when = "pixels"},
}
for _, section in ipairs(SECTIONS) do
    for j, stage in ipairs(section.stages) do
        STAGES[stage].y = section.top + (section.rows and section.rows[j] or section.h / 2)
    end
end
local nodes = {}
for k, stage in ipairs(STAGES) do
    local g = root:group {id = "node:" .. stage.id}
    nodes[k] = g:rectangle {
        id = "node:" .. stage.id .. ":body", center = P(NODE.x, stage.y), size = {NODE.w, NODE.h},
        fill = "background", stroke = "foreground", width = 1.25, layer = LAYER.body,
    }
    text(g, "node:" .. stage.id .. ":name", stage.name, NODE.x, stage.y - 12,
        {role = "code", size = 16, align = {0.5, 0.5}})
    text(g, "node:" .. stage.id .. ":when", stage.when, NODE.x, stage.y + 16,
        {size = 13, fill = "muted", align = {0.5, 0.5}})
end
-- Each edge carries the value that crosses it; the finding keeps its color across the plate.
local EDGES = {
    {"count = " .. count, "danger"},
    {"items.size() = " .. SIZE, "foreground"},
    {DRAWN .. " of " .. SIZE .. " drawn", "danger"},
}
for k, edge in ipairs(EDGES) do
    local from, to = STAGES[k].y + NODE.h / 2 + 6, STAGES[k + 1].y - NODE.h / 2 - 6
    root:arrow {
        id = "edge:" .. k, from = P(NODE.x, from), to = P(NODE.x, to),
        stroke = "foreground", fill = "foreground", width = 1, tip = 7, layer = LAYER.body,
    }
    text(root, "edge:" .. k .. ":value", edge[1], NODE.x + 12, (from + to) / 2,
        {role = "code", size = 13, fill = edge[2]})
end
-- update(dt) and draw() repeat every frame: one dashed return route spans the pair.
local LOOP_X = NODE.x - NODE.w / 2
local repeatRoute = root:route {
    id = "edge:every-frame", points = {
        P(LOOP_X, STAGES[3].y + 14), P(LOOP_X - 22, STAGES[3].y + 14),
        P(LOOP_X - 22, STAGES[2].y - 14), P(LOOP_X, STAGES[2].y - 14)},
    stroke = "muted", width = 1, tip = 7, dash = {5, 4}, layer = LAYER.body,
}

-- Hairlines separate the sections; no boxes or fills.
for k = 1, #SECTIONS - 1 do
    local y = SECTIONS[k].top + SECTIONS[k].h + GUTTER / 2
    root:line {id = "rule:" .. k, from = P(BODY, y), to = P(W - MARGIN, y), stroke = "border", width = 1, layer = LAYER.rule}
end

-- A section: a child Scene whose camera shows the root's pixel band, so P() and M() hold inside it.
local function band(section)
    local h = section.h
    section.scene = vis.scene {
        width = W - BODY, height = h, loop = true, theme = THEME,
        camera = {height = h,
            target = {(BODY + W) / 2 - W / 2, H / 2 - section.top - h / 2}},
    }
    return section.scene
end

-- ① load(): the level's sprites fill items, and items.size() is copied into count once --------
local ONE = band(SECTIONS[1])
local Y1 = {items = SECTIONS[1].top + 64, code = SECTIONS[1].top + 150}
local REGISTER = {x = X0 + 440, w = 72, h = 44}
text(ONE, "load:items", "items", LABEL_X, Y1.items, {fill = "muted"})
slots(ONE, "load", LOADED, Y1.items)
local loaded = {}
for i = 0, LOADED - 1 do
    loaded[i] = sprite(ONE, "load:sprite:" .. i, i + 1, slotX(i), Y1.items, 12, {fill = "foreground"})
end
text(ONE, "load:count", "count", REGISTER.x + REGISTER.w / 2, Y1.items - 40, {fill = "muted", align = {0.5, 0.5}})
local register = ONE:rectangle {
    id = "load:register", center = P(REGISTER.x + REGISTER.w / 2, Y1.items), size = {REGISTER.w, REGISTER.h},
    stroke = "danger", width = 1.5, layer = LAYER.slot,
}
-- The copied value: born at the end of items, it moves into count and stays there.
local copyFrom = M(rulerX(LOADED) + 24, Y1.items)
local copied = ONE:group {id = "load:copied", matrix = M(REGISTER.x + REGISTER.w / 2, Y1.items)}
copied:text {id = "load:copied:value", text = tostring(count), point = {0, 0}, align = {0.5, 0.5},
    role = "h3", fill = "danger", layer = LAYER.text}
local fixed = text(ONE, "load:fixed", "never written again", REGISTER.x + REGISTER.w + 16, Y1.items,
    {size = 13, fill = "danger"})
fragment(ONE, "load:code", "count = items.size();", X0, Y1.code, "count")

local function playLoad(S)
    local clear = {{target = copied, opacity = 0}, {target = fixed, opacity = 0}, {target = register, stroke = "border"}}
    for i = 0, LOADED - 1 do clear[#clear + 1] = {target = loaded[i], opacity = 0} end
    S:play(clear, 0.4, "ease_out")
    S:play({target = copied, transform = copyFrom}, 0.05, "linear")
    local fill = {}
    for i = 0, LOADED - 1 do fill[#fill + 1] = {target = loaded[i], opacity = 1} end
    S:play(fill, 0.3, "ease_out", 0.18)
    S:wait(0.4)
    S:play({target = copied, opacity = 1}, 0.25, "ease_out")
    S:play({target = copied, transform = M(REGISTER.x + REGISTER.w / 2, Y1.items)}, 0.7, "ease_in_out")
    S:play({{target = register, stroke = "danger"}, {target = fixed, opacity = 1}}, 0.3, "ease_out")
end

-- ② update(dt): items grows on spawn frames; count still says LOADED ----------------------------
local TWO = band(SECTIONS[2])
local Y2 = {frame = SECTIONS[2].top + 40, items = SECTIONS[2].top + 118, code = SECTIONS[2].top + 192}
local AXIS = {x0 = X0 + 6, step = 64}
local function frameX(f) return AXIS.x0 + f * AXIS.step end
text(TWO, "update:frame", "frame", LABEL_X, Y2.frame, {fill = "muted"})
TWO:line {id = "update:axis", from = P(frameX(0), Y2.frame), to = P(frameX(FRAMES), Y2.frame),
    stroke = "border", width = 1.25, layer = LAYER.rule}
for f = 0, FRAMES do
    TWO:line {id = "update:tick:" .. f, from = P(frameX(f), Y2.frame - 5), to = P(frameX(f), Y2.frame + 5),
        stroke = "border", width = 1.25, layer = LAYER.rule}
    text(TWO, "update:tick:" .. f .. ":label", tostring(f), frameX(f), Y2.frame + 18,
        {size = 11, fill = "muted", align = {0.5, 0.5}})
end
local cursor = TWO:group {id = "update:cursor", matrix = M(frameX(FRAMES), Y2.frame)}
cursor:line {id = "update:cursor:line", from = {0, 11}, to = {0, -11}, stroke = "foreground", width = 2, layer = LAYER.head}
text(TWO, "update:items", "items", LABEL_X, Y2.items, {fill = "muted"})
slots(TWO, "update", SIZE, Y2.items)
local grown = {}
for i = 0, SIZE - 1 do
    grown[i] = sprite(TWO, "update:sprite:" .. i, i + 1, slotX(i), Y2.items, 12, {fill = "foreground"})
end
TWO:line {id = "update:ruler", from = P(rulerX(count), Y2.items - 30), to = P(rulerX(count), Y2.items + 30),
    stroke = "danger", width = 1.5, dash = {6, 4}, layer = LAYER.mark}
text(TWO, "update:ruler:label", "count", rulerX(count) + 6, Y2.items - 40, {size = 13, fill = "danger"})
local SIZE_X = slotX(SIZE - 1) + SLOT.w / 2 + 40
local sizeText = text(TWO, "update:size", "items.size() = " .. SIZE, SIZE_X, Y2.items - 12,
    {role = "code", size = 13})
text(TWO, "update:count", "count = " .. count, SIZE_X, Y2.items + 12, {role = "code", size = 13, fill = "danger"})
fragment(TWO, "update:code", "items.push_back(spawn());", X0, Y2.code)

local function playUpdate(S)
    local clear = {{target = cursor, transform = M(frameX(0), Y2.frame)},
        {target = sizeText, text = "items.size() = " .. LOADED}}
    for i = LOADED, SIZE - 1 do clear[#clear + 1] = {target = grown[i], opacity = 0} end
    S:play(clear, 0.4, "ease_out")
    S:wait(0.3)
    for f = 1, FRAMES do
        S:play({target = cursor, transform = M(frameX(f), Y2.frame)}, 0.35, "linear")
        if sizes[f] > sizes[f - 1] then
            local i = sizes[f] - 1
            S:play({{target = grown[i], opacity = 1}, {target = sizeText, text = "items.size() = " .. sizes[f]}},
                0.3, "ease_out")
        end
    end
end

-- ③ draw() → screen: the loop reads count, so it stops before the sprites update(dt) added -------
local THREE = band(SECTIONS[3])
-- Two stages share this section, so each row sits level with its own spine node.
-- The code sits above the items so the drawn sprites travel down through clear space.
local Y3 = {code = STAGES[3].y - 56, items = STAGES[3].y, screen = STAGES[4].y}
local SCREEN = {x0 = X0 - 12, x1 = slotX(SIZE - 1) + SLOT.w / 2 + 12, h = 92}
text(THREE, "draw:items", "items", LABEL_X, Y3.items, {fill = "muted"})
slots(THREE, "draw", SIZE, Y3.items)
for i = 0, SIZE - 1 do
    sprite(THREE, "draw:sprite:" .. i, i + 1, slotX(i), Y3.items, 12, {fill = "foreground"})
end
THREE:line {id = "draw:ruler", from = P(rulerX(count), Y3.items - 30), to = P(rulerX(count), Y3.items + 30),
    stroke = "danger", width = 1.5, dash = {6, 4}, layer = LAYER.mark}
fragment(THREE, "draw:code", "for (int i = 0; i < count; i++) canvas.draw(items[i]);", X0, Y3.code, "count")
local head = THREE:group {id = "draw:head", matrix = M(slotX(DRAWN), Y3.items)}
local headBox = head:rectangle {id = "draw:head:box", center = {0, 0}, size = {SLOT.w + 6, SLOT.w + 6},
    stroke = "danger", width = 2, layer = LAYER.head}
text(THREE, "draw:screen", "screen", LABEL_X, Y3.screen, {fill = "muted"})
THREE:rectangle {id = "draw:screen:frame", center = P((SCREEN.x0 + SCREEN.x1) / 2, Y3.screen),
    size = {SCREEN.x1 - SCREEN.x0, SCREEN.h}, stroke = "foreground", width = 1.25,
    layer = LAYER.slot}
local shown, missing = {}, {}
for i = 0, SIZE - 1 do
    if i < DRAWN then
        shown[i] = sprite(THREE, "draw:shown:" .. i, i + 1, slotX(i), Y3.screen, 16, {fill = "foreground"})
    else
        missing[#missing + 1] = sprite(THREE, "draw:missing:" .. i, i + 1, slotX(i), Y3.screen, 16,
            {stroke = "danger", width = 1.5, dash = {4, 3}})
    end
end
-- The finding sits beside its evidence, not in a caption bar.
local caption = THREE:group {id = "draw:caption"}
-- 28 px between the lines leaves a clear gap in Pretendard and in the Viewer's taller serif.
text(caption, "draw:caption:finding", (SIZE - DRAWN) .. " of " .. SIZE .. " sprites never drawn",
    SCREEN.x1 + 24, Y3.screen - 14, {fill = "danger"})
text(caption, "draw:caption:reason", "draw() reads count, not items.size()",
    SCREEN.x1 + 24, Y3.screen + 14, {fill = "muted"})

local function playDraw(S)
    local clear = {{target = headBox, stroke = "focus"}, {target = head, transform = M(slotX(0), Y3.items), opacity = 0},
        {target = caption, opacity = 0}}
    for i = 0, DRAWN - 1 do
        clear[#clear + 1] = {target = shown[i], transform = M(slotX(i), Y3.items), opacity = 0}
    end
    for _, g in ipairs(missing) do clear[#clear + 1] = {target = g, opacity = 0} end
    S:play(clear, 0.4, "ease_out")
    S:play({target = head, opacity = 1}, 0.2, "ease_out")
    for i = 0, DRAWN - 1 do
        if i > 0 then S:play({target = head, transform = M(slotX(i), Y3.items)}, 0.3, "ease_in_out") end
        S:play({target = shown[i], opacity = 1}, 0.15, "ease_out")
        S:play({target = shown[i], transform = M(slotX(i), Y3.screen)}, 0.5, "ease_in_out")
    end
    S:play({target = head, transform = M(slotX(DRAWN), Y3.items)}, 0.3, "ease_in_out")
    S:play({target = headBox, stroke = "danger"}, 0.25, "ease_out")
    local absent = {}
    for _, g in ipairs(missing) do absent[#absent + 1] = {target = g, opacity = 1} end
    S:play(absent, 0.35, "ease_out", 0.12)
    S:play({target = caption, opacity = 1}, 0.35, "ease_out")
end

-- Timeline ---------------------------------------------------------------------------------------
local PLAYS = {playLoad, playUpdate, playDraw}
local LEAD, STAGGER, HOLD, GAP = 0.8, 0.6, 1.6, 0.5
if MODE == "loops" then
    -- Staggered peaks along the reading order; every section ends in its t = 0 pose.
    local loop = 0
    for k, section in ipairs(SECTIONS) do
        section.scene:wait(LEAD + (k - 1) * STAGGER)
        PLAYS[k](section.scene)
        loop = math.max(loop, section.scene:duration() + HOLD)
    end
    loop = math.ceil(loop)
    -- seek(loop), not wait(loop - duration()): wait counts from the cursor, which at() puts back.
    for _, section in ipairs(SECTIONS) do section.scene:seek(loop) end
    -- The return route flows by whole dash periods ({5, 4} = 9 px), so the root seam matches too.
    root:play({target = repeatRoute, dash_offset = -9 * math.ceil(loop)}, loop, "linear")
else
    -- One section at a time, top to bottom; the spine marks the stages being explained.
    local cursor = LEAD
    for k, section in ipairs(SECTIONS) do
        section.scene:wait(cursor)
        PLAYS[k](section.scene)
        section.from, section.to = cursor, section.scene:duration()
        cursor = section.to + GAP
    end
    local total = math.ceil(cursor + HOLD)
    for _, section in ipairs(SECTIONS) do
        section.scene:seek(total)
        local marked = {}
        for _, stage in ipairs(section.stages) do marked[#marked + 1] = nodes[stage] end
        root:at(section.from, function()
            local on, off = {}, {}
            for _, body in ipairs(marked) do
                on[#on + 1] = {target = body, stroke = "focus", width = 2}
                off[#off + 1] = {target = body, stroke = "foreground", width = 1.25}
            end
            root:play(on, 0.25, "ease_out")
            root:wait(section.to - section.from - 0.5)
            root:play(off, 0.25, "ease_out")
        end)
    end
    -- Each at() put the root's cursor back to 0, so wait(total - root:duration()) would end early.
    root:seek(total)
end

for _, section in ipairs(SECTIONS) do
    assert(math.abs(section.scene:duration() - root:duration()) < 1e-6, "every section shares the root clock")
    root:viewport(section.scene, {x = BODY / W, y = section.top / H, width = (W - BODY) / W, height = section.h / H})
end
return root
