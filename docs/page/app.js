// Hash router: each route renders one compiled MDX page into #app.
import * as components from "./components.js";
import {DOCS, API} from "./build/manifest.js";
import {createPlayer, destroyAll, retheme} from "./player.js";

const app = document.getElementById("app");
const mdx = (file) => import(`./build/${file}.js`);
let graphData, hostExamples;
const apiGraphData = () => graphData ??= fetch("./build/api/graph.json").then((response) => {
  if (!response.ok) throw new Error(`API graph (${response.status})`);
  return response.json();
});
const h = (tag, attrs = {}, ...children) => {
  const el = Object.assign(document.createElement(tag), attrs);
  el.append(...children);
  return el;
};

if (API.length) {
  const link = h("a", {href: "#/api", textContent: "API"});
  link.dataset.route = "api";
  document.querySelector('header nav [data-route="examples"]').before(link);
  for (const href of ["api-graph.css", "build/api/static/viscript/viewer.css"]) {
    document.head.append(h("link", {rel: "stylesheet", href}));
  }
}

function apiSidebar(pages, current) {
  const ancestors = new Set([current.id]);
  for (let page = current; page?.parent; page = pages.find((item) => item.id === page.parent)) ancestors.add(page.parent);
  const entries = (parent) => pages.filter((page) => (page.parent || null) === parent).map((page) => {
    const link = h("a", {href: `#/api/${page.id}`, textContent: page.title,
      className: page === current ? "on" : ancestors.has(page.id) ? "ancestor" : ""});
    if (page === current) link.setAttribute("aria-current", "page");
    const children = entries(page.id);
    return children.length ? h("details", {className: "api-branch", open: ancestors.has(page.id)},
      h("summary", {}, link), h("div", {className: "api-tree-children"}, ...children)) : link;
  });
  const tree = h("nav", {className: "api-tree", id: "api-tree"},
    h("h4", {textContent: "API reference"}), ...entries(null));
  tree.setAttribute("aria-label", "API reference pages");
  const toggle = h("button", {className: "api-nav-toggle", type: "button", textContent: `API / ${current.title}`});
  toggle.setAttribute("aria-controls", "api-tree");
  toggle.setAttribute("aria-expanded", "false");
  const graph = h("button", {className: "api-graph-launch", type: "button", textContent: "Graph"});
  graph.setAttribute("aria-label", "Open API graph");
  const side = h("aside", {className: "side api-side"}, h("div", {className: "api-nav-tools"}, toggle, graph), tree);
  toggle.onclick = () => toggle.setAttribute("aria-expanded", String(side.classList.toggle("is-open")));
  return side;
}

async function docsPage(id, route, pages) {
  const doc = pages.find((d) => d.id === id) ?? pages[0];
  let article;
  if (route === "api") {
    const response = await fetch(`./build/api/${doc.path}`);
    if (!response.ok) throw new Error(`API page ${doc.id} (${response.status})`);
    const template = document.createElement("template");
    template.innerHTML = await response.text();
    article = template.content.querySelector("article");
    if (doc.id === "overview") {
      const graph = h("section", {className: "api-graph-overview"});
      graph.dataset.graphMount = "global";
      article.querySelector("h1 + p").after(graph);
    }
  } else {
    const {default: Content} = await mdx(doc.file);
    article = h("article", {className: "prose"}, Content({components}));
  }
  const groups = [...new Set(pages.map((d) => d.group))];
  const side = route === "api" ? apiSidebar(pages, doc) : h("aside", {className: "side"}, ...groups.flatMap((g) => [h("h4", {textContent: g}),
    ...pages.filter((d) => d.group === g).map((d) => h("a", {href: `#/${route}/${d.id}`, textContent: d.title,
      className: [d === doc ? "on" : "", d.parent ? "child" : ""].join(" ").trim()}))]));
  const i = pages.indexOf(doc), prev = pages[i - 1], next = pages[i + 1];
  const pager = h("nav", {className: "pager"},
    prev ? h("a", {href: `#/${route}/${prev.id}`, textContent: `← ${prev.title}`}) : h("span"),
    next ? h("a", {href: `#/${route}/${next.id}`, textContent: `${next.title} →`}) : h("span"));
  article.append(pager);
  let rail = toc(article, doc.id, route);
  if (route === "api") {
    const graph = h("div");
    graph.dataset.graphMount = "local";
    rail = h("aside", {className: "api-rail"}, graph, rail);
  }
  return h("div", {className: route === "api" ? "docs api" : "docs"}, side, article, rail);
}

