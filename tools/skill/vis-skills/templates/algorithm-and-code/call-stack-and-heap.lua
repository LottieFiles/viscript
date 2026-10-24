-- Question: which state belongs to each call frame, and which object survives a return?
-- Variant: main -> buildUser(" Ada ") -> normalize(" Ada ") -> "Ada".
local scene = vis.scene {camera = {height = 540}}
local LAYER = {route = 8, body = 20, text = 40}
local TRACE = {
    {kind = "call", frame = "main", detail = "result = buildUser(...)"},
    {kind = "call", frame = "build", detail = "raw = \" Ada \""},
    {kind = "allocate", object = "user", value = "name = ?"},
    {kind = "call", frame = "normalize", detail = "input = \" Ada \""},
    {kind = "return", frame = "normalize", value = "\"Ada\"", to = "build"},
    {kind = "write", object = "user", value = "name = \"Ada\""},
    {kind = "return", frame = "build", value = "ref user@A0", to = "main"},
    {kind = "return", frame = "main", value = "user.name = \"Ada\""},
}
assert(TRACE[5].value == "\"Ada\"" and TRACE[8].value == "user.name = \"Ada\"")

-- Ink frames and object; the executing frame carries a transient `focus` outline, and `result` marks
-- only what survives: the heap object, its field, the returned reference, and the final caption.
local subtitle = scene:text {text = "main → buildUser → normalize · one object identity user@A0", point = {0, 190}, role = "code", fill = "muted", layer = LAYER.text, id = "scene-subtitle"}
scene:text {text = "CALL STACK", point = {-260, 140}, role = "code", fill = "muted", layer = LAYER.text, id = "stack-label"}
scene:text {text = "HEAP", point = {260, 140}, role = "code", fill = "muted", layer = LAYER.text, id = "heap-label"}

local FRAME = {main = {y = -75, label = "main"}, build = {y = 0, label = "buildUser(raw)"}, normalize = {y = 75, label = "normalize(input)"}}
local frameGroup, frameBody, frameLabel, frameDetail = {}, {}, {}, {}
for _, id in ipairs({"main", "build", "normalize"}) do
    local spec = FRAME[id]
    frameGroup[id] = scene:group {id = "frame:" .. id}
    frameBody[id] = scene:rectangle {center = {-260, spec.y}, size = {340, 70}, stroke = "foreground", width = 1.25, opacity = 0, layer = LAYER.body, id = "frame:" .. id .. ":body"}
    frameLabel[id] = scene:text {text = spec.label, point = {-260, spec.y + 16}, role = "code", fill = "foreground", opacity = 0, layer = LAYER.text, id = "frame:" .. id .. ":label"}
    frameDetail[id] = scene:text {text = "waiting", point = {-260, spec.y - 16}, role = "code", fill = "muted", opacity = 0, layer = LAYER.text, id = "frame:" .. id .. ":detail:0"}
end
local objectBody = scene:rectangle {center = {260, 15}, size = {300, 120}, stroke = "foreground", width = 1.25, opacity = 0, layer = LAYER.body, id = "object:user:body"}
local objectAddress = scene:text {text = "user@A0 (illustrative)", point = {260, 48}, role = "code", fill = "muted", opacity = 0, layer = LAYER.text, id = "object:user:address"}
local objectValue = scene:text {text = "name = ?", point = {260, -10}, role = "code", fill = "foreground", opacity = 0, layer = LAYER.text, id = "object:user:value:0"}
local returnRoute = scene:arrow {from = {-90, -75}, to = {105, -10}, tip = 7, stroke = "result", width = 1.5, opacity = 0, layer = LAYER.route, id = "return:user-to-main"}

local status, statusVersion, detailVersion = nil, 0, {main = 0, build = 0, normalize = 0}
local function setStatus(text, color)
    statusVersion = statusVersion + 1
    local nextStatus = scene:text {text = text, point = {0, -160}, role = "code", fill = color or "foreground", layer = LAYER.text, id = "status:" .. statusVersion}
    if status then scene:fade_transform(status, nextStatus, {duration = 0.16, curve = "gentle"}) else scene:fade_in(nextStatus, {duration = 0.22, curve = "gentle"}) end
    status = nextStatus
end
local function setDetail(id, text, color)
    detailVersion[id] = detailVersion[id] + 1
    local nextDetail = scene:text {text = text, point = {-260, FRAME[id].y - 16}, role = "code", fill = color or "muted", layer = LAYER.text, id = "frame:" .. id .. ":detail:" .. detailVersion[id]}
    scene:fade_transform(frameDetail[id], nextDetail, {duration = 0.18, curve = "gentle"})
    frameDetail[id] = nextDetail
end

scene:fade_in(subtitle, {duration = 0.32, curve = "gentle"})
local active  -- body of the executing frame
for _, event in ipairs(TRACE) do
    if event.kind == "call" then
        setStatus("call " .. event.frame .. " · create a distinct activation")
        local enter = {{target = frameBody[event.frame], opacity = 1, stroke = "focus", width = 2}, {target = frameLabel[event.frame], opacity = 1}}
        if active then enter[#enter + 1] = {target = active, stroke = "foreground", width = 1.25} end
        scene:tween(enter, {duration = 0.30, curve = "ease_out", lag = 0})
        active = frameBody[event.frame]
        local labelId = event.frame
        scene:tween({{target = frameDetail[labelId], opacity = 1}}, {duration = 0.16, curve = "gentle", lag = 0})
        setDetail(event.frame, event.detail)
    elseif event.kind == "allocate" then
        setStatus("buildUser allocates user@A0 on the heap")
        scene:tween({{target = objectBody, opacity = 1}, {target = objectAddress, opacity = 1}, {target = objectValue, opacity = 1}}, {duration = 0.36, curve = "ease_out", lag = 0})
    elseif event.kind == "write" then
        setStatus("normalize result becomes the object's field")
        local nextValue = scene:text {text = event.value, point = {260, -10}, role = "code", fill = "result", layer = LAYER.text, id = "object:user:value:1"}
        scene:fade_transform(objectValue, nextValue, {duration = 0.22, curve = "gentle"}); objectValue = nextValue
        scene:tween({{target = objectBody, stroke = "result", width = 1.5}}, {duration = 0.24, curve = "ease_in_out", lag = 0})
    else
        setStatus(event.frame .. " returns " .. event.value)
        setDetail(event.frame, "return " .. event.value, "foreground")
        if event.frame == "build" then scene:tween({{target = returnRoute, opacity = 1}}, {duration = 0.34, curve = "ease_out", lag = 0}) end
        -- The returning frame dims and hands the focus back to its caller; main's return clears it.
        local done = {{target = frameBody[event.frame], stroke = "foreground", width = 1.25}}
        if event.frame ~= "main" then
            done[1].opacity = 0.4
            done[#done + 1] = {target = frameDetail[event.frame], opacity = 0.4}
            done[#done + 1] = {target = frameBody[event.to], stroke = "focus", width = 2}
        end
        scene:tween(done, {duration = 0.28, curve = "ease_in_out", lag = 0})
        active = frameBody[event.to]
    end
    scene:wait(0.18)
end
local result = scene:text {text = "main returns, but user@A0 remains the returned object", point = {190, -160}, role = "text", fill = "result", layer = LAYER.text, id = "result:heap-survives"}
scene:fade_transform(status, result, {duration = 0.22, curve = "gentle"}); scene:wait(2.4)
return scene
