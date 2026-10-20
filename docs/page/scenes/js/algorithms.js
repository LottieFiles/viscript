// JavaScript versions of the "algorithms" examples, keyed by episode id.
// Each shares a prelude and helpers, mirroring the Lua sources in ../algorithms.js.
import {PRELUDE} from "./prelude.js";

// Bars are 1-based (B[1]..B[n]) like the Lua so the traces read the same.
const BARS = `
// each bar is a Group at its slot: body grows up from the baseline, value sits under it
function bars(vals, x0, x1, base, hmax) {
    const n = vals.length - 1, maxv = Math.max(...vals.slice(1));
    const w = (x1 - x0) / n;
    const B = [];
    for (let i = 1; i <= n; i++) {
        const v = vals[i], h = v / maxv * hmax, x = x0 + (i - 0.5) * w;
        const g = scene.group({id: "bar:" + i, matrix: at(x, base)});
        const r = g.rectangle({id: "bar:" + i + ":body", center: [0, h / 2], size: [w * 0.72, h],
                               fill: C.bar, stroke: C.none, layer: L.subject});
        T(g, "bar:" + i + ":value", String(v), 0, -0.22, 13, C.mute, null, L.label);
        B[i] = {g, r, v, h};
    }
    return [B, w];
}
const paint = (list, color, d) => scene.parallel(...list.map((b) => () => scene.fill(b.r, color, d ?? 0.14)));
function swap(B, i, j, w, d) {
    const a = B[i], b = B[j];
    scene.tween([{target: a.g, shift: [(j - i) * w, 0], arc: -Math.PI / 2},
                {target: b.g, shift: [(i - j) * w, 0], arc: -Math.PI / 2}], d ?? 0.32, "smooth");
    B[i] = b; B[j] = a;
}
`;

const BUBBLE = PRELUDE + BARS + `
const vals = [, 5, 2, 8, 3, 9, 1, 6];  // 1-based
const n = vals.length - 1;
// trace: every compare, every settle, and the early exit
const TRACE = [];
{
    const a = [...vals];
    let finished = false;
    for (let k = 0; k <= n - 2; k++) {
        let swapped = false;
        for (let i = 2; i <= n - k; i++) {
            const sw = a[i - 1] > a[i];
            TRACE.push({kind: "cmp", i, a: a[i - 1], b: a[i], swap: sw});
            if (sw) { [a[i - 1], a[i]] = [a[i], a[i - 1]]; swapped = true; }
        }
        TRACE.push({kind: "settle", i: n - k});
        if (!swapped) { TRACE.push({kind: "done", upto: n - k - 1}); finished = true; break; }
    }
    if (!finished) TRACE.push({kind: "done", upto: 1});
}
const x0 = -4.6, x1 = 4.6, base = -2.2, hmax = 3.6;
const [B, w] = bars(vals, x0, x1, base, hmax);
const X = (i) => x0 + (i - 0.5) * w;
// the wall between the unsorted prefix and the settled suffix
const wall = scene.line({id: "wall", from: [x1, base - 0.05], to: [x1, base + hmax + 0.1],
                         stroke: C.line, width: 2, layer: L.guide});
let wallX = x1;
const cueY = base + hmax + 0.45;
const seen = {};
for (const e of TRACE) {
    if (e.kind === "cmp") {
        const first = (e.swap && !seen.swap) || (!e.swap && !seen.keep);
        together(() => paint([B[e.i - 1], B[e.i]], C.warn),
                 () => {
                     if (first) say(e.swap ? e.a + " > " + e.b + "  swap" : e.a + " < " + e.b + "  keep",
                                    (X(e.i - 1) + X(e.i)) / 2, cueY);
                     else say(null);
                 });
        scene.wait(0.12);
        if (e.swap) { swap(B, e.i - 1, e.i, w); seen.swap = true; } else seen.keep = true;
        paint([B[e.i - 1]], C.bar, 0.1);
    } else if (e.kind === "settle") {
        const wx = x0 + (e.i - 1) * w;
        together(() => paint([B[e.i]], C.ok, 0.2),
                 () => scene.shift(wall, [wx - wallX, 0], 0.3, "smooth"),
                 () => {
                     if (!seen.settle) say("largest settles", X(e.i), base + B[e.i].h + 0.3, C.ok);
                     else say(null);
                 });
        wallX = wx;
        seen.settle = true;
    } else {
        const rest = B.slice(1, e.upto + 1);
        together(() => paint(rest, C.ok, 0.3),
                 () => scene.fade(wall, 0, 0.3),
                 () => {
                     if (rest.length > 1) say("no swaps  sorted", X(1) + w * (e.upto - 1) / 2, cueY, C.ok);
                     else say(null);
                 });
    }
}
scene.wait(2.4);
return scene;`;

