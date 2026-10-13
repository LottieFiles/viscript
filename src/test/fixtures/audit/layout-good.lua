local scene = vis.scene {
    width = 800, height = 450, fps = 20, loop = false, theme = "pro_white",
    camera = {mode = "fixed", view = "2d", target = {0, 0}, height = 8},
}

scene:rectangle {
    id = "audit:body", center = {0, 0}, size = {6.8, 1.8}, corner = 0.12,
    fill = "surface", stroke = "border", width = 2,
}
scene:text {
    id = "audit:label", text = "Semantic Equation · \u{cf54}\u{b4dc} \u{c2e4}\u{d589} \u{cd94}\u{c801}",
    point = {0, 0}, align = {0.5, 0.5}, role = "text", color = "foreground",
}

return scene
