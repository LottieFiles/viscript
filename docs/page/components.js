// Components available to every MDX page.
import {jsx} from "./jsx-runtime.js";
import {createPlayer, getLang, onLang, setLang} from "./player.js";
import {HERO} from "./scenes/hero.js";
import {SKILLS} from "./scenes/skills.js";
import {INTERACTIVE} from "./scenes/interactive.js";
import {COLLECTIONS} from "./scenes/collections.js";

const h = (type, props, ...children) => jsx(type, {...props, children});

export function Scene({code, js = null, editable = false, loop = false, interactive = false, name = "scene.lua"}) {
  const host = h("div", {className: "scene"});
  createPlayer(host, code, {js, editable, loop, interactive, name});
  return host;
}

export const Hero = ({children}) => h("section", {className: "hero"},
  h("div", {className: "hero-copy"}, children,
    h("div", {className: "cta"},
      h("a", {className: "btn solid", href: "#/docs"}, "Get started"),
      h("a", {className: "btn", href: "#/examples"}, "Examples"))),
  h("div", {className: "hero-stage"}, Scene({code: HERO, name: "hero.lua"})));

export const Feature = ({title, children}) => h("section", {className: "home-section"}, h("h2", {}, title), children);
export const BubbleSortDemo = () => Scene({code: INTERACTIVE.episodes.find((e) => e.id === "sort-stepper").code,
  interactive: true, name: "bubble-sort.lua"});
export const SkillsDemo = () => Scene({code: SKILLS, editable: true, name: "skills.lua"});

export function Gallery() {
  const episodes = COLLECTIONS.flatMap((c) => c.episodes.map((e) => ({...e, group: c.title})));
  const stage = h("div", {className: "gallery-stage"});
  const info = h("div", {className: "gallery-info"});
  const buttons = episodes.map((e) => h("button", {onClick: () => show(e)}, e.title));
  const list = h("nav", {className: "gallery-list", "aria-label": "Example collections"}, COLLECTIONS.map((c) => [h("h4", {}, c.title),
    buttons.filter((_, i) => episodes[i].group === c.title)]));
  let player = null, request = 0;
  const show = async (e) => {
    const current = ++request;
    player?.destroy();
    player = null;
    buttons.forEach((b, i) => b.classList.toggle("on", episodes[i] === e));
    const selected = buttons[episodes.indexOf(e)];
    if (selected.offsetTop < list.scrollTop || selected.offsetTop + selected.offsetHeight > list.scrollTop + list.clientHeight) {
      list.scrollTop = Math.max(0, selected.offsetTop - list.clientHeight / 2);
    }
    info.replaceChildren(h("h2", {}, e.title), h("p", {}, e.blurb ?? ""));
    stage.className = "gallery-stage";
    stage.replaceChildren();
    history.replaceState(null, "", `#/examples/${e.id}`);
    try {
      let code = e.code;
      if (e.src) {
        stage.append(h("p", {role: "status"}, "Loading example…"));
        const response = await fetch(e.src);
        if (!response.ok) throw new Error(`Could not load ${e.title} (${response.status})`);
        code = await response.text();
      }
      if (current !== request || !stage.isConnected) return;
      stage.replaceChildren();
      player = createPlayer(stage, code, {js: e.js ?? null, editable: true, loop: !!e.loop,
        interactive: !!e.interactive, clips: e.clips, name: `${e.id}.lua`});
    } catch (error) {
      if (current === request && stage.isConnected) stage.replaceChildren(h("pre", {className: "err", role: "alert"}, error.message));
    }
  };
  const want = location.hash.split("/")[2];
  queueMicrotask(() => show(episodes.find((e) => e.id === want) ?? episodes[0]));
  return h("div", {className: "gallery"},
    list,
    h("div", {className: "gallery-main"}, info, stage));
}

// Adjacent ```lang tab=JS``` / ```lang tab=Lua``` fences become one block with a shared language switch.
export const Tab = ({label, children}) => h("div", {className: "tab", "data-tab": label}, children);
export function Tabs({children}) {
  const panes = [children].flat(Infinity).filter((c) => c?.dataset?.tab);
  const buttons = panes.map((p) => h("button", {onClick: () => setLang(p.dataset.tab.toLowerCase())}, p.dataset.tab));
  const show = (lang) => {
    const pick = panes.find((p) => p.dataset.tab.toLowerCase() === lang) ?? panes[0];
    panes.forEach((p, i) => { p.hidden = p !== pick; buttons[i].classList.toggle("on", p === pick); });
  };
  show(getLang());
  onLang(show);
  return h("div", {className: "tabs"}, h("div", {className: "tab-bar"}, buttons), panes);
}
