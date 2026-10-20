// JavaScript versions of the "diagrams" examples, keyed by episode id.
import {PRELUDE as SHARED} from "./prelude.js";

const PRELUDE = SHARED + `const go = (o, x, y, d = 0.45, arc) => scene.tween(toward(o, x, y, arc), d, "smooth");
`;

const DIAGRAM = `
// vis.diagram is Lua-only; this builds the same picture from primitives.
// Nodes are a themed box with a label (and an optional detail line); edges are arrows between side centers.
function diagram(id, nodeSize) {
    const nodes = {}, edges = [];
    return {
        node(n) { nodes[n.id] = {...n, size: n.size ?? nodeSize}; return nodes[n.id]; },
        connect(e) { edges.push(e); },
        build() {
            const routes = scene.group({id: id + ":routes"});
            const portOf = (n, port) => {
                const [x, y] = n.position, [w, h] = n.size;
                return {left: [x - w / 2, y], right: [x + w / 2, y], top: [x, y + h / 2], bottom: [x, y - h / 2]}[port];
            };
            for (const e of edges) {
                routes.arrow({id: id + ":edge:" + e.id, from: portOf(e.from, e.fromPort ?? "right"),
                              to: portOf(e.to, e.toPort ?? "left"), stroke: "muted", layer: L.edge});
            }
            const nodeGroups = scene.group({id: id + ":nodes"});
            for (const n of Object.values(nodes)) {
                const [x, y] = n.position, [w, h] = n.size;
                n.group = nodeGroups.group({id: id + ":node:" + n.id});
                n.body = n.group.rectangle({id: id + ":node:" + n.id + ":body", center: [x, y], size: [w, h], corner: 0.08,
                                            fill: "background", stroke: "foreground", layer: L.subject});
                n.label = n.group.text({id: id + ":node:" + n.id + ":label", text: n.label, role: "text", color: "foreground",
                                        point: [x, n.detail ? y + h * 0.27 : y], layer: L.label});
                if (n.detail) {
                    n.detailText = n.group.text({id: id + ":node:" + n.id + ":detail", text: n.detail, role: "code",
                                                 color: "muted", point: [x, y - h * 0.27], layer: L.label});
                }
            }
        },
    };
}
`;

