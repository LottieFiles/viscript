local child = vis.scene {
    width = 400, height = 240, fps = 20, theme = "pro_white",
    camera = {mode = "fixed", view = "2d", target = {0, 0}, height = 4},
}
child:text {
    id = "viewport:text", text = "visible child", point = {0, 0},
    align = {0.5, 0.5}, role = "text", color = "foreground",
}

local root = vis.scene {
    width = 400, height = 240, fps = 20, theme = "pro_white",
    camera = {mode = "fixed", view = "2d", target = {0, 0}, height = 4},
}
root:text {
    id = "root:text", text = "covered root", point = {0, 0},
    align = {0.5, 0.5}, role = "text", color = "foreground",
}
root:viewport(child, {x = 0, y = 0, width = 1, height = 1})
return root