const INSERTION = PRELUDE + BARS + `
const vals = [, 6, 3, 8, 2, 7, 1, 5];  // 1-based
const n = vals.length - 1;
// trace: for each key, the larger neighbours it passes and the slot it lands in
const TRACE = [];
{
    const a = [...vals];
    for (let i = 2; i <= n; i++) {
        const key = a[i], shifts = [];
        let pos = i;
        while (pos > 1 && a[pos - 1] > key) {
            shifts.push({from: pos - 1, v: a[pos - 1]});
            a[pos] = a[pos - 1]; pos--;
        }
        a[pos] = key;
        TRACE.push({i, key, shifts, to: pos});
    }
}
const x0 = -4.6, x1 = 4.6, base = -2.2, hmax = 3.4;
const [B, w] = bars(vals, x0, x1, base, hmax);
const X = (i) => x0 + (i - 0.5) * w;
const lift = 0.6;
paint([B[1]], C.ok, 0.25);
TRACE.forEach((e, index) => {
    const step = index + 1;
    const cur = B[e.i];
    together(() => scene.fill(cur.r, C.accent, 0.15),
             () => scene.shift(cur.g, [0, lift], 0.28, "smooth"),
             () => { if (step === 1) say("key", X(e.i), base + cur.h + lift + 0.3, C.accent); else say(null); });
    let pos = e.i;
    e.shifts.forEach((s, k) => {
        together(() => scene.shift(B[s.from].g, [w, 0], 0.28, "smooth"),
                 () => scene.shift(cur.g, [-w, 0], 0.28, "smooth"),
                 () => {
                     if (step === 1 && k === 0) say(s.v + " > " + e.key + "  shift right", X(s.from), base + B[s.from].h + 0.35);
                     else if (step === 1) say(null);
                 });
        B[pos] = B[s.from];
        pos = s.from;
    });
    together(() => scene.shift(cur.g, [0, -lift], 0.28, "smooth"),
             () => scene.fill(cur.r, C.ok, 0.2),
             () => { if (step === 1) say(null); });
    B[pos] = cur;
    scene.wait(0.1);
});
scene.wait(2.4);
return scene;`;

