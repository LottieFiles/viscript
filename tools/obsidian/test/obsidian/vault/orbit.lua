-- A scene file: embedded in Scenes.md and opened in its own tab.
local scene = vis.scene {
    width = 640, height = 360, theme = "adaptive_vscode",
    camera = {view = "2d", target = {0, 0}, height = 7},
}
local a = scene:circle {center = {-3, 0}, radius = 0.6, fill = "accent"}
local b = scene:circle {center = {3, 0}, radius = 0.6, fill = "success"}
scene:text {text = "orbit.lua", point = {0, 2.6}, size = 20, role = "code"}
scene:grow_from_center(a, {duration = 0.6})
scene:fade_in(b, {duration = 0.6})
scene:shift(a, {2, 0}, {duration = 1})
scene:wait(0.4)
return scene
