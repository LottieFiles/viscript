# A surface morph with six endpoint labels

Start from [surface-morph.lua](../templates/graphics/3d/surface-morph.lua). It uses a 960 by 540 frame, perspective projection with a 42 degree field of view, an origin target, z-up, a 65 degree polar angle, and a linear azimuth orbit from -60 to +30 degrees. The camera radius is 8 world units. The scene morphs `0.25 * (x*x + y*y) - 1` into `0.25 * (x*x - y*y)` on `[-2, 2]` in both coordinates; the marker at `(1, 1)` follows that same retained morph parameter.

## Choose a visible view before placing labels

Choose a camera that reveals the bowl bottom and preserves the surface's physical depth. This template uses radius 8 and a 65 degree polar angle. Check the bowl rim and origin marker through the orbit before placing endpoint labels.

## Retain endpoint placement in pixels

Give the Space its real ranges and place labels at their endpoint values:

```lua
local label_size, tick_length = 18, 8
local label_gap = 2 * label_size * 4 / 3  -- two font ems
axes:labels {placement = "screen", size = label_size, gap = label_gap + tick_length,
    x = {{-3, "x"}, {3, "x"}}}
axes:labels {placement = "screen", size = label_size, gap = label_gap,
    y = {{-3, "y"}, {3, "y"}}, z = {{-2, "z"}, {2, "z"}}}
```

The six ordinary Text children inherit the Space fade, so `s:fade(axes, 1, 1)` shows the axes and labels together. Their anchors remain on the endpoints; the engine projects those anchors at each sample and offsets actual font ink outward by a pixel gap. No label needs a guessed world-coordinate extension, and rendering runs no user callback. Two font ems of gap keep y and z labels clear of this surface. The x labels use one additional tick length to separate the nearly aligned x and z endpoint projections as azimuth passes zero. At size 18 with an 8 pixel tick length these gaps are 48 and 56 pixels; they grow with the shared font size. The extra room covers the independently checked nearby range and font changes. Adjust the gap alongside the camera and font size, then inspect projected clearance.

Review the selected font, camera, canvas, ranges, and orbit together. The template uses physical depth, fixed world lighting, and retained height colors.

## Review in the delivery host

Use a loaded delivery font and inspect the bowl, midpoint, saddle, and the camera view where projected axes align:

```sh
viscript inspect surface-morph.lua --font "$VIS_FONT"
viscript audit surface-morph.lua --font "$VIS_FONT" --text-gap 4 --canvas-inset 4
viscript render surface-morph.lua --font "$VIS_FONT" --time 2 -o bowl.png
viscript render surface-morph.lua --font "$VIS_FONT" --time 7.6666667 -o aligned-axes.png
```

The 81 by 81 surface samples keep the height colors smooth while `mesh = {9, 9}` requests only the half-unit grid lines. At the morph midpoint, the height is `0.25*x*x - 0.5`: each fixed-x line is horizontal and has one height color. The marker moves from z = -0.5 to z = 0 with the same smooth curve. Orbit preserves the radius, polar angle, origin target, and vertical z-axis. Seek directly to any sample; the result depends only on that time.
