---
title: Overview
id: overview
order: 0
---

# Viscript API

Every public Viscript Lua and JavaScript function, organized by category and a single purpose per page. Related calls share a reference table and complete examples.

Each function table gives the Lua and JavaScript spellings; a dash marks an unavailable language. Examples are stored directly in Markdown: `viscript` fences contain complete Lua scenes, and `viscript-js` fences contain their JavaScript equivalents. Open this folder as an Obsidian vault with the Viscript plugin, or browse it with Quartz.

On the website, use **Try** to choose an example and the Lua/JS switch to change languages. In Obsidian, each Lua/JS pair shares one player with language buttons and a script panel. The API vault enables **Show source** by default; the player's code button can hide it. Input examples respond to the canvas. Host examples use ordinary JavaScript fences because they run in the embedding application.

## Categories

| API | Functions |
| --- | --- |
| [Scene](./Scene/index.md) | Construction, object lifecycle, timeline, cameras, views, trackers and audio cues. |
| [Object](./Object/index.md) | Shape, text and data factories, placement, parenting, masks and measurements. |
| [Animation](./Animation/index.md) | Reveal, transform, movement, emphasis, text, camera and scene animations. |
| [Group & Space](./Group/index.md) | Child layout, code and formula selection, sampled cells, voxels and axis labels. |
| [Diagram](./Diagram/index.md) | Nodes, edges, zones, building and access to generated objects. |
| [Interaction](./Interaction/index.md) | Lua UI, input, fixed-step runtime and state overlays. |
| [JavaScript host](./Host/index.md) | Loading, rendering, assets, fonts, input, audio and engine lifecycle. |
| [Utilities](./Utilities/index.md) | Curves, springs, deterministic random numbers, table helpers and compilation. |

## Start with a feature

- [Shapes](./Object/shapes.md), [Lines & Paths](./Object/lines-paths.md), or [Text & Numbers](./Object/text-numbers.md) to create objects.
- [Reveal](./Animation/reveal.md), [Movement](./Animation/movement.md), or [Paint & Opacity](./Animation/paint-opacity.md) to animate them.
- [Buttons, Sliders & Sampling](./Interaction/ui-controls.md) to make them interactive.
- [Engines & Loading](./Host/host-loading.md) to embed a scene in an application.

## Notation

`options` means a Lua table or JavaScript object. `?` marks an optional argument; braces in a Lua call are its table argument. Lua uses `:` for handle methods and `.` for namespace functions; JavaScript uses `.` for both. Lua method names use `snake_case`, while JavaScript methods generally use `camelCase`. Many option keys keep their Lua spelling in JavaScript.

Positions and distances use world units unless stated otherwise; durations use seconds and angles use radians. Child objects use their parent's coordinate space. Selection indices start at **1 in both languages**.

Shared factories are listed with their object family. Container constructors and additional layout/sampling methods are under [Group & Space](./Group/index.md). Object, Group and Space handles inherit the shared factories. JavaScript host methods operate on the engine returned by `createVis`, separately from the scene builder.

## Availability

The reference includes optional modules: TeX, Diagram, UI, Input, Audio, States and the retained Lua runtime. Their entries require the corresponding module in the runtime being used. Lua retained callbacks run during interaction; JavaScript builder callbacks run during scene construction.

This catalog covers Viscript's APIs and its customized Lua helpers. Ordinary Lua and JavaScript standard libraries are outside its scope. For guided examples and option usage, see [Docs](https://lottiefiles.github.io/viscript/#/docs).
