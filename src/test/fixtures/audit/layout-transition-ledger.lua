local function stage(id, text)
    local scene = vis.scene {
        width = 400, height = 240, fps = 10, theme = "pro_white",
        camera = {mode = "fixed", view = "2d", target = {0, 0}, height = 4},
    }
    scene:text {
        id = id, text = text, point = {0, 0},
        align = {0.5, 0.5}, role = "text", color = "foreground",
    }
    return scene
end

local first = stage("transition:first", "first stage")
local middle = stage("transition:middle", "skipped middle stage")
local last = stage("transition:last", "last stage")
local root = vis.scene {
    width = 400, height = 240, fps = 1, loop = false, theme = "pro_white",
    camera = {mode = "fixed", view = "2d", target = {0, 0}, height = 4},
}
root:scene_transition({first, middle, last}, {
    duration = 0.1, hold = 0, curve = "linear",
})
return root
