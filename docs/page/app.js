// Hash router: each route renders one compiled MDX page into #app.
import * as components from "./components.js";
import {DOCS} from "./build/manifest.js";
import {destroyAll, retheme} from "./player.js";

const app = document.getElementById("app");
const mdx = (file) => import(`./build/${file}.js`);
const h = (tag, attrs = {}, ...children) => {
  const el = Object.assign(document.createElement(tag), attrs);
  el.append(...children);
  return el;
};

async function docsPage(id) {
  const doc = DOCS.find((d) => d.id === id) ?? DOCS[0];
  const {default: Content} = await mdx(doc.file);
  const groups = [...new Set(DOCS.map((d) => d.group))];
  const side = h("aside", {className: "side"}, ...groups.flatMap((g) => [h("h4", {textContent: g}),
    ...DOCS.filter((d) => d.group === g).map((d) => h("a", {href: `#/docs/${d.id}`, textContent: d.title,
      className: [d === doc ? "on" : "", d.parent ? "child" : ""].join(" ").trim()}))]));
  const i = DOCS.indexOf(doc), prev = DOCS[i - 1], next = DOCS[i + 1];
  const pager = h("nav", {className: "pager"},
    prev ? h("a", {href: `#/docs/${prev.id}`, textContent: `← ${prev.title}`}) : h("span"),
    next ? h("a", {href: `#/docs/${next.id}`, textContent: `${next.title} →`}) : h("span"));
  const article = h("article", {className: "prose"}, Content({components}), pager);
  return h("div", {className: "docs"}, side, article, toc(article, doc.id));
}

// Give every h2/h3 a stable id and a link (#/docs/<doc>/<slug>), and list them in the right-hand TOC.
const slug = (text) => text.toLowerCase().replace(/[^\p{L}\p{N}]+/gu, "-").replace(/^-|-$/g, "");
let tocObserver = null;
function toc(article, docId) {
  const used = new Map();
  const links = [...article.querySelectorAll("h2, h3")].map((heading) => {
    const base = slug(heading.textContent) || "section";
    const n = used.get(base) ?? 0;
    used.set(base, n + 1);
    heading.id = n ? `${base}-${n}` : base;
    const href = `#/docs/${docId}/${heading.id}`;
    heading.append(h("a", {className: "anchor", href, ariaLabel: `Link to ${heading.textContent}`, textContent: "#"}));
    return h("a", {href, className: heading.tagName === "H3" ? "sub" : "", textContent: heading.firstChild.textContent});
  });
  const nav = h("nav", {className: "toc"}, ...(links.length ? [h("h4", {textContent: "On this page"}), ...links] : []));
  tocObserver?.disconnect();
  tocObserver = new IntersectionObserver((entries) => {
    for (const e of entries) if (e.isIntersecting) {
      for (const a of links) a.classList.toggle("on", a.getAttribute("href").endsWith(`/${e.target.id}`));
    }
  }, {rootMargin: "-64px 0px -70% 0px"});
  queueMicrotask(() => article.querySelectorAll("h2, h3").forEach((x) => tocObserver.observe(x)));
  return nav;
}

const scrollToHeading = (anchor) => {
  const el = anchor && document.getElementById(anchor);
  if (el) el.scrollIntoView({block: "start"});
  else scrollTo(0, 0);
};

let shownDoc = null;
async function render() {
  const [route = "", id, anchor] = location.hash.replace(/^#\/?/, "").split("/");
  // a heading link inside the open doc only scrolls; players keep running
  if (route === "docs" && shownDoc && (id ?? DOCS[0].id) === shownDoc) return scrollToHeading(anchor);
  destroyAll();
  shownDoc = route === "docs" ? (DOCS.find((d) => d.id === id) ?? DOCS[0]).id : null;
  let view;
  if (route === "docs") view = await docsPage(id);
  else {
    const {default: Content} = await mdx(route === "examples" ? "examples" : "index");
    view = h("div", {className: route === "examples" ? "page prose wide" : "page home"}, Content({components}));
  }
  app.replaceChildren(view);
  for (const a of document.querySelectorAll("header nav a")) a.classList.toggle("active", a.dataset.route === (route || "home"));
  if (route === "docs") scrollToHeading(anchor);
  else if (!(route === "examples" && id)) scrollTo(0, 0);
}

addEventListener("hashchange", render);
render();

document.getElementById("theme-toggle").onclick = () => {
  const next = document.documentElement.dataset.theme === "light" ? "dark" : "light";
  document.documentElement.dataset.theme = next;
  try { localStorage.setItem("viscript-theme", next); } catch { /* private mode */ }
  retheme();
};
