---
name: vis-obsidian
description: "Install the bundled viscript Obsidian plugin, embed scenes from separate vault files, and configure fonts, playback, and source visibility. Route animation authoring to vis-skills, interactive scenes to vis-interactive, and plugin source changes to vis-animation-dev."
---

# viscript Obsidian

Use this skill for the Obsidian host: install its bundled plugin, organize scene files, embed them in notes, and set fonts and player options. Write and review the animation with [vis-skills](../vis-skills/SKILL.md); use [vis-interactive](../vis-interactive/SKILL.md) for retained input and audio, and [vis-animation-dev](../vis-animation-dev/SKILL.md) for engine or plugin source changes.

## Install the included plugin

The skill includes the ready-to-use plugin in `assets/plugin/`: `main.js`, `manifest.json`, and `styles.css`. The runtime and default fonts are inside `main.js`; no source checkout or build is needed. Node.js runs the installer:

```sh
node /path/to/vis-obsidian/scripts/install.mjs /absolute/path/to/vault
```

Replace both paths with the actual skill directory and target vault. The installer copies the plugin to `<vault>/.obsidian/plugins/viscript/` and enables it, preserving existing settings, notes, workspace, and other plugins. Add `--show-source` to show scripts below note players by default. If Obsidian asks, allow community plugins; enable **Viscript** under **Settings → Community plugins**, or reload it after an update. Obsidian 1.13 or newer is required.

## Keep scenes in their own folder

By default, store complete Lua scenes in `Animations/<note folder path>/<note name>/*.lua` inside the vault. Mirror the note's vault-relative folders and use its filename without `.md` as the scene folder. For example, `Algorithms/Search.md` keeps its scenes together in `Animations/Algorithms/Search/`, such as `overview.lua` and `steps.lua`. A root note `Intro.md` uses `Animations/Intro/`. Notes carry the explanation and embed the relevant scene:

```markdown
The highlighted item narrows the search range.

![[Animations/Algorithms/Search/overview.lua]]
```

The embed follows edits to its source. Opening the `.lua` file shows its player and an editable source panel. **New viscript** in a folder's context menu creates a starting scene there. Leave the scene's theme unset or use `adaptive` to follow Obsidian's light and dark themes.

For a short one-off scene, an inline `viscript` or `viscript-lua` fence is Lua; `viscript-js` is JavaScript. Keep longer JavaScript scenes in the same document folder, for example `Animations/Algorithms/Search/overview.md` inside a `viscript-js` fence, then embed it with `![[Animations/Algorithms/Search/overview]]`. Standalone `.js` scene-file embeds are not supported.

Words in a Lua embed's alias or after a fence's language adjust that player:

```markdown
![[Animations/Algorithms/Search/overview.lua|600|loop|controls|Search overview]]
```

`600` sets width; `loop` replays immediately and `once` or `stop` holds the last frame. `paused` starts on the first frame; `autoplay` plays when visible. `controls` or `nocontrols` sets the play bar, and `left`, `center`, or `right` sets alignment. Other words supply an accessible label. Add `interactive` only to a scene authored to handle input.

Replay follows these embed options or **At the end** in the plugin settings (replay by default), rather than the Scene's `loop` flag. `interactive` gives input to the scene and holds its authored timeline at the first frame while retained interaction runs.

## Source and playback

Open **Settings → Viscript**:

| Control | Behavior |
| --- | --- |
| **Show source** | Show or hide the read-only script beneath note players. Each player's `</>` button overrides it for that player. Edit the Markdown fence or original `.lua` file to change the scene. |
| Scene tab's code button | Show or hide the editable source when a `.lua` file is open in its own tab. This is separate from **Show source** for notes. |
| **Controls** | Show the play bar on hover, always, or hide it. Scene tabs always show controls. |
| **Autoplay** | Play scenes as they appear; otherwise wait for play. Reduced-motion settings start them paused. |
| **At the end** | **Replay at once** or **Stop on the last frame**. |

While editing a fenced scene in Live Preview, Obsidian's editor supplies its source; the player above it does not duplicate the script. **Reload all scenes** refreshes players after a change.

## Fonts

The plugin bundles **Pretendard** for text, including Korean, and **JetBrains Mono** for code. Scene font names `Pretendard` and `JetBrainsMono` follow the configured text and code font choices. Device-installed fonts are not used. In **Settings → Viscript → Fonts**:

1. Put custom `.ttf`, `.otf`, `.ttc`, or `.otc` files in a vault folder such as `Fonts`, and select it as **Font folder**.
2. Choose **Another font** under **Text font** or **Code font**, then enter its family or full name. A scene can also request a font by name, such as `font = "Noto Serif KR"`; names are matched against the folder's fonts or filenames.
3. **Download fonts** optionally fetches missing fonts from Fontsource and saves them in the chosen folder, or `Fonts` when none is set. Turn it off to use bundled and vault fonts only. Missing fonts fall back to the configured text or code font.

The settings preview shows the rendered fonts. **Look for fonts again** rescans after a font change; the folder travels with the vault.

## Verify and deliver

Review the scene through vis-skills, then check its embed in Obsidian's reading view and Live Preview. Confirm fonts, source visibility, and playback with the intended settings. For sound, put the `.wav` file in the vault, reference it as a link relative to the note or scene, and press play to start audio. Deliver the note path, scene path, and any relevant setting; keep API and animation guidance in the authoring skills above.
