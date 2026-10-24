local scene = vis.scene {camera = { height = 11 }}

local world = scene:space {
    x = { -7, 7, 1 },
    y = { -4, 4, 1 },
    z = { -2, 2, 1 },
    color = "#293540",
    width = 1,
    id = "image-space",
}
local image = world:picture {
    asset = "../assets/pixels.svg",
    center = { -3.8, 0, 0 },
    width = 4,
    filter = "nearest",
    id = "source-image",
}
local cells = world:cell {
    origin = { 0, -2, 0 },
    size = { 4, 4 },
    mode = "padd",
    padding = 0.05,
    depth = 1,
    texture = "../assets/pixels.svg",
    source = { 0, 0, 8, 8 },
    destination = { 0, 0, 4, 4 },
    patches = {
        { region = { 1, 1, 2, 2 }, color = "#ffffff" },
    },
    id = "pixel-cells",
}

scene:create({ image, cells }, {duration = 0.9, curve = "ease_out", lag = 0.15})
scene:wait(0.4)
scene:look({
    view = "3d",
    eye = { 7, 6, 9 },
    projection = "perspective",
    fov = 0.75,
    near = 0.1,
    far = 100,
}, {duration = 1.2, curve = "ease_in_out"})
scene:wait(0.7)
return scene
