// Live scene players. The shared viscript player (tools/player, built into build/player/) draws
// every scene in one render worker; this module adds the page's editor, controls, language switch
// and light/dark adaptation.
import {createCatalog, createRenderHost, fontTable, VisPlayer} from "./build/player/player.js";
import {ASSETS, assetsFor, FONTS, FULL_PRETENDARD, needsFullPretendard} from "./resources.js";

const $ = (s, root = document) => root.querySelector(s);

const players = new Set();

// ---------- theme ----------
// Scenes are authored for dark; in light mode the scene theme and the hard-coded
// panel colours swap to their light counterparts (and back).
const DARK_TO_LIGHT = {
  "14171f": "f4f6fa", "3a4152": "c3c8d3", "e6e8ee": "1d2230", "7d8596": "6b7280", "262b38": "dde1e8",
  "1d2230": "e8ebf0", "2f3646": "cfd4dc", "2a3142": "d5dae2", "aab1bf": "4b5563", "1f2430": "e5e8ee",
  "26313b": "d5dae2", "2a3a55": "bcd3f5", "6cb6ff": "1f6feb", "e3b341": "b7791f", "ff7b72": "cf222e",
  "7ee787": "1a7f37", "c8a7ff": "8250df", "79c0ff": "0969da", "b8bcc2": "6e7781", "1b3350": "dbeafe", "1c3a26": "dcfce7", "3a2f12": "fdf0c9", "000000": "ffffff", "ff7a45": "e0561f", "151922": "eef0f4",
};
const LIGHT_TO_DARK = Object.fromEntries(Object.entries(DARK_TO_LIGHT).map(([d, l]) => [l, d]));
const mode = () => document.documentElement.dataset.theme === "light" ? "light" : "dark";
function themeAdapt(src) {
  const light = mode() === "light";
  const map = light ? DARK_TO_LIGHT : LIGHT_TO_DARK;
  return src
    .replace(/\b((?:theme|preset)\s*(?:=|:)\s*)"pro_(?:black|white)"/g, (_, key) => `${key}"${light ? "pro_white" : "pro_black"}"`)
    .replace(/#([0-9a-fA-F]{6})/g, (m, hex) => "#" + (map[hex.toLowerCase()] ?? hex));
}
export function retheme() {
  for (const p of players) p.retheme();
}

// ---------- rendering ----------
// One render worker for the page, started by the first player that comes into view. It asks for
// each font a scene sets text in by name; the runtime and the first fonts are preloaded by
// index.html on routes that open on a figure.
const url = (path) => new URL(path, import.meta.url);
const fontsWith = (pretendard) => fontTable(Object.fromEntries([...FONTS].map(([name, path]) =>
  [name, url(name === "Pretendard" ? pretendard : path)])));
let renderHost = null, catalog = null, fonts = fontsWith(FONTS.get("Pretendard")), fullPretendard = false;
function host() {
  if (renderHost) return renderHost;
  catalog = createCatalog({
    assets: Object.fromEntries([...ASSETS].map(([name, {path, mime}]) => [name, {url: url(path), mime}])),
    select: assetsFor,
  });
  renderHost = createRenderHost({
    server: {url: url("build/player/player-worker.js")},
    wasm: async () => {
      const response = await fetch(url("runtime/vis-wasm.wasm"));
      if (!response.ok) throw new Error(`runtime/vis-wasm.wasm (${response.status})`);
      return response.arrayBuffer();
    },
    assets: catalog,
    fonts: {fetch: (name, options) => fonts.fetch(name, options)},
  });
  return renderHost;
}
// The first scene with letters only the full Pretendard has switches "Pretendard" to it; scenes
// loaded from then on get it, and the others keep Pretendard Std's identical glyphs until they
// load again.
function widenFonts(source) {
  if (fullPretendard || !needsFullPretendard(source)) return;
  fullPretendard = true;
  fonts = fontsWith(FULL_PRETENDARD);
  host().refreshFonts();
}

// CodeMirror measures itself, so an editor created out of view is refreshed when it shows.
const editors = new WeakMap();
const editorObserver = new IntersectionObserver((entries) => {
  for (const entry of entries) if (entry.isIntersecting) editors.get(entry.target)?.refresh();
});

// Lua / JS choice is shared by every player and remembered.
const langs = new Set();
// JavaScript is the default; choosing Lua anywhere switches every player and code tab.
let lang = "js";
try { lang = localStorage.getItem("viscript-lang") === "lua" ? "lua" : "js"; } catch { /* private mode */ }
export const getLang = () => lang;
// Code tabs outside players subscribe here; they are dropped on every route change.
const tabListeners = new Set();
export function onLang(listener) { tabListeners.add(listener); }
export function setLang(next) {
  lang = next;
  try { localStorage.setItem("viscript-lang", next); } catch { /* private mode */ }
  for (const f of langs) f();
  for (const f of tabListeners) f(lang);
}

const STACK_LINES = 15;

export function createPlayer(el, source, {js = null, editable = false, name = "scene.lua", loop = false, interactive = false, clips = [], dark = false, maxRatio = 3, maxFps, quality = "auto"} = {}) {
  // dark: keep the authored dark palette in light mode (cinematic hero scenes)
  const adapt = dark ? (x) => x : themeAdapt;
  const originals = {lua: source, js};
  let mine = js && lang === "js" ? "js" : "lua";
  let code = adapt(originals[mine]), activeClip = null, debounce = 0;

  el.innerHTML = editable ? `
    <div class="view"><div class="canvas-wrap"><canvas></canvas></div>
      <div class="bar"><button class="pp" title="Play / pause">❚❚</button>
        <input type="range" aria-label="Scene time" min="0" max="1" step="0.001" value="0"><span class="t">0.00 / 0.00</span></div>
    </div>
    <div class="code"><div class="code-head"><span class="file"></span>${js ? `<span class="langs"><button data-lang="js">JS</button><button data-lang="lua">Lua</button></span>` : ""}<button class="reset">reset</button></div>
      <div class="cm-wrap"></div></div>
    <pre class="err" role="alert" hidden></pre>` : `<canvas></canvas><div class="progress"></div><pre class="err" role="alert" hidden></pre>`;
  el.classList.add(editable ? "player" : "stage-frame");
  const canvas = $("canvas", el);
  const fileName = () => mine === "js" ? name.replace(/\.lua$/, ".js") : name;
  // Sized before the player hands the canvas to the render worker.
  canvas.width = 960; canvas.height = 540;
  const err = $(".err", el), scrub = $("input", el), timeEl = $(".t", el), pp = $(".pp", el), bar = $(".progress", el);

  // Sections play once and hold their last frame; whole scenes replay, the last frame dissolving
  // over the restart, unless they loop seamlessly.
  const player = new VisPlayer({
    host: host(), canvas, resources: (text, language) => catalog.resources(text, language),
    autoplay: !clips.length, end: loop ? "loop" : "crossfade", interactive, maxFps, quality, maxRatio,
    label: name.replace(/\.lua$/, "").replace(/[-_]/g, " ") + " animation",
  });
  const load = () => {
    widenFonts(code);
    return player.load({source: code, language: mine, name: fileName()});
  };

  const showError = (message) => { err.textContent = message; err.hidden = false; };
  player.addEventListener("error", ({detail: {phase, message}}) => {
    if (phase === "load" || phase === "runtime" || phase === "timeout") showError(message);
    // A load error names the source line, a draw error does not: keep the load error visible.
    else if (phase === "render") { if (err.hidden) showError(message); }
    else showError(`Could not load this figure. ${message}`);
  });
  player.addEventListener("load", () => {
    err.hidden = true;
    const live = player.live;
    player.canvas.classList.toggle("live", live);
    player.canvas.classList.toggle("input", live && /ctx:(pointer|key|action)\(/.test(code));
    el.classList.toggle("live", live);
    // A scene shorter than the time shown starts over, outside any section.
    if (!player.range) selectClip(null);
  });
  player.addEventListener("frame", ({detail: {time, duration}}) => {
    if (scrub) { scrub.max = duration || 1; if (document.activeElement !== scrub) scrub.value = time; }
    if (timeEl) timeEl.textContent = `${time.toFixed(2)} / ${duration.toFixed(2)}`;
    if (bar) bar.style.width = duration > 0 ? `${Math.min(1, time / duration) * 100}%` : "0";
  });
  const showPlaying = () => { if (pp) pp.textContent = player.playing ? "❚❚" : "▶"; };
  for (const type of ["statechange", "play", "pause", "stop", "complete"]) player.addEventListener(type, showPlaying);

  const clipButtons = clips.map((clip) => {
    const button = document.createElement("button");
    button.type = "button";
    button.title = `Play ${clip.label}`;
    button.setAttribute("aria-label", `Play ${clip.label}`);
    button.setAttribute("aria-pressed", "false");
    const [left, top, width, height] = clip.bounds;
    Object.assign(button.style, {left: `${left * 100}%`, top: `${top * 100}%`, width: `${width * 100}%`, height: `${height * 100}%`});
    button.onclick = () => {
      selectClip(clip);
      player.playRange(...clip.range);
    };
    return button;
  });
  const selectClip = (clip) => {
    activeClip = clip;
    clipButtons.forEach((button, i) => button.setAttribute("aria-pressed", String(clips[i] === clip)));
  };
  if (clips.length) {
    const overlay = document.createElement("div");
    overlay.className = "scene-clips";
    overlay.setAttribute("role", "group");
    overlay.setAttribute("aria-label", "Animation sections");
    overlay.append(...clipButtons);
    canvas.parentElement.append(overlay);
  }
  showPlaying();

  const api = {
    retheme() { code = adapt(code); load(); },
    destroy() {
      langs.delete(api.switchLang); clearTimeout(debounce);
      editorObserver.unobserve(el); players.delete(api);
      player.destroy();
    },
  };

  if (editable) {
    // long code would leave the canvas floating beside a tall editor: stack it below instead
    const lines = Math.max(...[source, js].filter(Boolean).map((c) => c.split("\n").length));
    if (lines >= STACK_LINES) {
      el.classList.add("stacked");
      $(".cm-wrap", el).style.height = `${Math.min(lines, 34) * 20 + 12}px`;
    }
    const cm = CodeMirror($(".cm-wrap", el), {value: code, mode: mine === "js" ? "javascript" : "lua", lineNumbers: true, indentUnit: 4, viewportMargin: Infinity});
    cm.on("change", () => { code = cm.getValue(); clearTimeout(debounce); debounce = setTimeout(() => {
      player.clearRange(); selectClip(null); player.seek(0);
      if (clips.length) player.pause(); else player.play();
      load();
    }, 220); });
    $(".reset", el).onclick = () => cm.setValue(adapt(originals[mine]));
    const showLang = () => {
      $(".file", el).textContent = `${fileName()} — live`;
      for (const b of el.querySelectorAll(".langs button")) b.classList.toggle("on", b.dataset.lang === mine);
    };
    const switchLang = () => {
      const next = js && lang === "js" ? "js" : "lua";
      if (next === mine) return;
      mine = next;
      cm.setOption("mode", mine === "js" ? "javascript" : "lua");
      cm.setValue(adapt(originals[mine]));
      showLang();
    };
    for (const b of el.querySelectorAll(".langs button")) b.onclick = () => setLang(b.dataset.lang);
    langs.add(switchLang);
    api.switchLang = switchLang;
    showLang();
    pp.onclick = () => { if (player.playing) player.pause(); else player.play(); };
    scrub.oninput = () => { player.clearRange(); selectClip(null); player.pause(); player.seek(+scrub.value); };
    api.retheme = () => cm.setValue(adapt(cm.getValue()));
    editors.set(el, cm);
    editorObserver.observe(el);
  }

  players.add(api);
  load();
  return api;
}

export function destroyAll() {
  tabListeners.clear();
  for (const p of [...players]) p.destroy();
}