// Give every h2/h3 a stable id and a route link, and list them in the right-hand TOC.
const slug = (text) => text.toLowerCase().replace(/[^\p{L}\p{N}]+/gu, "-").replace(/^-|-$/g, "");
let tocObserver = null;
function toc(article, docId, route) {
  const used = new Map();
  const links = [...article.querySelectorAll("h2, h3")]
    .filter((heading) => !heading.nextElementSibling?.matches("[data-viscript]") &&
      !heading.nextElementSibling?.querySelector("[data-viscript]"))
    .map((heading) => {
      const base = route === "api" && heading.id ? heading.id : slug(heading.textContent) || "section";
      const n = used.get(base) ?? 0;
      used.set(base, n + 1);
      heading.id = n ? `${base}-${n}` : base;
      const href = `#/${route}/${docId}/${heading.id}`;
      heading.append(h("a", {className: "anchor", href, ariaLabel: `Link to ${heading.textContent}`, textContent: "#"}));
      return h("a", {href, className: heading.tagName === "H3" ? "sub" : "", textContent: heading.firstChild.textContent});
    });
  const nav = h("nav", {className: "toc"}, ...(links.length ? [h("h4", {textContent: "On this page"}), ...links] : []));
  return nav;
}

// Observe only the accepted route, after its article is attached to the page.
function observeToc(view) {
  const links = [...view.querySelectorAll(".toc a")];
  tocObserver?.disconnect();
  tocObserver = new IntersectionObserver((entries) => {
    for (const e of entries) if (e.isIntersecting) {
      for (const a of links) a.classList.toggle("on", a.getAttribute("href").endsWith(`/${e.target.id}`));
    }
  }, {rootMargin: "-64px 0px -70% 0px"});
  view.querySelectorAll("article h2, article h3").forEach((heading) => tocObserver.observe(heading));
}

const scrollToHeading = (anchor) => {
  const el = anchor && document.getElementById(anchor);
  if (el) el.scrollIntoView({block: "start"});
  else scrollTo(0, 0);
};

let shownDoc = null, apiCleanup = null, generation = 0;
async function render() {
  const [route = "", id, anchor] = location.hash.replace(/^#\/?/, "").split("/");
  if (route === "api" && !API.length) {
    location.replace("#/");
    return;
  }
  const pages = route === "api" ? API : route === "docs" ? DOCS : null;
  const doc = pages && (pages.find((d) => d.id === id) ?? pages[0]);
  const docKey = doc && `${route}/${doc.id}`;
  // a heading link inside the open doc only scrolls; players keep running
  if (docKey && shownDoc === docKey) return scrollToHeading(anchor);
  const mine = ++generation;
  apiCleanup?.();
  apiCleanup = null;
  destroyAll();
  hostExamples?.destroyHostExamples();
  tocObserver?.disconnect();
  shownDoc = docKey;
  let view;
  if (pages) view = await docsPage(id, route, pages);
  else {
    const {default: Content} = await mdx(route === "examples" ? "examples" : "index");
    view = h("div", {className: route === "examples" ? "page prose wide" : "page home"}, Content({components}));
  }
  if (mine !== generation) return;
  app.replaceChildren(view);
  observeToc(view);
  if (route === "api") {
    const [{mountApiExamples}, {mountApiGraph}, graph, host] = await Promise.all([
      import("./build/api/static/viscript/viewer.js"), import("./api-graph.js"), apiGraphData(),
      import("./host-example.js"),
    ]);
    if (mine !== generation) return;
    hostExamples = host;
    const cleanups = [mountApiExamples(view, {createPlayer, createHostExample: host.createHostExample})];
    for (const container of view.querySelectorAll("[data-graph-mount]")) {
      cleanups.push(mountApiGraph(container, graph, {current: doc.id, global: container.dataset.graphMount === "global"}));
    }
    view.querySelector(".api-graph-launch").onclick = () =>
      view.querySelector('[data-graph-mount="local"] [data-graph-expand]').click();
    apiCleanup = () => { for (const cleanup of cleanups) cleanup(); };
  }
  for (const a of document.querySelectorAll("header nav a")) a.classList.toggle("active", a.dataset.route === (route || "home"));
  if (pages) scrollToHeading(anchor);
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
