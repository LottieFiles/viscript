# @viscript/player

Plays viscript scenes on canvases in a web page, an editor or an app built on web views. The
WebAssembly runtime compiles and draws each scene in a render worker, so a heavy scene never
blocks typing or scrolling; the page keeps a light player per canvas that asks for frames.

It is the player behind the [documentation site](../../docs/page) and the
[Obsidian plugin](../obsidian), written in TypeScript and bundled with esbuild.

```text
 main thread                                   render worker (or the page, as a fallback)
 ─────────────────────────────────────         ──────────────────────────────────────────
 VisPlayer ── one per canvas                   RenderServer
   loads, plays, seeks, follows the theme        one session per player
   lazy: loads when seen, frees when not   ─▶    InstancePool: WASM instances shared by
 Scheduler ── one per window                       sessions, fonts loaded once per instance
   one animation frame loop, observers           Surface: frames onto the transferred
 RenderHost ── one per page or plugin              OffscreenCanvas
   starts the server, coalesces frames,    ◀─    asks the host for fonts by name and
   replaces a worker that hangs                    for image bytes
```

## Words

- **Player** (`VisPlayer`): one per canvas. It loads a scene, plays, pauses and seeks it, follows
  the host's theme, loads when seen and frees its engine when not.
- **Render host** (`createRenderHost`): one per page or plugin. It starts the render server, gives
  it the runtime, fonts and images, coalesces frame requests and replaces a worker that hangs.
- **Render server** (`RenderServer`): in the render worker, or on the main thread as a fallback.
  It compiles scenes and draws their frames, with one session per player.
- **Instance pool** (`InstancePool`): the WASM instances the server shares between sessions; a
  font is loaded once per instance.
- **Surface**: the server's side of a player's canvas, the `OffscreenCanvas` it draws frames onto.
- **Scheduler**: one per window; the animation frame loop and the observers that tell players
  when they are seen, resized or hidden.
- **Catalog** (`createCatalog`): a fixed table of images, each fetched once, and which of them a
  scene loads.
