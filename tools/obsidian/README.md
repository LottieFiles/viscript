# viscript for Obsidian

Plays viscript scenes, animated explanations written in Lua or JavaScript, in your notes: as
code blocks, as embedded `.lua` files, and in a tab of their own with the source beside them.
The viscript WebAssembly runtime draws them in a render worker through the shared player in
[`tools/player`](../player), so notes stay responsive while scenes play.

## Install

The [vis-obsidian skill](../skill/vis-obsidian/SKILL.md) includes the built plugin,
its WASM runtime and fonts. Install that bundle into an existing vault:

```sh
node tools/skill/vis-obsidian/scripts/install.mjs /path/to/vault
```

Add `--show-source` to show scripts below note players. The installer preserves
existing notes and settings. Enable **viscript** in **Community plugins**, or
reload it after an update.

Build the plugin from the checkout and copy it into a vault:

```sh
cd tools/obsidian
npm ci && npm ci --prefix ../player
npm run build                                  # main.js
node esbuild.config.mjs --vault /path/to/vault # builds and copies it to the vault
```

To refresh the plugin included with the skill, run
`npm run bundle:skill --prefix tools/obsidian` from the repository root.

The copy goes to `<vault>/.obsidian/plugins/viscript/` (`main.js`, `manifest.json`,
`styles.css`). Turn it on under **Settings → Community plugins**. While working on the plugin,
`npm run dev -- --vault /path/to/vault` rebuilds on every change; reload the plugin to see it.

It needs Obsidian 1.13 or newer, on a computer, a phone or a tablet.

## Scenes in notes

A code block holds a scene. `viscript` and `viscript-lua` are Lua, and `viscript-js` is
JavaScript; **Insert a scene code block** in the command palette starts one.

````markdown
```viscript loop
local scene = vis.scene {width = 640, height = 360, camera = {height = 7}}
local ball = scene:circle {center = {-3, 0}, radius = 0.5, fill = "accent"}
scene:grow_from_center(ball, 0.6)
scene:shift(ball, {6, 0}, 1.5)
scene:wait(0.5)
return scene
```
````

A `.lua` file in the vault is embedded like an image, `![[orbit.lua]]`, and opens as a scene in
its own tab with its source below. Edits there are saved to the file and redraw the scene, and
every note that embeds the file follows. An error shows under the scene, with its
line, until the source is fixed.

**New viscript**, beside **New canvas** when you right-click a folder in the file explorer, makes
`Untitled.lua` there from the scene above, numbered past the names the folder holds. It opens in
the scene tab with its name selected, so typing names the file. It is there while
**Open .lua files as scenes** is on.

