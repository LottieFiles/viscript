-- Advanced reference: estimate pixel coverage and magnify one boundary sample pattern.
local scene = vis.scene {
    width = 960, height = 540, fps = 30, loop = false,
    camera = {mode = "fixed", view = "2d", target = {0, 0}, height = 6.4},
}

local columns, rows, samples = 8, 6, 10
local circle = {center = {3.25, 2.4}, radius = 2.15}
local function inside(x, y)
    return (x - circle.center[1])^2 + (y - circle.center[2])^2 <= circle.radius^2
end
local function coverageAt(pixelX, pixelY)
    local covered = 0
    for sy = 0, samples - 1 do
        for sx = 0, samples - 1 do
            local x = pixelX - 0.5 + (sx + 0.5) / samples
            local y = pixelY - 0.5 + (sy + 0.5) / samples
            if inside(x, y) then covered = covered + 1 end
        end
    end
    return covered / (samples * samples)
end
local function coverageColor(value)
    if value <= 0 then return "#00000000" end
    return string.format("#4fc1ff%02x", math.floor(40 + 215 * value + 0.5))
end
local function coveragePatches()
    local result = {}
    for y = 0, rows - 1 do
        for x = 0, columns - 1 do result[#result + 1] = {region = {x, y, 1, 1}, color = coverageColor(coverageAt(x, y))} end
    end
    return result
end
local function selectBoundaryPixel()
    local best, error = nil, math.huge
    for y = 0, rows - 1 do
        for x = 0, columns - 1 do
            local value = coverageAt(x, y)
            if value > 0 and value < 1 and math.abs(value - 0.5) < error then best, error = {x = x, y = y, coverage = value}, math.abs(value - 0.5) end
        end
    end
    return best
end
local selected = selectBoundaryPixel()
local gridScale, gridCenter = 0.68, {-2.4, 0}
local gridMatrix = {gridScale, 0, 0, gridCenter[1] - 0.5 * (columns - 1) * gridScale,
                    0, gridScale, 0, gridCenter[2] - 0.5 * (rows - 1) * gridScale,
                    0, 0, 1, 0, 0, 0, 0, 1}
local function mapPoint(matrix, point)
    return {matrix[1] * point[1] + matrix[2] * point[2] + matrix[4], matrix[5] * point[1] + matrix[6] * point[2] + matrix[8]}
end

-- The vector boundary and raster coverage occupy the same pixel coordinate system. The selected pixel and
-- its magnifier stay in ink; `result` marks the shape, its covered subsamples, and the measured value.
local grid = scene:space {id = "coverage-grid-space", x = {0, columns - 1, 1}, y = {0, rows - 1, 1}, opacity = 0, matrix = gridMatrix}
local base = grid:cell {id = "coverage-base", origin = {-0.5, -0.5}, size = {columns, rows}, mode = "padd", padding = 0.05, color = "surface", layer = -5}
local coverage = grid:cell {id = "coverage-field", origin = {-0.5, -0.5}, size = {columns, rows}, mode = "padd", padding = 0.05, patches = coveragePatches()}
local boundary = grid:circle {
    id = "coverage-shape-boundary", center = circle.center, radius = circle.radius,
    fill = "#4fc1ff12", stroke = "result", width = 2, layer = 20,
}
local focus = grid:rectangle {
    id = "coverage-selected-pixel", center = {selected.x, selected.y}, size = {0.94, 0.94},
    fill = "#00000000", stroke = "foreground", width = 2, layer = 30,
}

-- A second Cell expands the exact binary subsamples used by coverageAt.
local detailScale, detailCenter = 0.18, {3.15, 0}
local detailMatrix = {detailScale, 0, 0, detailCenter[1] - 0.5 * (samples - 1) * detailScale,
                      0, detailScale, 0, detailCenter[2] - 0.5 * (samples - 1) * detailScale,
                      0, 0, 1, 0, 0, 0, 0, 1}
local detail = scene:space {id = "coverage-detail-space", x = {0, samples - 1, 1}, y = {0, samples - 1, 1}, opacity = 0, matrix = detailMatrix}
local detailPatches = {}
for sy = 0, samples - 1 do
    for sx = 0, samples - 1 do
        local x = selected.x - 0.5 + (sx + 0.5) / samples
        local y = selected.y - 0.5 + (sy + 0.5) / samples
        detailPatches[#detailPatches + 1] = {region = {sx, sy, 1, 1}, color = inside(x, y) and "result" or "surface"}
    end
end
local detailCell = detail:cell {id = "coverage-subsamples", origin = {-0.5, -0.5}, size = {samples, samples}, mode = "padd", padding = 0.08, patches = detailPatches}
local detailFrame = detail:rectangle {id = "coverage-detail-frame", center = {4.5, 4.5}, size = {10, 10}, fill = "#00000000", stroke = "foreground", width = 1.25, layer = 30}
local sourceTop = mapPoint(gridMatrix, {selected.x + 0.5, selected.y + 0.5})
local sourceBottom = mapPoint(gridMatrix, {selected.x + 0.5, selected.y - 0.5})
local targetTop = mapPoint(detailMatrix, {-0.5, 9.5})
local targetBottom = mapPoint(detailMatrix, {-0.5, -0.5})
local wedge = {
    scene:line {id = "coverage-wedge-top", from = sourceTop, to = targetTop, stroke = "muted", width = 1, layer = 10},
    scene:line {id = "coverage-wedge-bottom", from = sourceBottom, to = targetBottom, stroke = "muted", width = 1, layer = 10},
}
local labels = {
    scene:text {id = "coverage-detail-label", text = string.format("%d × %d subsamples", samples, samples), point = {detailCenter[1], 1.38}, role = "code", fill = "muted", align = {0.5, 0.5}, layer = 40},
    scene:text {id = "coverage-value", text = string.format("coverage(%d, %d) = %.2f", selected.x, selected.y, selected.coverage), point = {detailCenter[1], -1.42}, role = "code", fill = "result", align = {0.5, 0.5}, layer = 40},
}

-- Reveal the continuous boundary first, then its discrete area estimate.
scene:create(base, 0.55, "ease_out")
scene:create(boundary, 0.75, "ease_out")
scene:create(coverage, 0.75, "linear")
scene:create(focus, 0.28, "ease_out")
scene:create(wedge, 0.4, "ease_out", 0.06)
scene:create({detailCell, detailFrame}, 0.65, "ease_out", 0.04)
scene:fade_in(labels[1], {shift = {0, 0.08}, duration = 0.28, curve = "gentle"})
scene:fade_in(labels[2], {shift = {0, 0.08}, duration = 0.3, curve = "gentle"})
scene:wait(1.3)
return scene