const HTTP = PRELUDE + DIAGRAM + `
// topology: one request path, DNS beside the client, cache and DB behind the server
const W = 1.4, H = 1.06;
const d = diagram("http", [W, H]);
const N = {};
for (const s of [
    {id: "client", label: "Client", detail: "browser", x: -4.55, y: 0},
    {id: "dns", label: "DNS", detail: "resolver", x: -4.55, y: 1.95},
    {id: "lb", label: "Load balancer", detail: "L7 proxy", x: -1.65, y: 0, w: 1.75},
    {id: "server", label: "Server", detail: "app :8080", x: 1.3, y: 0},
    {id: "cache", label: "Cache", detail: "empty", x: 4.5, y: 0},
    {id: "db", label: "Database", detail: "users table", x: 1.3, y: -1.95},
]) {
    N[s.id] = d.node({id: s.id, label: s.label, detail: s.detail, position: [s.x, s.y], size: [s.w ?? W, H]});
}
d.connect({id: "resolve", from: N.client, to: N.dns, fromPort: "top", toPort: "bottom"});
d.connect({id: "forward", from: N.client, to: N.lb});
d.connect({id: "route", from: N.lb, to: N.server});
d.connect({id: "lookup", from: N.server, to: N.cache});
d.connect({id: "query", from: N.server, to: N.db, fromPort: "bottom", toPort: "top"});
d.build();
// park points beside a node, clear of the arrow tip
function side(id, dir) {
    const [x, y] = N[id].position, w = N[id].size[0], g = 0.42;
    if (dir === "left") return [x - w / 2 - g, y];
    if (dir === "right") return [x + w / 2 + g, y];
    if (dir === "top") return [x, y + H / 2 + 0.3];
    return [x, y - H / 2 - 0.3];
}
const hop = (o, id, dir, d, arc) => go(o, ...side(id, dir), d, arc);
const mark = (id, color) => scene.stroke(N[id].body, color, 0.2);
function tok(id, s, color, x, y) {
    const o = token(id, s, x, y, {size: [0.56, 0.3], text: 13, stroke: color});
    scene.fadeIn(o.g, {duration: 0.25});
    return o;
}

scene.wait(0.6);
// request 1: resolve the name first
const req = tok("req1", "GET", C.accent, ...side("client", "top"));
say("1  look up api.example.com", -2.6, 1.95, C.warn, [0, 0.5]);
hop(req, "dns", "bottom", 0.55);
mark("dns", C.accent);
say("DNS answers 10.0.0.5", -2.6, 1.95, C.warn, [0, 0.5]);
scene.wait(0.5);
hop(req, "client", "top", 0.5);
together(() => mark("dns", C.line), () => hop(req, "client", "right", 0.4, -1.8));
// through the balancer to one server
say("2  balancer picks a server", -1.65, 1.95);
hop(req, "lb", "left", 0.5);
mark("lb", C.accent);
hop(req, "lb", "right", 0.4, -2.3);
together(() => mark("lb", C.line), () => hop(req, "server", "left", 0.5));
mark("server", C.accent);
hop(req, "server", "right", 0.4, -2.3);
// cache miss -> DB
say("3  check the cache", 2.9, 1.95);
hop(req, "cache", "left", 0.5);
mark("cache", C.danger);
say("cache miss", 4.5, 1.95, C.danger);
scene.wait(0.5);
hop(req, "server", "right", 0.45);
together(() => mark("cache", C.line), () => hop(req, "server", "bottom", 0.45, -1.8));
say("4  query the database", 1.85, -0.95, C.warn, [0, 0.5]);
hop(req, "db", "top", 0.45);
mark("db", C.ok);
scene.wait(0.4);
hop(req, "server", "bottom", 0.45);
// the server turns the row into a response and fills the cache
together(() => scene.fadeOut(req.g, {duration: 0.2}), () => mark("db", C.line));
const resp = tok("resp1", "200", C.ok, req.x, req.y);
say("5  store the row in cache", 2.9, 1.95, C.ok);
together(() => scene.fadeOut(N.cache.detailText, {duration: 0.2}), () => mark("cache", C.ok));
const filled = T(scene, "node:cache:detail:filled", "users/7", N.cache.position[0], N.cache.position[1] - 0.29, 15, C.ok);
scene.fadeIn(filled, {duration: 0.25});
scene.wait(0.4);
// response back along the same path
say(null);
hop(resp, "server", "left", 0.45, -1.8);
together(() => mark("server", C.line), () => mark("cache", C.line), () => hop(resp, "lb", "right", 0.5));
hop(resp, "lb", "left", 0.4, 2.3);
hop(resp, "client", "right", 0.5);
mark("client", C.ok);
say("200 OK  (DNS + miss + DB)", -4.95, -0.85, C.ok, [0, 0.5]);
scene.wait(0.9);
// request 2: name already known, cache now hits
together(() => scene.fadeOut(resp.g, {duration: 0.25}), () => mark("client", C.line));
const req2 = tok("req2", "GET", C.accent, ...side("client", "right"));
say("same request again  (DNS cached)", -4.95, -0.85, C.warn, [0, 0.5]);
hop(req2, "lb", "left", 0.4);
hop(req2, "lb", "right", 0.3, -2.3);
hop(req2, "server", "left", 0.4);
hop(req2, "server", "right", 0.3, -2.3);
hop(req2, "cache", "left", 0.4);
mark("cache", C.ok);
say("cache hit  no DB trip", 5.2, 1.95, C.ok, [1, 0.5]);
scene.wait(0.5);
scene.fadeOut(req2.g, {duration: 0.2});
const resp2 = tok("resp2", "200", C.ok, req2.x, req2.y);
hop(resp2, "server", "right", 0.4);
hop(resp2, "server", "left", 0.3, 2.3);
hop(resp2, "lb", "right", 0.4);
hop(resp2, "lb", "left", 0.3, 2.3);
hop(resp2, "client", "right", 0.4);
together(() => mark("client", C.ok), () => say("200 OK  skipped DNS + DB", -4.95, -0.85, C.ok, [0, 0.5]));
scene.wait(2.6);
return scene;`;

