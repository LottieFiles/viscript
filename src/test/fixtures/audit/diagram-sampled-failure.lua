local scene = vis.scene {
    width = 800, height = 450, fps = 20, loop = false, theme = "pro_white",
    camera = {mode = "fixed", view = "2d", target = {0, 0}, height = 9},
}

local diagram = vis.diagram(scene, {
    id = "sampled-failure", layout = "manual", node_size = {1, 1},
})
local left = diagram:node {id = "left", label = "L", position = {-3, 0}}
local right = diagram:node {id = "right", label = "R", position = {3, 0}}
local blocker = diagram:node {id = "blocker", label = "B", position = {0, 2}}
local bottom = diagram:node {id = "bottom", label = "D", position = {4, -2}}
local top = diagram:node {id = "top", label = "U", position = {4, 2}}
diagram:connect {
    id = "horizontal", from = left, to = right, route = "straight",
}
local vertical = diagram:connect {
    id = "vertical", from = bottom, to = top, route = "straight",
    from_port = "top", to_port = "bottom",
}
local built = diagram:build()

scene:shift(built:object(blocker), {0, -2}, {duration = 0.5, curve = "linear"})
scene:shift(built:object(blocker), {0, 2}, {duration = 0.5, curve = "linear"})
scene:shift(built:object(vertical), {-4, 0}, {duration = 0.5, curve = "linear"})
scene:shift(built:object(vertical), {4, 0}, {duration = 0.5, curve = "linear"})
scene:shift(built:root(), {12, 0}, {duration = 0.5, curve = "linear"})
scene:shift(built:root(), {-12, 0}, {duration = 0.5, curve = "linear"})

return scene
