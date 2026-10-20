// Quartz supplies normal highlighted code fences. The main site's player is
// injected here so API examples share its worker, WASM runtime, fonts and theme.
const node = (tag, attrs = {}) => Object.assign(document.createElement(tag), attrs);

export function mountApiExamples(container = document, {createPlayer, createHostExample} = {}) {
  const examples = new Map();
  let anchor = null;
  const hidden = [];
  for (const marker of container.querySelectorAll("[data-viscript]")) {
    const code = marker.matches("code") ? marker : marker.querySelector("code");
    if (!code) continue;
    const id = marker.dataset.apiExample;
    if (!id) continue;
    const block = marker.matches("code") ? code.closest("figure") ?? code.closest("pre") : marker;
    if (!block) continue;
    let example = examples.get(id);
    if (!example) {
      example = {id, label: marker.dataset.exampleLabel || id, sources: {}, interactive: false};
      examples.set(id, example);
    }
    example.sources[marker.dataset.viscript] = marker.dataset.source ?? code.textContent;
    example.interactive ||= marker.dataset.interactive === "true";
    // Example headings remain in the Markdown vault; the published view uses a
    // compact selector, with one canvas and source editor for the whole topic.
    const heading = block.previousElementSibling;
    if (heading?.matches("h3")) {
      anchor ??= heading;
      hidden.push({element: heading, wasHidden: heading.hidden});
      heading.hidden = true;
    }
    anchor ??= block;
    hidden.push({element: block, wasHidden: block.hidden});
    block.hidden = true;
  }
  if (!examples.size) return () => {};

  const section = node("section", {className: "viscript-showcase"});
  const header = node("div", {className: "viscript-showcase-header"});
  const select = node("select", {ariaLabel: "Choose API example"});
  for (const example of examples.values()) select.append(node("option", {value: example.id, textContent: example.label}));
  const choice = node("label", {className: "viscript-showcase-choice"});
  choice.append(node("span", {textContent: "Try"}), select);
  header.append(node("span", {className: "viscript-showcase-caption", textContent: "Live example"}), choice);
  const stage = node("div", {className: "viscript-showcase-stage"});
  section.append(header, stage);
  anchor.before(section);

  let player = null, started = false, destroyed = false;
  const show = () => {
    if (destroyed || !stage.isConnected) return;
    started = true;
    player?.destroy();
    player = null;
    stage.replaceChildren();
    stage.className = "viscript-showcase-stage";
    const example = examples.get(select.value);
    stage.dataset.example = example.id;
    if (example.sources.host) {
      if (createHostExample) player = createHostExample(stage, {id: example.id, hostCode: example.sources.host});
    } else if (createPlayer) {
      player = createPlayer(stage, example.sources.lua ?? example.sources.js, {
        js: example.sources.lua ? example.sources.js ?? null : null,
        language: example.sources.lua ? "lua" : "js",
        editable: true,
        interactive: example.interactive,
        name: `${example.id.replaceAll(".", "-")}.${example.sources.lua ? "lua" : "js"}`,
      });
    }
  };
  select.onchange = show;
  // A topic's API table can be long. Wait until its example approaches the
  // viewport before asking the shared runtime to load the first scene.
  const observer = new IntersectionObserver((entries) => {
    if (entries.some((entry) => entry.isIntersecting)) { observer.disconnect(); show(); }
  }, {rootMargin: "320px"});
  observer.observe(section);

  return () => {
    if (destroyed) return;
    destroyed = true;
    observer.disconnect();
    if (started) player?.destroy();
    for (const {element, wasHidden} of hidden) element.hidden = wasHidden;
    section.remove();
  };
}
