// Live scene players: one WASM runtime per visible player, released when hidden.
import {createVis, evaluateScene} from "./runtime/client.js";

const $ = (s, root = document) => root.querySelector(s);
const DPR = Math.min(2, Math.max(1, Math.round(devicePixelRatio || 1)));
const BOOT = "local s = vis.scene {width = 16, height = 16}\nreturn s";

// ---------- runtime pool ----------
// Each visible player owns one WASM instance; hidden ones release theirs.
const fonts = Promise.all([
  ["Source Serif 4", "runtime/SourceSerif4-Semibold.ttf"],
  ["IBM Plex Sans KR", "runtime/IBMPlexSansKR-SemiBold.ttf"],
  ["Pretendard", "runtime/Pretendard.ttf"],
  ["JetBrainsMono", "runtime/JetBrainsMono.ttf"],
].map(async ([name, url]) => {
  const response = await fetch(new URL(url, import.meta.url));
  if (!response.ok) throw new Error(`Could not load font: ${url}`);
  return [name, await response.arrayBuffer()];
}));

// Images that documentation scenes load with picture{asset = ...}.
const assets = Promise.all([["earth.png", "assets/earth.png", "png"]].map(async ([name, url, mime]) => {
  const response = await fetch(new URL(url, import.meta.url));
  if (!response.ok) throw new Error(`Could not load asset: ${url}`);
  return [name, await response.arrayBuffer(), mime];
}));

async function makeRuntime() {
  const [fontData, assetData] = await Promise.all([fonts, assets]);
  const rt = await createVis(BOOT, "boot.lua", {renderEngine: "cpu"});
  for (const [name, bytes] of fontData) rt.font(name, bytes.slice(0), "ttf");
  for (const [name, bytes, mime] of assetData) rt.asset(name, bytes.slice(0), mime);
  return rt;
}

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
    .replace(/theme( = |: )"pro_(black|white)"/g, (_, sep) => `theme${sep}"${light ? "pro_white" : "pro_black"}"`)
    .replace(/#([0-9a-fA-F]{6})/g, (m, hex) => "#" + (map[hex.toLowerCase()] ?? hex));
}
export function retheme() {
  for (const p of players) p.retheme();
}
const observer = new IntersectionObserver((entries) => {
  for (const e of entries) e.target.__player?.visible(e.isIntersecting);
}, {rootMargin: "200px"});

// Render at the integer pixel ratio just above the canvas's real device size, then resample to that exact size.
// Browser zoom and small stages give non-integer scales; letting CSS stretch the raster aliases thin lines,
// so we downscale ourselves with smoothing, halving first when the gap is large.
const fitSrc = document.createElement("canvas"), fitTmp = document.createElement("canvas");
function drawFit(rt, canvas, t, maxRatio = 3) {
  const [lw, lh] = rt.size();
  const w = Math.max(1, Math.round(canvas.clientWidth * (devicePixelRatio || 1)));
  const h = Math.max(1, Math.round(w * lh / lw));
  const ratio = Math.min(maxRatio, Math.max(1, Math.ceil(w / lw - 0.01)));
  if (lw * ratio === w) { rt.draw(canvas, t, ratio); return; }
  rt.draw(fitSrc, t, ratio);
  let from = fitSrc, fw = fitSrc.width, fh = fitSrc.height;
  const tc = fitTmp.getContext("2d");
  while (fw / 2 >= w * 1.5) {
    const nw = Math.round(fw / 2), nh = Math.round(fh / 2);
    if (fitTmp.width < nw || fitTmp.height < nh) { fitTmp.width = Math.max(fitTmp.width, nw); fitTmp.height = Math.max(fitTmp.height, nh); }
    tc.imageSmoothingQuality = "high";
    tc.drawImage(from, 0, 0, fw, fh, 0, 0, nw, nh);
    from = fitTmp; fw = nw; fh = nh;
  }
  if (canvas.width !== w) canvas.width = w;
  if (canvas.height !== h) canvas.height = h;
  const ctx = canvas.getContext("2d");
  ctx.imageSmoothingQuality = "high";
  ctx.drawImage(from, 0, 0, fw, fh, 0, 0, w, h);
}

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

