-- Reference study: carry one closed mesh and one stable vertex through an
-- explicit right-handed, column-vector, OpenGL-depth MVP convention.
-- The visible "concept camera" owns V/P; the presentation camera only lets the
-- viewer inspect the object, frustum, image plane, and projection ray together.
local page = vis.scene {
    width = 960, height = 540, fps = 30, loop = false, theme = "adaptive",
    camera = {mode = "fixed", view = "2d", target = {0, 0}, height = 6},
}

local function add(a, b) return {a[1] + b[1], a[2] + b[2], a[3] + b[3]} end
local function subtract(a, b) return {a[1] - b[1], a[2] - b[2], a[3] - b[3]} end
local function scale(v, amount) return {v[1] * amount, v[2] * amount, v[3] * amount} end
local function dot(a, b) return a[1] * b[1] + a[2] * b[2] + a[3] * b[3] end
local function cross(a, b)
    return {
        a[2] * b[3] - a[3] * b[2],
        a[3] * b[1] - a[1] * b[3],
        a[1] * b[2] - a[2] * b[1],
    }
end
local function normalize(v)
    local length = math.sqrt(dot(v, v))
    assert(length > 1e-9, "cannot normalize a zero vector")
    return scale(v, 1 / length)
end
local function multiply4(a, b)
    local result = {}
    for row = 0, 3 do
        for column = 0, 3 do
            local value = 0
            for k = 0, 3 do value = value + a[row * 4 + k + 1] * b[k * 4 + column + 1] end
            result[row * 4 + column + 1] = value
        end
    end
    return result
end
local function apply4(matrix, vector)
    local result = {}
    for row = 0, 3 do
        local base = row * 4
        result[row + 1] = matrix[base + 1] * vector[1]
            + matrix[base + 2] * vector[2]
            + matrix[base + 3] * vector[3]
            + matrix[base + 4] * vector[4]
    end
    return result
end
local function format4(value)
    return string.format("(%.2f, %.2f, %.2f, %.2f)", value[1], value[2], value[3], value[4])
end
local function format3(value)
    return string.format("(%.2f, %.2f, %.2f)", value[1], value[2], value[3])
end
local function point3(value) return {value[1], value[2], value[3]} end
local function translation(x, y, z)
    return {1, 0, 0, x, 0, 1, 0, y, 0, 0, 1, z, 0, 0, 0, 1}
end
local function scaleMatrix(x, y, z)
    return {x, 0, 0, 0, 0, y, 0, 0, 0, 0, z, 0, 0, 0, 0, 1}
end
local function rotateX(angle)
    local c, s = math.cos(angle), math.sin(angle)
    return {1, 0, 0, 0, 0, c, -s, 0, 0, s, c, 0, 0, 0, 0, 1}
end
local function rotateY(angle)
    local c, s = math.cos(angle), math.sin(angle)
    return {c, 0, s, 0, 0, 1, 0, 0, -s, 0, c, 0, 0, 0, 0, 1}
end

local model = multiply4(
    translation(0.48, -0.12, 0.32),
    multiply4(rotateY(math.rad(31)), multiply4(rotateX(math.rad(-12)), scaleMatrix(1.14, 0.92, 1.06)))
)
local conceptEye, conceptTarget, conceptUp = {3.45, 2.55, 4.75}, {0.25, 0.12, 0}, {0, 1, 0}
local forward = normalize(subtract(conceptTarget, conceptEye))
local right = normalize(cross(forward, conceptUp))
local cameraUp = cross(right, forward)
local view = {
    right[1], right[2], right[3], -dot(right, conceptEye),
    cameraUp[1], cameraUp[2], cameraUp[3], -dot(cameraUp, conceptEye),
    -forward[1], -forward[2], -forward[3], dot(forward, conceptEye),
    0, 0, 0, 1,
}
local aspect, near, far, fov = 16 / 9, 0.25, 30, math.rad(46)
local focal = 1 / math.tan(fov / 2)
local projection = {
    focal / aspect, 0, 0, 0,
    0, focal, 0, 0,
    0, 0, (far + near) / (near - far), (2 * far * near) / (near - far),
    0, 0, -1, 0,
}