Words after the code block's language, or in the embed's alias, set options:
`![[orbit.lua|480|loop|Two circles]]`, ```` ```viscript-js center 520 ````.

| Words | Option |
|---|---|
| `600` | Width in pixels; the scene keeps its proportions |
| `loop`, `once` or `stop` | What happens at the end |
| `autoplay` or `play`, `paused` or `still` | Whether the scene plays as soon as it is seen |
| `controls`, `nocontrols` | Always show the play bar, or hide it |
| `left`, `center`, `right` | Alignment |
| `interactive` | The scene takes pointer and key input, for scenes built to |

Other words name the scene for screen readers, as an image's alt text does.

In Live Preview, a code block you are editing keeps its scene above the source, and the scene
follows what you type once you pause, with any error under it. A click on it plays or pauses it
and the timeline seeks without leaving the code; an `interactive` scene takes input once the cursor
leaves the block.

Turn on **Show source** in the plugin settings to keep each note player's script below it.
The code button on a player shows or hides that script; **Copy source** copies the original
Lua or JavaScript. These panels are read-only: edit the Markdown code block to change a scene.
While a block is being edited, Obsidian's editor supplies the source and the player above it
shows no second copy.

The API reference marks each example with `<!-- api-example: name -->`. An adjacent Lua
`viscript` fence and JavaScript `viscript-js` fence under that marker share one player, with
language buttons to switch the scene and its source. Both sources stay in Markdown. Unmarked
scenes keep their separate players.

Scene audio cues and runtime sounds can name a `.wav` file in the vault, resolved like a link
from the note or scene file. Sound scenes render on the main thread for Web Audio; other scenes
keep the shared render worker. Click the player's play button to start sound. Playback, pause,
seeking and replay keep the sound in step with the scene.

Scenes that name no theme, or name `adaptive`, take their colors from the Obsidian theme and
follow it between light and dark, and set their text in the text font below. A click plays or
pauses a scene; the play bar appears on hover, and on touch screens it stays under the scene.
Scenes load when they scroll into view and let go of their memory when they leave it.

Commands: **Insert a scene code block**, **Toggle autoplay**, **Reload all scenes** and
**Look for fonts again**.

## Fonts

Scenes ask for fonts by name, as ThorVG's web players do. The plugin ships the fonts the
engine's themes set text in, the files the VS Code extension ships: Pretendard for text, Korean
included, and JetBrains Mono NL for code. Fonts installed on the device are never used, so a scene
draws the same on a computer, a phone or a tablet. A scene can also name another font, as
`font = "Noto Serif KR"`. The plugin gives each name a font:

- **Text font**: the bundled Pretendard, or another font by name.
- **Code font**: the bundled JetBrains Mono NL, or another font by name.
- **A font a scene names** is looked for in the font folder, by its name or its file's name,
  then on [Fontsource](https://fontsource.org). Failing those, its text is drawn in the text font,
  or in the code font for a name that reads as a monospaced font's, such as Fira Code; the scene
  plays either way. The names the themes use always take the bundled fonts.
- **Font folder**: a vault folder of fonts for scenes to name. It syncs with the vault, so your
  other devices have the same fonts.
- **Download fonts**, on unless you turn it off: a font that is not in the folder is downloaded
  from Fontsource and saved in the font folder (`Fonts` if none is set, which then becomes the
  font folder), so the next scene, and the vault on your other devices, find it there.

Letters a text's font lacks are drawn from the runtime's built-in fonts when they have them, and
are otherwise left out, such as Korean in JetBrains Mono. TrueType and OpenType fonts both work
(`.ttf`, `.otf`, and the collections `.ttc` and `.otc`). The settings show how many fonts were
found and a preview of text and code as scenes draw them. A changed font setting, or a font
added to the folder, loads the scenes again with their fonts.

## JavaScript scenes

A JavaScript scene is a program that runs when its note is shown. The plugin runs it in a
sandbox: a hidden frame with no origin of its own, under a policy that refuses every network
request, where each scene gets a new worker that goes once the scene is built, or after 10 seconds
if it never is. From there the scene's code reaches neither Node nor Obsidian, its storage, your
notes and files, the network, nor other scenes. What the scene builds comes back as Lua, which the
render worker draws as it draws Lua scenes: inside the runtime, with no files, network or system.
Where a system does not hold the sandbox's workers to that policy, JavaScript scenes are not run,
and say so.

## Network

The plugin uses the network only to download fonts, and only while **Download fonts** is on: for
a font a scene or the settings name that is not in the font folder, it asks Fontsource's API
(`api.fontsource.org`) whether it has the font, and downloads the font's file from jsDelivr
(`cdn.jsdelivr.net`). The requests carry the font's name and nothing else from your notes or
settings. The fonts the themes use ship with the plugin and are never downloaded.

Downloaded fonts are saved in the font folder and found there from then on. A download that fails
leaves the text in the text font until **Look for fonts again**. With **Download fonts** off, the
plugin stays off the network.

## Other settings

| Setting | |
|---|---|
| Autoplay | Play scenes as soon as they appear; scenes start paused when the system asks for reduced motion |
| At the end | Replay at once (the default), or stop on the last frame |
| Controls | The play bar on hover, always, or never |
| Show source | Show each note player's script by default; its code button can also show or hide it |
| Frame rate | Up to 60 frames a second (30 on phones), or 30, 24 or 15 |
| Sharpness | Balanced, a little under the screen's resolution on dense screens, or Sharpest |
| Render in a worker | Draw scenes off Obsidian's main thread; sound scenes use the main thread |
| Open .lua files as scenes | Open `.lua` files in the scene tab; takes effect when Obsidian next starts |

## How it is built

`main.js` is the whole plugin: the plugin itself, the render server and the WebAssembly
runtime, the compiler JavaScript scenes run in, and the two bundled fonts, gzipped into the file
and each unpacked when a scene first needs it, so nothing is downloaded to play scenes. The fonts,
mostly Pretendard's Korean letters, make up 1.6 MB of its 2.6 MB. It needs WebAssembly and
`DecompressionStream`, which iPhones and iPads have from iOS 16.4. One render worker draws scenes
without sound, including those in popout windows and the settings window; a shared main-thread
runtime draws sound scenes. Fonts load once into each runtime instance that scenes share.

## Test

```sh
npm run check            # TypeScript
npm test                 # option words, reading scene sources, which font each name gets, new file names
npm run test:obsidian    # builds, then drives the Obsidian installed here
```

`test:obsidian` starts the installed Obsidian with a profile folder and a vault made for the run. It
never opens your vaults, and it stops before changing anything if Obsidian opens another one. It
checks reading view, Live Preview with a code block being edited, embeds, the scene tab with editing
and errors, both themes, **New viscript** in a folder's menu, the settings, the fonts each name
gets, downloading fonts from Fontsource with and without a font folder set, the worker and the main
thread, what JavaScript scenes can reach, and turning the plugin off and on; the downloads are
skipped when Fontsource cannot be reached. Set `OBSIDIAN` to the executable if it is not in the
usual place, and `E2E_DIR` to keep the vault and screenshots. Without Obsidian installed, the run is
skipped.

## Limits

- Sounds that scenes start are not played.
- The sandbox for JavaScript scenes is tested on the desktop app. On phones and tablets they run
  only where the system holds the sandbox's workers to its policy.

## Licenses

The plugin is MIT-licensed, as viscript is. `main.js` also includes ThorVG (MIT), Lua (MIT),
LodePNG (zlib), miniaudio (public domain or MIT-0), CodeMirror's legacy modes (MIT), and the
fonts Pretendard, Pretendard Std, JetBrains Mono and Noto Sans Math (SIL Open Font License
1.1); their notices are at the top of `main.js`.