const SIGNUP = PRELUDE + `
// lanes are roles; columns are time. Each step belongs to one lane, messages cross lanes in order.
const lanes = [{id: "user", label: "User"}, {id: "browser", label: "Browser"},
               {id: "api", label: "API"}, {id: "email", label: "Email service"}];
const LH = 1.15, top = 2.4, x0 = -5.2, x1 = 5.2, div = -3.82;
const laneY = {};
lanes.forEach((ln, k) => {
    laneY[ln.id] = top - (k + 0.5) * LH;
    T(scene, "lane:" + ln.id, ln.label, x0 + 0.05, laneY[ln.id], 13, C.mute, [0, 0.5]);
});
for (let k = 0; k <= lanes.length; k++) {
    const y = top - k * LH;
    scene.line({id: "lane:rule:" + k, from: [x0, y], to: [x1, y], stroke: C.line, width: 1, layer: L.guide});
}
scene.line({id: "lane:divider", from: [div, top], to: [div, top - lanes.length * LH], stroke: C.line, width: 1, layer: L.guide});
// steps in time order: lane, label, and the payload carried to the next step
const steps = [
    {lane: "user", label: "submit form", send: "email+pw"},
    {lane: "browser", label: "POST /signup", send: "JSON"},
    {lane: "api", label: "create user", send: "token"},
    {lane: "email", label: "send link", send: "link"},
    {lane: "user", label: "click link", send: "click"},
    {lane: "browser", label: "GET /verify", send: "token"},
    {lane: "api", label: "set verified"},
];
const SW = 1.18, SH = 0.5, sx0 = -3.08, sdx = 1.27;
steps.forEach((st, i) => {
    st.x = sx0 + i * sdx; st.y = laneY[st.lane];
    st.box = scene.rectangle({id: "step:" + (i + 1), center: [st.x, st.y], size: [SW, SH], corner: 0.06,
                              fill: C.panel, stroke: C.line, width: 1.5, layer: L.subject});
    st.text = T(scene, "step:" + (i + 1) + ":label", st.label, st.x, st.y, 12, C.mute);
});
// each message: right edge of one step, along its lane, then across lanes into the next step
function message(a, b) {
    const ex = b.x, ey = b.y + (b.y < a.y ? SH / 2 : -SH / 2);
    return [[a.x + SW / 2, a.y], [ex, a.y], [ex, ey]];
}
const activate = (st, color) =>
    together(() => scene.stroke(st.box, color, 0.2), () => scene.fill(st.text, C.text, 0.2));
scene.wait(0.6);
activate(steps[0], C.accent);
say("user fills in the form", steps[0].x - SW / 2, -2.68, C.warn, [0, 0.5]);
scene.wait(0.5);
const notes = {
    2: ["browser sends the request", C.warn],
    3: ["row saved  status = pending", C.warn],
    4: ["API hands off: email is async", C.warn],
    5: ["time passes  user opens inbox", C.warn],
    6: ["link hits the verify endpoint", C.warn],
    7: ["status = verified", C.ok],
};
for (let i = 1; i < steps.length; i++) {
    const a = steps[i - 1], b = steps[i];
    const pts = message(a, b);
    const r = scene.route({id: "msg:" + i, points: pts, tip: 9, stroke: C.mute, width: 1.5, layer: L.edge});
    scene.create(r, 0.45, "ease_out");
    // payload starts clear of the sender's box and stops before the receiver's
    const len = (pts[1][0] - pts[0][0]) + Math.abs(pts[2][1] - pts[1][1]);
    const tk = token("payload:" + i, a.send, pts[0][0] + 0.45, pts[0][1], {size: [0.74, 0.3], text: 11, stroke: C.accent});
    scene.fadeIn(tk.g, {duration: 0.15});
    scene.moveAlong(tk.g, r, {from: 0.45 / len, to: 1 - 0.36 / len, duration: 0.8, curve: "smooth"});
    together(() => scene.fadeOut(tk.g, {duration: 0.15}),
             () => scene.stroke(a.box, C.mute, 0.2),
             () => activate(b, i === steps.length - 1 ? C.ok : (i === 2 ? C.warn : C.accent)),
             () => {
                 const [text, color] = notes[i + 1];
                 const right = i + 1 >= 5;
                 say(text, right ? b.x + SW / 2 : b.x - SW / 2, -2.68, color, right ? [1, 0.5] : [0, 0.5]);
             });
    scene.wait(i === 4 ? 0.9 : 0.5);
}
// the pending row from step 3 is the one the last step flips
together(() => scene.stroke(steps[2].box, C.ok, 0.3), () => scene.stroke(steps[6].box, C.ok, 0.3));
scene.wait(2.6);
return scene;`;