local objectVertices = {}
for index, point in ipairs({
    {-0.90, -0.72, -0.68, 1}, {0.90, -0.72, -0.68, 1},
    {0.90, 0.82, -0.68, 1}, {-0.90, 0.82, -0.68, 1},
    {-0.90, -0.72, 0.68, 1}, {0.90, -0.72, 0.68, 1},
    {0.90, 0.82, 0.68, 1}, {-0.90, 0.82, 0.68, 1},
}) do
    objectVertices[index] = {id = "v" .. index - 1, point = point}
end
local cubeFaces = {
    {1, 2, 3, 4}, {5, 8, 7, 6}, {1, 5, 6, 2},
    {4, 3, 7, 8}, {2, 6, 7, 3}, {1, 4, 8, 5},
}
local cubeEdges = {
    {1, 2}, {2, 3}, {3, 4}, {4, 1}, {5, 6}, {6, 7}, {7, 8}, {8, 5},
    {1, 5}, {2, 6}, {3, 7}, {4, 8},
}
local function edgeLines(group, points, color, width, idPrefix, layer)
    local lines = {}
    for index, edge in ipairs(cubeEdges) do
        lines[index] = group:line {
            from = points[edge[1]], to = points[edge[2]], color = color, width = width,
            layer = layer, id = idPrefix .. index,
        }
    end
    return lines
end
for _, vertex in ipairs(objectVertices) do
    vertex.world = apply4(model, vertex.point)
    vertex.view = apply4(view, vertex.world)
    vertex.clip = apply4(projection, vertex.view)
    assert(math.abs(vertex.clip[4]) > 1e-9, "perspective divide requires non-zero w")
    vertex.ndc = {
        vertex.clip[1] / vertex.clip[4],
        vertex.clip[2] / vertex.clip[4],
        vertex.clip[3] / vertex.clip[4],
    }
    assert(math.abs(vertex.ndc[1]) < 1 and math.abs(vertex.ndc[2]) < 1, "mesh must fit the concept camera")
end
local selected = objectVertices[7]
local objectPoints = {}
for index, vertex in ipairs(objectVertices) do objectPoints[index] = point3(vertex.point) end

local planeDistance = 1.25
local planeCenter = add(conceptEye, scale(forward, planeDistance))
local halfHeight = math.tan(fov / 2) * planeDistance
local halfWidth = halfHeight * aspect
local planeCorners = {
    add(add(planeCenter, scale(right, -halfWidth)), scale(cameraUp, -halfHeight)),
    add(add(planeCenter, scale(right, halfWidth)), scale(cameraUp, -halfHeight)),
    add(add(planeCenter, scale(right, halfWidth)), scale(cameraUp, halfHeight)),
    add(add(planeCenter, scale(right, -halfWidth)), scale(cameraUp, halfHeight)),
}
local planeProjected = {}
for index, vertex in ipairs(objectVertices) do
    local worldPoint = {vertex.world[1], vertex.world[2], vertex.world[3]}
    local direction = subtract(worldPoint, conceptEye)
    local divisor = dot(direction, forward)
    assert(divisor > 1e-9, "projected vertex must be in front of the concept camera")
    planeProjected[index] = add(conceptEye, scale(direction, planeDistance / divisor))
end

