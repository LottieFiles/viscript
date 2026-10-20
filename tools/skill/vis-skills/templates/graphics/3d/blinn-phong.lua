-- Reference study: a moving point light re-evaluates flat Blinn-Phong shading on
-- an explicit indexed low-poly mesh. viscript has no scene light/material system;
-- every face color below is calculated author-side from the same N/L/V/H data
-- shown at the selected surface sample.
local page = vis.scene {
    width = 960, height = 540, fps = 30, loop = false,
    theme = "adaptive",
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
local function length(v) return math.sqrt(dot(v, v)) end
local function normalize(v)
    local magnitude = length(v)
    assert(magnitude > 1e-9, "cannot normalize a zero vector")
    return scale(v, 1 / magnitude)
end
local function clamp(value, low, high) return math.max(low, math.min(high, value)) end
local function translation(point)
    return {1, 0, 0, point[1], 0, 1, 0, point[2], 0, 0, 1, point[3], 0, 0, 0, 1}
end
local function frameFromX(origin, direction)
    local x = normalize(direction)
    local helper = math.abs(x[2]) > 0.92 and {0, 0, 1} or {0, 1, 0}
    local z = normalize(cross(x, helper))
    local y = cross(z, x)
    return {
        x[1], y[1], z[1], origin[1],
        x[2], y[2], z[2], origin[2],
        x[3], y[3], z[3], origin[3],
        0, 0, 0, 1,
    }
end
local function srgb(linear)
    linear = clamp(linear, 0, 1)
    if linear <= 0.0031308 then return 12.92 * linear end
    return 1.055 * (linear ^ (1 / 2.4)) - 0.055
end
local function hexByte(value) return math.floor(clamp(value, 0, 1) * 255 + 0.5) end
local function rgbHex(rgb)
    return string.format("#%02x%02x%02x", hexByte(srgb(rgb[1])), hexByte(srgb(rgb[2])), hexByte(srgb(rgb[3])))
end

-- Explicit vertex/index buffers for a closed, faceted gem mesh.
local vertices = {{0, 1.48, 0}}
local segments = 6
for i = 0, segments - 1 do
    local angle = 2 * math.pi * i / segments
    vertices[#vertices + 1] = {1.08 * math.cos(angle), 0.55, 1.08 * math.sin(angle)}
end
for i = 0, segments - 1 do
    local angle = 2 * math.pi * (i + 0.5) / segments
    vertices[#vertices + 1] = {0.86 * math.cos(angle), -0.66, 0.86 * math.sin(angle)}
end
vertices[#vertices + 1] = {0, -1.42, 0}

local indices = {}
for i = 0, segments - 1 do
    local upper0 = 2 + i
    local upper1 = 2 + ((i + 1) % segments)
    local lower0 = 2 + segments + i
    local lower1 = 2 + segments + ((i + 1) % segments)
    indices[#indices + 1] = {1, upper0, upper1}
    indices[#indices + 1] = {upper0, lower0, lower1}
    indices[#indices + 1] = {upper0, lower1, upper1}
    indices[#indices + 1] = {#vertices, lower1, lower0}
end

local function faceFromIndices(faceIndices)
    local a, b, c = vertices[faceIndices[1]], vertices[faceIndices[2]], vertices[faceIndices[3]]
    local centroid = scale(add(add(a, b), c), 1 / 3)
    local normal = normalize(cross(subtract(b, a), subtract(c, a)))
    if dot(normal, centroid) < 0 then
        b, c = c, b
        faceIndices = {faceIndices[1], faceIndices[3], faceIndices[2]}
        normal = normalize(cross(subtract(b, a), subtract(c, a)))
    end
    return {indices = faceIndices, points = {a, b, c}, centroid = centroid, normal = normal}
end

local faces = {}
for _, faceIndices in ipairs(indices) do faces[#faces + 1] = faceFromIndices(faceIndices) end

-- Pick a visible asymmetric face instead of hard-coding a sample point.
local desiredNormal = normalize({0.80, 0.45, 1.00})
local selectedIndex, selectedScore = 1, -math.huge
for index, face in ipairs(faces) do
    local score = dot(face.normal, desiredNormal) + 0.05 * face.centroid[2]
    if score > selectedScore then selectedIndex, selectedScore = index, score end
end
local selected = faces[selectedIndex]
local sample = selected.centroid
local normal = selected.normal

local viewPosition = {4.60, 3.25, 6.10}
local lightA = {-0.75, 2.35, 2.10}
local lightB = {2.45, 1.55, -1.30}
local baseColor = {0.075, 0.32, 0.72}
local ambientStrength, diffuseStrength, specularStrength, shininess = 0.07, 0.82, 0.72, 28

local function evaluate(position, faceNormal, lightPosition)
    local light = normalize(subtract(lightPosition, position))
    local view = normalize(subtract(viewPosition, position))
    local halfVector = normalize(add(light, view))
    local nDotL = math.max(dot(faceNormal, light), 0)
    local nDotH = math.max(dot(faceNormal, halfVector), 0)
    local ambient = ambientStrength
    local diffuse = diffuseStrength * nDotL
    local specular = nDotL > 0 and specularStrength * (nDotH ^ shininess) or 0
    local linear = {
        baseColor[1] * (ambient + diffuse) + specular,
        baseColor[2] * (ambient + diffuse) + specular,
        baseColor[3] * (ambient + diffuse) + specular,
    }
    return {
        L = light, V = view, H = halfVector,
        nDotL = nDotL, nDotH = nDotH,
        ambient = ambient, diffuse = diffuse, specular = specular,
        color = rgbHex(linear),
    }
end

local stateA = evaluate(sample, normal, lightA)
local stateB = evaluate(sample, normal, lightB)
local faceColorsA, faceColorsB = {}, {}
for index, face in ipairs(faces) do
    faceColorsA[index] = evaluate(face.centroid, face.normal, lightA).color
    faceColorsB[index] = evaluate(face.centroid, face.normal, lightB).color
end
assert(math.abs(stateA.nDotL - stateB.nDotL) > 0.15, "light motion must visibly change the selected sample")

local spatial = vis.scene {
    width = 620, height = 410, fps = 30, loop = false, theme = "adaptive",
    camera = {
        mode = "fixed", view = "3d", eye = viewPosition, target = {0, 0.05, 0}, up = {0, 1, 0},
        projection = "perspective", fov = 0.62, near = 0.1, far = 30,
    },
}
local space = spatial:space {
    x = {-4.2, 4.5, 1}, y = {-2.0, 4.2, 1}, z = {-3.0, 4.8, 1},
    opacity = 0.12, id = "blinn:space",
}
local mesh = space:group {id = "blinn:mesh"}
local faceObjects = {}
for index, face in ipairs(faces) do
    faceObjects[index] = mesh:polygon {
        points = face.points,
        fill = faceColorsA[index], stroke = index == selectedIndex and "result" or "border",
        width = index == selectedIndex and 2.0 or 1.0,
        id = "blinn:face:" .. index,
    }
end
local samplePoint = mesh:point {
    point = sample, radius = 7, fill = "result", stroke = "background", width = 1.5,
    layer = 8, id = "blinn:sample",
}

local function vectorOwner(id, direction, color, vectorLength, labelOffset, labelExtra)
    local owner = space:group {matrix = frameFromX(sample, direction), id = "blinn:" .. id .. ":owner"}
    local vector = owner:vector {
        origin = {0, 0, 0}, value = {vectorLength, 0, 0}, color = color,
        width = 2, tip = 7, layer = 12, id = "blinn:" .. id,
    }
    local label = owner:text {
        text = id, point = {vectorLength + (labelExtra or 0.20), labelOffset or 0, 0}, role = "code", fill = color,
        layer = 20, id = "blinn:" .. id .. ":label",
    }
    return owner, vector, label
end

local normalOwner, normalVector, normalLabel = vectorOwner("N", normal, "accent", 1.25)
local lightVectorOwner, lightVector, lightVectorLabel = vectorOwner("L", stateA.L, "warning", 1.45, -0.20, 0.10)
local viewVectorOwner, viewVector, viewVectorLabel = vectorOwner("V", stateA.V, "secondary", 1.35)
local halfVectorOwner, halfVector, halfVectorLabel = vectorOwner("H", stateA.H, "success", 1.62, 0.20, 0.62)
local lightOwner = space:group {matrix = translation(lightA), id = "blinn:light-owner"}
local lightPoint = lightOwner:point {
    point = {0, 0, 0}, radius = 10, fill = "warning", stroke = "background", width = 1.5,
    layer = 10, id = "blinn:light",
}
local lightLabel = lightOwner:text {
    text = "point light", point = {0, -0.42, 0}, align = {0.5, 0.5}, role = "code", fill = "warning",
    layer = 20, id = "blinn:light-label",
}

spatial:create(space, 0.30, "ease_out")
spatial:draw_border_then_fill(faceObjects, 0.62, "ease_out", 0.018)
spatial:create(samplePoint, 0.24, "ease_out")
spatial:create(lightOwner, 0.32, "ease_out")
spatial:create({normalVector, lightVector, viewVector}, 0.62, "ease_out", 0.08)
spatial:fade_in(normalLabel, {duration = 0.16, curve = "gentle"})
spatial:fade_in(lightVectorLabel, {duration = 0.16, curve = "gentle"})
spatial:fade_in(viewVectorLabel, {duration = 0.16, curve = "gentle"})
spatial:wait(0.38)
spatial:create(halfVector, 0.48, "ease_out")
spatial:fade_in(halfVectorLabel, {duration = 0.18, curve = "gentle"})
spatial:wait(0.72)
local lightMoveStart = spatial:duration()

local changes = {
    {target = lightOwner, transform = translation(lightB)},
    {target = lightVectorOwner, transform = frameFromX(sample, stateB.L)},
    {target = halfVectorOwner, transform = frameFromX(sample, stateB.H)},
}
for index, object in ipairs(faceObjects) do
    changes[#changes + 1] = {target = object, fill = faceColorsB[index]}
end
spatial:play(changes, 1.45, "ease_in_out", 0)
local lightMoveEnd = spatial:duration()
spatial:wait(2.15)
local spatialEnd = spatial:duration()

local ledger = vis.scene {
    width = 336, height = 405, fps = 30, loop = false, theme = "adaptive",
    camera = {mode = "fixed", view = "2d", target = {0, 0}, height = 7.4},
}
local formula = ledger:text {
    text = "H = normalize(L + V)\nC = ambient + diffuse + specular",
    point = {-2.78, 2.75}, align = {0, 0.5}, role = "code", size = 12.0,
    fill = "foreground", id = "blinn:formula",
}
local function stateGroup(id, y, label, state)
    local group = ledger:group {id = "blinn:ledger:" .. id}
    group:text {
        text = label, point = {-2.78, y + 0.43}, align = {0, 0.5}, role = "h3",
        fill = id == "B" and "result" or "muted", id = "blinn:ledger:" .. id .. ":title",
    }
    group:text {
        text = string.format("N·L = %.3f   N·H = %.3f", state.nDotL, state.nDotH),
        point = {-2.78, y}, align = {0, 0.5}, role = "code", size = 12.0,
        fill = "foreground", id = "blinn:ledger:" .. id .. ":dots",
    }
    group:text {
        text = string.format("A %.3f  D %.3f  S %.3f", state.ambient, state.diffuse, state.specular),
        point = {-2.78, y - 0.42}, align = {0, 0.5}, role = "code", size = 12.0,
        fill = "foreground", id = "blinn:ledger:" .. id .. ":terms",
    }
    group:rectangle {
        center = {2.45, y - 0.18}, size = {0.92, 0.92},
        fill = state.color, stroke = id == "B" and "result" or "border", width = 1.25,
        id = "blinn:ledger:" .. id .. ":swatch",
    }
    return group
end
local poseA = stateGroup("A", 1.35, "LIGHT POSE A", stateA)
local arrow = ledger:text {
    text = "light moves → recompute dependents", point = {0, -0.12}, role = "code",
    size = 11.5, fill = "warning", id = "blinn:recompute",
}
local poseB = stateGroup("B", -1.20, "LIGHT POSE B", stateB)
local invariant = ledger:text {
    text = string.format("sample face #%d · N and V stay fixed", selectedIndex),
    point = {0, -3.12}, role = "code", size = 11.5, fill = "muted", id = "blinn:invariant",
}
ledger:fade_in(formula, {duration = 0.34, curve = "gentle"})
ledger:fade_in(poseA, {shift = {0.10, 0}, duration = 0.48, curve = "ease_out"})
local function waitUntil(scene, time)
    local remaining = time - scene:duration()
    if remaining > 0 then scene:wait(remaining) end
end
waitUntil(ledger, lightMoveStart - 0.26)
ledger:fade_in(arrow, {duration = 0.26, curve = "gentle"})
ledger:fade_in(poseB, {shift = {0.10, 0}, duration = lightMoveEnd - lightMoveStart, curve = "ease_out"})
ledger:fade_in(invariant, {duration = 0.28, curve = "gentle"})
waitUntil(ledger, spatialEnd)

local subtitle = page:text {
    text = "move the light → rebuild L and H → recompute every face color",
    point = {0, 2.25}, role = "text", fill = "muted", id = "blinn:subtitle",
}
page:fade_in(subtitle, {duration = 0.28, curve = "gentle"})
page:viewport(spatial, {x = 0.015, y = 0.18, width = 0.613, height = 0.72})
page:viewport(ledger, {x = 0.638, y = 0.18, width = 0.336, height = 0.72})
return page
