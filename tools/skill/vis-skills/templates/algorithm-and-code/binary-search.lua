-- Question: how can one comparison reject half of a closed candidate interval?
-- Variant: ascending data, zero-based indices, closed [lo, hi], return any match.
-- Edit VALUES and TARGET; the closed-range trace is regenerated below.
-- Replace the trace builder when reviewing a half-open or duplicate-selecting implementation.
local scene = vis.scene {camera = {height = 540}}

local VALUES = {2, 5, 8, 12, 16, 23, 38}
local TARGET = 23

local function buildClosedRangeTrace(values, target)
    local trace, lo, hi = {}, 0, #values - 1
    while lo <= hi do
        local mid = lo + math.floor((hi - lo) / 2)
        local value = values[mid + 1]
        local event = {lo = lo, hi = hi, mid = mid, value = value}
        if value < target then
            event.relation = "<"
            event.rejectedLo, event.rejectedHi = lo, mid
            lo = mid + 1
        elseif value > target then
            event.relation = ">"
            event.rejectedLo, event.rejectedHi = mid, hi
            hi = mid - 1
        else
            event.relation = "="
            trace[#trace + 1] = event
            return trace, mid, lo, hi
        end
        event.nextLo, event.nextHi = lo, hi
        trace[#trace + 1] = event
    end
    return trace, nil, lo, hi
end

assert(#VALUES > 0)
local TRACE, FOUND_INDEX, FINAL_LO, FINAL_HI = buildClosedRangeTrace(VALUES, TARGET)
-- The array is the subject: cells are ink slots, the candidate interval carries the one hue (accent),
-- the probe at mid is a transient `focus` outline, and the found cell takes `result`.
local CELL, STEP, Y = 84, 100, 40
local START_X = -(#VALUES - 1) * STEP / 2
local LAYER = {range = 8, slot = 10, body = 20, text = 40}

local contract = scene:text {
    text = "closed interval [lo, hi]  ·  target = " .. TARGET,
    point = {0, 150}, role = "code", fill = "muted",
    layer = LAYER.text, id = "search-contract",
}

local array = scene:group {id = "search:array"}
local cells, bodies, values = {}, {}, {}
for index, value in ipairs(VALUES) do
    local logical = index - 1
    local x = START_X + logical * STEP
    local cell = array:group {id = "array-cell:" .. logical}
    local body = cell:rectangle {
        center = {x, Y}, size = {CELL, CELL},
        stroke = "foreground", width = 1,
        layer = LAYER.body, id = "array-cell:" .. logical .. ":body",
    }
    values[index] = cell:text {
        text = tostring(value), point = {x, Y}, role = "h3",
        fill = "foreground", layer = LAYER.text,
        id = "array-cell:" .. logical .. ":value",
    }
    cell:text {
        text = tostring(logical), point = {x, Y - 72}, role = "code",
        fill = "muted", layer = LAYER.text,
        id = "array-cell:" .. logical .. ":index",
    }
    cells[index], bodies[index] = cell, body
end

local function rangeBox(lo, hi, id)
    local left, right
    if lo <= hi then
        left = START_X + lo * STEP - CELL / 2 - 5
        right = START_X + hi * STEP + CELL / 2 + 5
    else
        left = START_X + (lo - 0.5) * STEP
        right = left
    end
    return scene:rectangle {
        center = {(left + right) / 2, Y}, size = {right - left + 12, CELL + 16},
        stroke = "accent", width = 1.5,
        layer = LAYER.range, id = id,
    }
end

local range = rangeBox(TRACE[1].lo, TRACE[1].hi, "range:step-1")
local invariant = scene:text {
    text = "invariant: if target exists, it is inside [lo, hi]",
    point = {0, -150}, role = "text", fill = "muted",
    layer = LAYER.text, id = "search-invariant",
}

scene:fade_in(contract, {shift = {0, -5}, duration = 0.34, curve = "gentle"})
scene:fade_in(array, {shift = {0, 7}, duration = 0.55, curve = "ease_out"})
scene:create(range, 0.42, "ease_out")
scene:fade_in(invariant, {duration = 0.28, curve = "gentle"})

local status
local function setStatus(event, step)
    local nextStatus = scene:text {
        text = string.format(
            "lo=%d  mid=%d  hi=%d  ·  a[%d]=%s %s %s",
            event.lo, event.mid, event.hi, event.mid,
            tostring(event.value), event.relation, tostring(TARGET)
        ),
        point = {0, -100}, role = "code", fill = "foreground",
        layer = LAYER.text, id = "status:step-" .. step,
    }
    if status then scene:fade_transform(status, nextStatus, 0.20, "gentle")
    else scene:fade_in(nextStatus, {duration = 0.28, curve = "gentle"}) end
    status = nextStatus
end

for step, event in ipairs(TRACE) do
    setStatus(event, step)
    scene:play({
        {target = bodies[event.mid + 1], stroke = "focus", width = 2},
        {target = values[event.mid + 1], fill = "focus"},
    }, 0.42, "ease_in_out", 0)
    scene:wait(0.45)

    if event.relation ~= "=" then
        local reject = {
            {target = bodies[event.mid + 1], stroke = "foreground", width = 1},
            {target = values[event.mid + 1], fill = "foreground"},
        }
        for logical = event.rejectedLo, event.rejectedHi do
            reject[#reject + 1] = {target = cells[logical + 1], opacity = 0.4}
        end
        scene:play(reject, 0.45, "ease_in_out", 0)
        local nextRange = rangeBox(event.nextLo, event.nextHi, "range:step-" .. (step + 1))
        scene:fade_transform(range, nextRange, 0.30, "ease_in_out")
        range = nextRange
    end
end

local resultText = FOUND_INDEX and ("found target at index " .. FOUND_INDEX)
    or string.format("not found  ·  empty interval [%d, %d]", FINAL_LO, FINAL_HI)
local result = scene:text {
    text = resultText, point = {0, -100}, role = "text",
    fill = "result", layer = LAYER.text,
    id = FOUND_INDEX and "result:found-index" or "result:not-found",
}
scene:fade_transform(status, result, 0.22, "gentle")
local finish = {{target = range, stroke = "result"}}
if FOUND_INDEX then
    -- A stroke highlight, with the value recolored in the same play, keeps the text readable.
    finish[#finish + 1] = {target = bodies[FOUND_INDEX + 1], stroke = "result", width = 2}
    finish[#finish + 1] = {target = values[FOUND_INDEX + 1], fill = "result"}
end
scene:play(finish, 0.48, "ease_in_out", 0)
scene:wait(2.2)
return scene
