local scene = vis.scene {camera = {height = 7}}

local figure = scene:group {id = "figure"}
local subject = figure:circle {id = "subject"}
figure:text {text = "viscript", role = "h2", id = "subject-label"}

scene:fade_in(figure, {scale = 0.97, duration = 0.55, curve = "gentle"})
scene:indicate(subject, {scale = 1.06, duration = 0.45, curve = "ease_in_out"})
scene:wait(0.65)
return scene
