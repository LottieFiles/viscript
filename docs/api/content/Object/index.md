---
title: Object
id: object
order: 2
parent: overview
---

# Object

Choose an object family to see its Lua/JavaScript functions and examples together.

Every factory is available on Scene, Object, Group, and Space. The receiver becomes the new object's parent; a Scene adds it at the root. Each call returns an object handle, except the Group builders and `brace` noted on their pages. Lua always takes an options table, including an empty table. `opts?` marks an optional JavaScript argument.

## Geometry

- [Shapes](./shapes.md) — points, circles, rectangles, and polygons.
- [Arcs, Angles & Braces](./arcs-angles-braces.md) — circular sweeps, angle marks, and span labels.
- [Lines & Paths](./lines-paths.md) — segments, arrows, vectors, curves, and connectors.
- [Boolean Regions](./boolean-regions.md) — union, intersection, difference, and exclusion.
- [Plots & Areas](./plots.md) — function plots, filled areas, and Riemann sums.
- [Vector Fields & Contours](./vector-fields.md) — vector arrows, streamlines, and scalar level curves.
- [3D Surfaces](./surfaces.md) — sampled surface grids and wire meshes.

## Text & data

- [Text & Numbers](./text-numbers.md) — plain text, numeric values, labels, and measurements.
- [Rich Text & TeX](./rich-text-tex.md) — styled text, formulas, and formula parts.
- [Code Blocks & Search](./code-blocks.md) — syntax coloring and token selection.
- [Matrices & Tables](./matrices-tables.md) — rectangular data and selectable rows.
- [Pictures & Cell Grids](./pictures-cells.md) — image sources and dense colored grids.

## Shared behavior

- [Placement & Parenting](./object-placement.md) — relative placement and child handles.
- [Styling & Masks](./object-styling.md) — shared paint options and object masks.
- [Group & Space](../Group/index.md) — containers, layout, coordinate axes, and sampling.
