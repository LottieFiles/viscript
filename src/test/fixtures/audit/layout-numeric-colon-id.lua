local scene = vis.scene {
    width = 400, height = 240, fps = 20, theme = "pro_white",
    camera = {mode = "fixed", view = "2d", target = {0, 0}, height = 4},
}

scene:rectangle {
    id = "numeric:body:12", center = {0, 0}, size = {5.5, 1.5}, corner = 0.12,
    fill = "surface", stroke = "border", width = 2,
}
scene:text {
    id = "numeric:label:34", text = "numeric suffix IDs", point = {0, 0},
    align = {0.5, 0.5}, role = "text", color = "foreground",
}
return scene
