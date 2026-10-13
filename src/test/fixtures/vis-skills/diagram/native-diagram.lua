-- Requires a Lua host built with -Dmodules=diagram.
-- Question: which boundary owns each stage, and how does one request cross it?
local scene = vis.scene {
    width = 960, height = 540, fps = 30, loop = false,
    camera = {mode = "fixed", view = "2d", target = {0, -0.35}, height = 11.5},
}

local diagram = vis.diagram(scene, {
    id = "request-flow",
    direction = "lr",
    origin = {-6.9, -0.35},
    node_size = {2.8, 1.15},
    rank_gap = 1.8,
    node_gap = 0.7,
    zone_padding = 0.8,
    route_width = 0.08,
    arrow_length = 0.32,
    arrow_width = 0.28,
    corner = 0.22,
})

local client = diagram:node {id = "client", label = "Client", detail = "request", rank = 0}
local gateway = diagram:node {id = "gateway", label = "Gateway", detail = "policy", rank = 1}
local service = diagram:node {id = "service", label = "Service", detail = "operation", rank = 2}
local store = diagram:node {id = "store", label = "Store", detail = "result", rank = 3}

local request = diagram:connect {
    id = "request", label = "HTTP", from = client, to = gateway,
    route = "straight", flow = true,
}
local dispatch = diagram:connect {
    id = "dispatch", label = "call", from = gateway, to = service,
    route = "straight", flow = true,
}
local persist = diagram:connect {
    id = "persist", label = "write", from = service, to = store,
    route = "straight", flow = true,
}

diagram:zone {id = "external", label = "External", members = {client}}
diagram:zone {id = "runtime", label = "Runtime", members = {gateway, service}}
diagram:zone {id = "state", label = "State", members = {store}}

local built = diagram:build()  -- attaches one ordinary Object subtree to scene
local flows = {built:edge(request), built:edge(dispatch), built:edge(persist)}

scene:fade_in(built:zones(), {duration = 0.4, curve = "gentle"})
scene:fade_in(built:nodes(), {duration = 0.6, curve = "ease_out"})
scene:fade_in(built:routes(), {duration = 0.65, curve = "ease_out"})
scene:fade_in(built:annotations(), {duration = 0.3, curve = "gentle"})
-- Native flow edges use one directed Object with a {5, 4} shaft dash; one full period keeps the seam exact.
scene:play({
    {target = flows[1], dash_offset = -9},
    {target = flows[2], dash_offset = -9},
    {target = flows[3], dash_offset = -9},
}, 1.15, "linear")
scene:indicate(built:node_body(store), {color = "result", scale = 1.04, duration = 0.45})
scene:wait(2.0)
return scene
