local scene = vis.scene {
    width = 800, height = 450, fps = 20, loop = false, theme = "pro_white",
    camera = {mode = "fixed", view = "2d", target = {0, 0}, height = 9},
}

local diagram = vis.diagram(scene, {
    id = "auto-route", layout = "manual", node_size = {2, 2},
})
local source = diagram:node {id = "source", label = "S", position = {-4, 0}}
local target = diagram:node {id = "target", label = "T", position = {4, 0}}
diagram:node {id = "blocker", label = "B", position = {0, 0}}
diagram:connect {id = "automatic", from = source, to = target}
diagram:build()

return scene