const QUICK = PRELUDE + BARS + `
const vals = [, 7, 2, 9, 4, 1, 8, 3, 6];  // 1-based
const n = vals.length - 1;
const x0 = -4.8, x1 = 1.3, base = -1.6, hmax = 3.4;
const maxv = Math.max(...vals.slice(1));
const step = Math.min(0.48, 3.6 / n);
const [B, w] = bars(vals, x0, x1, base, hmax);
const X = (i) => x0 + (i - 0.5) * w;
// legend: colour plus words, so colour is never the only cue
scene.rectangle({id: "legend:lt", center: [2.05, 2.1], size: [0.18, 0.18], fill: C.accent, stroke: C.none, layer: L.subject});
T(scene, "legend:lt:label", "< pivot", 2.25, 2.1, 12, C.mute, [0, 0.5]);
scene.rectangle({id: "legend:ge", center: [3.35, 2.1], size: [0.18, 0.18], fill: C.warn, stroke: C.none, layer: L.subject});
T(scene, "legend:ge:label", "≥ pivot", 3.55, 2.1, 12, C.mute, [0, 0.5]);
// call stack: each frame is one quickSort(lo..hi) call: the only extra memory
T(scene, "stack:title", "CALL STACK", 1.95, 1.6, 11, C.mute, [0, 0.5]);
const frames = [];
let fid = 0;
function push(lo, hi) {
    fid++;
    const y = 1.2 - frames.length * step;
    if (frames.length) scene.stroke(frames.at(-1).b, C.line, 0.1);
    const f = token("frame:" + fid, "quick_sort(" + (lo - 1) + ".." + (hi - 1) + ")", 3.4, y,
                    {size: [2.9, step * 0.84], width: 1.2, text: 13, layer: L.subject});
    scene.fadeIn(f.g, {shift: [0.25, 0], duration: 0.22});
    frames.push(f);
}
function pop() {
    const f = frames.pop();
    scene.fadeOut(f.g, {shift: [0.25, 0], duration: 0.2});
    if (frames.length) scene.stroke(frames.at(-1).b, C.accent, 0.1);
}
// pivot height line across the active range
const th = scene.rectangle({id: "pivot-line", center: [(x0 + x1) / 2, base], size: [x1 - x0, 0.03],
                            fill: C.alt, stroke: C.none, layer: L.label - 1});
scene.set({target: th, opacity: 0});
function threshold(lo, hi, v) {
    const cx = x0 + (lo - 1 + hi) / 2 * w, y = base + v / maxv * hmax;
    scene.tween({target: th, position: [cx, y], size: [(hi - lo + 1) * w, 0.03], opacity: 0.9}, 0.3, "smooth");
}
// scan pointers: i = end of the < pivot zone, j = scanner
const pi = T(scene, "ptr:i", "i", X(1), base - 0.6, 14, C.accent);
const pj = T(scene, "ptr:j", "j", X(1), base - 0.9, 14, C.warn);
scene.set({target: pi, opacity: 0}); scene.set({target: pj, opacity: 0});
let ip = 1, jp = 1;
const mv = (t, from, to) => { if (from !== to) scene.shift(t, [(to - from) * w, 0], 0.18, "smooth"); };
function qs(lo, hi) {
    push(lo, hi);
    if (lo >= hi) {
        if (lo === hi) paint([B[lo]], C.ok, 0.2);
        scene.wait(0.15); pop(); return;
    }
    together(() => paint([B[hi]], C.alt, 0.18), () => threshold(lo, hi, vals[hi]),
             () => mv(pi, ip, lo), () => mv(pj, jp, lo),
             () => scene.fade(pi, 1, 0.2), () => scene.fade(pj, 1, 0.2));
    ip = lo; jp = lo;
    let i = lo;
    for (let j = lo; j <= hi - 1; j++) {
        mv(pj, jp, j); jp = j;
        if (vals[j] < vals[hi]) {
            if (i !== j) { swap(B, i, j, w); [vals[i], vals[j]] = [vals[j], vals[i]]; }
            paint([B[i]], C.accent, 0.12);
            i++;
            mv(pi, ip, i); ip = i;
        } else {
            paint([B[j]], C.warn, 0.12);
        }
    }
    if (i !== hi) { swap(B, i, hi, w, 0.4); [vals[i], vals[hi]] = [vals[hi], vals[i]]; }
    const reset = [];
    for (let k = lo; k <= hi; k++) if (k !== i) reset.push(B[k]);
    together(() => paint(reset, C.bar, 0.15), () => scene.fill(B[i].r, C.ok, 0.2),
             () => scene.fade(th, 0, 0.2), () => scene.fade(pi, 0, 0.2), () => scene.fade(pj, 0, 0.2));
    qs(lo, i - 1);
    qs(i + 1, hi);
    pop();
}
qs(1, n);
scene.wait(2.4);
return scene;`;

