local scene = vis.scene {
    width = 400, height = 240, fps = 20, theme = "pro_white",
    camera = {mode = "fixed", view = "2d", target = {0, 0}, height = 4},
}

local panel = scene:rectangle {
    id = "ancestor:body", center = {0, 0}, size = {5.5, 1.5}, corner = 0.12,
    fill = "surface", stroke = "border", width = 2,
}
panel:label {
    id = "ancestor:label", text = "owned child", point = {0, 0},
    align = {0.5, 0.5}, role = "text", color = "foreground", layer = 1,
}
return scene