local spatial = vis.scene {
    width = 620, height = 410, fps = 30, loop = false, theme = "adaptive",
    camera = {
        mode = "fixed", view = "3d", eye = {7.20, 4.60, 0.00}, target = {1.90, 1.05, 2.45},
        up = {0, 1, 0}, projection = "perspective", fov = 0.74, near = 0.1, far = 40,
    },
}
local space = spatial:space {
    x = {-3.0, 5.4, 1}, y = {-2.2, 3.8, 1}, z = {-3.0, 6.7, 1},
    opacity = 0.11, id = "mvp:world-space",
}
local mesh = space:group {id = "mvp:object-mesh"}
local meshFaces, meshPoints = {}, {}
for index, face in ipairs(cubeFaces) do
    local points = {}
    for _, vertexIndex in ipairs(face) do
        points[#points + 1] = point3(objectVertices[vertexIndex].point)
    end
    meshFaces[index] = mesh:polygon {
        points = points, fill = "#4fc1ff30",
        stroke = "border", width = 1.0, id = "mvp:object-face:" .. index,
    }
end
local meshEdges = edgeLines(mesh, objectPoints, "accent", 1.5, "mvp:object-edge:")
for index, vertex in ipairs(objectVertices) do
    meshPoints[index] = mesh:point {
        point = objectPoints[index], radius = index == 7 and 8 or 4,
        fill = index == 7 and "result" or "accent", stroke = "background", width = 1,
        layer = 10, id = "mvp:" .. vertex.id,
    }
end
local selectedLabel = mesh:text {
    text = selected.id, point = {selected.point[1], selected.point[2] + 0.22, selected.point[3]},
    role = "code", fill = "result", layer = 20, id = "mvp:selected-label",
}

local conceptCamera = space:group {id = "mvp:concept-camera"}
local cameraPoint = conceptCamera:point {
    point = conceptEye, radius = 9, fill = "warning", stroke = "background", width = 1.5,
    layer = 10, id = "mvp:camera-eye",
}
local cameraLabel = conceptCamera:text {
    text = "camera C", point = add(conceptEye, {0.34, 0.56, 0}), align = {0, 0.5}, role = "code",
    fill = "warning", layer = 20, id = "mvp:camera-label",
}
local imagePlane = conceptCamera:polygon {
    points = planeCorners, fill = "#ffd86620", stroke = "warning", width = 1.25,
    id = "mvp:image-plane",
}
local frustum = {}
local function addFrustumLine(from, to, width, id)
    frustum[#frustum + 1] = conceptCamera:line {
        from = from, to = to, color = "warning", width = width, id = id,
    }
end
for index, corner in ipairs(planeCorners) do
    addFrustumLine(conceptEye, corner, 1, "mvp:frustum-ray:" .. index)
end
for index = 1, 4 do
    addFrustumLine(planeCorners[index], planeCorners[index % 4 + 1], 1.25, "mvp:frustum-edge:" .. index)
end
local projectedEdges = edgeLines(conceptCamera, planeProjected, "result", 1.5, "mvp:projected-edge:", 5)
local projectionRay = conceptCamera:line {
    from = conceptEye,
    to = {selected.world[1], selected.world[2], selected.world[3]},
    color = "result", width = 2, layer = 8, id = "mvp:selected-ray",
}
local projectedPoint = conceptCamera:point {
    point = planeProjected[7], radius = 7, fill = "result", stroke = "background", width = 1.2,
    layer = 10, id = "mvp:selected-projection",
}

spatial:create(space, 0.30, "ease_out")
spatial:draw_border_then_fill(meshFaces, 0.54, "ease_out", 0.025)
spatial:create(meshEdges, 0.50, "ease_out", 0.02)
spatial:create(meshPoints, 0.28, "ease_out", 0.025)
spatial:fade_in(selectedLabel, {duration = 0.16, curve = "gentle"})
local objectReady = spatial:duration()
spatial:wait(0.48)
spatial:transform(mesh, model, 1.15, "ease_in_out")
local worldReady = spatial:duration()
spatial:wait(0.55)
spatial:create({cameraPoint, imagePlane}, 0.42, "ease_out", 0.08)
spatial:fade_in(cameraLabel, {duration = 0.18, curve = "gentle"})
spatial:create(frustum, 0.52, "ease_out", 0.025)
local viewReady = spatial:duration()
spatial:wait(0.42)
spatial:create(projectionRay, 0.76, "linear")
local clipReady = spatial:duration()
spatial:create(projectedEdges, 0.48, "ease_out", 0.018)
local ndcReady = spatial:duration()
spatial:create(projectedPoint, 0.24, "ease_out")
local viewportReady = spatial:duration()
spatial:wait(2.10)
local spatialEnd = spatial:duration()

local ledger = vis.scene {
    width = 336, height = 405, fps = 30, loop = false, theme = "adaptive",
    camera = {mode = "fixed", view = "2d", target = {0, 0}, height = 7.4},
}
-- Stage labels stay ink; only the final ndc stage carries the result hue.
local rows = {
    {"object", format4(selected.point), "muted"},
    {"world = M p", format4(selected.world), "muted"},
    {"view = V p", format4(selected.view), "muted"},
    {"clip = P p", format4(selected.clip), "muted"},
    {"ndc = xyz / w", format3(selected.ndc), "result"},
}
local rowObjects = {}
for index, row in ipairs(rows) do
    local y = 3.15 - (index - 1) * 0.87
    local group = ledger:group {id = "mvp:stage:" .. index}
    group:text {
        text = row[1], point = {-2.75, y + 0.215}, align = {0, 0.5}, role = "code", size = 11.5,
        fill = row[3], id = "mvp:stage-label:" .. index,
    }
    group:text {
        text = row[2], point = {-2.75, y - 0.215}, align = {0, 0.5}, role = "code", size = 11,
        fill = "foreground", id = "mvp:stage-value:" .. index,
    }
    rowObjects[index] = group
end
local screenCenter, screenSize = {0, -2.04}, {4.80, 2.20}
local viewport = ledger:rectangle {
    center = screenCenter, size = screenSize,
    fill = "#00000000", stroke = "border", width = 1, id = "mvp:viewport-frame",
}
local function toMiniScreen(ndc)
    return {
        screenCenter[1] + ndc[1] * screenSize[1] * 0.46,
        screenCenter[2] + ndc[2] * screenSize[2] * 0.46,
    }
end
local screenPoints = {}
for index, vertex in ipairs(objectVertices) do screenPoints[index] = toMiniScreen(vertex.ndc) end
local screenEdges = edgeLines(ledger, screenPoints, "result", 1.5, "mvp:viewport-edge:")
local screenPoint = ledger:point {
    point = toMiniScreen(selected.ndc), radius = 6, fill = "result", stroke = "background", width = 1,
    layer = 10, id = "mvp:viewport-selected",
}
local viewportLabel = ledger:text {
    text = "viewport: same mesh, same v6", point = {0, -3.44}, role = "code",
    size = 11.5, fill = "result", id = "mvp:viewport-label",
}
local function waitUntil(scene, time)
    local remaining = time - scene:duration()
    if remaining > 0 then scene:wait(remaining) end
end
local rowTimes = {objectReady, worldReady, viewReady, clipReady, ndcReady}
for index, group in ipairs(rowObjects) do
    waitUntil(ledger, rowTimes[index] - 0.28)
    ledger:fade_in(group, {shift = {0, 0.06}, duration = 0.28, curve = "gentle"})
end
waitUntil(ledger, viewportReady - 0.30)
ledger:create(viewport, 0.30, "ease_out")
ledger:create(screenEdges, 0.48, "ease_out", 0.018)
ledger:create(screenPoint, 0.22, "ease_out")
ledger:fade_in(viewportLabel, {duration = 0.28, curve = "gentle"})
waitUntil(ledger, spatialEnd)

local subtitle = page:text {
    text = "one object mesh · one concept camera · one vertex lineage into the viewport",
    point = {0, 2.25}, role = "text", fill = "muted", id = "mvp:subtitle",
}
page:fade_in(subtitle, {duration = 0.28, curve = "gentle"})
page:viewport(spatial, {x = 0.015, y = 0.18, width = 0.613, height = 0.72})
page:viewport(ledger, {x = 0.638, y = 0.18, width = 0.336, height = 0.72})
return page
