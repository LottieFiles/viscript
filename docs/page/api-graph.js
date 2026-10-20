const NS = "http://www.w3.org/2000/svg";
const element = (tag, attrs = {}, text = "") => {
  const node = document.createElement(tag);
  Object.assign(node, attrs);
  if (text) node.textContent = text;
  return node;
};
const svgElement = (tag, attrs = {}) => {
  const node = document.createElementNS(NS, tag);
  for (const [key, value] of Object.entries(attrs)) node.setAttribute(key, value);
  return node;
};

/** A documentation graph built from the vault's links and page hierarchy. */
export function mountApiGraph(container, data, {current, global = false, modal = false} = {}) {
  const neighbors = new Set([current]);
  for (const edge of data.links) {
    if (edge.source === current) neighbors.add(edge.target);
    if (edge.target === current) neighbors.add(edge.source);
  }
  const nodes = data.nodes.filter((node) => global || neighbors.has(node.id)).map((node) => ({...node}));
  const byId = new Map(nodes.map((node) => [node.id, node]));
  const links = data.links.filter((edge) => byId.has(edge.source) && byId.has(edge.target)).map((edge) => ({...edge}));
  const connections = new Map(nodes.map((node) => [node.id, new Set([node.id])]));
  for (const edge of links) {
    connections.get(edge.source).add(edge.target);
    connections.get(edge.target).add(edge.source);
  }
  const root = element("section", {className: `api-graph${global ? " api-graph-global" : ""}`});
  root.dataset.apiGraph = "";
  root.dataset.graphMode = global ? "global" : "local";
  root.setAttribute("aria-label", global ? "API graph view" : "Related API pages");
  const header = element("div", {className: "api-graph-header"});
  const heading = element("span", {className: "api-graph-title"}, global ? "Graph view" : "Local graph");
  const count = element("span", {className: "api-graph-count"}, `${nodes.length} pages`);
  const expand = element("button", {type: "button", className: "api-graph-expand", title: "Open full API graph"}, "↗");
  expand.dataset.graphExpand = "";
  expand.setAttribute("aria-label", "Open full API graph");
  header.append(heading, count);
  if (!modal) header.append(expand);
  const stage = element("div", {className: "api-graph-stage"});
  const svg = svgElement("svg", {role: "group", "aria-label": "API pages and their connections. Select a page to open it."});
  const world = svgElement("g");
  const edgeLayer = svgElement("g", {"aria-hidden": "true"});
  const nodeLayer = svgElement("g");
  world.append(edgeLayer, nodeLayer);
  svg.append(world);
  stage.append(svg);
  const controls = element("div", {className: "api-graph-controls"});
  const button = (label, text, action) => {
    const control = element("button", {type: "button", title: label}, text);
    control.setAttribute("aria-label", label);
    control.addEventListener("click", action);
    controls.append(control);
  };
  let width = 0, height = 0, transform = {x: 0, y: 0, k: 1}, drag = null, moved = false, expandedCleanup = null;
  const applyTransform = () => world.setAttribute("transform", `translate(${transform.x} ${transform.y}) scale(${transform.k})`);
  const zoom = (factor, x = width / 2, y = height / 2) => {
    const next = Math.max(.4, Math.min(4, transform.k * factor));
    const ratio = next / transform.k;
    transform = {x: x - (x - transform.x) * ratio, y: y - (y - transform.y) * ratio, k: next};
    applyTransform();
  };
  const draw = () => {
    for (const edge of links) {
      const a = byId.get(edge.source), b = byId.get(edge.target);
      edge.el.setAttribute("x1", a.x); edge.el.setAttribute("y1", a.y);
      edge.el.setAttribute("x2", b.x); edge.el.setAttribute("y2", b.y);
    }
    for (const node of nodes) node.el.setAttribute("transform", `translate(${node.x} ${node.y})`);
  };
  const reset = () => {
    transform = {x: 0, y: 0, k: 1};
    applyTransform();
  };
  button("Zoom in", "+", () => zoom(1.25));
  button("Zoom out", "−", () => zoom(.8));
  button("Reset graph view", "↺", reset);
  stage.append(controls);
  if (global) stage.append(element("span", {className: "api-graph-hint"}, "Drag to explore · Scroll to zoom"));
  root.append(header, stage);
  container.append(root);

  for (const edge of links) {
    edge.el = svgElement("line", {class: `api-graph-edge api-graph-edge-${edge.kind}`});
    edgeLayer.append(edge.el);
  }
  const highlight = (id) => {
    const connected = connections.get(id);
    for (const node of nodes) {
      node.el.classList.toggle("is-dim", !!connected && !connected.has(node.id));
      node.el.classList.toggle("is-highlighted", node.id === id);
    }
    for (const edge of links) {
      edge.el.classList.toggle("is-dim", !!id && edge.source !== id && edge.target !== id);
      edge.el.classList.toggle("is-highlighted", edge.source === id || edge.target === id);
    }
  };
  for (const node of nodes) {
    const category = !node.parent || node.parent === "overview";
    node.el = svgElement("a", {href: `#/api/${node.id}`, class: `api-graph-node${node.id === current ? " is-current" : ""}${category ? " is-category" : ""}`,
      "data-node-id": node.id, "aria-label": `${node.title}${node.id === current ? ", current page" : ""}`});
    if (node.id === current) node.el.setAttribute("aria-current", "page");
    const title = svgElement("title");
    title.textContent = node.title;
    const label = svgElement("text", {y: category ? 19 : 17, "text-anchor": "middle", "aria-hidden": "true"});
    label.textContent = node.title;
    node.el.append(title, svgElement("circle", {class: "api-graph-hit", r: 14}),
      svgElement("circle", {class: "api-graph-dot", r: node.id === current ? 6 : category ? 5 : 3.6}), label);
    if (!global && nodes.length <= 6) node.el.classList.add("has-label");
    node.el.addEventListener("pointerenter", () => { if (!drag) highlight(node.id); });
    node.el.addEventListener("pointerleave", () => { if (!drag) highlight(null); });
    node.el.addEventListener("focus", () => highlight(node.id));
    node.el.addEventListener("blur", () => highlight(null));
    nodeLayer.append(node.el);
  }

  // Category anchors keep the map legible; link forces reveal cross-category references.
  const layout = () => {
    const bounds = stage.getBoundingClientRect();
    const nextWidth = Math.max(180, bounds.width), nextHeight = Math.max(160, bounds.height);
    if (nextWidth === width && nextHeight === height) return;
    width = nextWidth; height = nextHeight;
    svg.setAttribute("viewBox", `0 0 ${width} ${height}`);
    const categories = [...new Set(nodes.filter((node) => node.id !== "overview").map((node) => node.category || node.parent || node.id))];
    const radiusX = Math.max(45, width * .33 - 28), radiusY = Math.max(38, height * .32 - 16);
    const groups = new Map();
    for (const node of nodes) {
      const category = node.category || node.parent || node.id;
      const index = categories.indexOf(category), angle = index * Math.PI * 2 / Math.max(1, categories.length) - Math.PI / 2;
      const center = !global && node.id === current || node.id === "overview";
      node.ax = center ? width / 2 : width / 2 + Math.cos(angle) * radiusX;
      node.ay = center ? height / 2 : height / 2 + Math.sin(angle) * radiusY;
      const ordinal = groups.get(category) || 0;
      groups.set(category, ordinal + 1);
      const spread = global ? 32 : Math.min(width, height) * .24;
      node.x = node.ax + (center ? 0 : Math.cos(ordinal * 2.399) * spread);
      node.y = node.ay + (center ? 0 : Math.sin(ordinal * 2.399) * spread);
    }
    if (!global) nodes.filter((node) => node.id !== current).forEach((node, i) => {
      const angle = i * Math.PI * 2 / Math.max(1, nodes.length - 1) - Math.PI / 2;
      node.ax = node.x = width / 2 + Math.cos(angle) * radiusX;
      node.ay = node.y = height / 2 + Math.sin(angle) * radiusY;
    });
    for (let step = 0; step < 160; step++) {
      for (const node of nodes) { node.fx = (node.ax - node.x) * .06; node.fy = (node.ay - node.y) * .06; }
      for (let i = 0; i < nodes.length; i++) for (let j = i + 1; j < nodes.length; j++) {
        const a = nodes[i], b = nodes[j], dx = a.x - b.x || .1, dy = a.y - b.y || .1;
        const distance = Math.max(20, Math.hypot(dx, dy));
        const force = Math.min(3, (global ? 650 : 900) / (distance * distance));
        a.fx += dx / distance * force; a.fy += dy / distance * force;
        b.fx -= dx / distance * force; b.fy -= dy / distance * force;
      }
      for (const edge of links) {
        const a = byId.get(edge.source), b = byId.get(edge.target), dx = b.x - a.x, dy = b.y - a.y;
        const force = edge.kind === "parent" ? .004 : .001;
        a.fx += dx * force; a.fy += dy * force; b.fx -= dx * force; b.fy -= dy * force;
      }
      for (const node of nodes) {
        node.x = Math.max(22, Math.min(width - 22, node.x + node.fx));
        node.y = Math.max(25, Math.min(height - 36, node.y + node.fy));
      }
    }
    reset(); draw();
  };
  const point = (event) => {
    const box = svg.getBoundingClientRect();
    return {x: event.clientX - box.left, y: event.clientY - box.top};
  };
  const onDown = (event) => {
    if (event.button !== 0) return;
    const p = point(event), node = byId.get(event.target.closest("[data-node-id]")?.dataset.nodeId);
    drag = {id: event.pointerId, node, x: p.x, y: p.y, tx: transform.x, ty: transform.y};
    moved = false;
  };
  const onMove = (event) => {
    if (!drag || drag.id !== event.pointerId) return;
    const p = point(event), dx = p.x - drag.x, dy = p.y - drag.y;
    if (!moved && Math.hypot(dx, dy) < 4) return;
    if (!moved) svg.setPointerCapture(event.pointerId);
    moved = true;
    root.classList.add("is-dragging");
    if (drag.node) {
      drag.node.x = (p.x - transform.x) / transform.k;
      drag.node.y = (p.y - transform.y) / transform.k;
      draw();
    } else {
      transform.x = drag.tx + dx; transform.y = drag.ty + dy; applyTransform();
    }
  };
  const onUp = () => { drag = null; root.classList.remove("is-dragging"); highlight(null); };
  const onClick = (event) => { if (moved) { event.preventDefault(); event.stopPropagation(); moved = false; } };
  const onWheel = (event) => {
    event.preventDefault();
    const p = point(event);
    zoom(Math.exp(-event.deltaY * .002), p.x, p.y);
  };
  svg.addEventListener("pointerdown", onDown);
  svg.addEventListener("pointermove", onMove);
  svg.addEventListener("pointerup", onUp);
  svg.addEventListener("pointercancel", onUp);
  svg.addEventListener("click", onClick, true);
  svg.addEventListener("wheel", onWheel, {passive: false});

  expand.addEventListener("click", () => {
    if (expandedCleanup) return;
    const returnFocus = document.activeElement;
    const dialog = element("dialog", {className: "api-graph-modal"});
    dialog.setAttribute("aria-label", "All API pages");
    const close = element("button", {type: "button", className: "api-graph-close", title: "Close graph"}, "×");
    close.setAttribute("aria-label", "Close graph");
    dialog.append(close);
    document.body.append(dialog);
    dialog.showModal();
    const unmount = mountApiGraph(dialog, data, {current, global: true, modal: true});
    expandedCleanup = () => {
      unmount(); dialog.remove(); expandedCleanup = null;
      if (returnFocus?.isConnected && returnFocus.getClientRects().length) returnFocus.focus();
      else if (expand.isConnected && expand.getClientRects().length) expand.focus();
    };
    dialog.addEventListener("close", () => expandedCleanup?.(), {once: true});
    dialog.addEventListener("click", (event) => {
      if (event.target === dialog || event.target.closest("[data-node-id]")) dialog.close();
    });
    close.addEventListener("click", () => dialog.close());
    close.focus();
  });
  const observer = new ResizeObserver(layout);
  observer.observe(stage);
  layout();
  return () => {
    observer.disconnect();
    expandedCleanup?.();
    root.remove();
  };
}
