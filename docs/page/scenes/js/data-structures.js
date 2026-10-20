// JavaScript versions of the "data-structures" examples, keyed by episode id.
// Each mirrors its Lua scene in ../algorithms.js call for call, so both render the same frames.
import {PRELUDE} from "./prelude.js";

const STACK = PRELUDE + `
const start = [3, 7];
const OPS = [["push", 1], ["push", 9], ["pop"], ["pop"], ["push", 4], ["push", 8], ["pop"]];
const x = 0, y0 = -2.0, h = 0.58;
scene.line({id: "wall:left", from: [x - 0.62, y0 - 0.34], to: [x - 0.62, 1.9], stroke: C.line, width: 2, layer: L.region});
scene.line({id: "wall:right", from: [x + 0.62, y0 - 0.34], to: [x + 0.62, 1.9], stroke: C.line, width: 2, layer: L.region});
scene.line({id: "wall:floor", from: [x - 0.62, y0 - 0.34], to: [x + 0.62, y0 - 0.34], stroke: C.line, width: 2, layer: L.region});
const S = [];
let n = 0;
const item = (v) => token("item:" + ++n, String(v), x, y0 + S.length * h);
for (const v of start) S.push(item(v));
let tpos = y0 + (S.length - 1) * h;
const top = T(scene, "ptr:top", "top", x - 0.85, tpos, 14, C.warn, [1, 0.5]);
function point() {
    const y = y0 + Math.max(S.length - 1, 0) * h;
    if (y !== tpos) { scene.shift(top, [0, y - tpos], 0.22, "smooth"); tpos = y; }
}
scene.wait(0.5);
for (const [op, v] of OPS) {
    if (op === "push") {
        const o = item(v);
        scene.fadeIn(o.g, {shift: [0, 1.2], duration: 0.3});
        S.push(o);
        point();
    } else {
        const o = S.pop();
        scene.tween({target: o.b, stroke: C.warn}, 0.15);
        scene.parallel(() => scene.fadeOut(o.g, {shift: [2.2, 0.6], duration: 0.3}), point);
    }
    scene.wait(0.15);
}
scene.wait(2.4);
return scene;`;

const QUEUE = PRELUDE + `
const start = [4, 8];
const OPS = [["enq", 2], ["enq", 6], ["deq"], ["deq"], ["enq", 5], ["enq", 9], ["deq"]];
const x0 = -3.6, gap = 1.05, y = 0;
scene.line({id: "rail:top", from: [x0 - 0.6, y + 0.4], to: [x0 + 7 * gap, y + 0.4], stroke: C.line, width: 2, layer: L.region});
scene.line({id: "rail:bottom", from: [x0 - 0.6, y - 0.4], to: [x0 + 7 * gap, y - 0.4], stroke: C.line, width: 2, layer: L.region});
const Q = [];
let n = 0;
const item = (v) => token("item:" + ++n, String(v), x0 + Q.length * gap, y);
for (const v of start) Q.push(item(v));
T(scene, "ptr:front", "front", x0, y - 0.75, 13, C.accent);
let rpos = Q.length - 1;
const rear = T(scene, "ptr:rear", "rear", x0 + rpos * gap, y + 0.75, 13, C.warn);
scene.wait(0.5);
for (const [op, v] of OPS) {
    if (op === "enq") {
        const i = Q.length;
        const o = item(v);
        scene.fadeIn(o.g, {shift: [1.0, 0], duration: 0.3});
        Q.push(o);
        if (i !== rpos) scene.shift(rear, [(i - rpos) * gap, 0], 0.2, "smooth");
        rpos = i;
    } else {
        const o = Q.shift();
        scene.tween({target: o.b, stroke: C.accent}, 0.12);
        const m = Q.map((q) => toward(q, q.x - gap, q.y));
        m.push({target: rear, shift: [-gap, 0]});
        rpos -= 1;
        scene.parallel(() => scene.fadeOut(o.g, {shift: [-1.2, 0], duration: 0.3}),
                       () => scene.tween(m, 0.3, "smooth"));
    }
    scene.wait(0.15);
}
scene.wait(2.4);
return scene;`;