const MERGE = PRELUDE + BARS + `
const vals = [, 6, 2, 7, 4, 8, 1, 5, 3];  // 1-based
const n = vals.length - 1;
const x0 = -4.6, x1 = 4.6, top = 0.1, low = -2.5, hmax = 1.65;
const [B, w] = bars(vals, x0, x1, top, hmax);
const X = (i) => x0 + (i - 0.5) * w;
const drop = low - top;
T(scene, "region:array", "ARRAY", -5.1, top + hmax + 0.15, 11, C.mute, [0, 0.5]);
T(scene, "region:temp", "TEMP", -5.1, low + hmax + 0.15, 11, C.mute, [0, 0.5]);
for (let i = 1; i <= n; i++) {
    scene.rectangle({id: "temp-slot:" + i, center: [X(i), low + hmax / 2], size: [w * 0.8, hmax + 0.1],
                     fill: C.none, stroke: C.line, width: 1, layer: L.region});
}
function merge(lo, mid, hi) {
    const left = B.slice(lo, mid + 1), right = B.slice(mid + 1, hi + 1);
    together(() => paint(left, C.alt, 0.15), () => paint(right, C.accent, 0.15));
    let a = lo, b = mid + 1, k = lo;
    const tmp = [];
    const take = (col) => {
        scene.tween([{target: B[col].g, shift: [(k - col) * w, drop]}, {target: B[col].r, stroke: C.none}], 0.3, "smooth");
        tmp[k] = {b: B[col], v: vals[col]};
        k++;
    };
    while (a <= mid && b <= hi) {
        scene.tween([{target: B[a].r, stroke: C.text, width: 2}, {target: B[b].r, stroke: C.text, width: 2}], 0.15);
        scene.wait(0.12);
        if (vals[a] <= vals[b]) { scene.stroke(B[b].r, C.none, 0.08); take(a); a++; }
        else { scene.stroke(B[a].r, C.none, 0.08); take(b); b++; }
    }
    while (a <= mid) { take(a); a++; }
    while (b <= hi) { take(b); b++; }
    const up = [];
    for (let c = lo; c <= hi; c++) {
        up.push({target: tmp[c].b.g, shift: [0, -drop]});
        B[c] = tmp[c].b; vals[c] = tmp[c].v;
    }
    scene.tween(up, 0.35, "smooth");
    paint(B.slice(lo, hi + 1), lo === 1 && hi === n ? C.ok : C.bar, 0.15);
}
function ms(lo, hi) {
    if (lo >= hi) return;
    const mid = Math.floor((lo + hi) / 2);
    ms(lo, mid); ms(mid + 1, hi);
    merge(lo, mid, hi);
}
scene.wait(0.4);
ms(1, n);
scene.wait(2.4);
return scene;`;

