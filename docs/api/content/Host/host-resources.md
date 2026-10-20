---
title: Fonts, Images & Theme
id: host-resources
order: 2
parent: host
---

# Fonts, Images & Theme

Register resources and an adaptive host theme before loading the scene that uses them. Engines in the same WebAssembly instance share registered fonts.

## Functions

| Lua | JavaScript | Purpose |
| --- | --- | --- |
| — | `host.font(name, bytes, mime?)` | Register a TTF/OTF font before loading text; default MIME is `ttf`; fonts are shared within the WebAssembly instance. |
| — | `host.missingFonts()` | Return the font names needed by loaded text but not yet registered. |
| — | `host.asset(name, bytes, mime)` | Register a PNG/SVG image before loading content that uses it. |
| — | `host.hostTheme(palette)` | Set colors and font for `adaptive` Scenes; requires `dark`, with optional semantic colors, `objects` and `font`. |

Resources accept `Uint8Array` or `ArrayBuffer`. `asset` accepts `png`, `svg`, `image/png` and `image/svg+xml`. A font name's first registration wins; register fonts before loading so text measurement uses them.

Host snippets use the variables from [Shared Host Setup](./setup.md).

## Examples

### host.font

<!-- api-example: host.font -->

```javascript
const host = await createVis({module});
host.font("Example Sans", fontBytes, "ttf");
host.load(`local scene = vis.scene {width = 640, height = 360}
scene:text {id = "registered-font", text = "Registered font", font = "Example Sans", size = 30}
return scene`);
write({missingFonts: host.missingFonts()});
host.draw(canvas, 0.5);
```

### host.missingFonts

<!-- api-example: host.missingFonts -->

```javascript
const host = await createVis({module});
host.load(`local scene = vis.scene {width = 640, height = 360}
scene:text {text = "Font lookup", font = "ExampleFont", size = 24}
return scene`);
write(host.missingFonts());
host.draw(canvas, 0.5);
```

### host.asset

<!-- api-example: host.asset -->

```javascript
const host = await createVis({module});
host.asset("earth.png", imageBytes, "png");
host.load(`local scene = vis.scene {width = 640, height = 360}
scene:picture {asset = "earth.png", width = 3}
return scene`);
write("earth.png registered before loading.");
host.draw(canvas, 0.5);
```

### host.hostTheme

<!-- api-example: host.hostTheme -->

```javascript
const host = await createVis({module});
host.hostTheme({dark: true, accent: "#e3b341"});
host.load(`local scene = vis.scene {width = 640, height = 360, theme = "adaptive"}
scene:circle {radius = 1, fill = "accent"}
return scene`);
write(host.adaptiveTheme);
host.draw(canvas, 0.5);
```