const REVERSE = PRELUDE + `
const vals = [1, 2, 3, 4, 5];
const x0 = -2.7, gap = 1.35, y = 0.4;
// node i sits at X(i), i = 1..vals.length; X(0) and X(n + 1) are the None ends
const X = (i) => x0 + (i - 1) * gap;
vals.forEach((v, k) => {
    const i = k + 1;
    scene.rectangle({id: "node:" + i, center: [X(i), y], size: [0.6, 0.5], corner: 0.04,
                     fill: C.panel, stroke: C.accent, width: 1.5, layer: L.subject});
    T(scene, "node:" + i + ":label", String(v), X(i), y, 15);
});
T(scene, "none:left", "None", X(0) + 0.1, y, 12, C.mute);
T(scene, "none:right", "None", X(vals.length + 1) - 0.1, y, 12, C.mute);
const nextArrow = [];
for (let i = 1; i <= vals.length; i++) {
    nextArrow[i] = scene.arrow({id: "next:" + i, from: [X(i) + 0.32, y], to: [X(i + 1) - 0.36, y],
                                stroke: C.line, width: 2, tip: 8, layer: L.edge});
}
const ptr = (id, label, i, color, dy) => ({t: T(scene, id, label, X(i), y + dy, 13, color), i});
function move(p, i) { scene.shift(p.t, [(i - p.i) * gap, 0], 0.3, "smooth"); p.i = i; }
const prev = ptr("ptr:prev", "prev", 0, C.alt, -0.75);
const head = ptr("ptr:head", "head", 1, C.accent, 0.72);
const nxt = ptr("ptr:next", "next", 1, C.warn, -1.15);
scene.set({target: nxt.t, opacity: 0});
scene.wait(0.4);
for (let i = 1; i <= vals.length; i++) {
    together(() => move(nxt, i + 1), () => scene.fade(nxt.t, 1, 0.2),
             () => scene.stroke(nextArrow[i], C.warn, 0.2));
    // the same pointer swings round its node: it now points back at prev
    together(() => scene.rotate(nextArrow[i], -Math.PI, {about: [X(i), y], duration: 0.55, curve: "smooth"}),
             () => scene.stroke(nextArrow[i], C.ok, 0.55));
    move(prev, i);
    together(() => move(head, i + 1), () => scene.fade(nxt.t, 0, 0.2));
}
say("new head", X(vals.length), y + 0.72, C.ok);
scene.indicate(prev.t, {color: C.ok, scale: 1.3, duration: 0.6});
scene.wait(2.4);
return scene;`;