const BINARY = PRELUDE + `
const arr = [, 2, 5, 8, 12, 16, 23, 38, 45, 56, 72, 91];  // 1-based
const n = arr.length - 1;
const target = 56;
// trace: every probe with its window and verdict
const TRACE = [];
{
    let lo = 1, hi = n;
    while (lo <= hi) {
        const mid = Math.floor((lo + hi) / 2);
        const verdict = arr[mid] === target ? "hit" : arr[mid] < target ? "right" : "left";
        TRACE.push({lo, hi, mid, verdict});
        if (verdict === "hit") break;
        else if (verdict === "right") lo = mid + 1;
        else hi = mid - 1;
    }
}
const w = Math.min(0.8, 9.2 / n);
const x0 = -w * n / 2, y = 0.1;
const X = (i) => x0 + (i - 0.5) * w;
const cells = [], box = [];
for (let i = 1; i <= n; i++) {
    cells[i] = scene.group({id: "cell:" + i});
    box[i] = cells[i].rectangle({id: "cell:" + i + ":box", center: [X(i), y], size: [w * 0.9, 0.6], corner: 0.03,
                                 fill: C.panel, stroke: C.line, width: 1.2, layer: L.subject});
    T(cells[i], "cell:" + i + ":value", String(arr[i]), X(i), y, 15, C.text);
    T(scene, "cell:" + i + ":index", String(i - 1), X(i), y - 0.5, 10, C.mute);
}
T(scene, "target", "target  " + target, x0, 1.75, 16, C.warn, [0, 0.5]);
// the live window [lo, hi]: one outline that shrinks
const winW = (lo, hi) => (hi - lo + 1) * w;
const win = scene.rectangle({id: "window", center: [(X(1) + X(n)) / 2, y], size: [winW(1, n) + 0.08, 0.8],
                             corner: 0.06, fill: C.none, stroke: C.accent, width: 2, layer: L.edge});
let wlo = 1, whi = n;
const plo = T(scene, "ptr:lo", "lo", X(1), y - 0.95, 13, C.accent);
const phi = T(scene, "ptr:hi", "hi", X(n), y - 0.95, 13, C.accent);
const pmid = T(scene, "ptr:mid", "mid", X(1), y + 0.65, 13, C.warn);
scene.set({target: pmid, opacity: 0});
scene.wait(0.5);
for (const e of TRACE) {
    together(() => scene.tween({target: pmid, position: [X(e.mid), y + 0.65], opacity: 1}, 0.35, "smooth"),
             () => scene.tween({target: box[e.mid], stroke: C.warn}, 0.25));
    const v = arr[e.mid];
    const rel = e.verdict === "hit" ? "=" : e.verdict === "right" ? "<" : ">";
    if (e.verdict === "hit") {
        together(() => scene.tween({target: box[e.mid], fill: "#7ee78733", stroke: C.ok}, 0.35),
                 () => say(v + " = " + target + "  found at index " + (e.mid - 1), X(e.mid), y + 1.1, C.ok));
        scene.indicate(box[e.mid], {color: C.ok, scale: 1.12, duration: 0.6});
        break;
    }
    say(v + " " + rel + " " + target, X(e.mid), y + 1.1);
    scene.wait(0.35);
    let nlo = e.lo, nhi = e.hi;
    const dim = [];
    if (e.verdict === "right") {
        for (let k = e.lo; k <= e.mid; k++) dim.push(k);
        nlo = e.mid + 1;
    } else {
        for (let k = e.mid; k <= e.hi; k++) dim.push(k);
        nhi = e.mid - 1;
    }
    const ncx = (X(nlo) + X(nhi)) / 2;
    scene.parallel(
        () => scene.tween({target: win, position: [ncx, y], size: [winW(nlo, nhi) + 0.08, 0.8]}, 0.45, "smooth"),
        () => { if (nlo !== wlo) scene.shift(plo, [(nlo - wlo) * w, 0], 0.45, "smooth"); },
        () => { if (nhi !== whi) scene.shift(phi, [(nhi - whi) * w, 0], 0.45, "smooth"); },
        () => scene.stroke(box[e.mid], C.line, 0.2),
        ...dim.map((k) => () => scene.fade(cells[k], 0.25, 0.3)),
    );
    wlo = nlo; whi = nhi;
}
scene.wait(2.4);
return scene;`;