const EVENTLOOP = PRELUDE + `
// the program and the real output order: A D C B
const code = [
    {src: 'console.log("A")'},
    {src: 'setTimeout(() => log("B"), 0)'},
    {src: 'Promise.resolve().then(() => log("C"))'},
    {src: 'console.log("D")'},
];
const cx0 = -5.05, cy0 = 2.15, cdy = 0.46;
scene.rectangle({id: "code:panel", center: [-3.25, 1.47], size: [4.0, 2.05], corner: 0.06,
                 fill: C.panel, stroke: C.line, width: 1, layer: L.region});
code.forEach((ln, i) => {
    ln.y = cy0 - i * cdy;
    T(scene, "code:" + (i + 1), ln.src, cx0, ln.y, 14, C.text, [0, 0.5]);
});
const cursor = scene.rectangle({id: "code:cursor", center: [-3.25, code[0].y], size: [3.85, 0.38], corner: 0.04,
                                fill: C.bar, stroke: C.none, layer: L.region + 1, opacity: 0});
// regions
function region(id, label, x, y, w, h) {
    scene.rectangle({id, center: [x, y], size: [w, h], corner: 0.06, fill: C.none,
                     stroke: C.line, width: 1, layer: L.region});
    T(scene, id + ":title", label, x - w / 2, y + h / 2 + 0.2, 12, C.mute, [0, 0.5]);
}
region("console", "console output", -3.25, -1.95, 4.0, 0.8);
region("stack", "call stack", 0.1, -0.35, 1.9, 4.1);
region("webapi", "Web APIs", 3.8, 1.6, 2.8, 1.0);
region("micro", "microtask queue", 3.8, 0.05, 2.8, 0.7);
region("task", "task queue", 3.8, -1.45, 2.8, 0.7);
scene.arrow({id: "loop:micro", from: [2.4, 0.05], to: [1.08, 0.05], stroke: C.mute, width: 1.5, tip: 9, layer: L.edge});
scene.arrow({id: "loop:task", from: [2.4, -1.45], to: [1.08, -1.45], stroke: C.mute, width: 1.5, tip: 9, layer: L.edge});
// call stack frames grow from the bottom
const frames = [];
let fid = 0;
const sx = 0.1, sb = -2.1, fstep = 0.5;
function push(label, color = C.accent) {
    fid++;
    const f = token("frame:" + fid, label, sx, sb + frames.length * fstep, {size: [1.65, 0.4], text: 13, stroke: color, layer: L.subject});
    scene.fadeIn(f.g, {shift: [0, 0.2], duration: 0.25});
    frames.push(f);
    return f;
}
const pop = () => scene.fadeOut(frames.pop().g, {shift: [0.3, 0], duration: 0.22});
let outN = 0;
function out(s, from) {
    outN++;
    const o = token("out:" + s, s, -1.25, from.y, {size: [0.42, 0.38], text: 14, stroke: C.ok});
    scene.fadeIn(o.g, {duration: 0.15});
    go(o, -4.75 + (outN - 1) * 0.6, -1.95, 0.55);
}
function line(i) {
    scene.tween({target: cursor, position: [-3.25, code[i].y], opacity: 1}, 0.25, "smooth");
}
function cb(id, label, color, x, y) {
    const o = token(id, label, x, y, {size: [1.1, 0.36], text: 13, stroke: color});
    scene.fadeIn(o.g, {duration: 0.15});
    return o;
}

const SX = -5.05, SY = -0.05, QX = 3.8, GX = 1.72;
// callbacks travel through the gutter between the stack and the queues
function via(o, x, y) {
    if (Math.abs(o.x - GX) > 0.01) go(o, GX, o.y, 0.25);
    go(o, GX, y, 0.35);
    go(o, x, y, 0.3);
}
scene.wait(0.6);
push("script", C.mute);
// line 1: synchronous
line(0);
let f = push('log("A")');
out("A", f);
pop();
// line 2: setTimeout hands its callback to the timer
line(1);
f = push("setTimeout");
const cbB = cb("cb:B", "() => B", C.warn, GX, f.y);
say("setTimeout: timer starts, 0 ms", SX, SY, C.warn, [0, 0.5]);
via(cbB, QX, 1.55);
pop();
say("timer done: callback waits as a task", SX, SY, C.warn, [0, 0.5]);
via(cbB, QX, -1.45);
// line 3: already-resolved promise queues a microtask
scene.wait(0.4);
line(2);
f = push(".then");
const cbC = cb("cb:C", "() => C", C.alt, GX, f.y);
say("resolved promise: then is a microtask", SX, SY, C.alt, [0, 0.5]);
via(cbC, QX, 0.05);
pop();
// line 4: synchronous
line(3);
say(null);
scene.wait(0.4);
f = push('log("D")');
out("D", f);
pop();
// script ends: stack empties, microtasks drain before the next task
scene.fade(cursor, 0, 0.25);
pop();
say("stack empty: microtasks run first", SX, SY, C.alt, [0, 0.5]);
scene.wait(0.4);
via(cbC, sx, sb);
out("C", cbC);
scene.fadeOut(cbC.g, {shift: [0.3, 0], duration: 0.22});
say("then the next task", SX, SY, C.warn, [0, 0.5]);
scene.wait(0.4);
via(cbB, sx, sb);
out("B", cbB);
scene.fadeOut(cbB.g, {shift: [0.3, 0], duration: 0.22});
say("printed A D C B, not A B C D", SX, SY, C.ok, [0, 0.5]);
scene.wait(2.6);
return scene;`;

