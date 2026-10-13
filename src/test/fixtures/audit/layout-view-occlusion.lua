local root = vis.scene {
    width = 400, height = 240, fps = 20, theme = "pro_white",
    camera = {mode = "fixed", view = "2d", target = {0, 0}, height = 4},
}
root:text {
    id = "root:text", text = "covered root", point = {0, 0},
    align = {0.5, 0.5}, role = "text", color = "foreground",
}
-- The view looks away from the text and fills the middle of the frame with the opaque
-- background of the scene, so it hides the text it does not show.
root:view({x = 0.2, y = 0.3, width = 0.6, height = 0.4}, {camera = {target = {20, 20}}})
return root