const GRAPH = `
const P = {A: [-3.6, 1.2], B: [-4.7, -0.1], C: [-2.5, -0.1], D: [-5.0, -1.5], E: [-3.8, -1.5], F: [-1.9, -1.5], G: [-0.6, 0.4]};
const ADJ = {A: ["B", "C"], B: ["A", "D", "E"], C: ["A", "F", "G"], D: ["B"], E: ["B"], F: ["C"], G: ["C"]};
const ORDER = ["A", "B", "C", "D", "E", "F", "G"];
for (const k of ORDER) P[k] = [P[k][0] * 1.15 + 3.2, P[k][1] * 1.1 + 0.25];
const N = {}, E = {};
const ekey = (a, b) => a < b ? a + b : b + a;
for (const a of ORDER) {
    for (const b of ADJ[a]) {
        if (a < b) E[ekey(a, b)] = scene.line({id: "edge:" + ekey(a, b), from: P[a], to: P[b], stroke: C.line, width: 2, layer: L.edge});
    }
}
// opaque node bodies keep the labels readable over the edges
for (const k of ORDER) {
    N[k] = scene.circle({id: "node:" + k, center: P[k], radius: 0.3, fill: C.panel, stroke: C.line, width: 2, layer: L.subject});
    T(scene, "node:" + k + ":label", k, P[k][0], P[k][1], 15, C.text);
}
// a horizontal strip of tokens (queue / stack) along the bottom
function strip(id, label, x0, y) {
    T(scene, id + ":title", label, x0, y + 0.42, 11, C.mute, [0, 0.5]);
    const items = [], w = 0.5;
    let n = 0;
    return {
        push(k) {
            n++;
            const o = token(id + ":" + n, k, x0 + (items.length + 0.5) * w, y, {size: [w * 0.86, 0.42], width: 1.2, text: 13});
            scene.fadeIn(o.g, {shift: [0, 0.2], duration: 0.25});
            items.push(o);
        },
        popFront() {
            const o = items.shift();
            const d = items.map((q) => toward(q, q.x - w, q.y));
            scene.parallel(() => scene.fadeOut(o.g, {shift: [-0.3, 0], duration: 0.25}),
                           () => { if (d.length) scene.tween(d, 0.25, "smooth"); });
        },
        popBack() {
            const o = items.pop();
            scene.fadeOut(o.g, {shift: [0, 0.2], duration: 0.25});
        },
    };
}
`;

const BFS = PRELUDE + GRAPH + `
const Q = strip("queue", "QUEUE", -5.1, -2.45);
const queue = ["A"];
const seen = {A: true};
scene.wait(0.3);
scene.tween({target: N.A, stroke: C.warn}, 0.2);
Q.push("A");
while (queue.length) {
    const node = queue.shift();
    together(() => Q.popFront(),
             () => scene.tween({target: N[node], fill: "#1b3350", stroke: C.accent}, 0.25));
    for (const nb of ADJ[node]) {
        scene.stroke(E[ekey(node, nb)], C.warn, 0.15);
        if (!seen[nb]) {
            seen[nb] = true;
            together(() => scene.tween({target: N[nb], stroke: C.warn}, 0.2),
                     () => scene.tween({target: E[ekey(node, nb)], stroke: C.accent, width: 4}, 0.2));
            Q.push(nb);
            queue.push(nb);
        } else {
            scene.stroke(E[ekey(node, nb)], C.line, 0.12);
        }
    }
    scene.tween({target: N[node], fill: "#1c3a26", stroke: C.ok}, 0.2);
}
scene.wait(2.4);
return scene;`;

const DFS = PRELUDE + GRAPH + `
const S = strip("stack", "CALL STACK", -5.1, -2.45);
const seen = {};
function dfs(node, parent) {
    S.push(node);
    seen[node] = true;
    scene.tween({target: N[node], fill: "#1b3350", stroke: C.accent}, 0.25);
    for (const nb of ADJ[node]) {
        if (nb !== parent) scene.stroke(E[ekey(node, nb)], C.warn, 0.15);
        if (!seen[nb]) {
            scene.tween({target: E[ekey(node, nb)], stroke: C.accent, width: 4}, 0.2);
            dfs(nb, node);
        } else if (nb !== parent) {
            scene.stroke(E[ekey(node, nb)], C.line, 0.12);
        }
    }
    scene.tween({target: N[node], fill: "#1c3a26", stroke: C.ok}, 0.2);
    S.popBack();
}
scene.wait(0.3);
dfs("A", null);
scene.wait(2.4);
return scene;`;

