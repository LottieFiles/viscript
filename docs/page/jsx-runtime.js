// The JSX runtime compiled MDX imports: elements are real DOM nodes.
export const Fragment = Symbol("Fragment");
const SVG = new Set(["svg", "path", "circle", "rect", "g", "line", "polyline"]);
const append = (parent, child) => {
  if (child == null || child === false || child === true) return;
  if (Array.isArray(child)) child.forEach((c) => append(parent, c));
  else parent.append(child instanceof Node ? child : String(child));
};
export function jsx(type, props) {
  const {children, ...rest} = props ?? {};
  if (typeof type === "function") return type(props ?? {});
  if (type === Fragment) { const f = document.createDocumentFragment(); append(f, children); return f; }
  const el = SVG.has(type) ? document.createElementNS("http://www.w3.org/2000/svg", type) : document.createElement(type);
  for (const [k, v] of Object.entries(rest)) {
    if (v == null || v === false) continue;
    if (k.startsWith("on") && typeof v === "function") el.addEventListener(k.slice(2).toLowerCase(), v);
    else if (k === "style" && typeof v === "object") Object.assign(el.style, v);
    else el.setAttribute(k === "className" ? "class" : k === "htmlFor" ? "for" : k, v === true ? "" : v);
  }
  append(el, children);
  return el;
}
export const jsxs = jsx;
