# Graphics evidence

Use this profile for one evidence-bearing 2D, screen-space, 3D, or 3D-mathematics mechanism. The visible substrate must discriminate the correct mechanism from a plausible wrong one. A vector is geometry, a framebuffer is a pixel field, a transform changes a coordinate space, a ray terminates at a verified hit, and a lighting response derives from the displayed operands.

## Choose the domain boundary

Use mathematical geometry for vectors, points, curves, transforms, and coordinate spaces. Use `Cell` for a dense rectangular field, `Picture` for raster or SVG content, and an outlined cell or Point for one referenced sample. A renderer Surface or framebuffer is represented by `Cell` or `Picture`; viscript's geometric Surface is a sampled 3D surface, not pixel storage.

Keep world-space and screen-space evidence distinct. World geometry, meshes, cameras, rays, lighting, shadows, and kinematics belong to the 3D part of the model. Coverage, stored pixels, G-buffers, image sampling, filtering, compositing, post-process passes, convolution, and blur are screen-space evidence. A hybrid scene may show their relationship, but each resource and coordinate conversion must remain explicit.

State coordinate handedness, vector convention, matrix order, depth range, units, camera role, and any shader or algorithm variant that affects the result. Distinguish the presentation camera from a camera that is part of the concept.

## Derive geometry and buffers

Compute related geometry once from independent inputs. Reuse the same derived endpoints, intersections, projections, normals, samples, spans, matrix results, joint transforms, and output pixels for geometry and labels. For an affine change, transform the owning Space or Group with the real matrix; do not slide disconnected grid lines or precompute unrelated snapshots.

Keep points, vectors, directed segments, pixel cells, areas, vertices, primitives, fragments, samples, and stored pixels visually distinct. A vector carries a value from an origin; a Point is a location; an Arrow is a general directed segment. Pixel traversal separates global order, current sample, operation, write, and accumulated output. Pixel zoom must reference the same image buffer and coordinate in both views.

For filtering and post-process evidence, preserve input, intermediate or history resources, representative footprint, border policy, filtering color space, alpha convention, and output. Use ping-pong resources when a pass cannot read and write the same resource. Derive convolution output from the displayed kernel and state when a two-pass blur is equivalent only because the kernel is separable.

## 3D truth and camera discipline

Use recognizable bounded geometry when depth, orientation, topology, occlusion, or spatial composition is claimed. Preserve stable identities for vertices, primitives, rays, hits, joints, resources, and representative pixels across spaces and stages. Derive transformed points, normals, intersections, child joints, visibility, and shading terms from the same matrices and vectors shown on screen.

Resolve actual pipeline semantics: vertex work is per vertex, geometry work is per primitive, tessellation operates on patches according to the named API, and compute dispatch is not a late graphics stage. Distinguish clipping, coverage, fragment tests, blending, multisampling, and stored output whenever the reviewed claim depends on them.

Lock the presentation camera while the mechanism operates unless camera motion is evidence. Scout a pose that exposes depth and fits the full motion envelope. Use a camera-only move afterward only to reveal a named relation; never orbit continuously to substitute for object motion or computation.

viscript projects educational 3D geometry into a 2D backend. Do not imply a programmable GPU pipeline, general indexed-mesh renderer, depth buffer, material graph, or shadow system that the public API does not provide. Treat illustrative viscript shading as a visualization of verified values, not as execution of the reviewed shader. `SurfaceMesh` is a row-major lattice, a `Picture` under perspective is an affine approximation, and spatial `Cell` voxels use illustrative built-in lighting; disclose these boundaries when they affect the claim.

## Acceptance

- The chosen object or buffer family directly represents the claimed operand or resource.
- Every dependent geometry, sample, buffer value, and displayed result follows canonical source data after an asymmetric perturbation.
- Coordinate spaces, camera roles, pipeline stages, and storage identities remain distinguishable.
- A shadow includes verified visibility evidence; a material does not silently change topology.
- The final frame retains the local input, mechanism, result, and invariant without relying on camera motion or playback.