const DIAMETER = PRELUDE + `
// an unweighted tree; ADJ[i] lists the neighbours of node i
const EDGES = [[1, 2], [1, 3], [2, 4], [2, 5], [3, 6], [4, 7], [4, 8], [6, 9], [9, 10]];
const n = 10;
const ADJ = [];
for (let i = 1; i <= n; i++) ADJ[i] = [];
for (const [u, v] of EDGES) { ADJ[u].push(v); ADJ[v].push(u); }

// hang the tree from root: depth sets the row, leaves take columns left to right,
// a parent sits over the middle of its children
const TOP = 2.55, ROW = 0.68, COL = 1.05;
function hang(root) {
    const depth = [], parent = [], x = [];
    let leaves = 0;
    function walk(v, p, d) {
        depth[v] = d; parent[v] = p;
        const xs = [];
        for (const w of ADJ[v]) if (w !== p) { walk(w, v, d + 1); xs.push(x[w]); }
        if (xs.length === 0) x[v] = ++leaves;
        else x[v] = (xs[0] + xs[xs.length - 1]) / 2;
    }
    walk(root, null, 0);
    const pos = [];
    for (let v = 1; v <= n; v++) pos[v] = [(x[v] - (leaves + 1) / 2) * COL - 0.6, TOP - depth[v] * ROW];
    return [pos, depth, parent];
}

const [pos] = hang(3);
const node = [];
for (let v = 1; v <= n; v++) node[v] = token("node:" + v, String(v), pos[v][0], pos[v][1], {circle: 0.24, stroke: C.line, text: 13});
const link = [];
EDGES.forEach(([u, v], i) => {
    link[i] = scene.connector(node[u].g, node[v].g, {id: "edge:" + (i + 1),
                               stroke: C.line, width: 2, tip: 0, layer: L.edge});
});
// the hand holding the tree: a string from the top of the frame to the root
const hand = scene.line({id: "hand", from: [pos[3][0], 3.2], to: [pos[3][0], pos[3][1] + 0.24],
                         stroke: C.mute, width: 2, layer: L.guide});

// light the tree one depth at a time; return the deepest node
function sweep(depth, color) {
    let deepest = 1, rows = 0;
    for (let v = 1; v <= n; v++) {
        if (depth[v] > depth[deepest]) deepest = v;
        rows = Math.max(rows, depth[v]);
    }
    for (let d = 0; d <= rows; d++) {
        const row = [];
        for (let v = 1; v <= n; v++) if (depth[v] === d) row.push({target: node[v].b, stroke: color});
        scene.tween(row, 0.18);
    }
    return deepest;
}
function reset() {
    const all = [];
    for (let v = 1; v <= n; v++) all.push({target: node[v].b, stroke: C.line, fill: C.panel});
    scene.tween(all, 0.25);
}

scene.wait(0.4);
say("hold any node — the rest hangs below", 0, -2.75, C.mute, [0.5, 0.5]);
const [, depth] = hang(3);
const a = sweep(depth, C.accent);
scene.tween({target: node[a].b, stroke: C.danger, fill: "#3a1d1d"}, 0.3);
say("the lowest node is one end of the diameter", 0, -2.75, C.danger, [0.5, 0.5]);
scene.wait(0.8);

// pick that node up: every node falls into its new row at once
reset();
scene.tween({target: node[a].b, stroke: C.danger, fill: "#3a1d1d"}, 0.2);
const [pos2, depth2, parent2] = hang(a);
const moves = [];
for (let v = 1; v <= n; v++) moves.push(toward(node[v], pos2[v][0], pos2[v][1]));
moves.push({target: hand, from: [pos2[a][0], 3.2], to: [pos2[a][0], pos2[a][1] + 0.24]});
say("pick it up", 0, -2.75, C.danger, [0.5, 0.5]);
scene.tween(moves, 1.4, "ease_in_out");
scene.wait(0.3);

const b = sweep(depth2, C.accent);
scene.tween({target: node[b].b, stroke: C.ok, fill: "#1c3a26"}, 0.3);

// the path from the bottom back up to the hand is the diameter
const path = [];
for (let v = b; parent2[v]; v = parent2[v]) {
    EDGES.forEach(([p, q], i) => {
        if ((p === v && q === parent2[v]) || (q === v && p === parent2[v]))
            path.push({target: link[i], stroke: C.warn, width: 4});
    });
}
scene.tween(path, 0.5);

// measure top to bottom
const mx = 4.6;
const ruler = scene.arrow({id: "ruler", from: [mx, pos2[a][1]], to: [mx, pos2[b][1]], stroke: C.warn, width: 2, tip: 10, layer: L.label});
const ticks = [
    scene.line({id: "tick:top", from: [mx - 0.2, pos2[a][1]], to: [mx + 0.2, pos2[a][1]], stroke: C.warn, width: 2, layer: L.label}),
    scene.line({id: "tick:bottom", from: [mx - 0.2, pos2[b][1]], to: [mx + 0.2, pos2[b][1]], stroke: C.warn, width: 2, layer: L.label}),
];
scene.create(ticks, 0.25);
scene.create(ruler, 0.6);
say("diameter = " + depth2[b] + " edges", mx - 0.35, (pos2[a][1] + pos2[b][1]) / 2, C.warn, [1, 0.5]);
scene.wait(2.4);
return scene;`;