export function createPlayer(host, source, {js = null, editable = false, name = "scene.lua", onEnd = null, loop = false, dark = false, maxRatio = 3} = {}) {
  // dark: keep the authored dark palette in light mode (cinematic hero scenes)
  const adapt = dark ? (x) => x : themeAdapt;
  const originals = {lua: source, js};
  let mine = js && lang === "js" ? "js" : "lua";
  let code = adapt(originals[mine]), rt = null, pending = null, t = 0, playing = true, last = 0, releaseTimer = 0, shown = false;
  // retained (interactive) scenes: input events queue here and are pumped once per frame
  let live = false, events = [], consumed = true, buttons = 0;

  host.innerHTML = editable ? `
    <div class="view"><canvas></canvas>
      <div class="bar"><button class="pp" title="Play / pause">❚❚</button>
        <input type="range" aria-label="Scene time" min="0" max="1" step="0.001" value="0"><span class="t">0.00 / 0.00</span></div>
    </div>
    <div class="code"><div class="code-head"><span class="file"></span>${js ? `<span class="langs"><button data-lang="js">JS</button><button data-lang="lua">Lua</button></span>` : ""}<button class="reset">reset</button></div>
      <div class="cm-wrap"></div></div>
    <pre class="err" role="alert" hidden></pre>` : `<canvas></canvas><div class="progress"></div><pre class="err" role="alert" hidden></pre>`;
  host.classList.add(editable ? "player" : "stage-frame");
  const canvas = $("canvas", host);
  const fileName = () => mine === "js" ? name.replace(/\.lua$/, ".js") : name;
  canvas.setAttribute("aria-label", name.replace(/\.lua$/, "").replace(/[-_]/g, " ") + " animation");
  canvas.width = 960; canvas.height = 540;
  const err = $(".err", host), scrub = $("input", host), timeEl = $(".t", host), pp = $(".pp", host), bar = $(".progress", host);

  // restart without a cut: freeze the final frame on top and fade it into the replay underneath
  const ghost = document.createElement("canvas");
  ghost.setAttribute("aria-hidden", "true");
  ghost.className = "ghost";
  canvas.after(ghost);
  const crossfade = () => {
    ghost.width = canvas.width; ghost.height = canvas.height;
    ghost.getContext("2d").drawImage(canvas, 0, 0);
    ghost.style.transition = "none"; ghost.style.opacity = 1;
    ghost.getBoundingClientRect();
    ghost.style.transition = ""; ghost.style.opacity = 0;
  };
  const setPlaying = (p) => { playing = p; if (pp) pp.textContent = p ? "❚❚" : "▶"; };
  const load = () => {
    if (!rt) return;
    try {
      if (mine === "js") rt.load(evaluateScene(code, fileName()));
      else rt.load(code, name);
      live = Boolean(rt.retainedLua && rt.input && rt.beginInputFrame && rt.advanceRuntime);
      events = []; consumed = true;
      canvas.classList.toggle("live", live);
      canvas.classList.toggle("input", live && /ctx:(pointer|key|action)\(/.test(code));
      host.classList.toggle("live", live);
      canvas.tabIndex = live ? 0 : -1;
      const [w, h] = rt.size();
      canvas.style.aspectRatio = `${w} / ${h}`;
      if (t > rt.duration) t = 0;
      if (err) err.hidden = true;
    } catch (e) {
      if (err) { err.textContent = String(e.message || e); err.hidden = false; }
      else console.error(e);
    }
  };
  const ensure = () => {
    if (rt || pending) return;
    pending = makeRuntime().then((r) => { pending = null; if (!shown) { r.destroy(); return; } rt = r; load(); })
      .catch((e) => {
        pending = null;
        err.textContent = "Could not load this figure. " + (e.message || e); err.hidden = false;
      });
  };

  const api = {
    visible(v) {
      shown = v; clearTimeout(releaseTimer);
      if (v) { ensure(); api.refresh?.(); }
      else releaseTimer = setTimeout(() => { if (!shown && rt) { rt.destroy(); rt = null; } }, 2000);
    },
    tick(now) {
      if (!rt || !shown) { last = now; return; }
      const d = rt.duration;
      if (playing && d > 0) {
        t += (now - (last || now)) / 1000;
        if (loop) { if (t >= d) t %= d; }  // seamless: the scene ends where it starts
        // others replay at once, the last frame dissolving over the restart instead of holding
        else if (t >= d) { if (onEnd?.()) return; t %= d; crossfade(); }
      }
      const dt = Math.min(0.1, (now - (last || now)) / 1000);
      last = now;
      if (live) {
        try {
          if (consumed) rt.beginInputFrame(rt.runtimeTime);
          for (const e of events.splice(0)) rt.input({x: 0, y: 0, dx: 0, dy: 0, wheelX: 0, wheelY: 0, button: -1,
            buttons, modifiers: 0, pointerId: 0, time: rt.runtimeTime, ...e});
          const r = rt.advanceRuntime(dt);
          consumed = !r || r.steps > 0;
        } catch (e) {
          live = false;
          if (err) { err.textContent = String(e.message || e); err.hidden = false; }
        }
      }
      try {
        if (canvas.clientWidth) drawFit(rt, canvas, Math.min(t, d), maxRatio);
      } catch { /* shown via load error */ }
      if (scrub) { scrub.max = d || 1; if (document.activeElement !== scrub) scrub.value = Math.min(t, d); }
      if (timeEl) timeEl.textContent = `${Math.min(t, d).toFixed(2)} / ${d.toFixed(2)}`;
      if (bar) bar.style.width = d > 0 ? `${Math.min(1, t / d) * 100}%` : "0";
    },
    retheme() { code = adapt(code); load(); },
    destroy() { langs.delete(api.switchLang); clearTimeout(releaseTimer); observer.unobserve(host); players.delete(api); rt?.destroy(); rt = null; shown = false; },
  };

  if (editable) {
    // long code would leave the canvas floating beside a tall editor: stack it below instead
    const lines = Math.max(...[source, js].filter(Boolean).map((c) => c.split("\n").length));
    if (lines >= STACK_LINES) {
      host.classList.add("stacked");
      $(".cm-wrap", host).style.height = `${Math.min(lines, 34) * 20 + 12}px`;
    }
    const cm = CodeMirror($(".cm-wrap", host), {value: code, mode: mine === "js" ? "javascript" : "lua", lineNumbers: true, indentUnit: 4, viewportMargin: Infinity});
    let debounce = 0;
    cm.on("change", () => { code = cm.getValue(); clearTimeout(debounce); debounce = setTimeout(() => { t = 0; setPlaying(true); load(); }, 220); });
    $(".reset", host).onclick = () => cm.setValue(adapt(originals[mine]));
    const showLang = () => {
      $(".file", host).textContent = `${fileName()} — live`;
      for (const b of host.querySelectorAll(".langs button")) b.classList.toggle("on", b.dataset.lang === mine);
    };
    const switchLang = () => {
      const next = js && lang === "js" ? "js" : "lua";
      if (next === mine) return;
      mine = next;
      cm.setOption("mode", mine === "js" ? "javascript" : "lua");
      cm.setValue(adapt(originals[mine]));
      showLang();
    };
    for (const b of host.querySelectorAll(".langs button")) b.onclick = () => setLang(b.dataset.lang);
    langs.add(switchLang);
    api.switchLang = switchLang;
    showLang();
    pp.onclick = () => { if (rt && t >= rt.duration) t = 0; setPlaying(!playing); };
    scrub.oninput = () => { t = +scrub.value; setPlaying(false); };
    api.refresh = () => cm.refresh();
    api.retheme = () => cm.setValue(adapt(cm.getValue()));
  }
  // canvas -> logical scene pixels (top-left origin)
  const at = (e) => {
    const box = canvas.getBoundingClientRect(), [w, h] = rt ? rt.size() : [960, 540];
    return {x: (e.clientX - box.left) * w / box.width, y: (e.clientY - box.top) * h / box.height};
  };
  const KEYS = new Set(["ArrowLeft", "ArrowRight", "ArrowUp", "ArrowDown", " ", "Enter", "Escape", "Shift", "Tab", "+", "-"]);
  const keyOf = (e) => KEYS.has(e.key) ? e.key : /^[a-z0-9]$/i.test(e.key) ? e.key.toUpperCase() : null;
  canvas.addEventListener("pointerdown", (e) => {
    if (!live) return;
    canvas.focus({preventScroll: true});
    canvas.setPointerCapture(e.pointerId);
    buttons = e.buttons;
    events.push({type: "pointerdown", ...at(e), button: e.button});
  });
  canvas.addEventListener("pointermove", (e) => { if (live) events.push({type: "pointermove", ...at(e)}); });
  canvas.addEventListener("pointerup", (e) => {
    if (!live) return;
    buttons = e.buttons;
    events.push({type: "pointerup", ...at(e), button: e.button});
  });
  canvas.addEventListener("pointerleave", (e) => { if (live && !e.buttons) events.push({type: "pointermove", x: -1, y: -1}); });
  for (const type of ["keydown", "keyup"]) {
    canvas.addEventListener(type, (e) => {
      const key = keyOf(e);
      if (!live || !key || e.metaKey || e.ctrlKey || e.altKey) return;
      e.preventDefault();
      if (type === "keydown" && e.repeat) return;
      events.push({type, key});
    });
  }
  canvas.addEventListener("blur", () => { if (live && rt) { events = []; buttons = 0; try { rt.releaseInput(rt.runtimeTime); } catch { /* torn down */ } } });
  host.__player = api;
  players.add(api);
  observer.observe(host);
  return api;
}

(function loop(now) {
  for (const p of players) p.tick(now);
  requestAnimationFrame(loop);
})(0);

export function destroyAll() {
  tabListeners.clear();
  for (const p of [...players]) p.destroy();
}
