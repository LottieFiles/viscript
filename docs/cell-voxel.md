# Cells and voxels

A Space can sample a coordinate field into colored cells or voxels. Use this for
implicit equations, occupancy maps, and scalar fields.

`cell` samples X/Y into one batched Cell at Z=0. `voxel` samples X/Y/Z into one
batched Cell per Z slice. Both return a Group that can move, fade, and transform
with the rest of the scene.

## Sampling

Callbacks run while the scene is authored. Their colors become retained data:

1. Read the Space ranges and sampling options.
2. Sample each coordinate at time zero, or across `duration` at the requested `fps`.
3. Retain both time endpoints and attach the completed field Group.
4. Advance the Scene cursor by `duration`.
5. Interpolate retained RGBA colors when rendering any requested time.

Each axis follows the Space lattice around zero:

```text
first = ceil(range.min / range.step) * range.step
value(i) = first + i * range.step
```

Coordinates are cell centers. Each cell's dimensions are the range steps; the
Space and ancestor models place them in the scene. `mode="full"` fills that
footprint, while `mode="padd"` adds a proportional gap. The defaults are
`mode="padd"` and `padding=0.05`.

A temporal field begins at the current Scene cursor and holds its last frame after
its duration. Fields in a detached C++ subtree share a start when the subtree is
attached, and attachment advances by their longest duration.

## Lua

```lua
local scene = vis.scene {width = 960, height = 540}
local field = scene:space {
    x = {-2, 2, 0.25}, y = {-2, 2, 0.25}, z = {-2, 2, 0.25},
}

local cells = field:cell(function(x, y, time)
    local a = 1.5 + 0.5 * time
    return x*x/(a*a) + y*y/2.25 <= 1 and "#0891b2" or "#00000000"
end, {duration = 1, fps = 24})

local voxels = field:voxel(function(x, y, z, time)
    local radius = 1 + 0.5 * time
    if x*x + y*y + z*z > radius*radius then return "#00000000" end
    return y < 0 and "#2563eb" or "#7c3aed"
end, {padding = 0.08, duration = 1, fps = 24})

return scene
```

The callback's final argument is field-local time. Transparent colors leave the
corresponding coordinate visually empty.

## JavaScript

The builder samples the callback and serializes retained colors into Lua source:

```js
const field = scene.space({
    x: [-2, 2, 0.25], y: [-2, 2, 0.25], z: [-2, 2, 0.25],
});

const cells = field.cell((x, y, time) => {
    const a = 1.5 + 0.5 * time;
    return x*x/(a*a) + y*y/2.25 <= 1 ? "#0891b2" : "#00000000";
}, {duration: 1, fps: 24});

const voxels = field.voxel((x, y, z, time) => {
    const radius = 1 + 0.5 * time;
    if (Math.hypot(x, y, z) > radius) return "#00000000";
    return y < 0 ? "#2563eb" : "#7c3aed";
}, {padding: 0.08, duration: 1, fps: 24});
```

## C++

The native callback writes a Color and returns its status. `data` passes
caller-supplied sampling context through unchanged.

```cpp
static bool sample(float x, float y, float z, float time,
                   vis::Color& color, void*) noexcept
{
    auto radius = 1.0f + time * 0.5f;
    color = x*x + y*y + z*z < radius*radius
          ? vis::Color::hex("#2563eb")
          : vis::Color::hex("#00000000");
    return true;
}

auto space = vis::Space::gen({-2, 2, 0.25f},
                              {-2, 2, 0.25f},
                              {-2, 2, 0.25f});
vis::Group* voxels = nullptr;
auto result = space->voxel(sample, voxels, nullptr,
                           vis::CellMode::Padd, 0.08f, {1.0f, 24u});
if (result == vis::Result::Success) result = scene->add(space);
```

The public types are `CellSampler`, `VoxelSampler`, and `SampleTime`. See
[Scene composition](scene-composition.md) for ownership and timeline usage.
