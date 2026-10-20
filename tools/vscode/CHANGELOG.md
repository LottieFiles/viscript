# Changelog

## 0.7.80

- Play scenes with the shared viscript player, as the documentation site and the Obsidian plugin do.
- Click the canvas to play or pause; **Camera → CONTROL ON/OFF** chooses whether drags, the wheel, and keys move an interactive camera.
- Keep the time and whether the scene plays when the same file is edited.
- Hold a paused scene's retained runtime until playback resumes.
- Export PNG frames at the display's pixel ratio.
- Fit the canvas to the player always; the FIT/RAW sizes, wheel zoom, middle-drag pan, and the frame-rate readout are gone.
- Follow the editor theme in scenes that name no theme, which are now `adaptive`.
- Draw scenes that name no size at 960×540, the size the documentation and skills write them at.
- Show 6 world units vertically through a camera that names no height, as the documentation's scenes do.

## 0.7.79

- Preview Lua scenes with live reload, seeking, camera controls, and adaptive editor themes.
- Browse named visualization series and inspect linked source or verified Git diffs.
- Export PNG frames and GIF/MP4 playback ranges.
- Author TeX formulas, mathematical fields, and semantic diagrams with the bundled runtime.
- Build interactive scenes with UI, Input, retained Lua Runtime, States, and Audio modules.
- Use generated Lua completion, hover documentation, and LuaLS definitions.