const BST = PRELUDE + `
const keys = [50, 30, 70, 20, 40, 60, 80, 35, 65];
const R = 0.3;
// trace: each key's comparison path and its parent slot
const TRACE = [];
let maxd = 0;
{
    let tree = null;
    for (const k of keys) {
        const path = [];
        let n = tree, parent = null, side = null;
        while (n) {
            path.push({node: n.v, go: k < n.v ? "left" : "right"});
            parent = n; side = k < n.v ? "l" : "r";
            n = n[side];
        }
        const leaf = {v: k};
        if (parent) parent[side] = leaf; else tree = leaf;
        TRACE.push({key: k, path});
        maxd = Math.max(maxd, path.length);
    }
}
const dy = Math.min(1.1, 3.4 / Math.max(maxd, 1));
// the keys still to insert wait in a row along the top
const rgap = 0.7;
const rx0 = -(keys.length - 1) * rgap / 2;
const row = keys.map((k, i) => token("key:" + k, String(k), rx0 + i * rgap, 2.55, {circle: R, stroke: C.mute, text: 14}));
// the invariant every comparison relies on
T(scene, "bst:rule", "left subtree  <  node  <  right subtree", -5.1, -2.8, 13, C.mute, [0, 0.5]);
let count = T(scene, "bst:count", "0 keys · 0 comparisons", 5.1, -2.8, 13, C.mute, [1, 0.5]);
// comparison label: text on an opaque plate so tree edges never cut through it
const utf8Length = (s) => new TextEncoder().encode(s).length;
let cmpN = 0, cmpCur = null;
function compare(s, x, y, color) {
    cmpN += 1;
    const g = scene.group({id: "cmp:" + cmpN, matrix: at(x, y), opacity: 0});
    g.rectangle({id: "cmp:" + cmpN + ":plate", center: [0, 0], size: [utf8Length(s) * 0.085 + 0.3, 0.34], corner: 0.06,
                 fill: "#000000", stroke: C.none, layer: L.focus - 1});
    T(g, "cmp:" + cmpN + ":text", s, 0, 0, 15, color, undefined, L.focus);
    if (cmpCur) scene.fadeOut(cmpCur, {duration: 0.15});
    scene.tween({target: g, opacity: 1}, 0.2);
    cmpCur = g;
}
function compareClear() {
    if (cmpCur) { scene.fadeOut(cmpCur, {duration: 0.15}); cmpCur = null; }
}
const placed = {};   // key -> {x, y, o}
let total = 0;
TRACE.forEach((e, k) => {
    const i = k + 1;
    const probe = row[k];
    say(null);
    scene.tween({target: probe.b, stroke: C.warn}, 0.15);
    let x = 0, y = 1.45, span = 2.2;
    if (e.path.length === 0) say("empty tree: " + e.key + " becomes the root", 0, 0.75);
    for (const p of e.path) {
        const nd = placed[p.node];
        // stand beside the node on the side the key is about to take
        const side = p.go === "left" ? -1 : 1;
        scene.tween(toward(probe, nd.x + side * 0.8, nd.y + 0.05), 0.32, "smooth");
        const lt = p.go === "left";
        say(null);
        compare(\`\${e.key} \${lt ? "<" : ">"} \${p.node}  →  \${p.go}\`,
                nd.x + side * 0.8, nd.y + 0.62, lt ? C.accent : C.alt);
        scene.indicate(nd.o.b, {color: C.warn, scale: 1.1, duration: 0.45});
        scene.wait(0.35);
        x = p.go === "left" ? nd.x - span : nd.x + span;
        y = nd.y - dy;
        span = span / 2;
    }
    const parent = e.path[e.path.length - 1];
    compareClear();
    scene.tween(toward(probe, x, y), 0.35, "smooth");
    if (parent) say(\`empty slot: \${parent.go} child of \${parent.node}\`, x, y - 0.52, C.ok);
    total += e.path.length;
    const nc = T(scene, "bst:count:" + i, \`\${i} keys · \${total} comparison\${total === 1 ? "" : "s"}\`, 5.1, -2.8, 13, C.mute, [1, 0.5]);
    scene.fadeOut(count, {duration: 0.12}); scene.fadeIn(nc, {duration: 0.15});
    count = nc;
    scene.stroke(probe.b, C.accent, 0.15);
    placed[e.key] = {x, y, o: probe};
    if (parent) {
        const pn = placed[parent.node];
        const ddx = x - pn.x, ddy = y - pn.y;
        const len = Math.sqrt(ddx * ddx + ddy * ddy);
        const edge = scene.line({id: "edge:" + parent.node + "-" + e.key,
                                 from: [pn.x + ddx / len * R, pn.y + ddy / len * R],
                                 to: [x - ddx / len * R, y - ddy / len * R], stroke: C.line, width: 2, layer: L.edge});
        scene.create(edge, 0.25);
    }
    scene.wait(0.25);
});
say(null);
scene.wait(2.4);
return scene;`;

