---
title: Diagram
id: diagram
order: 5
parent: overview
---

# Diagram

The optional Lua diagram module lays out semantic nodes, edges and zones, then builds ordinary objects for Scene animations. JavaScript currently has no diagram builder.

| Page | APIs |
| --- | --- |
| [Builder](./diagram-builder.md) | Configure layout, declare nodes, edges and zones, and build the diagram. |
| [Built groups](./diagram-groups.md) | Select the whole diagram or its zones, routes, nodes and annotations. |
| [Built parts](./diagram-parts.md) | Select the body, label and detail of individual nodes, edges and zones. |

Build once before selecting parts. Returned objects support the same [Animation](../Animation/index.md) calls as authored geometry.
