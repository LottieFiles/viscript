// The JavaScript twin of ../prelude.js, which the JavaScript versions of those scenes open with.
export const PRELUDE = `const scene = vis.scene({camera: {height: 6}});
const C = {panel: "#14171f", line: "#3a4152", text: "#e6e8ee", mute: "#7d8596", bar: "#2a3142",
           accent: "#6cb6ff", warn: "#e3b341", danger: "#ff7b72", ok: "#7ee787", alt: "#c8a7ff",
           none: "#00000000"};
// draw-order bands: guides < regions < edges < subjects < tokens < labels < transient focus
const L = {guide: 1, region: 2, edge: 3, subject: 5, token: 6, label: 8, focus: 10};
const at = (x, y) => [1, 0, 0, x, 0, 1, 0, y, 0, 0, 1, 0, 0, 0, 0, 1];
const T = (parent, id, s, x, y, size, color, align, layer) =>
    parent.text({id, text: s, point: [x, y], size: size ?? 16, font: "Pretendard",
                 color: color ?? C.text, align: align ?? [0.5, 0.5], layer: layer ?? L.label});
// one short phrase beside the action; the next call fades it out and takes its place, say(null) fades it
let sayN = 0, sayCur = null;
function say(s, x, y, color, align) {
    if (s == null) {
        if (sayCur) { scene.fadeOut(sayCur, {duration: 0.2}); sayCur = null; }
        return;
    }
    sayN++;
    const next = T(scene, "say:" + sayN, s, x, y, 15, color ?? C.warn, align, L.focus);
    if (sayCur) scene.fadeOut(sayCur, {duration: 0.15});
    scene.fadeIn(next, {duration: 0.2});
    sayCur = next;
}
const together = (...tracks) => scene.parallel(...tracks);
// a token is one Group (box + label around [0, 0]) that moves as a unit
function token(id, s, x, y, o = {}) {
    const layer = o.layer ?? L.token;
    const g = scene.group({id, matrix: at(x, y)});
    const b = o.circle
        ? g.circle({id: id + ":box", radius: o.circle, fill: o.fill ?? C.panel,
                    stroke: o.stroke ?? C.accent, width: o.width ?? 2, layer})
        : g.rectangle({id: id + ":box", size: o.size ?? [0.9, 0.48], corner: 0.04,
                       fill: o.fill ?? C.panel, stroke: o.stroke ?? C.accent, width: o.width ?? 1.5, layer});
    const t = T(g, id + ":label", s, 0, 0, o.text ?? 15, o.color ?? C.text, null, layer + 2);
    return {g, b, t, x, y, v: s};
}
// descriptor that carries a token to (x, y), which it records
function toward(o, x, y, arc) {
    o.x = x; o.y = y;
    return arc === undefined ? {target: o.g, position: [x, y]} : {target: o.g, position: [x, y], arc};
}
`;