- **Font provider**: answers `fetch(name)` with a font's bytes or `null`; see [Fonts](#fonts).
- **Compiler** (`createSandboxCompiler`): runs JavaScript scenes in a sandboxed frame so that the
  server is sent only the Lua they build; see [Scene code](#scene-code).
- **Frozen**: a playing scene out of view; no frames are drawn, and after `releaseDelay` its
  engine is freed until it is seen again.

## Use

```js
import {chainFonts, createCatalog, createRenderHost, fontTable, runtimeFonts, VisPlayer} from "./player.js";

const catalog = createCatalog({
    assets: {"earth.png": {url: "assets/earth.png", mime: "png"}},
    // The images one scene loads, here those it names.
    select: (source) => source.includes("earth.png") ? ["earth.png"] : [],
});
const host = createRenderHost({
    server: {url: "player-worker.js"},
    wasm: () => fetch("runtime/vis-wasm.wasm").then((response) => response.arrayBuffer()),
    // Fonts by the names scenes set text in: Pretendard from the page, JetBrains Mono from the runtime.
    fonts: chainFonts(fontTable({Pretendard: "fonts/Pretendard.ttf"}), runtimeFonts("runtime/")),
    assets: catalog,
});

const player = new VisPlayer({
    host, container: document.querySelector("#figure"),
    resources: (source, language) => catalog.resources(source, language),
    end: "crossfade",
});
player.addEventListener("error", ({detail}) => console.warn(detail.phase, detail.message));
player.load({source: luaSource, language: "lua", name: "scene.lua"});
```

Fonts come from the host's `fonts` provider, by the names scenes set text in (see
[Fonts](#fonts)). `createCatalog` serves a fixed table of images, each fetched once on first use,
and `select` picks the ones each scene loads; a host with images of its own, such as the Obsidian
plugin, implements the player's `resources` and the host's `assets` itself.

### The render host

`createRenderHost` starts the render server the first time a player opens a session.

- **`server`**: the server bundle, as a URL (`{url}`) or as source text (`{code}`); a host that
  cannot serve files, such as an Obsidian plugin, starts the worker from a blob.
- **`wasm`**: returns `vis-wasm.wasm`. It is compiled once and every instance shares it.
- **`fonts`**: a `FontProvider`, asked for each font a scene sets text in (see [Fonts](#fonts)),
  and **`fontTimeout`**, how long a font may take to arrive (15000 milliseconds).
- **`assets`**: gives the bytes of the images players list in their `resources`.
- **`mode`**: `"auto"` renders in a worker where the browser can transfer a canvas to one
  (`OffscreenCanvas`), and on the main thread otherwise. `"inline"` always uses the main thread.
- **`hangTimeout`**: a worker that stops answering for this long is replaced. Its players get
  new canvases and load again; the scene that stalled it reports a `timeout` error.
- **`inlineJavaScript`**: `false` refuses JavaScript scenes on the main thread (see below).
- **`compiler`**: runs JavaScript scenes before the server sees them, which is then sent only
  the Lua they build; `createSandboxCompiler` makes one (see below).

### The player

`new VisPlayer({host, canvas | container, resources, …})` takes a canvas, or makes one in
`container`; `resources` lists the images each scene loads. Options: `autoplay`, `end`
(`"auto"`, `"loop"`, `"crossfade"`, `"stop"`), `maxFps`, `quality` (`"auto"` or `"high"`),
`maxRatio`, `interactive`, `theme` (colors for scenes that adapt to the host), `label`,
`releaseDelay` and `rootMargin`.

Methods: `load(scene)`, `reload()`, `play()`, `pause()`, `stop()`, `seek(time)`,
`playRange(start, end)`, `clearRange()`, `configure(options)`, `on(type, listener)` and
`destroy()`. Properties: `state`, `info`, `duration`, `time`, `playing`, `range`, `live`,
`error` and `canvas`, which is replaced when a worker is, so read it again after events.

| State | Meaning |
|---|---|
| `loading` | Not yet drawn: waiting to be seen, compiling, or fetching fonts and images |
| `playing`, `paused` | Drawn, and playing or not |
| `stopped` | Back at the start after `stop()`, or held at the end when `end` is `"stop"` |
| `frozen` | Playing, but out of view: no frames are drawn, and after `releaseDelay` the engine is freed |
| `error` | The scene failed; the next `load` tries again |
| `destroyed` | Gone for good |

Events, all `CustomEvent`s: `load`, `play`, `pause`, `stop`, `freeze`, `frame`, `loop`,
`complete`, `error`, `sound`, `statechange` and `destroyed`. An `error` carries its `phase`:
`load`, `render`, `runtime`, `resource`, `engine` or `timeout`.

### Scene code

A JavaScript scene is a program. Without a compiler it runs where the server does. In the
worker, the server first removes what some hosts add to workers, such as Node in Obsidian, along
with the means to start other workers and the storage the worker shares with the page; the code
can still use the network and read the other scenes the worker is sent. Because scene code can
post messages as the server, the host sends the server only the bytes of images some load named
and the fonts its provider gives; scenes choose the font names, so a provider must be safe to ask
for any. On the main thread a JavaScript scene can do whatever the page can, so a host may pass
`inlineJavaScript: false` to run JavaScript scenes only in the worker; they then fail with a
`load` error where none runs.

A host that shows scenes it did not write passes a compiler instead:

```js
import {createRenderHost, createSandboxCompiler} from "./player.js";

const code = await fetch("player-compiler.js").then((response) => response.text());
const host = createRenderHost({server, wasm, fonts, compiler: createSandboxCompiler({code})});
```

The sandbox is a hidden frame with an opaque origin, under a policy that refuses every network
request. It starts a worker for each scene, which runs the scene's code with the runtime's
JavaScript API and answers with the Lua the scene builds; the server is sent that Lua and never
runs JavaScript, in the worker or on the main thread. From the sandbox the code reaches neither
the page, its storage, the render worker, the network, nor other scenes. A scene still running
after `timeout` milliseconds (10000) is stopped and fails with a `timeout` error, and a player
showing it then waits for `reload()` or a new `load` instead of trying again whenever it comes
into view. The frame checks once that its workers are held to the policy, and where they are not,
JavaScript scenes fail with a `load` error rather than run. Starting the frame takes about a
tenth of a second, the first time a JavaScript scene loads; each scene then adds a few
milliseconds. Lua scenes run inside the runtime and reach nothing outside it.

### Several windows

A host can show players in more than one window from one script; Obsidian does, in popout
windows and its settings window. Each window gets its own `Scheduler`, taken from the canvas's
document: its animation frame loop, `IntersectionObserver`, `ResizeObserver`, device pixel
ratio, visibility and media queries. Another window's observers never see the elements, and a
hidden window's frames stop on their own.

### Fonts

Fonts work as in ThorVG's web players. The engine draws text in the font registered under the
name the text is set in, and in its own default font when no font is: the Latin, Greek, figures,
arrows and math signs of Pretendard Std, built into the runtime. A missing font never fails a
scene, and the player does not report it.

When a scene loads, the render server asks the host's `fonts` provider for each name the scene
sets text in that it has no font for, registers the fonts the provider gives in the scene's WASM
instance, and loads the scene once more, so it shows in its fonts from its first frame. Each name
is asked for once. A provider that answers `null` leaves the name to the default font; one that
fails, or takes longer than `fontTimeout`, is asked again on a later load. A name keeps the first
font it was given in an instance, so a host whose fonts change calls `host.refreshFonts()`: the
server forgets its answers and starts new instances, and scenes get the new fonts as they load.

A provider has the shape of WebCanvas's `FontProvider`, `fetch(name) → Promise<{data, type} |
null>`, so providers carry over between the two. TrueType (`"ttf"`) and OpenType with CFF outlines
(`"otf"`) both load, and `type` is read from the bytes when left out.

- `fontTable({name: url | () => bytes})` serves a fixed table, each font fetched once.
- `runtimeFonts(base)` serves JetBrains Mono, the code font shipped with the runtime.
- `FontsourceProvider` fetches any family [Fontsource](https://fontsource.org) has from jsDelivr,
  as WebCanvas's provider does, in the nearest weight and in a font's Korean, Japanese or Chinese
  file where it has one, which holds its Latin letters too. Where `fetch` cannot reach the CDN,
  the host passes its own `request` function; the Obsidian plugin passes `requestUrl`.
- `chainFonts(...providers)` asks each in turn and answers with the first font.

`THEME_FONTS` names the families the engine's themes set text in: `Pretendard` for text,
`JetBrainsMono` for code, and `Source Serif 4` with `IBM Plex Sans KR` in `adaptive_vscode`. The
documentation site serves Pretendard Std under `Pretendard`, and the full Pretendard once a scene
shows Hangul. The Obsidian plugin ships all four, the files the VS Code extension ships, and
answers other names from the vault's font folder and Fontsource.

`fonts/sfnt.ts` reads a font's names, characters and whether it is monospaced, and splits a face
out of a collection; `FontLibrary` indexes a host's font files by family and loads only the faces
it needs.

## Where it comes from

The structure follows [thorvg.web](https://github.com/thorvg/thorvg.web). From its
`lottie-player` come the player's states and events, loading when seen, freezing when not, and
the device-pixel-ratio policy. From its WebCanvas come the split where WASM rasterizes into
memory and the page presents the pixels on a canvas, and fonts by name from a `FontProvider`.
The worker and the shared instances are viscript's own. viscript scenes are Lua or JavaScript programs, not Lottie
files.

## Build

```sh
npm ci
npm run build            # dist/player.js (ES module), dist/player-worker.js and dist/player-compiler.js
node build.mjs --runtime ../skill/vis-skills/assets/wasm --out dist --no-minify
```

The server bundle includes the runtime's JavaScript (`client.js`, `bindings.js`, `vis-wasm.js`)
from `--runtime`, by default the skill's bundle (`tools/skill/vis-skills/assets/wasm`), and the
compiler bundle its JavaScript API (`bindings.js`); both are classic scripts. The host fetches `vis-wasm.wasm` itself. Other builds call `build.mjs`: the
documentation site's `scripts/build-page.mjs` writes the files with `buildWeb`, and the Obsidian
plugin embeds the server's and the compiler's source with `bundleServer` and `bundleCompiler`.

## Test

```sh
npm run check            # TypeScript
npm test                 # policy, theme, fonts and font providers, the instance pool, the server and the host
npm run test:browser     # Chromium: worker and main thread, fonts, input, a hung worker, scrolling, the sandbox
```

`test:browser` uses Playwright's Chromium, or the browser `CHROMIUM` names.

## Limits

- Sounds the retained runtime starts arrive as `sound` events; the player does not play audio.
- Without `OffscreenCanvas`, or when the worker cannot start, scenes are drawn on the main thread.
