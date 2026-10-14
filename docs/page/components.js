// Components available to every MDX page.
import {jsx} from "./jsx-runtime.js";
import {createPlayer, getLang, onLang, setLang} from "./player.js";
import {HERO} from "./scenes/hero.js";
import {COLLECTIONS} from "./scenes/collections.js";

const h = (type, props, ...children) => jsx(type, {...props, children});

export function Scene({code, js = null, editable = false, loop = false, name = "scene.lua"}) {
  const host = h("div", {className: "scene"});
  createPlayer(host, code, {js, editable, loop, name});
  return host;
}

export const Hero = ({children}) => h("section", {className: "hero"},
  h("div", {className: "hero-copy"}, children,
    h("div", {className: "cta"},
      h("a", {className: "btn solid", href: "#/docs"}, "Get started"),
      h("a", {className: "btn", href: "#/examples"}, "Examples"))),
  h("div", {className: "hero-stage"}, Scene({code: HERO, loop: true, name: "hero.lua"})));

export const Features = ({children}) => h("div", {className: "features"}, children);
export const Feature = ({title, children}) => h("div", {className: "feature"}, h("h3", {}, title), children);

export function Gallery() {
  const episodes = COLLECTIONS.flatMap((c) => c.episodes.map((e) => ({...e, group: c.title})));
  const stage = h("div", {className: "gallery-stage"});
  const info = h("div", {className: "gallery-info"});
  const buttons = episodes.map((e) => h("button", {onClick: () => show(e)}, e.title));
  let player = null;
  const show = (e) => {
    player?.destroy();
    buttons.forEach((b, i) => b.classList.toggle("on", episodes[i] === e));
    info.replaceChildren(h("h2", {}, e.title), h("p", {}, e.blurb ?? ""));
    stage.replaceChildren();
    player = createPlayer(stage, e.code, {js: e.js ?? null, editable: true, loop: !!e.loop, name: `${e.id}.lua`});
    history.replaceState(null, "", `#/examples/${e.id}`);
  };
  const want = location.hash.split("/")[2];
  queueMicrotask(() => show(episodes.find((e) => e.id === want) ?? episodes[0]));
  return h("div", {className: "gallery"},
    h("nav", {className: "gallery-list"}, COLLECTIONS.map((c) => [h("h4", {}, c.title),
      buttons.filter((_, i) => episodes[i].group === c.title)])),
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