const HEAP = PRELUDE + `
const input = [7, 5, 9, 3, 6, 8, 1];
const PRE = 3;   // the first pushes are already settled at t = 0
// Heap positions are 1-based like the textbook layout: the parent of i is floor(i / 2).
// heap[i - 1] holds position i.
const parentOf = (i) => Math.floor(i / 2);
// trace: for each push, the chain of compares up toward the root
const TRACE = [];
let settled = null;
{
    const heap = [];
    input.forEach((v, k) => {
        heap.push(v);
        let i = heap.length;
        const steps = [];
        while (i > 1) {
            const p = parentOf(i);
            steps.push({i, p, swap: heap[p - 1] > heap[i - 1]});
            if (heap[p - 1] <= heap[i - 1]) break;
            [heap[p - 1], heap[i - 1]] = [heap[i - 1], heap[p - 1]];
            i = p;
        }
        TRACE.push({v, at: heap.length, steps});
        if (k + 1 === PRE) settled = [...heap];
    });
    for (let i = 2; i <= heap.length; i++) console.assert(heap[parentOf(i) - 1] <= heap[i - 1]);
}
function pos(i) {
    const d = Math.floor(Math.log2(i) + 1e-9);
    const k = i - 2 ** d;
    return [-4.4 + (k + 0.5) / 2 ** d * 8.8, 1.75 - d * 1.2];
}
const R = 0.3;
const ax0 = -2.2, aw = 0.75, ay = -2.35;
const AX = (i) => ax0 + (i - 1) * aw;
T(scene, "array:title", "ARRAY", ax0 - 0.55, ay, 11, C.mute, [1, 0.5]);
for (let i = 1; i <= input.length; i++) {
    scene.rectangle({id: "slot:" + i, center: [AX(i), ay], size: [aw * 0.9, 0.5], corner: 0.03,
                     fill: C.none, stroke: C.line, width: 1, layer: L.region});
    T(scene, "slot:" + i + ":index", String(i - 1), AX(i), ay - 0.42, 10, C.mute);
}
// tree tokens and array texts, keyed by heap position
const N = {}, A = {};
function edge(i) {
    const [x, y] = pos(i);
    const [px, py] = pos(parentOf(i));
    const dx = x - px, dy = y - py;
    const l = Math.sqrt(dx * dx + dy * dy);
    return scene.line({id: "edge:" + i, from: [px + dx / l * R, py + dy / l * R], to: [x - dx / l * R, y - dy / l * R],
                       stroke: C.line, width: 2, layer: L.edge});
}
function node(i, v) {
    const [x, y] = pos(i);
    N[i] = token("node:" + v, String(v), x, y, {circle: R});
    A[i] = T(scene, "arr:" + v, String(v), AX(i), ay, 15);
}
// tree and array views of the settled heap, visible from the first frame
settled.forEach((v, k) => {
    node(k + 1, v);
    if (k > 0) edge(k + 1);
});
// arc angle that lifts a value 'rise' above the row while it travels d slots
const hop = (d, rise) => 4 * Math.atan(2 * rise / (d * aw));
function swap(i, j) {
    const [xi, yi] = pos(i);
    const [xj, yj] = pos(j);
    // tree tokens pass each other on opposite arcs; in the array both values hop, one higher
    scene.tween([toward(N[i], xj, yj, 0.9), toward(N[j], xi, yi, 0.9),
                {target: A[i], shift: [(j - i) * aw, 0], arc: hop(i - j, 0.74)},
                {target: A[j], shift: [(i - j) * aw, 0], arc: -hop(i - j, 0.36)}], 0.45, "smooth");
    [N[i], N[j]] = [N[j], N[i]];
    [A[i], A[j]] = [A[j], A[i]];
}
scene.wait(0.5);
for (const e of TRACE.slice(PRE)) {
    // append: the value lands in the next free slot of both views at once
    node(e.at, e.v);
    const ed = edge(e.at);
    together(() => scene.fadeIn(N[e.at].g, {scale: 0.6, duration: 0.3}),
             () => scene.fadeIn(A[e.at], {shift: [0, 0.3], duration: 0.3}),
             () => scene.create(ed, 0.3));
    for (const s of e.steps) {
        scene.tween([{target: N[s.i].b, stroke: C.warn}, {target: N[s.p].b, stroke: C.warn}], 0.15);
        if (s.swap) swap(s.i, s.p); else scene.wait(0.15);
        scene.tween([{target: N[s.i].b, stroke: C.accent}, {target: N[s.p].b, stroke: C.accent}], 0.12);
    }
    scene.wait(0.15);
}
scene.tween({target: N[1].b, stroke: C.ok, fill: "#1c3a26"}, 0.3);
const [rx, ry] = pos(1);
say("min", rx + 0.5, ry, C.ok, [0, 0.5]);
scene.indicate(N[1].b, {color: C.ok, scale: 1.15, duration: 0.6});
scene.wait(2.4);
return scene;`;

