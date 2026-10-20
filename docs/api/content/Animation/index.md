---
title: Animation
id: animation
order: 3
parent: overview
---

# Animation

Schedule one change or combine related changes in a `play` descriptor. Each page groups one kind of animation with its functions, editable example scene and matching properties.

| Page | Functions and properties |
| --- | --- |
| [State & Scheduling](./state-scheduling.md) | `play`, `set`, `save` / `restore`, draw order. |
| [Reveal](./reveal.md) | `create` / `uncreate`, fill reveal, borders, writing, outline progress. |
| [Growth](./growth.md) | Grow from a center or edge; shrink to the center. |
| [Movement](./movement.md) | Shift, transform, rotate, scale, arrange and movement descriptors. |
| [Path & Field Motion](./path-motion.md) | Follow a path, integrate a vector field and warp geometry. |
| [Paint & Opacity](./paint-opacity.md) | Fade in / out, transitions, colors, stroke and effects. |
| [Emphasis](./emphasis.md) | Indicate, wiggle, temporary frames, flashes, rings and highlights. |
| [Morph & Code Edits](./morph-edits.md) | Morph, cross-fade, match pieces and animate code edits. |
| [Shape Properties](./shape-properties.md) | Radius, angles, sizes, corners, points, endpoints and curve controls. |
| [Plot & Field Properties](./plot-properties.md) | Functions, scalar samples, contour levels and sampling ranges. |
| [Text & Values](./text-properties.md) | Text replacement, retyping, numbers and vector values. |

## Shared timing

Methods schedule changes at the scene cursor. `target` is one object; `targets` accepts one object or a list. `?` marks an optional argument. Unless stated otherwise, duration defaults to 1 second, curve to `smooth`, and lag to 0.

For methods with positional `duration?, curve?`, a `timing` table/object may replace both. Methods with positional `lag?` also accept it inside that table/object. Do not mix table/object timing with later positional arguments. `lag` delays each subsequent item by `duration * lag` seconds.