const CI = PRELUDE + DIAGRAM + `
const W = 1.55, H = 1.06, Y = 0.55;
const d = diagram("ci", [W, H]);
const spec = [
    {id: "push", label: "Push", detail: "git push", x: -4.3},
    {id: "build", label: "Build", detail: "compile", x: -2.15},
    {id: "test", label: "Test", detail: "unit tests", x: 0},
    {id: "package", label: "Package", detail: "image", x: 2.15},
    {id: "deploy", label: "Deploy", detail: "staging", x: 4.3},
];
const N = {};
for (const s of spec) N[s.id] = d.node({id: s.id, label: s.label, detail: s.detail, position: [s.x, Y]});
// wider than the stages so its longer detail stays inside the box
N.report = d.node({id: "report", label: "Report", detail: "notify author", position: [0, -1.6], size: [2.0, H]});
for (let k = 0; k < spec.length - 1; k++) {
    d.connect({id: spec[k].id + "-" + spec[k + 1].id, from: N[spec[k].id], to: N[spec[k + 1].id]});
}
d.connect({id: "test-report", from: N.test, to: N.report, fromPort: "bottom", toPort: "top"});
d.build();
const mark = (id, color, dur = 0.2) => scene.stroke(N[id].body, color, dur);
// test detail is the stage's state: swap its Text in place
let det = N.test.detailText, detN = 0;
function setTest(s, color) {
    scene.fadeOut(det, {duration: 0.15});
    detN++;
    det = T(scene, "test:detail:" + detN, s, N.test.position[0], N.test.position[1] - 0.29, 15, color);
    scene.fadeIn(det, {duration: 0.2});
}
const PY = Y + H / 2 + 0.32;
function run(sha, color) {
    const o = token("commit:" + sha, sha, N.push.position[0], PY, {size: [0.8, 0.32], text: 13, stroke: color});
    scene.fadeIn(o.g, {duration: 0.25});
    return o;
}
function stage(o, id, result) {
    go(o, N[id].position[0], PY, 0.45);
    mark(id, C.accent);
    scene.wait(0.35);
    if (result) mark(id, result);
}
scene.wait(1.0);
// run 1: commit a1f3 breaks a test
const r1 = run("a1f3", C.accent);
say("run 1", -5.1, 2.55, C.warn, [0, 0.5]);
mark("push", C.ok);
stage(r1, "build", C.ok);
stage(r1, "test");
together(() => mark("test", C.danger), () => setTest("1 failed", C.danger),
         () => say("test fails  pipeline stops", 0.95, 2.55, C.danger, [0, 0.5]));
together(() => mark("report", C.danger, 0.3),
         () => scene.fade(N.package.group, 0.35, 0.3), () => scene.fade(N.deploy.group, 0.35, 0.3));
scene.wait(1.6);
// run 2: the fix goes through every stage
together(() => scene.fadeOut(r1.g, {duration: 0.25}),
         () => { for (const id of ["push", "build", "test", "report"]) mark(id, C.line, 0.25); },
         () => scene.fade(N.package.group, 1, 0.25), () => scene.fade(N.deploy.group, 1, 0.25),
         () => say("run 2  fix pushed", -5.1, 2.55, C.warn, [0, 0.5]));
setTest("unit tests", C.mute);
const r2 = run("b7c2", C.ok);
mark("push", C.ok);
stage(r2, "build", C.ok);
stage(r2, "test");
together(() => mark("test", C.ok), () => setTest("42 passed", C.ok));
stage(r2, "package", C.ok);
stage(r2, "deploy", C.ok);
say("all green  deployed to staging", 5.2, 2.55, C.ok, [1, 0.5]);
scene.wait(2.6);
return scene;`;

