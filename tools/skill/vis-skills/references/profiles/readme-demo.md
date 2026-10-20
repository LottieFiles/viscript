# README demo format

Use this profile for short looping clips that sit above a paragraph in a README, landing page, or docs section and show what one feature does: a command and its output, an input and the picture it becomes, a host embedding a Scene. The clip comes first and the paragraph explains it, so the clip carries the claim without a title or caption. This is a format profile: combine it with the subject profile that owns the truth (an algorithm trace takes [algorithm-and-code](algorithm-and-code.md), a curve takes [math-shortform](math-shortform.md)).

Start from [command-to-result.lua](../../templates/readme-demo/command-to-result.lua). The viscript README clips under `docs/assets/` (hero, steps, ai, embed, render, browser, skills, build) are full examples of this style.

## One claim per clip

Write one sentence the clip proves, such as "one script renders to PNG, GIF and MP4" or "optional modules plug into the core at configure time". Then fill a cause → effect table:

| beat | cause (what the reader does) | effect (what appears) | camera |
|---|---|---|---|

Not every clip is a command. When the feature is the subject itself (a curve, a sort, a construction, a model), let the subject profile's motion be the clip: a tangent tracing a derivative, merge passes, de Casteljau lerps, attention weights over a sentence. Then no terminal or code panel appears at all.

Most clips are two to four beats: a close-up on the cause, a widen to show the effect, an optional variation (retype the command, a second input), and a hold. A hero clip may chain up to four acts that slide in from the side one after another, one act per feature.

## Look

- **Theme.** `pro_white` on a white canvas. Set it explicitly: the README background is white on GitHub's light theme, and the clip must not look like a dark box there.
- **Ink.** Structure is `foreground` ink: square corners, thick borders (2.5–3 px), no shadows or rounded cards. A dark `foreground`-filled panel stands for a terminal; a white panel with an ink border stands for an output, window, or canvas.
- **One hue.** `accent` (or a light tint such as `#e8f0fb`, `#0a66c233`) marks the subject, and `success` marks completion (a finished build, a written file). Everything else is ink, `muted`, or `border`.
- **No gradient or TeX titles.** Plain text only. Gradients read as decoration, and TeX distorts when the export size differs from the authored size.
- **Labels.** Panel labels are mono caps (`role = "code"`, size 11–12) above the panel's top-left corner: `TERMINAL`, `SCENE.GIF`, `MODULES`. Code and commands use `role = "code"`; prose labels are rare and `muted`.
- **Placeholders.** Something that does not exist yet is a dashed `border` slot (`dash = {5, 4}`); it becomes a solid tile when it arrives. Dash itself cannot animate, so cross-fade a solid tile over the slot.

## Motion

- **Camera.** The configured camera is the opening close-up on the cause. `scene:focus(handles, {padding = 0.06, duration = 0.7, curve = "ease_in_out"})` widens to show the effect, and `scene:focus(nil, ...)` returns to the opening framing before the loop ends. Padding is a fraction. Keep it to one or two moves per clip.
- **Plane text.** The camera moves, so set every label in world units: `orientation = "plane"` with `size = points * PT`, where `PT = (4/3) * H / height`, `H` is the camera height of the wide shot, and `height` is the canvas height in pixels. Billboard text stays the same pixel size while everything else zooms, and it does not rescale with `--width`.
- **When the camera moves.** Use the close-up → widen move when cause and effect are side by side. When the subject fills one framed panel with a title and status line (a sort, a construction, attention), keep the camera still: zooming on a part cuts the title off.
- **Off-frame content.** While the camera is on the cause, hide the effect side (opacity 0) and fade it in during the second half of the widen. Start the fade no earlier than about 60% of the camera move (0.45 s into 0.7 s); earlier, the labels show while still cut by the frame edge, which looks broken and fails the audit.
- **Verbs.** `write` and `retype` for commands, `create` for wires and arrows (`progress` 0 → 1), `grow_from_center` with a `back` curve for an arriving subject, an `arc` shift for a hop, `scale` for a short squash, and a progress bar's `size` with a matching `shift` for work in progress. Stagger a wave of similar items by about 0.1 s.
- **Crossing items.** When items swap places, give right-movers an `arc` hop and slide left-movers flat, so crossing items never sit at the same height. Keep the hop small enough to stay inside the panel.
- **Labels that belong to slots.** Index or value labels under moving items stay at their slots and change text after each pass (fade out, `set` text, fade in). Labels that travel with their items collide mid-move.
- **Loop.** Set `loop = true` and end on the exact frame at `t = 0`: fade out what appeared, reset moved objects with `set` and their starting `position`, and return the camera. Hold the result for about 1 s before the reset; the whole clip is usually 5–12 s.

## Layout

- Canvas 1200 wide; height 336–480 for a strip under a paragraph, 600 for a hero. Fix the width and derive the height from the content.
- Cause on the left and effect on the right, joined by a straight ink arrow or wire. Keep equal margins to the outermost ink, labels included.
- Code shown in a clip is real: every call exists in the API, or it is plain Lua the reader could write. Never invent a scene method to make a line shorter; split it over lines instead.
- Hand-set data (attention weights, timings) must look plausible and sum correctly. Say in the paragraph when values are illustrative.
- Text in a panel is short enough to fit inside it at the close-up. Shorten the command rather than shrink the type; check it with `--contain <text> <panel> 8`.

## Export and embed

```sh
viscript render demo.lua -o demo.gif --width 960 --height 336 --fps 15 --font Pretendard.ttf --font JetBrainsMono.ttf
```

- Pass both `--width` and `--height`, scaled together (1200x420 → 960x336); `--width` alone keeps the authored height and stretches the frame.
- 960 px wide at 15 fps (20 for a hero) keeps a clip around 0.5–1.5 MB. Author at 1200 and export at 960; plane text and shapes scale, TeX does not.
- Keep the `.lua` beside the `.gif` under `docs/assets/` so the clip can be regenerated.
- Embed centered, before the paragraph it illustrates:

```html
<p align="center"><img src="docs/assets/demo.gif" alt="one sentence the clip proves" width="800"></p>
```

- A screen recording (an editor, a browser) stays a recording: convert it with ffmpeg `palettegen`/`paletteuse` at 960 px and 12 fps, and link the GIF to the original video.

## Review

In addition to the core review, check:

- `audit` with `--contain` for every command or label inside a panel;
- `t = 0` and the exact final time render the same frame (the loop seam). Scenes driven by a tracker can differ by float noise; compare a difference image, not bytes;
- the close-up and the wide shot each show no text clipped by the frame;
- the exported GIF at its final width: labels still legible at 800 px display width.
