-- Build with -Dmodules=diagram. The builder emits ordinary Scene Objects.
local scene = vis.scene {
    width = 800, height = 450, fps = 30, loop = false, theme = "pro_white",
    camera = {mode = "fixed", view = "2d", target = {0, 0}, height = 7.5},
}

local diagram = vis.diagram(scene, {
    id = "request-flow", direction = "lr", origin = {-4.8, 0},
    node_size = {2.4, 1.05}, rank_gap = 1.0,
})
local input = diagram:node {id = "input", label = "Input", rank = 0}
local transform = diagram:node {id = "transform", label = "Transform", rank = 1}
local output = diagram:node {id = "output", label = "Output", rank = 2}
local first = diagram:connect {
    id = "decode", from = input, to = transform, flow = true,
}
local second = diagram:connect {
    id = "encode", from = transform, to = output, flow = true,
}
local built = diagram:build()

scene:fade_in(built:nodes(), {duration = 0.35, curve = "gentle"})
scene:create(built:routes(), 0.55, "ease_out")
scene:play({
    {target = built:edge(first), dash_offset = -9},
    {target = built:edge(second), dash_offset = -9},
}, 1.1, "linear")
scene:indicate(built:node_body(output), {color = "result", duration = 0.35})
scene:wait(0.8)
return scene