const GIT = PRELUDE + `
// commit graph: x is time, y is the branch a commit was made on (hashes illustrative)
const Y = {main: 0.9, feature: -0.9};
const R = 0.26;
function commit(id, hash, lane, x, color) {
    const c = {id, x, y: Y[lane], lane};
    c.g = scene.group({id: "commit:" + id});
    c.dot = c.g.circle({id: "commit:" + id + ":dot", center: [x, c.y], radius: R, fill: C.panel,
                        stroke: color, width: 2.5, layer: L.subject});
    c.hash = T(c.g, "commit:" + id + ":hash", hash, x, lane === "main" ? c.y - 0.5 : c.y + 0.5, 12, C.mute);
    return c;
}
function edge(a, b) {
    const dx = b.x - a.x, dy = b.y - a.y, n = Math.hypot(dx, dy);
    const ux = dx / n, uy = dy / n;
    return scene.line({id: "parent:" + b.id + ":" + a.id, from: [a.x + ux * R, a.y + uy * R],
                       to: [b.x - ux * R, b.y - uy * R], stroke: C.line, width: 2.5, layer: L.edge});
}
const X = [-4.4, -2.75, -1.1, 0.55, 2.2, 3.85];
const c1 = commit("c1", "3f2a", "main", X[0], C.accent);
const c2 = commit("c2", "9b1e", "main", X[1], C.accent);
edge(c1, c2);
// branch pointers are labels that move; HEAD sits on the checked-out branch
const pill = (id, s, color, x, y) => token(id, s, x, y, {size: [1.0, 0.34], text: 13, stroke: color});
const PY = 0.62;
const main = pill("ref:main", "main", C.accent, c2.x, c2.y + PY);
const HX = -1.1;
let head = pill("ref:head", "HEAD", C.warn, c2.x + HX, c2.y + PY);
function newcommit(id, hash, lane, x, color, ...parents) {
    const c = commit(id, hash, lane, x, color);
    const es = parents.map((p) => edge(p, c));
    scene.set({target: c.g, opacity: 0});
    for (const e of es) scene.set({target: e, progress: 0});
    for (const e of es) scene.create(e, 0.35, "ease_out");
    scene.fadeIn(c.g, {duration: 0.3});
    return c;
}
const SX = -5.1, SY = 2.5;
scene.wait(1.0);
// 1. new branch: a second pointer at the same commit
say("git switch -c feature", SX, SY, C.warn, [0, 0.5]);
const feature = pill("ref:feature", "feature", C.alt, c2.x, -0.05);
scene.fadeIn(feature.g, {duration: 0.3});
scene.wait(0.3);
// switching only moves HEAD to the other pointer
function switchTo(ref) {
    scene.fadeOut(head.g, {duration: 0.2});
    head = pill("ref:head:" + ref.v, "HEAD", C.warn, ref.x + HX, ref.y);
    scene.fadeIn(head.g, {duration: 0.25});
}
switchTo(feature);
scene.wait(0.5);
// 2. commits on feature move only the feature pointer
say("git commit  (on feature)", SX, SY, C.warn, [0, 0.5]);
const f1 = newcommit("f1", "c47d", "feature", X[2], C.alt, c2);
together(() => go(feature, f1.x, f1.y - PY, 0.5), () => go(head, f1.x + HX, f1.y - PY, 0.5));
scene.wait(0.3);
const f2 = newcommit("f2", "e810", "feature", X[3], C.alt, f1);
together(() => go(feature, f2.x, f2.y - PY, 0.45), () => go(head, f2.x + HX, f2.y - PY, 0.45));
scene.wait(0.5);
// 3. back on main, a hotfix lands: history diverges
say("git switch main", SX, SY, C.warn, [0, 0.5]);
switchTo(main);
scene.wait(0.3);
say("git commit  (hotfix on main)", SX, SY, C.warn, [0, 0.5]);
const c3 = newcommit("c3", "1d06", "main", X[4], C.accent, c2);
together(() => go(main, c3.x, c3.y + PY, 0.45), () => go(head, c3.x + HX, c3.y + PY, 0.45));
say("main and feature have diverged", SX, SY, C.warn, [0, 0.5]);
scene.wait(1.4);
// 4. merge: one new commit with two parents
say("git merge feature", SX, SY, C.warn, [0, 0.5]);
const m = newcommit("m", "77ac", "main", X[5], C.ok, c3, f2);
together(() => go(main, m.x, m.y + PY, 0.45), () => go(head, m.x + HX, m.y + PY, 0.45));
say("merge commit: two parents", SX, SY, C.ok, [0, 0.5]);
scene.wait(2.6);
return scene;`;

