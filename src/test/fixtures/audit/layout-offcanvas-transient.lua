local scene = vis.scene {
    width = 400, height = 240, fps = 20, loop = false, theme = "pro_white",
    camera = {mode = "fixed", view = "2d", target = {0, 0}, height = 4},
}

local label = scene:text {
    id = "offcanvas:label", text = "temporary excursion", point = {0, 0},
    align = {0.5, 0.5}, role = "text", color = "foreground",
}

-- The encoded samples are t=0 and t=0.05; at t=0.05 the Text is wholly clipped.
scene:shift(label, {10, 0}, {duration = 0.05, curve = "linear"})
scene:shift(label, {-10, 0}, {duration = 0.05, curve = "linear"})
return scene
