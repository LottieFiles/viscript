# Architecture

viscript stores scenes independently of ThorVG paints. At a requested time, the core
samples Objects, composes their transforms, projects geometry, and builds a display
list. A renderer translates that list into a retained ThorVG paint tree.

```text
C++ / Lua / JavaScript
        |
        v
Scene tree + Object trees + timelines
        |
        v
sample -> compose -> project -> clip -> sort
        |
        v
DisplayList -> retained ThorVG paints
        |
        +-> CPU RGBA
        +-> OpenGL target
```

## Scene model

A Scene owns root Objects. Every Object can own children; `Group` is a paintless
container and `Space` adds coordinate ranges, axes, grid, and labels. Each Object
has an affine local-to-parent matrix:

```text
world = root.model * ... * parent.model * object.model * localPoint
```

The ownership tree controls destruction. A flat Scene registry supplies stable
identity and timeline lookup. Attachment captures geometry, style, and model as the
base state. Later animation adds retained clips. Sampling combines those snapshots
with the clips and visibility intervals at the requested time.

The world is right-handed. Cameras project into pixels with +X right and +Y down.
Orthographic cameras use a vertical world height; perspective cameras use a
vertical field of view. Near/far clipping precedes projection.

See [Scene composition](scene-composition.md) for authoring and animation examples.

## Sampling pipeline

A frame proceeds through these steps:

1. Sample Object, camera, geometry, and field clips.
2. Apply registered runtime modifiers in registration order.
3. Compose ancestors, family bounds, and derived Connector endpoints.
4. Project and clip geometry into display commands.
5. Split surface-occluded strokes in 3D, then sort by layer, depth, and attachment.
6. Lower shown Views through their own cameras.
7. Compose Viewports and transition stages as clipped groups.
8. Update the retained paint tree, draw, and synchronize output.

Sampling reads the requested time directly. Repeated or out-of-order requests
reconstruct the same frame for the same authored scene and retained interaction
state. The authoring cursor and host interaction clocks are separate from rendering.

## Authoring modules

Diagram and TeX compile source descriptions into ordinary Object trees.

Diagram retains a versioned IR with semantic IDs, authored settings, computed
placements, routes, and Object bindings. The generated root owns this receipt, so
layout tools can inspect it after the builder and Lua VM are released. Static
validation reads the receipt; sampled validation joins it to renderer paths.

TeX parses and typesets math in process using the Scene's fonts. It produces a
`typeset` Group containing Text glyphs, Rectangles, and Paths. The module also adds
formula parts to `spans` and supplies TeX-key pairing for matching transforms.

Core sampling treats both results like hand-authored Objects. See [TeX math](tex.md)
and [Layout audit](layout-audit.md).

## Sampled mathematics

Riemann sums, vector fields, stream lines, and contours are constructed while the
scene is authored. Function callbacks produce retained rectangles, vectors, or plot
pieces. Trackers bake changing values and geometry into frame-cadence clips.

Cell and Voxel fields retain color snapshots on a coordinate/time lattice. Rendering
interpolates those snapshots. SurfaceMesh stores a rectangular 3D lattice and lowers
its patches into shaded faces, with optional mesh lines and per-sample colors.

These features share the same bounds, animation, view, and export paths. See
[Cells and voxels](cell-voxel.md).

## Interaction modules

`RuntimeModifier` is the composition seam between authored sampling and interaction.
UI, Input, States, and Lua Runtime register their state here; renderers consume the
resulting visual state.

- UI selects authored control faces and handles pointer capture.
- Input stores logical key/pointer state, action maps, Object bindings, and camera
  deltas.
- States retains named relative states and advances on a host interaction clock.
- Lua Runtime retains an explicitly registered fixed-step callback. Successful
  updates commit Object overlays and sound events for the host to consume.

Hosts dispatch pointer events to UI, then forward unhandled events to Input. They
advance simulation clocks and provide audio transport separately. A normal Lua load
releases its VM after construction; `vis.runtime` retains it with the Scene.

See [States](states.md), [Audio](audio.md), and [Modules](modules.md).

## Retained rendering

Each renderer keeps one ThorVG tree. Display commands map to reusable paints and
composition groups. A frame updates changed path geometry, transforms, text,
colors, gradients, strokes, and effects. Viewports, isolated Objects, masks, blends,
and transition stages keep their own composition groups.

The display list remains the common source for drawing, hit queries, and layout
reports. CPU and OpenGL renderers use the same lowering and paint-update logic.
Renderer selection belongs to the host; CPU is the default.

Assets retain encoded image bytes for ThorVG Picture rendering. SVG remains vector;
PNG, JPEG, and WebP remain raster. Cell texture sampling decodes straight-alpha RGBA
when needed. Fonts are registered with the renderer, and measurement uses the same
font metrics as rendering.

## Output and inspection

PNG writes a sampled frame. GIF and video savers drive a fixed frame cadence; video
streams RGBA to FFmpeg. Audio-enabled CLI export supplies a mixed soundtrack.
WASM exposes synchronized RGBA for browser presentation and encoding.

Layout reports flatten composition into root pixels and connect authored Objects
to sampled physical visuals and paths. The audit command aggregates those reports
across the export timeline. See [Layout audit](layout-audit.md).

## Source layout

| Directory | Responsibility |
|---|---|
| `src/core/scene/` | Object model, timelines, sampling, and composition |
| `src/core/renderer/` | Display lists, layout reports, and ThorVG paints |
| `src/core/loaders/` | Assets and image decoding |
| `src/core/savers/` | Snapshot and animation output |
| `src/core/lua/` | Scene authoring binding |
| `src/core/{diagram,tex,states,ui,input,lua_runtime,audio}/` | Optional modules |
| `src/inc/` | Public headers |
| `src/bindings/` | WASM ABI and JavaScript builder/client |
| `src/cli/` | Native commands |
| `src/test/` | Core, module, rendering, and host tests |

Each source directory owns its `meson.build`; `src/core/meson.build` assembles the
selected libraries and `src/meson.build` configures hosts.