const FLOW = PRELUDE + `
// topology: three sources fan into one log, one job reads it and writes to two sinks
function node(id, label, detail, x, y, w = 1.5, h = 0.9) {
    const box = scene.rectangle({id, center: [x, y], size: [w, h], corner: 0.06,
                                 fill: C.panel, stroke: C.line, width: 1.5, layer: L.subject});
    T(scene, id + ":label", label, x, y + 0.14, 16);
    return [box, T(scene, id + ":detail", detail, x, y - 0.18, 13, C.mute)];
}
const box = {
    clicks: node("clicks", "Clicks", "web", -4.2, 1.6)[0],
    orders: node("orders", "Orders", "api", -4.2, 0)[0],
    sensors: node("sensors", "Sensors", "iot", -4.2, -1.6)[0],
    log: node("log", "Event log", "3 partitions", -1.3, 0, 1.6, 1.3)[0],
    warehouse: node("warehouse", "Warehouse", "hourly", 4.2, 1.2)[0],
    dashboard: node("dashboard", "Dashboard", "live", 4.2, -1.2)[0],
};
let jobDetail;
[box.job, jobDetail] = node("job", "Stream job", "1 worker", 1.4, 0, 1.6, 0.9);
// every edge is a dashed route; moving its dash offset makes the dashes travel toward the tip
const edge = (id, points) =>
    scene.route({id, points, stroke: C.line, width: 2, tip: 10, dash: [9, 7], layer: L.edge});
const E = {};
for (const [id, y] of [["clicks", 1.6], ["orders", 0], ["sensors", -1.6]]) {
    E[id] = edge("e:" + id, [[-3.45, y], [-2.75, y], [-2.75, y * 0.3], [-2.1, y * 0.3]]);
}
E.read = edge("e:read", [[-0.5, 0], [0.6, 0]]);
E.warehouse = edge("e:warehouse", [[2.2, 0.2], [2.8, 0.2], [2.8, 1.2], [3.45, 1.2]]);
E.dashboard = edge("e:dashboard", [[2.2, -0.2], [2.8, -0.2], [2.8, -1.2], [3.45, -1.2]]);
const ORDER = ["clicks", "orders", "sensors", "read", "warehouse", "dashboard"];

// run every edge's dashes for dur seconds; speed[id] is pixels per second (default 48)
const offset = {};
function flow(dur, speed = {}) {
    together(...ORDER.map((id) => {
        offset[id] = (offset[id] ?? 0) - (speed[id] ?? 48) * dur;
        const o = offset[id];
        return () => scene.tween({target: E[id], dash_offset: o}, dur, "linear");
    }));
}
const tint = (ids, color) => together(...ids.map((id) => () => scene.stroke(E[id], color, 0.3)));

scene.wait(0.6);
// 1. events stream in at a steady rate
say("events stream in", 0, 2.55, C.accent);
tint(ORDER, C.accent);
flow(3);
// 2. a spike: sources speed up, but the single worker cannot keep pace with the dashboard
say("traffic spike  dashboard lags", 0, 2.55, C.warn);
together(() => tint(["dashboard"], C.warn), () => scene.stroke(box.dashboard, C.warn, 0.3));
flow(3, {clicks: 96, orders: 96, sensors: 96, read: 60, warehouse: 48, dashboard: 14});
// 3. a second worker joins and the whole path runs fast
together(() => scene.fadeOut(jobDetail, {duration: 0.2}),
         () => scene.fadeIn(T(scene, "job:detail2", "2 workers", 1.4, -0.18, 13, C.ok), {duration: 0.3}),
         () => scene.stroke(box.job, C.ok, 0.3),
         () => say("second worker  caught up", 0, 2.55, C.ok));
together(() => tint(["dashboard"], C.accent), () => scene.stroke(box.dashboard, C.line, 0.3));
flow(3.4, {clicks: 96, orders: 96, sensors: 96, read: 96, warehouse: 72, dashboard: 72});
return scene;`;

export default {
  "http-lifecycle": HTTP,
  "signup-swimlane": SIGNUP,
  "event-loop": EVENTLOOP,
  "git-merge": GIT,
  "ci-pipeline": CI,
  "data-flow": FLOW,
};
