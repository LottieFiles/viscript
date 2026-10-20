import {HOST_SETUP, RETAINED_SOURCE, toneBytes} from "./build/api/static/viscript/host-setup.js";
import {onSiteTheme, siteTheme} from "./site-theme.js";

const widgets = new Set();
// A switch between the site's light and dark themes runs every example again, so the scenes it
// loads take the new colours.
onSiteTheme(() => { for (const widget of widgets) widget.retheme(); });
// createVis's test for an options object, as the runtime's client makes it.
const plain = (value) => {
  const prototype = value ? Object.getPrototypeOf(value) : undefined;
  return prototype === Object.prototype || prototype === null;
};
let environment;
const node = (tag, attrs = {}) => Object.assign(document.createElement(tag), attrs);
async function sharedEnvironment() {
  if (!environment) environment = (async () => {
    const client = await import("./runtime/client.js");
    const module = await client.createVisModule();
    const bytes = async (path) => new Uint8Array(await (await fetch(new URL(path, import.meta.url))).arrayBuffer());
    const [fontBytes, imageBytes] = await Promise.all([bytes("runtime/PretendardStd-Regular.ttf"), bytes("assets/earth.png")]);
    const anchor = await client.createVis({module});
    anchor.font("Pretendard", fontBytes, "ttf");
    return {client, module, fontBytes, imageBytes, anchor};
  })();
  return environment;
}

function definition(vis) {
  const scene = vis.scene({width: 640, height: 360, camera: {mode: "interactive"}});
  const dot = scene.circle({id: "dot", center: [-1, 0], radius: 0.6, fill: "accent"});
  scene.rectangle({id: "box", center: [1.3, 0], size: [2, 2], stroke: "foreground", fill: "#00000000"});
  scene.shift(dot, [1.5, 0], 1);
  return scene;
}

export function createHostExample(container, example) {
  container.classList.add("host-example");
  const canvas = node("canvas", {width: 640, height: 360, ariaLabel: `${example.id} result`});
  const setup = node("details");
  setup.append(node("summary", {textContent: "Shared setup and scene source"}));
  setup.append(node("pre", {textContent: HOST_SETUP +
    (example.hostCode.includes("retainedSource") ? `\n\nconst retainedSource = ${JSON.stringify(RETAINED_SOURCE)};` : "")}));
  const code = node("div", {className: "host-code"});
  const run = node("button", {className: "btn", textContent: "Run example", type: "button"});
  const reset = node("button", {className: "btn", textContent: "Reset", type: "button"});
  const controls = node("div", {className: "host-controls"});
  controls.append(run, reset);
  const output = node("output", {ariaLive: "polite"});
  container.append(canvas, controls, code, output, setup);
  const editor = CodeMirror(code, {value: example.hostCode, mode: "javascript", lineNumbers: true, indentUnit: 4});
  const engines = new Set();
  let generation = 0, disposed = false;
  const cleanup = () => { for (const engine of engines) engine.destroy(); engines.clear(); };
  const write = (value) => {
    output.textContent += (typeof value === "string" ? value : JSON.stringify(value, null, 2)) + "\n";
  };
  const execute = async (preview = false) => {
    const current = ++generation;
    cleanup();
    run.disabled = true;
    output.textContent = "Loading…";
    try {
      const {client, module, fontBytes, imageBytes} = await sharedEnvironment();
      if (disposed || current !== generation) return;
      // Each engine takes the site's theme before its first scene, so a scene that names no theme
      // draws in the site's colours, as the players' scenes do. A scene given here loads after it.
      const createVis = async (...args) => {
        const [scene, name, options] = plain(args[0]) ? [undefined, undefined, args[0]] : args;
        const engine = await client.createVis(options ?? {});
        if (disposed || current !== generation) { engine.destroy(); throw new Error("Example closed."); }
        engines.add(engine);
        engine.hostTheme(siteTheme());
        if (scene !== undefined) {
          try {
            engine.load(scene, name);
          } catch (error) {
            engines.delete(engine);
            engine.destroy();
            throw error;
          }
        }
        return engine;
      };
      const source = definition(client.vis);
      output.textContent = "";
      if (preview) {
        const engine = await createVis(source, "preview.js", {module});
        engine.draw(canvas, 0.5);
        write("Press Run example to execute the audio API.");
      } else {
        const values = {...client, module, source, canvas, write, fontBytes, imageBytes,
          toneBytes: toneBytes(), retainedSource: RETAINED_SOURCE, createVis};
        const AsyncFunction = Object.getPrototypeOf(async function() {}).constructor;
        await new AsyncFunction(...Object.keys(values), editor.getValue())(...Object.values(values));
        if (!output.textContent) write("Example completed.");
      }
    } catch (error) {
      if (!disposed && current === generation) { output.dataset.error = "true"; output.textContent = error.message; }
    } finally {
      if (!disposed && current === generation) run.disabled = false;
    }
  };
  run.onclick = () => { delete output.dataset.error; execute(); };
  reset.onclick = () => { editor.setValue(example.hostCode); delete output.dataset.error; execute(example.id.startsWith("host.audio.")); };
  const widget = {
    destroy() { disposed = true; generation++; cleanup(); widgets.delete(widget); },
    retheme() { execute(example.id.startsWith("host.audio.")); },
  };
  widgets.add(widget);
  queueMicrotask(() => { editor.refresh(); execute(example.id.startsWith("host.audio.")); });
  return widget;
}

export function destroyHostExamples() {
  for (const widget of [...widgets]) widget.destroy();
}
