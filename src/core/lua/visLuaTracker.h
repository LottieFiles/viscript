#ifndef _VIS_LUA_TRACKER_H_
#define _VIS_LUA_TRACKER_H_

namespace vis
{

// Value trackers: an animated number that bound objects follow. Construction samples
// every binding along the eased value and schedules short linear segments, so the
// timeline stays deterministic and renderers never call back into Lua. The segments of a
// track wait until a scene call that adds or reads clips, or the end of the script, so a
// later track of other trackers that overlaps it on a binding can merge with it.
static constexpr const char* TRACKER_PRELUDE = R"lua(
local methods, ease, settle, fps, unknown, where, pend, timing = ...
local type, error, ipairs, select, unpack, pcall = type, error, ipairs, select, table.unpack, pcall
local tonumber = tonumber
local sort = table.sort
local ceil, min, max, abs = math.ceil, math.min, math.max, math.abs
local sub, byte, format = string.sub, string.byte, string.format
local KEYS = {"transform", "opacity", "stroke", "fill", "progress", "dash_offset", "tail", "tip",
              "width", "radius", "size", "corner", "trim_distance", "gradient", "blur", "shadow", "value",
              "points", "from", "to", "origin", "control1", "control2", "fn", "x_range", "y_range",
              "discontinuities", "center", "vertex", "start", "angle", "level", "values"}
-- Times closer than this are one moment, so windows that share no more do not overlap.
local EPSILON = 1e-4
-- Samples that may wait to play; a track that would keep more plays them all at once.
local PENDING_LIMIT = 32768
local trackers = {}   -- tracker table -> {scene, value, windows}
local bindings = {}   -- scene -> list of bindings
local bound = {}      -- object -> binding
local pending = {}    -- tracks whose segments wait to play, in the order they were authored
local waiting = 0     -- samples the pending tracks keep
local authored = 0    -- tracks authored so far, which orders the tracks a binding merges
local current         -- the track whose segments play, which a failure names

local function record(tracker, level)
    local entry = trackers[tracker]
    if not entry then error("expected a tracker from scene:tracker", level + 1) end
    return entry
end

-- The message of a failure raised in this chunk without its "tracker:<line>: " position, and
-- whether it had one.
local function strip(message)
    if type(message) == "string" and sub(message, 1, 8) == "tracker:" then
        local i = 9
        while (byte(message, i) or 0) >= 48 and (byte(message, i) or 0) <= 57 do i = i + 1 end
        if i > 9 and sub(message, i, i + 1) == ": " then return sub(message, i + 2), true end
    end
    return message, false
end

-- Calls fn, a C function, for the method the author called at level, as error counts levels:
-- a failure names the author's call, as the failure of a scene method written in C does,
-- rather than a line of this chunk.
local function call(level, fn, ...)
    local ok, result = pcall(fn, ...)
    if not ok then error((strip(result)), level + 1) end
    return result
end

-- Whether v reads as a component of a bound shift the way descriptor reads it: absent, or a
-- number or a string Lua converts to one.
local function component(v)
    return not v or tonumber(v) ~= nil
end

local function evaluate(binding, args, level)
    local state = binding.fn(unpack(args, 1, #binding.trackers))
    if type(state) ~= "table" then error("a bound function must return a state table", level + 1) end
    local key = unknown(state)
    if key then error("a bound state cannot set '" .. key .. "'", level + 1) end
    local shift = state.shift
    if shift ~= nil and state.transform ~= nil then
        error("a bound state cannot contain both shift and transform", level + 1)
    end
    -- Checked here, so a bad shift fails at the author's call rather than in descriptor.
    if shift ~= nil and (type(shift) ~= "table" or not component(shift[1]) or not component(shift[2])
                         or not component(shift[3])) then
        error("a bound shift must be a table of numbers", level + 1)
    end
    return state
end

local function sample(binding, level)
    local args = {}
    for i, tracker in ipairs(binding.trackers) do args[i] = trackers[tracker].value end
    -- Not a tail call, which would drop this level from the levels an error counts.
    local state = evaluate(binding, args, level + 1)
    return state
end

-- Shift is relative to the object's state at bind time; segments carry the delta from the
-- shift the segment before gave, so deltas are taken in the order the segments play.
local function descriptor(binding, state)
    local out = {target = binding.object}
    for _, key in ipairs(KEYS) do out[key] = state[key] end
    if state.shift ~= nil then
        local s, last = state.shift, binding.shift
        local x, y, z = s[1] or 0, s[2] or 0, s[3] or 0
        out.shift = {x - last[1], y - last[2], z - last[3]}
        binding.shift = {x, y, z}
    end
    return out
end

local function overlaps(start, finish, window)
    return start < window.finish - EPSILON and window.start < finish - EPSILON
end

local function includes(list, item)
    for _, value in ipairs(list) do
        if value == item then return true end
    end
    return false
end

local function shared(first, second)
    for _, value in ipairs(first) do
        if includes(second, value) then return true end
    end
    return false
end

-- The step of a track whose time is within EPSILON of time, or nil.
local function stepAt(track, time)
    local low, high = 0, track.steps
    while low < high do
        local middle = (low + high) // 2
        if track.bounds[middle] < time - EPSILON then low = middle + 1 else high = middle end
    end
    if abs(track.bounds[low] - time) <= EPSILON then return low end
    return nil
end

-- The value a tracker has at time among the tracks of a merged binding: the value of the
-- track that moves it then, at its own samples exactly; else the end of the last track that
-- moved it before; else the start of the first that moves it after; else the value it had
-- when the first of those tracks was authored.
local function valueAt(group, tracker, time)
    local before, after
    for _, track in ipairs(group.tracks) do
        for _, move in ipairs(track.moves) do
            if move.tracker == tracker then
                if time >= track.start - EPSILON and time <= track.finish + EPSILON then
                    local step = stepAt(track, time)
                    if step then return step == 0 and move.from or move.values[step] end
                    local progress = max(0, min(1, (time - track.start) / (track.finish - track.start)))
                    return move.from + (move.to - move.from) * ease(track.curve, progress)
                elseif track.finish < time then
                    if not before or track.finish > before.finish then
                        before = {finish = track.finish, value = move.to}
                    end
                elseif not after or track.start < after.start then
                    after = {start = track.start, value = move.from}
                end
            end
        end
    end
    if before then return before.value end
    if after then return after.value end
    return group.tracks[1].snapshot[tracker]
end

-- Plays one binding through the tracks that overlap on it: a segment between each pair of
-- neighboring sample times of all of them, at the values every tracker has then.
local function playGroup(group)
    local binding, tracks = group.binding, group.tracks
    sort(tracks, function(a, b) return a.order < b.order end)
    current = tracks[#tracks]
    local marks = {}
    for _, track in ipairs(tracks) do
        for step = 0, track.steps do marks[#marks + 1] = {time = track.bounds[step], track = track, step = step} end
    end
    sort(marks, function(a, b)
        if a.time ~= b.time then return a.time < b.time end
        return a.track.order < b.track.order
    end)
    local kept = {marks[1]}
    for i = 2, #marks do
        if marks[i].time > kept[#kept].time + EPSILON then kept[#kept + 1] = marks[i] end
    end
    local scene = tracks[1].scene
    for i = 2, #kept do
        local from, to = kept[i - 1], kept[i]
        local args = {}
        for j, tracker in ipairs(binding.trackers) do args[j] = valueAt(group, tracker, to.time) end
        local state = evaluate(binding, args, 1)
        local duration = to.time - from.time
        scene:seek(from.time)
        scene:play({descriptor(binding, state)}, duration, "linear")
    end
end

-- Plays the segments of the pending tracks in the order they were authored, each where its
-- track left room for it, and returns every scene to its authoring time.
local function run()
    local list = pending
    pending, waiting = {}, 0
    local resume, scenes = {}, {}
    for _, track in ipairs(list) do
        current = track
        local scene = track.scene
        if resume[scene] == nil then
            resume[scene] = scene:time()
            scenes[#scenes + 1] = scene
        end
        for _, binding in ipairs(track.affected) do
            local group = track.groups[binding]
            if group and not group.played then
                group.played = true
                playGroup(group)
            end
        end
        current = track
        for step = 1, track.steps do
            local batch = {}
            for i, binding in ipairs(track.affected) do
                local state = track.states[step][i]
                if state and not track.groups[binding] then batch[#batch + 1] = descriptor(binding, state) end
            end
            if #batch > 0 then
                scene:seek(track.bounds[step - 1])
                scene:play(batch, track.bounds[step] - track.bounds[step - 1], "linear")
            end
        end
        for _, binding in ipairs(track.affected) do
            local windows = binding.windows
            windows[#windows + 1] = {start = track.start, finish = track.finish, place = track.place}
        end
    end
    for _, scene in ipairs(scenes) do scene:seek(resume[scene]) end
end

local function flush()
    if #pending == 0 then return end
    local ok, message = pcall(run)
    if not ok then
        local stripped, inside = strip(message)
        if inside and current then message = current.where .. stripped end
        error(message, 0)
    end
end

function methods.tracker(scene, value)
    if type(value) ~= "number" or value ~= value or value == math.huge or value == -math.huge then
        error("tracker value must be a finite number", 2)
    end
    local tracker = {}
    trackers[tracker] = {scene = scene, value = value, windows = {}}
    tracker.get = function() return trackers[tracker].value end
    return tracker
end

function methods.bind(scene, object, source, fn)
    if type(fn) ~= "function" then error("bind requires a function", 2) end
    local list = trackers[source] and {source} or source
    if type(list) ~= "table" or #list == 0 then error("bind requires a tracker or an array of trackers", 2) end
    for _, tracker in ipairs(list) do
        if record(tracker, 2).scene ~= scene then error("tracker belongs to another scene", 2) end
    end
    if bound[object] then error("an object can follow only one binding", 2) end
    local binding = {object = object, trackers = list, fn = fn, shift = {0, 0, 0}, windows = {}}
    local state = sample(binding, 2)
    local first = descriptor(binding, state)
    settle(scene, first)
    bound[object] = binding
    bindings[scene] = bindings[scene] or {}
    local all = bindings[scene]
    all[#all + 1] = binding
    return object
end

-- scene:track(tracker, value, duration, curve) or scene:track({{tracker, value}, ...}, duration, curve)
-- moves the trackers now, samples the bound functions once a frame and leaves room on the
-- timeline; the segments play at the next scene call that adds or reads clips, or when the
-- script ends. Until then a later track that overlaps this one on a binding of other trackers
-- merges with it, and the binding shows the values of all of them at every sample.
function methods.track(scene, first, ...)
    local moves, duration, curve
    if trackers[first] then
        moves = {{first, (...)}}
        duration, curve = select(2, ...)
    else
        if type(first) ~= "table" or #first == 0 then error("track requires a tracker or an array of {tracker, value}", 2) end
        moves = first
        duration, curve = ...
    end
    if type(duration) == "table" then
        if curve ~= nil then error("timing options cannot be combined with positional arguments", 2) end
        local values = call(2, timing, duration)
        duration, curve = values[1], values[2]
        if duration == nil then duration = 1 end
    end
    if type(duration) ~= "number" or not (duration > 0) or duration == math.huge then
        error("track duration must be a positive finite number", 2)
    end
    -- An unknown curve, or a scene that can no longer be authored, fails at the author's call.
    local kind = type(curve)
    if kind ~= "nil" and kind ~= "string" and kind ~= "number" and kind ~= "table" then
        error("animation curve must be a preset name or a table", 2)
    end
    call(2, ease, curve, 1)
    local rate = call(2, fps, scene)
    local moving, moved = {}, {}
    for i, move in ipairs(moves) do
        if type(move) ~= "table" then error("track requires a tracker or an array of {tracker, value}", 2) end
        local entry = record(move[1], 2)
        if entry.scene ~= scene then error("tracker belongs to another scene", 2) end
        local target = move[2]
        if type(target) ~= "number" or target ~= target or target == math.huge or target == -math.huge then
            error("track target must be a finite number", 2)
        end
        moving[i] = {tracker = move[1], entry = entry, from = entry.value, to = target, values = {}}
        moved[move[1]] = true
    end
    local affected = {}
    for _, binding in ipairs(bindings[scene] or {}) do
        for _, tracker in ipairs(binding.trackers) do
            if moved[tracker] then affected[#affected + 1] = binding break end
        end
    end
    local steps = max(1, ceil(duration * rate - 1e-6))
    if #affected == 0 then
        for _, move in ipairs(moving) do move.entry.value = move.to end
        call(2, scene.wait, scene, duration)
        return scene
    end
    local start = call(2, scene.time, scene)
    local finish = start + duration
    local here = where(2)
    local place = sub(here, -2) == ": " and sub(here, 1, -3) or here
    for _, move in ipairs(moving) do
        for _, window in ipairs(move.entry.windows) do
            if overlaps(start, finish, window) and shared(window.affected, affected) then
                error(format("this track and the track at %s move the same tracker at once, until %.2f s; "
                    .. "a tracker has one value at a time", window.place, window.finish), 2)
            end
        end
    end
    for _, binding in ipairs(affected) do
        for _, window in ipairs(binding.windows) do
            if overlaps(start, finish, window) then
                error(format("this track overlaps the track at %s, whose segments have already played on "
                    .. "the same object; author tracks that overlap with only wait, seek, at, parallel, "
                    .. "mark, bind and factories between them", window.place), 2)
            end
        end
    end
    authored = authored + 1
    local track = {scene = scene, where = here, place = place, order = authored, start = start,
                   steps = steps, curve = curve, bounds = {[0] = start},
                   moves = moving, affected = affected, states = {}, snapshot = {}, groups = {}}
    for _, binding in ipairs(affected) do
        for _, tracker in ipairs(binding.trackers) do track.snapshot[tracker] = trackers[tracker].value end
        local group
        for _, other in ipairs(pending) do
            if other.scene == scene and overlaps(start, finish, other) and includes(other.affected, binding) then
                local joined = other.groups[binding]
                if not joined then
                    joined = {binding = binding, tracks = {other}}
                    other.groups[binding] = joined
                end
                if not group then
                    group = joined
                elseif joined ~= group then
                    for _, member in ipairs(joined.tracks) do
                        group.tracks[#group.tracks + 1] = member
                        member.groups[binding] = group
                    end
                end
            end
        end
        if group then
            group.tracks[#group.tracks + 1] = track
            track.groups[binding] = group
        end
    end
    for step = 1, steps do
        local u = ease(curve, step / steps)
        for _, move in ipairs(moving) do
            move.entry.value = step == steps and move.to or move.from + (move.to - move.from) * u
            move.values[step] = move.entry.value
        end
        local states = {}
        for i, binding in ipairs(affected) do
            if not track.groups[binding] then states[i] = sample(binding, 2) end
        end
        track.states[step] = states
        call(2, scene.seek, scene, start + duration * (step / steps))
        track.bounds[step] = scene:time()
    end
    track.finish = track.bounds[steps]
    for _, move in ipairs(moving) do
        local windows = move.entry.windows
        windows[#windows + 1] = {start = start, finish = track.finish, place = place, affected = affected}
    end
    pending[#pending + 1] = track
    waiting = waiting + steps * #affected
    if waiting > PENDING_LIMIT then flush() else pend() end
    return scene
end

return flush
)lua";

}  // namespace vis

#endif
