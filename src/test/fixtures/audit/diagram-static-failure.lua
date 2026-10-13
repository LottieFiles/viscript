local scene = vis.scene {
    width = 800, height = 450, fps = 20, loop = false, theme = "pro_white",
    camera = {mode = "fixed", view = "2d", target = {0, 1}, height = 9},
}

local diagram = vis.diagram(scene, {
    id = "layout-failure", layout = "manual", node_size = {1, 1},
})
local source = diagram:node {id = "source", label = "S", position = {-4, 0}}
local target = diagram:node {id = "target", label = "T", position = {4, 0}}
diagram:node {id = "blocker", label = "B", position = {0, 0}}
diagram:node {id = "first", label = "1", position = {0, 3}}
diagram:node {id = "second", label = "2", position = {0.5, 3}}
local direction_source = diagram:node {
    id = "direction-source", label = "DS", position = {-4, -2},
}
local direction_target = diagram:node {
    id = "direction-target", label = "DT", position = {4, -2},
}
diagram:connect {
    id = "crossing", from = source, to = target, route = "straight",
}
diagram:connect {
    id = "wrong-direction", from = direction_source, to = direction_target,
    from_port = "top", to_port = "top", route = "straight",
}
diagram:build()

return scene
