---
title: Bounds & Layout Reports
id: host-inspection
order: 4
parent: host
---

# Bounds & Layout Reports

Inspect the actual sampled frame using authored object IDs. Bounds and padding use root output pixels; these queries complement the rendered canvas.

## Functions

| Lua | JavaScript | Purpose |
| --- | --- | --- |
| — | `host.bounds(id, time?)` | Return an Object's sampled `{x, y, width, height}` in root output pixels; time defaults to 0. |
| — | `host.intersects(firstId, secondId, time?, padding?)` | Check sampled Object bounds for overlap or insufficient padding; defaults are 0. |
| — | `host.layoutReport(time?, padding?)` | Return sampled Scenes, Objects, visuals, paths, collisions and containments; defaults are 0. |

Host snippets use the variables from [Shared Host Setup](./setup.md).

## Examples

### host.bounds

<!-- api-example: host.bounds -->

```javascript
const host = await createVis({module});
host.load(source);
write(host.bounds("dot", 0.5));
host.draw(canvas, 0.5);
```

### host.intersects

<!-- api-example: host.intersects -->

```javascript
const host = await createVis({module});
host.load(source);
write({intersects: host.intersects("dot", "box", 0.5)});
host.draw(canvas, 0.5);
```

### host.layoutReport

<!-- api-example: host.layoutReport -->

```javascript
const host = await createVis({module});
host.load(source);
const report = host.layoutReport(0.5, 4);
write({objects: report.objects.length, collisions: report.collisions});
host.draw(canvas, 0.5);
```
