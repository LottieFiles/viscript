# Scene themes

A Theme supplies a Scene's background, text styles, object-color cycle, semantic
colors, stroke width, corner roundness, gradients, and axis colors. Child Scenes keep
their own Theme and palette position.

## Presets

| Lua / JavaScript token | C++ preset | Appearance |
|---|---|---|
| `pro_white` | `ProWhite` | Light background; default preset |
| `pro_black` | `ProBlack` | Dark background |
| `adaptive_vscode` | `AdaptiveVscode` | Palette supplied by the editor host |

The presets use Pretendard for `h1`, `h2`, `h3`, and `text`, and JetBrainsMono for
`code`. Register the font files before loading scenes that measure text:

```sh
build/viscript render scene.lua -o frame.png   --font src/examples/assets/Pretendard.ttf   --font src/examples/assets/JetBrainsMono.ttf
```

The preview host resolves `adaptive_vscode` from the active editor colors and
rebuilds adaptive scenes when the editor Theme changes. Direct C++ use defaults to
Pro White until a host supplies a palette.

## Semantic colors

Style fields accept hexadecimal colors or Theme roles:

| Role | Use |
|---|---|
| `background`, `foreground` | Canvas and primary content |
| `muted` | Secondary labels and structure |
| `accent`, `secondary` | Emphasis |
| `success`, `warning`, `danger`, `info` | Status |
| `surface`, `border` | Panels and boundaries |
| `result` | Derived value or output |
| `focus` | Current subject |

Uncolored shapes take successive colors from `objects`. Explicit colors and
semantic roles keep their assigned color. `indicate` uses `focus` and restores the
incoming color afterward.

A role's font, like a text's, can be a comma list such as
`"Source Serif 4, IBM Plex Sans KR"`: each character is set in the first loaded font
of the list that has it, then in the runtime's math or default font, and the first
loaded font sets the line height.

Text resolves font, size, and color independently from its role. A Group's stroke,
fill, and width supply defaults to descendant shapes; a nearer Group takes
precedence. In a `typeset` Group, its fill also colors text that has no explicit
color. Space resolves its omitted axes, grid, and label colors from the Theme.

## Custom Theme

Set the Theme while creating the Scene. Partial settings extend `pro_white`, or the
chosen `preset`; `objects` replaces the color cycle.

```lua
local scene = vis.scene {
    theme = {
        preset = "pro_white",
        background = "#f8fafc",
        objects = {"#2563eb", "#b45309", "#7c3aed"},
        colors = {
            result = "#a16207",
            focus = "#2563eb",
            surface = "#f1f5f9",
            border = "#cbd5e1",
        },
        text = {
            h1 = {font = "Pretendard", size = 38, color = "#0f172a"},
            text = {font = "Pretendard", size = 20, color = "#334155"},
            code = {font = "JetBrainsMono", size = 18, color = "#9b3600"},
        },
        object_width = 1.5,
        roundness = 0.1,
        axis = {x = "#cbd5e1", y = "#cbd5e1", grid = "#e2e8f0"},
    },
}

local subject = scene:circle {radius = 1}
scene:text {text = "42", point = {0, -1.5}, fill = "result"}
scene:indicate(subject)
```

`roundness`, from 0 to 0.5, is the share of a box's shorter side by which
Theme-styled structural boxes round their corners. A Diagram without an explicit
`corner` applies it to its node size, rounds zones 1.5 times as much, and draws
terminal nodes as pills only when corners round. It is 0, square, in every preset;
set it in a custom Theme to round them.

JavaScript uses the same fields through `SceneConfig.theme`. C++ copies a `Theme`
into the Scene:

```cpp
auto theme = vis::Theme::preset(vis::ThemePreset::ProWhite);
theme.objects[0] = vis::Color::hex("#2563eb");
theme.objects[1] = vis::Color::hex("#b45309");
theme.objectCount = 2;
theme.colors.result = vis::Color::hex("#a16207");
auto result = scene->theme(theme);
```

## Gradients

`gradient = true` uses the resolved paint color and the Theme's `end_gradient_stop`.
A color selects that end stop directly. A table supplies a linear, radial, or conic
gradient:

```lua
scene:rectangle {size = {3, 2}, fill = "#fff", gradient = {
    type = "linear", stops = {"accent", "warning"},
    from = {-1.5, 0}, to = {1.5, 0},
}}
scene:circle {radius = 1, fill = "#fff", gradient = {
    type = "radial", stops = {{0, "#ffd166"}, {0.6, "accent"}, {1, "surface"}},
    center = {0, 0}, radius = 1, focal = {-0.3, 0.3},
}}
scene:circle {radius = 1, fill = "#fff", gradient = {
    type = "conic", stops = {"accent", "success", "warning", "accent"}, angle = -90,
}}
```

Bare stops are evenly spaced. `{offset, color}` entries set positions explicitly.
Gradient geometry uses local Object coordinates and follows the model and camera.
Omitted geometry derives from the projected shape. Stop alpha multiplies the
shape's paint alpha.

C++ supplies a `Gradient` to `Object::gradient`. Animation interpolates compatible
gradient stops and geometry; a change of kind or layout switches at the midpoint.
