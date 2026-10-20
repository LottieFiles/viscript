local scene = vis.scene {
    width = 960,
    height = 540,
    fps = 30,
    loop = false,
    camera = {mode = "fixed", view = "2d", target = {0, 0}, height = 7},
}

local figure = scene:group {id = "figure"}
local subject = figure:circle {center = {0, 0}, radius = 1, id = "subject"}
figure:text {
    text = "viscript",
    point = {0, 0},
    role = "h2",
    id = "subject-label",
}

scene:fade_in(figure, {scale = 0.97, duration = 0.55, curve = "gentle"})
scene:indicate(subject, {scale = 1.06, duration = 0.45, curve = "ease_in_out"})
scene:wait(0.65)
return scene
