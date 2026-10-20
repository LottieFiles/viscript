# TeX math

`vis-tex` typesets math in process using the Scene's fonts. A formula becomes an
ordinary `typeset` Group of Text glyphs and filled shapes, ready for layout,
animation, rendering, and audit.

## Build

```sh
meson setup build-tex -Dmodules=tex
meson compile -C build-tex
```

The module installs `vis_tex.h`, `libvis-tex`, and `vis-tex.pc`, and adds formula
factories to assembled Lua hosts. The bundled WASM runtime includes it.

Register the formula's fonts before loading the scene:

```sh
build-tex/viscript render formula.lua -o formula.png \
  --font src/examples/assets/Pretendard.ttf
```

A math font such as STIX Two Math supplies additional symbols and math alphabets.
Font metrics determine glyph spacing, rules, scripts, and delimiters.

## Formulas and parts

```lua
local scene = vis.scene {}
local identity = scene:tex {
    tex = "e^{i\\pi} + 1 = 0",
    point = {0, 1.2}, role = "h2", id = "identity",
}
local sum = scene:tex {
    tex = {"a^2", "+", {"b^2", color = "danger"}, "=", {"c^2", id = "hyp"}},
    point = {0, -0.6}, role = "h2", id = "sum",
}
scene:write(identity, 1)
scene:play({target = sum:part("c^2"), fill = "accent"}, 0.5)
scene:wait(0.3)
return scene
```

`scene:tex` creates a root; `object:tex` creates a child. `tex` accepts a string or a
list of styled parts. A part uses its own `id`, or `<formula-id>/<part-number>`.
Glyphs and shapes receive IDs below their formula or part.

| Option | Purpose |
|---|---|
| `point`, `align` | Pin the formula's box; default alignment is centered |
| `baseline` | Pin the baseline at `point` |
| `size`, `font`, `role`, `orientation` | Text styling and placement |
| `display` | Display math by default; `false` selects inline math |
| `color`, `fill` | Color the glyphs and shapes |
| `id`, `opacity`, `progress`, `layer` | Group identity and visual state |
| `isolate`, `mask`, `blend`, `blur`, `shadow` | Group composition |

`part(tex, occurrence)` finds a part or glyph by its TeX key, ignoring whitespace.
Use parts to keep terms together and assign their colors explicitly.

Pin adjacent text and formulas to the same baseline:

```lua
scene:text {text = "Area of a disc:", point = {-0.1, 0}, align = {1, 0}, baseline = true}
scene:tex {tex = "A = \\pi r^2", point = {0.1, 0}, align = {0, 0}, baseline = true}
```

## Matching formulas

Matching pairs equal parts first, then equal glyph and shape keys in sequence, then
remaining equal keys. Matching pieces travel; other pieces cross-fade.

```lua
local target = scene:tex {
    tex = {{"c^2", color = "accent"}, "=", "a^2", "+", {"b^2", color = "danger"}},
    point = {0, -0.6}, role = "h2", id = "target",
}
scene:match_transform(sum, target, 1.2, "smooth", {arc = math.pi / 2})
```

Explicit `pairs` select correspondences. `strings=false` uses those pairs, and
`copy=true` keeps the source formula while copies of the paired pieces move:

```lua
local target = scene:tex {tex = {"c^2"}, point = {2, 0}, id = "selected"}
scene:match_transform(sum, target, 1, "smooth", {
    copy = true, strings = false,
    pairs = {{sum:part("c^2"), target:part("c^2")}},
})
```

## Inline math

`spans` places `{tex = ...}` parts beside text on shared baselines. Word wrapping
keeps formulas and attached punctuation together.

```lua
local proof = scene:spans {
    "For a right triangle, ",
    {tex = {"a^2", "+", "b^2", "=", "c^2"}, id = "pyth"},
    ", so its hypotenuse is ",
    {tex = "c = \\sqrt{a^2 + b^2}", color = "accent"}, ".",
    split = "words", wrap = 8, leading = 1.5, size = 32, id = "proof",
}
scene:write(proof:children(), 1.5, "smooth", 0.05)
```

Inline parts use text style by default. Their font, size, role, fill, and layer come
from the row unless set explicitly. `rise` shifts a part's baseline by a fraction of
the row's line height. The row measures formula height and depth when spacing lines.

## JavaScript

The builder exposes `tex` on Scene and Object handles and returns a `TexHandle`:

```js
const sum = scene.tex({
    tex: ["a^2", "+", {text: "b^2", color: "danger"}, "=", "c^2"],
    id: "sum", role: "h2",
});
scene.play({target: sum.part("c^2"), fill: "accent"}, 0.5);

const proof = scene.spans([
    "For a right triangle, ", {tex: "a^2 + b^2 = c^2", id: "pyth"}, ".",
], {wrap: 8, size: 32, baseline: true});
```

Array parts appear in `children()`. `part()` selects pieces by TeX key when the
native runtime typesets the formula.

## Math notation

The parser handles common mathematical notation:

| Notation | Examples |
|---|---|
| Scripts, fractions, roots | `x_i^2`, `\frac{a}{b}`, `\sqrt[n]{x}` |
| Symbols and relations | `\alpha`, `\infty`, `\times`, `\le`, `\in`, `\to` |
| Operators | `\sum`, `\int`, `\sin`, `\operatorname{rank}` |
| Delimiters and accents | `\left( ... \right)`, `\hat{x}`, `\overline{AB}` |
| Alphabets and text | `\mathbf{x}`, `\mathbb{R}`, `\text{area}` |
| Arrays | `matrix`, `pmatrix`, `cases`, `aligned` with `&` and `\\` |
| Spacing and style | `\,`, `\quad`, `\displaystyle`, `\textstyle` |

Bounds and emphasis effects use each glyph's ink box. `viscript audit` checks the
formula's characters against the registered font. See [Layout audit](layout-audit.md).

## C++

```cpp
#include "vis_tex.h"

const char* parts[] = {"a^2", "+", "b^2"};
tex::Options options;
options.role = TextRole::H2;
char error[256];
auto formula = tex::Formula::gen(scene, parts, 3u, options, error, sizeof(error));
if (formula) {
    auto result = scene->add(formula->take());
    delete formula;
}
```

`Formula` exposes its root, part groups, pieces, and TeX keys. `tex::pair` produces
piece pairs for `MatchOptions::pairs`; `take` transfers the generated root.
