---
title: Canvas & Pixel Rendering
id: host-rendering
order: 3
parent: host
---

# Canvas & Pixel Rendering

Render a sampled frame as RGBA pixels or draw it to a canvas. Logical output size and raster pixel ratio are separate controls.

## Functions

| Lua | JavaScript | Purpose |
| --- | --- | --- |
| — | `host.render(time)` | Render seconds into the Scene and return a copied `Uint8ClampedArray` of RGBA pixels. |
| — | `host.draw(canvas, time, pixelRatio?)` | Draw a frame to a 2D canvas; pixel ratio is a positive integer, default 1. |
| — | `host.size()` | Return logical output `[width, height]` in pixels. |
| — | `host.resize(width, height)` | Resize the loaded Scene's raster output. |

Host snippets use the variables from [Shared Host Setup](./setup.md).

## Examples

### host.render

<!-- api-example: host.render -->

```javascript
const host = await createVis({module});
host.load(source);
const pixels = host.render(0.5);
const [width, height] = host.size();
canvas.width = width; canvas.height = height;
canvas.getContext("2d").putImageData(new ImageData(pixels, width, height), 0, 0);
write({bytes: pixels.length, width, height});
```

### host.draw

<!-- api-example: host.draw -->

```javascript
const host = await createVis({module});
host.load(source);
host.draw(canvas, 0.5, 2);
write({canvasWidth: canvas.width, canvasHeight: canvas.height});
```

### host.size

<!-- api-example: host.size -->

```javascript
const host = await createVis({module});
host.load(source);
write(host.size());
host.draw(canvas, 0.5);
```

### host.resize

<!-- api-example: host.resize -->

```javascript
const host = await createVis({module});
host.load(source);
host.resize(800, 450);
write(host.size());
host.draw(canvas, 0.5);
```
