local function add_diagram(scene, x)
    local diagram = vis.diagram(scene, {
        id = "shared-diagram", layout = "manual", node_size = {2, 1},
    })
    diagram:node {id = "node", label = "N", position = {x, 0}}
    diagram:build()
end

local child = vis.scene {
    width = 400, height = 450, fps = 20, loop = false, theme = "pro_white",
    background = "#ffffff00",
    camera = {mode = "fixed", view = "2d", target = {0, 0}, height = 8},
}
add_diagram(child, 0)

local root = vis.scene {
    width = 800, height = 450, fps = 20, loop = false, theme = "pro_white",
    camera = {mode = "fixed", view = "2d", target = {0, 0}, height = 8},
}
add_diagram(root, -2)
root:viewport(child, {x = 0.5, y = 0, width = 0.5, height = 1})

return root
