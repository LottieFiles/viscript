-- Question: which state belongs to each call frame, and which object survives a return?
-- Variant: main -> buildUser(" Ada ") -> normalize(" Ada ") -> "Ada".
local scene = vis.scene {width = 960, height = 540, fps = 30, loop = false, theme = "adaptive_vscode", camera = {mode = "fixed", view = "2d", target = {0, 0}, height = 540}}
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

local title = scene:text {text = "Calls · temporary frames, surviving heap objects", point = {0, 230}, role = "h2", fill = "foreground", layer = LAYER.text, id = "scene-title"}
local subtitle = scene:text {text = "main → buildUser → normalize · one object identity user@A0", point = {0, 194}, role = "code", fill = "muted", layer = LAYER.text, id = "scene-subtitle"}
scene:text {text = "CALL STACK", point = {-260, 145}, role = "code", fill = "accent", layer = LAYER.text, id = "stack-label"}
scene:text {text = "HEAP", point = {260, 145}, role = "code", fill = "accent", layer = LAYER.text, id = "heap-label"}

local FRAME = {main = {y = -70, label = "main"}, build = {y = 0, label = "buildUser(raw)"}, normalize = {y = 70, label = "normalize(input)"}}
local frameGroup, frameBody, frameLabel, frameDetail = {}, {}, {}, {}
for _, id in ipairs({"main", "build", "normalize"}) do
    local spec = FRAME[id]
    frameGroup[id] = scene:group {id = "frame:" .. id}
    frameBody[id] = scene:rectangle {center = {-260, spec.y}, size = {340, 66}, corner = 6, fill = "surface", stroke = "border", width = 2, opacity = 0, layer = LAYER.body, id = "frame:" .. id .. ":body"}
    frameLabel[id] = scene:text {text = spec.label, point = {-260, spec.y + 15}, role = "code", fill = "foreground", opacity = 0, layer = LAYER.text, id = "frame:" .. id .. ":label"}
    frameDetail[id] = scene:text {text = "waiting", point = {-260, spec.y - 15}, role = "code", fill = "muted", opacity = 0, layer = LAYER.text, id = "frame:" .. id .. ":detail:0"}
end
local objectBody = scene:rectangle {center = {260, 25}, size = {300, 110}, corner = 8, fill = "surface", stroke = "border", width = 2, opacity = 0, layer = LAYER.body, id = "object:user:body"}
local objectAddress = scene:text {text = "user@A0 (illustrative)", point = {260, 58}, role = "code", fill = "muted", opacity = 0, layer = LAYER.text, id = "object:user:address"}
local objectValue = scene:text {text = "name = ?", point = {260, 5}, role = "code", fill = "foreground", opacity = 0, layer = LAYER.text, id = "object:user:value:0"}
local returnRoute = scene:arrow {from = {-90, -70}, to = {105, 5}, tip = 11, stroke = "result", width = 3, opacity = 0, layer = LAYER.route, id = "return:user-to-main"}

local status, statusVersion, detailVersion = nil, 0, {main = 0, build = 0, normalize = 0}
local function setStatus(text, color)
    statusVersion = statusVersion + 1
    local nextStatus = scene:text {text = text, point = {0, -185}, role = "code", fill = color or "focus", layer = LAYER.text, id = "status:" .. statusVersion}
    if status then scene:fade_transform(status, nextStatus, 0.16, "gentle") else scene:fade_in(nextStatus, {duration = 0.22, curve = "gentle"}) end
    status = nextStatus
end
local function setDetail(id, text, color)
    detailVersion[id] = detailVersion[id] + 1
    local nextDetail = scene:text {text = text, point = {-260, FRAME[id].y - 15}, role = "code", fill = color or "muted", layer = LAYER.text, id = "frame:" .. id .. ":detail:" .. detailVersion[id]}
    scene:fade_transform(frameDetail[id], nextDetail, 0.18, "gentle")
    frameDetail[id] = nextDetail
end

scene:fade_in(title, {shift = {0, -6}, duration = 0.42, curve = "gentle"}); scene:fade_in(subtitle, {duration = 0.32, curve = "gentle"})
for _, event in ipairs(TRACE) do
    if event.kind == "call" then
        setStatus("call " .. event.frame .. " · create a distinct activation")
        scene:play({{target = frameBody[event.frame], opacity = 1, stroke = "focus"}, {target = frameLabel[event.frame], opacity = 1}}, 0.30, "ease_out", 0)
        local labelId = event.frame
        scene:play({{target = frameDetail[labelId], opacity = 1}}, 0.16, "gentle", 0)
        setDetail(event.frame, event.detail)
    elseif event.kind == "allocate" then
        setStatus("buildUser allocates user@A0 on the heap")
        scene:play({{target = objectBody, opacity = 1, stroke = "focus"}, {target = objectAddress, opacity = 1}, {target = objectValue, opacity = 1}}, 0.36, "ease_out", 0)
    elseif event.kind == "write" then
        setStatus("normalize result becomes the object's field", "success")
        local nextValue = scene:text {text = event.value, point = {260, 5}, role = "code", fill = "result", layer = LAYER.text, id = "object:user:value:1"}
        scene:fade_transform(objectValue, nextValue, 0.22, "gentle"); objectValue = nextValue
        scene:play({{target = objectBody, stroke = "result"}}, 0.24, "ease_in_out", 0)
    else
        setStatus(event.frame .. " returns " .. event.value, event.frame == "main" and "result" or "success")
        setDetail(event.frame, "return " .. event.value, "success")
        if event.frame == "build" then scene:play({{target = returnRoute, opacity = 1}}, 0.34, "ease_out", 0) end
        if event.frame ~= "main" then scene:play({{target = frameBody[event.frame], opacity = 0.28}, {target = frameDetail[event.frame], opacity = 0.28}}, 0.28, "ease_in_out", 0) end
    end
    scene:wait(0.18)
end
local result = scene:text {text = "main returns, but user@A0 remains the returned object", point = {190, -125}, role = "text", fill = "result", layer = LAYER.text, id = "result:heap-survives"}
scene:fade_transform(status, result, 0.22, "gentle"); scene:wait(2.4)
return scene