const FIB = PRELUDE + `
const n = 9;
const dp = [0, 1];
for (let i = 2; i <= n; i++) dp[i] = dp[i - 1] + dp[i - 2];
const x0 = -4.5, w = 0.9, y = -0.4;
const X = (i) => x0 + (i + 0.5) * w;
const cells = [];
for (let i = 0; i <= n; i++) {
    cells[i] = scene.rectangle({id: "dp:" + i, center: [X(i), y], size: [w * 0.9, 0.62], corner: 0.03,
                                fill: C.panel, stroke: C.line, width: 1.2, layer: L.subject});
    T(scene, "dp:" + i + ":index", String(i), X(i), y - 0.55, 10, C.mute);
}
T(scene, "dp:title", "dp", x0 - 0.05, y + 0.6, 12, C.mute, [0, 0.5]);
function put(i, color) {
    const t = T(scene, "dp:" + i + ":value", String(dp[i]), X(i), y, 15, color ?? C.text);
    scene.fadeIn(t, {scale: 0.7, duration: 0.25});
}
scene.wait(0.3);
put(0, C.mute);
put(1);
// two read arcs, i-1 (low) and i-2 (high), slide right one cell per step
const a1 = scene.curve({id: "read:1", from: [X(1), y + 0.32], control1: [X(1), y + 0.8], control2: [X(2) - 0.12, y + 0.8],
                        to: [X(2) - 0.12, y + 0.34], stroke: C.accent, width: 2, layer: L.edge});
const a2 = scene.curve({id: "read:2", from: [X(0), y + 0.32], control1: [X(0), y + 1.35], control2: [X(2) + 0.12, y + 1.35],
                        to: [X(2) + 0.12, y + 0.34], stroke: C.alt, width: 2, layer: L.edge});
for (let i = 2; i <= n; i++) {
    const reads = () => scene.tween([{target: cells[i - 1], stroke: C.accent}, {target: cells[i - 2], stroke: C.alt}], 0.2);
    if (i === 2) together(reads, () => scene.create([a1, a2], 0.4, "smooth"));
    else together(reads, () => scene.tween([{target: a1, shift: [w, 0]}, {target: a2, shift: [w, 0]}], 0.35, "smooth"));
    put(i);
    scene.tween([{target: cells[i - 1], stroke: C.line}, {target: cells[i - 2], stroke: C.line},
                {target: cells[i], stroke: C.ok}], 0.2);
}
scene.uncreate([a1, a2], 0.3);
say("dp[" + n + "] = " + dp[n], X(n), y - 1.05, C.ok);
scene.indicate(cells[n], {color: C.ok, scale: 1.1, duration: 0.6});
scene.wait(2.4);
return scene;`;

export default {
  "bubble-sort": BUBBLE,
  "insertion-sort": INSERTION,
  "quick-sort": QUICK,
  "merge-sort": MERGE,
  "binary-search": BINARY,
  "bfs": BFS,
  "dfs": DFS,
  "tree-diameter": DIAMETER,
  "fibonacci-dp": FIB,
};
