-- A dot that moves and then turns: render --times takes the mark between the beats by name.
local scene = vis.scene {width = 320, height = 180}

local dot = scene:circle {id = "dot", center = {-2, 0}, radius = 0.4, fill = "accent", width = 0}
scene:shift(dot, {4, 0}, {duration = 1})
scene:mark("moved")
scene:tween({target = dot, fill = "success"}, {duration = 0.5})

return scene