const HASH = PRELUDE + `
const keys = ["cat", "dog", "ant", "owl", "bee", "fox", "emu"];
const q = "emu";
const M = 5;
const h = (k) => [...k].reduce((s, ch) => s + ch.charCodeAt(0), 0);
const bx = -4.4, by0 = 1.25, bh = 0.78;
const B = [], chains = [];
for (let i = 0; i < M; i++) {
    const y = by0 - i * bh;
    B[i] = scene.rectangle({id: "bucket:" + i, center: [bx, y], size: [0.7, 0.56], corner: 0.03,
                            fill: C.panel, stroke: C.line, width: 1.5, layer: L.subject});
    T(scene, "bucket:" + i + ":index", String(i), bx, y, 14, C.mute);
    chains[i] = [];
}
// keys waiting to be inserted, along the top
const pend = keys.map((key, k) => token("key:" + key, key, -1.6 + k * 1.0, 2.55, {size: [0.86, 0.46], stroke: C.mute, text: 14}));
const sx = 3.4, sy = 0.5;   // where a key is hashed
function hashAt(o, key) {
    const hv = h(key);
    scene.tween(toward(o, sx, sy), 0.4, "smooth");
    say(\`h(\${key}) = \${hv},  \${hv} mod \${M} = \${hv % M}\`, sx, sy - 0.55);
    return hv % M;
}
scene.wait(0.4);
keys.forEach((key, k) => {
    const o = pend[k];
    const i = hashAt(o, key);
    scene.stroke(B[i], C.warn, 0.2);
    const x = bx + 1.25 + chains[i].length * 1.35, y = by0 - i * bh;
    const fx = chains[i].length === 0 ? bx + 0.37 : x - 1.35 + 0.45;
    say(null);
    scene.tween([toward(o, x, y), {target: o.b, stroke: C.accent}], 0.45, "smooth");
    const a = scene.arrow({id: "link:" + key, from: [fx, y], to: [x - 0.45, y], stroke: C.line, width: 1.5, tip: 7, layer: L.edge});
    scene.parallel(() => scene.create(a, 0.2), () => scene.stroke(B[i], C.line, 0.2));
    chains[i].push({o, k: key});
});
// lookup: hash once, then walk one short chain
const probe = token("query", q + "?", sx, 2.55, {size: [0.86, 0.46], stroke: C.warn, text: 14});
scene.fadeIn(probe.g, {shift: [0, -0.3], duration: 0.3});
const i = hashAt(probe, q);
scene.stroke(B[i], C.warn, 0.2);
let hit = false;
for (const c of chains[i]) {
    scene.tween({target: c.o.b, stroke: C.warn}, 0.2);
    if (c.k === q) {
        hit = true;
        together(() => scene.tween({target: c.o.b, stroke: C.ok, fill: "#1c3a26"}, 0.3),
                 () => scene.stroke(probe.b, C.ok, 0.3),
                 () => say("found in bucket " + i, sx, sy - 0.55, C.ok));
        break;
    }
    scene.tween({target: c.o.b, stroke: C.accent}, 0.15);
}
if (!hit) {
    together(() => scene.tween({target: B[i], stroke: C.danger}, 0.3),
             () => say("not in the table", sx, sy - 0.55, C.danger));
}
scene.wait(2.4);
return scene;`;

export default {
  "stack": STACK,
  "queue": QUEUE,
  "reverse-list": REVERSE,
  "bst-insert": BST,
  "binary-heap": HEAP,
  "hash-table": HASH,
};
