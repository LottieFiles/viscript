// Demo collection: system and process diagrams.
// Same conventions as algorithms.js: no in-scene title (the page prints title + blurb), t = 0 shows
// the structure, tokens with identity travel along real edges, one short phrase names each step.
import {PRELUDE as SHARED} from "./prelude.js";

const PRELUDE = SHARED + `local function go(o, x, y, d, arc) scene:tween(toward(o, x, y, arc), {duration = d or 0.45, curve = "smooth"}) end
`;

// ---------------------------------------------------------------- 1. HTTP request lifecycle
const HTTP = PRELUDE + `
-- topology: one request path, DNS beside the client, cache and DB behind the server
local W, H = 1.4, 1.06
local spec = {
    {id = "client", label = "Client", detail = "browser", x = -4.55, y = 0},
    {id = "dns", label = "DNS", detail = "resolver", x = -4.55, y = 1.95},
    {id = "lb", label = "Load balancer", detail = "L7 proxy", x = -1.65, y = 0, w = 1.75},
    {id = "server", label = "Server", detail = "app :8080", x = 1.3, y = 0},
    {id = "cache", label = "Cache", detail = "empty", x = 4.5, y = 0},
    {id = "db", label = "Database", detail = "users table", x = 1.3, y = -1.95},
}
local d = vis.diagram(scene, {id = "http", layout = "manual", node_size = {W, H}, route_width = 0.025,
                                arrow_length = 0.14, arrow_width = 0.14, corner = 0.08,
                                layers = {zones = L.region, routes = L.edge, nodes = L.subject, annotations = L.label}})
local N, P = {}, {}
for _, s in ipairs(spec) do
    s.w = s.w or W
    N[s.id] = d:node {id = s.id, label = s.label, detail = s.detail, position = {s.x, s.y}, size = {s.w, H}}
    P[s.id] = s
end
d:connect {id = "resolve", from = N.client, to = N.dns, from_port = "top", to_port = "bottom", route = "straight"}
d:connect {id = "forward", from = N.client, to = N.lb, route = "straight"}
d:connect {id = "route", from = N.lb, to = N.server, route = "straight"}
d:connect {id = "lookup", from = N.server, to = N.cache, route = "straight"}
d:connect {id = "query", from = N.server, to = N.db, from_port = "bottom", to_port = "top", route = "straight"}
local B = d:build()
-- node label/detail Text come from the diagram (theme font = Pretendard)
local body = {}
for _, s in ipairs(spec) do body[s.id] = B:body(N[s.id]) end
-- park points beside a node, clear of the arrow tip
local function side(id, dir)
    local s, g = P[id], 0.42
    if dir == "left" then return s.x - s.w / 2 - g, s.y
    elseif dir == "right" then return s.x + s.w / 2 + g, s.y
    elseif dir == "top" then return s.x, s.y + H / 2 + 0.3
    else return s.x, s.y - H / 2 - 0.3 end
end
local function hop(o, id, dir, d, arc) local x, y = side(id, dir); go(o, x, y, d, arc) end
local function mark(id, color) scene:stroke(body[id], color, {duration = 0.2}) end
local TS = {size = {0.56, 0.3}, text = 13}
local function tok(id, s, color, x, y)
    local o = token(id, s, x, y, {size = TS.size, text = TS.text, stroke = color})
    scene:fade_in(o.g, {duration = 0.25})
    return o
end

scene:wait(0.6)
-- request 1: resolve the name first
local cx, cy = side("client", "top")
local req = tok("req1", "GET", C.accent, cx, cy)
say("1  look up api.example.com", -2.6, 1.95, C.warn, {0, 0.5})
hop(req, "dns", "bottom", 0.55)
mark("dns", C.accent)
say("DNS answers 10.0.0.5", -2.6, 1.95, C.warn, {0, 0.5})
scene:wait(0.5)
hop(req, "client", "top", 0.5)
together(function() mark("dns", C.line) end, function() hop(req, "client", "right", 0.4, -1.8) end)
-- through the balancer to one server
say("2  balancer picks a server", -1.65, 1.95)
hop(req, "lb", "left", 0.5)
mark("lb", C.accent)
hop(req, "lb", "right", 0.4, -2.3)
together(function() mark("lb", C.line) end, function() hop(req, "server", "left", 0.5) end)
mark("server", C.accent)
hop(req, "server", "right", 0.4, -2.3)
-- cache miss -> DB
say("3  check the cache", 2.9, 1.95)
hop(req, "cache", "left", 0.5)
mark("cache", C.danger)
say("cache miss", 4.5, 1.95, C.danger)
scene:wait(0.5)
hop(req, "server", "right", 0.45)
together(function() mark("cache", C.line) end, function() hop(req, "server", "bottom", 0.45, -1.8) end)
say("4  query the database", 1.85, -0.95, C.warn, {0, 0.5})
hop(req, "db", "top", 0.45)
mark("db", C.ok)
scene:wait(0.4)
hop(req, "server", "bottom", 0.45)
-- the server turns the row into a response and fills the cache
together(function() scene:fade_out(req.g, {duration = 0.2}) end, function() mark("db", C.line) end)
local resp = tok("resp1", "200", C.ok, req.x, req.y)
say("5  store the row in cache", 2.9, 1.95, C.ok)
local old = B:detail(N.cache)
local ox, oy = P.cache.x, P.cache.y - 0.29
together(function() scene:fade_out(old, {duration = 0.2}) end, function() mark("cache", C.ok) end)
local filled = T(scene, "node:cache:detail:filled", "users/7", ox, oy, 15, C.ok)
scene:fade_in(filled, {duration = 0.25})
scene:wait(0.4)
-- response back along the same path
say(nil)
hop(resp, "server", "left", 0.45, -1.8)
together(function() mark("server", C.line) end, function() mark("cache", C.line) end,
         function() hop(resp, "lb", "right", 0.5) end)
hop(resp, "lb", "left", 0.4, 2.3)
hop(resp, "client", "right", 0.5)
mark("client", C.ok)
say("200 OK  (DNS + miss + DB)", -4.95, -0.85, C.ok, {0, 0.5})
scene:wait(0.9)
-- request 2: name already known, cache now hits
together(function() scene:fade_out(resp.g, {duration = 0.25}) end, function() mark("client", C.line) end)
local qx, qy = side("client", "right")
local req2 = tok("req2", "GET", C.accent, qx, qy)
say("same request again  (DNS cached)", -4.95, -0.85, C.warn, {0, 0.5})
hop(req2, "lb", "left", 0.4)
hop(req2, "lb", "right", 0.3, -2.3)
hop(req2, "server", "left", 0.4)
hop(req2, "server", "right", 0.3, -2.3)
hop(req2, "cache", "left", 0.4)
mark("cache", C.ok)
say("cache hit  no DB trip", 5.2, 1.95, C.ok, {1, 0.5})
scene:wait(0.5)
scene:fade_out(req2.g, {duration = 0.2})
local resp2 = tok("resp2", "200", C.ok, req2.x, req2.y)
hop(resp2, "server", "right", 0.4)
hop(resp2, "server", "left", 0.3, 2.3)
hop(resp2, "lb", "right", 0.4)
hop(resp2, "lb", "left", 0.3, 2.3)
hop(resp2, "client", "right", 0.4)
together(function() mark("client", C.ok) end, function() say("200 OK  skipped DNS + DB", -4.95, -0.85, C.ok, {0, 0.5}) end)
scene:wait(2.6)
return scene`;

// ---------------------------------------------------------------- 2. swimlane: sign-up with email verification
const SIGNUP = PRELUDE + `
-- lanes are roles; columns are time. Each step belongs to one lane, messages cross lanes in order.
local lanes = {{id = "user", label = "User"}, {id = "browser", label = "Browser"},
               {id = "api", label = "API"}, {id = "email", label = "Email service"}}
local LH, top, x0, x1, div = 1.15, 2.4, -5.2, 5.2, -3.82
local laneY = {}
for k, ln in ipairs(lanes) do
    laneY[ln.id] = top - (k - 0.5) * LH
    T(scene, "lane:" .. ln.id, ln.label, x0 + 0.05, laneY[ln.id], 13, C.mute, {0, 0.5})
end
for k = 0, #lanes do
    local y = top - k * LH
    scene:line {id = "lane:rule:" .. k, from = {x0, y}, to = {x1, y}, stroke = C.line, width = 1, layer = L.guide}
end
scene:line {id = "lane:divider", from = {div, top}, to = {div, top - #lanes * LH}, stroke = C.line, width = 1, layer = L.guide}
-- steps in time order: lane, label, and the payload carried to the next step
local steps = {
    {lane = "user", label = "submit form", send = "email+pw"},
    {lane = "browser", label = "POST /signup", send = "JSON"},
    {lane = "api", label = "create user", send = "token"},
    {lane = "email", label = "send link", send = "link"},
    {lane = "user", label = "click link", send = "click"},
    {lane = "browser", label = "GET /verify", send = "token"},
    {lane = "api", label = "set verified"},
}
local SW, SH, sx0, sdx = 1.18, 0.5, -3.08, 1.27
for i, st in ipairs(steps) do
    st.x, st.y = sx0 + (i - 1) * sdx, laneY[st.lane]
    st.box = scene:rectangle {id = "step:" .. i, center = {st.x, st.y}, size = {SW, SH}, corner = 0.06,
                              fill = C.panel, stroke = C.line, width = 1.5, layer = L.subject}
    st.text = T(scene, "step:" .. i .. ":label", st.label, st.x, st.y, 12, C.mute)
end
-- each message: right edge of one step, along its lane, then across lanes into the next step
local function message(a, b)
    local down = b.y < a.y
    local ex, ey = b.x, b.y + (down and SH / 2 or -SH / 2)
    return {{a.x + SW / 2, a.y}, {ex, a.y}, {ex, ey}}
end
local function activate(st, color)
    together(function() scene:stroke(st.box, color, {duration = 0.2}) end, function() scene:fill(st.text, C.text, {duration = 0.2}) end)
end
scene:wait(0.6)
activate(steps[1], C.accent)
say("user fills in the form", steps[1].x - SW / 2, -2.68, C.warn, {0, 0.5})
scene:wait(0.5)
local notes = {
    [2] = {"browser sends the request", C.warn},
    [3] = {"row saved  status = pending", C.warn},
    [4] = {"API hands off: email is async", C.warn},
    [5] = {"time passes  user opens inbox", C.warn},
    [6] = {"link hits the verify endpoint", C.warn},
    [7] = {"status = verified", C.ok},
}
for i = 1, #steps - 1 do
    local a, b = steps[i], steps[i + 1]
    local pts = message(a, b)
    local r = scene:route {id = "msg:" .. i, points = pts, tip = 9, stroke = C.mute, width = 1.5, layer = L.edge}
    scene:create(r, {duration = 0.45, curve = "ease_out"})
    -- payload starts clear of the sender's box and stops before the receiver's
    local len = (pts[2][1] - pts[1][1]) + math.abs(pts[3][2] - pts[2][2])
    local f0, f1 = 0.45 / len, 1 - 0.36 / len
    local tk = token("payload:" .. i, a.send, pts[1][1] + 0.45, pts[1][2], {size = {0.74, 0.3}, text = 11, stroke = C.accent})
    scene:fade_in(tk.g, {duration = 0.15})
    scene:move_along(tk.g, r, {from = f0, to = f1, duration = 0.8, curve = "smooth"})
    together(function() scene:fade_out(tk.g, {duration = 0.15}) end,
             function() scene:stroke(a.box, C.mute, {duration = 0.2}) end,
             function() activate(b, i == #steps - 1 and C.ok or (i == 2 and C.warn or C.accent)) end,
             function()
                 local n = notes[i + 1]
                 local nx, al = b.x - SW / 2, {0, 0.5}
                 if i + 1 >= 5 then nx, al = b.x + SW / 2, {1, 0.5} end
                 say(n[1], nx, -2.68, n[2], al)
             end)
    scene:wait(i == 4 and 0.9 or 0.5)
end
-- the pending row from step 3 is the one the last step flips
together(function() scene:stroke(steps[3].box, C.ok, {duration = 0.3}) end, function() scene:stroke(steps[7].box, C.ok, {duration = 0.3}) end)
scene:wait(2.6)
return scene`;

// ---------------------------------------------------------------- 3. event loop: setTimeout vs Promise.then
const EVENTLOOP = PRELUDE + `
-- the program and the real output order: A D C B
local code = {
    {src = 'console.log("A")'},
    {src = 'setTimeout(() => log("B"), 0)'},
    {src = 'Promise.resolve().then(() => log("C"))'},
    {src = 'console.log("D")'},
}
local cx0, cy0, cdy = -5.05, 2.15, 0.46
scene:rectangle {id = "code:panel", center = {-3.25, 1.47}, size = {4.0, 2.05}, corner = 0.06,
                 fill = C.panel, stroke = C.line, width = 1, layer = L.region}
for i, ln in ipairs(code) do
    ln.y = cy0 - (i - 1) * cdy
    T(scene, "code:" .. i, ln.src, cx0, ln.y, 14, C.text, {0, 0.5})
end
local cursor = scene:rectangle {id = "code:cursor", center = {-3.25, code[1].y}, size = {3.85, 0.38}, corner = 0.04,
                                fill = C.bar, stroke = C.none, layer = L.region + 1, opacity = 0}
-- regions
local function region(id, label, x, y, w, h)
    scene:rectangle {id = id, center = {x, y}, size = {w, h}, corner = 0.06, fill = C.none,
                     stroke = C.line, width = 1, layer = L.region}
    T(scene, id .. ":title", label, x - w / 2, y + h / 2 + 0.2, 12, C.mute, {0, 0.5})
end
region("console", "console output", -3.25, -1.95, 4.0, 0.8)
region("stack", "call stack", 0.1, -0.35, 1.9, 4.1)
region("webapi", "Web APIs", 3.8, 1.6, 2.8, 1.0)
region("micro", "microtask queue", 3.8, 0.05, 2.8, 0.7)
region("task", "task queue", 3.8, -1.45, 2.8, 0.7)
scene:arrow {id = "loop:micro", from = {2.4, 0.05}, to = {1.08, 0.05}, stroke = C.mute, width = 1.5, tip = 9, layer = L.edge}
scene:arrow {id = "loop:task", from = {2.4, -1.45}, to = {1.08, -1.45}, stroke = C.mute, width = 1.5, tip = 9, layer = L.edge}
-- call stack frames grow from the bottom
local frames, fid = {}, 0
local sx, sb, fstep = 0.1, -2.1, 0.5
local function push(label, color)
    fid = fid + 1
    local f = token("frame:" .. fid, label, sx, sb + #frames * fstep, {size = {1.65, 0.4}, text = 13, stroke = color or C.accent, layer = L.subject})
    scene:fade_in(f.g, {shift = {0, 0.2}, duration = 0.25})
    frames[#frames + 1] = f
    return f
end
local function pop()
    local f = table.remove(frames)
    scene:fade_out(f.g, {shift = {0.3, 0}, duration = 0.22})
end
local outN = 0
local function out(s, from)
    outN = outN + 1
    local o = token("out:" .. s, s, -1.25, from.y, {size = {0.42, 0.38}, text = 14, stroke = C.ok})
    scene:fade_in(o.g, {duration = 0.15})
    go(o, -4.75 + (outN - 1) * 0.6, -1.95, 0.55)
end
local function line(i)
    scene:tween({target = cursor, position = {-3.25, code[i].y}, opacity = 1}, {duration = 0.25, curve = "smooth"})
end
local function cb(id, label, color, x, y)
    local o = token(id, label, x, y, {size = {1.1, 0.36}, text = 13, stroke = color})
    scene:fade_in(o.g, {duration = 0.15})
    return o
end

local SX, SY, QX, GX = -5.05, -0.05, 3.8, 1.72
-- callbacks travel through the gutter between the stack and the queues
local function via(o, x, y)
    if math.abs(o.x - GX) > 0.01 then go(o, GX, o.y, 0.25) end
    go(o, GX, y, 0.35)
    go(o, x, y, 0.3)
end
scene:wait(0.6)
push("script", C.mute)
-- line 1: synchronous
line(1)
local f = push('log("A")')
out("A", f)
pop()
-- line 2: setTimeout hands its callback to the timer
line(2)
f = push("setTimeout")
local cbB = cb("cb:B", "() => B", C.warn, GX, f.y)
say("setTimeout: timer starts, 0 ms", SX, SY, C.warn, {0, 0.5})
via(cbB, QX, 1.55)
pop()
say("timer done: callback waits as a task", SX, SY, C.warn, {0, 0.5})
via(cbB, QX, -1.45)
-- line 3: already-resolved promise queues a microtask
scene:wait(0.4)
line(3)
f = push(".then")
local cbC = cb("cb:C", "() => C", C.alt, GX, f.y)
say("resolved promise: then is a microtask", SX, SY, C.alt, {0, 0.5})
via(cbC, QX, 0.05)
pop()
-- line 4: synchronous
line(4)
say(nil)
scene:wait(0.4)
f = push('log("D")')
out("D", f)
pop()
-- script ends: stack empties, microtasks drain before the next task
scene:fade(cursor, 0, {duration = 0.25})
pop()
say("stack empty: microtasks run first", SX, SY, C.alt, {0, 0.5})
scene:wait(0.4)
via(cbC, sx, sb)
out("C", cbC)
scene:fade_out(cbC.g, {shift = {0.3, 0}, duration = 0.22})
say("then the next task", SX, SY, C.warn, {0, 0.5})
scene:wait(0.4)
via(cbB, sx, sb)
out("B", cbB)
scene:fade_out(cbB.g, {shift = {0.3, 0}, duration = 0.22})
say("printed A D C B, not A B C D", SX, SY, C.ok, {0, 0.5})
scene:wait(2.6)
return scene`;

// ---------------------------------------------------------------- 4. git: branch, commit, merge
const GIT = PRELUDE + `
-- commit graph: x is time, y is the branch a commit was made on (hashes illustrative)
local Y = {main = 0.9, feature = -0.9}
local R = 0.26
local commits = {}
local function commit(id, hash, lane, x, color)
    local c = {id = id, x = x, y = Y[lane], lane = lane}
    c.g = scene:group {id = "commit:" .. id}
    c.dot = c.g:circle {id = "commit:" .. id .. ":dot", center = {x, c.y}, radius = R, fill = C.panel,
                        stroke = color, width = 2.5, layer = L.subject}
    local hy = lane == "main" and c.y - 0.5 or c.y + 0.5
    c.hash = T(c.g, "commit:" .. id .. ":hash", hash, x, hy, 12, C.mute)
    commits[id] = c
    return c
end
local function edge(a, b)
    local dx, dy = b.x - a.x, b.y - a.y
    local n = math.sqrt(dx * dx + dy * dy)
    local ux, uy = dx / n, dy / n
    return scene:line {id = "parent:" .. b.id .. ":" .. a.id, from = {a.x + ux * R, a.y + uy * R},
                       to = {b.x - ux * R, b.y - uy * R}, stroke = C.line, width = 2.5, layer = L.edge}
end
local X = {-4.4, -2.75, -1.1, 0.55, 2.2, 3.85}
local c1 = commit("c1", "3f2a", "main", X[1], C.accent)
local c2 = commit("c2", "9b1e", "main", X[2], C.accent)
edge(c1, c2)
-- branch pointers are labels that move; HEAD sits on the checked-out branch
local function pill(id, s, color, x, y) return token(id, s, x, y, {size = {1.0, 0.34}, text = 13, stroke = color}) end
local PY = 0.62
local main = pill("ref:main", "main", C.accent, c2.x, c2.y + PY)
local HX = -1.1
local head = pill("ref:head", "HEAD", C.warn, c2.x + HX, c2.y + PY)
local function newcommit(id, hash, lane, x, color, ...)
    local c = commit(id, hash, lane, x, color)
    local es = {}
    for _, p in ipairs({...}) do es[#es + 1] = edge(p, c) end
    scene:set({target = c.g, opacity = 0})
    for _, e in ipairs(es) do scene:set({target = e, progress = 0}) end
    for _, e in ipairs(es) do scene:create(e, {duration = 0.35, curve = "ease_out"}) end
    scene:fade_in(c.g, {duration = 0.3})
    return c
end
local SX, SY = -5.1, 2.5
scene:wait(1.0)
-- 1. new branch: a second pointer at the same commit
say("git switch -c feature", SX, SY, C.warn, {0, 0.5})
local feature = pill("ref:feature", "feature", C.alt, c2.x, -0.05)
scene:fade_in(feature.g, {duration = 0.3})
scene:wait(0.3)
-- switching only moves HEAD to the other pointer
local function switch(ref)
    scene:fade_out(head.g, {duration = 0.2})
    head = pill("ref:head:" .. ref.v, "HEAD", C.warn, ref.x + HX, ref.y)
    scene:fade_in(head.g, {duration = 0.25})
end
switch(feature)
scene:wait(0.5)
-- 2. commits on feature move only the feature pointer
say("git commit  (on feature)", SX, SY, C.warn, {0, 0.5})
local f1 = newcommit("f1", "c47d", "feature", X[3], C.alt, c2)
together(function() go(feature, f1.x, f1.y - PY, 0.5) end, function() go(head, f1.x + HX, f1.y - PY, 0.5) end)
scene:wait(0.3)
local f2 = newcommit("f2", "e810", "feature", X[4], C.alt, f1)
together(function() go(feature, f2.x, f2.y - PY, 0.45) end, function() go(head, f2.x + HX, f2.y - PY, 0.45) end)
scene:wait(0.5)
-- 3. back on main, a hotfix lands: history diverges
say("git switch main", SX, SY, C.warn, {0, 0.5})
switch(main)
scene:wait(0.3)
say("git commit  (hotfix on main)", SX, SY, C.warn, {0, 0.5})
local c3 = newcommit("c3", "1d06", "main", X[5], C.accent, c2)
together(function() go(main, c3.x, c3.y + PY, 0.45) end, function() go(head, c3.x + HX, c3.y + PY, 0.45) end)
say("main and feature have diverged", SX, SY, C.warn, {0, 0.5})
scene:wait(1.4)
-- 4. merge: one new commit with two parents
say("git merge feature", SX, SY, C.warn, {0, 0.5})
local m = newcommit("m", "77ac", "main", X[6], C.ok, c3, f2)
together(function() go(main, m.x, m.y + PY, 0.45) end, function() go(head, m.x + HX, m.y + PY, 0.45) end)
say("merge commit: two parents", SX, SY, C.ok, {0, 0.5})
scene:wait(2.6)
return scene`;

// ---------------------------------------------------------------- 5. CI pipeline: a failing test stops the deploy
const CI = PRELUDE + `
local W, H, Y = 1.55, 1.06, 0.55
local spec = {
    {id = "push", label = "Push", detail = "git push", x = -4.3},
    {id = "build", label = "Build", detail = "compile", x = -2.15},
    {id = "test", label = "Test", detail = "unit tests", x = 0},
    {id = "package", label = "Package", detail = "image", x = 2.15},
    {id = "deploy", label = "Deploy", detail = "staging", x = 4.3},
}
local d = vis.diagram(scene, {id = "ci", layout = "manual", node_size = {W, H}, route_width = 0.025,
                                arrow_length = 0.14, arrow_width = 0.14, corner = 0.08,
                                layers = {zones = L.region, routes = L.edge, nodes = L.subject, annotations = L.label}})
local N, P = {}, {}
for _, s in ipairs(spec) do
    s.y = Y
    N[s.id] = d:node {id = s.id, label = s.label, detail = s.detail, position = {s.x, s.y}}
    P[s.id] = s
end
P.report = {id = "report", x = 0, y = -1.6}
-- wider than the stages so its longer detail stays inside the box
N.report = d:node {id = "report", label = "Report", detail = "notify author", position = {P.report.x, P.report.y},
                   size = {2.0, H}}
for k = 1, #spec - 1 do
    d:connect {id = spec[k].id .. "-" .. spec[k + 1].id, from = N[spec[k].id], to = N[spec[k + 1].id], route = "straight"}
end
d:connect {id = "test-report", from = N.test, to = N.report, from_port = "bottom", to_port = "top", route = "straight"}
local B = d:build()
local body = {}
for _, s in ipairs(spec) do body[s.id] = B:body(N[s.id]) end
body.report = B:body(N.report)
local function mark(id, color, dur) scene:stroke(body[id], color, {duration = dur or 0.2}) end
-- test detail is the stage's state: swap its Text in place
local det = B:detail(N.test)
local detN = 0
local function setTest(s, color)
    scene:fade_out(det, {duration = 0.15})
    detN = detN + 1
    det = T(scene, "test:detail:" .. detN, s, P.test.x, P.test.y - 0.29, 15, color)
    scene:fade_in(det, {duration = 0.2})
end
local PY = Y + H / 2 + 0.32
local function run(sha, color)
    local o = token("commit:" .. sha, sha, P.push.x, PY, {size = {0.8, 0.32}, text = 13, stroke = color})
    scene:fade_in(o.g, {duration = 0.25})
    return o
end
local function stage(o, id, result)
    go(o, P[id].x, PY, 0.45)
    mark(id, C.accent)
    scene:wait(0.35)
    if result then mark(id, result) end
end
scene:wait(1.0)
-- run 1: commit a1f3 breaks a test
local r1 = run("a1f3", C.accent)
say("run 1", -5.1, 2.55, C.warn, {0, 0.5})
mark("push", C.ok)
stage(r1, "build", C.ok)
stage(r1, "test")
together(function() mark("test", C.danger) end, function() setTest("1 failed", C.danger) end,
         function() say("test fails  pipeline stops", 0.95, 2.55, C.danger, {0, 0.5}) end)
together(function() mark("report", C.danger, 0.3) end,
         function() scene:fade(B:object(N.package), 0.35, {duration = 0.3}) end, function() scene:fade(B:object(N.deploy), 0.35, {duration = 0.3}) end)
scene:wait(1.6)
-- run 2: the fix goes through every stage
together(function() scene:fade_out(r1.g, {duration = 0.25}) end,
         function() for _, id in ipairs({"push", "build", "test", "report"}) do mark(id, C.line, 0.25) end end,
         function() scene:fade(B:object(N.package), 1, {duration = 0.25}) end, function() scene:fade(B:object(N.deploy), 1, {duration = 0.25}) end,
         function() say("run 2  fix pushed", -5.1, 2.55, C.warn, {0, 0.5}) end)
setTest("unit tests", C.mute)
local r2 = run("b7c2", C.ok)
mark("push", C.ok)
stage(r2, "build", C.ok)
stage(r2, "test")
together(function() mark("test", C.ok) end, function() setTest("42 passed", C.ok) end)
stage(r2, "package", C.ok)
stage(r2, "deploy", C.ok)
say("all green  deployed to staging", 5.2, 2.55, C.ok, {1, 0.5})
scene:wait(2.6)
return scene`;

// ---------------------------------------------------------------- 6. Data flow: dashes travel at the rate data moves
const FLOW = PRELUDE + `
-- topology: three sources fan into one log, one job reads it and writes to two sinks
local function node(id, label, detail, x, y, w, h)
    local box = scene:rectangle {id = id, center = {x, y}, size = {w or 1.5, h or 0.9}, corner = 0.06,
                                 fill = C.panel, stroke = C.line, width = 1.5, layer = L.subject}
    T(scene, id .. ":label", label, x, y + 0.14, 16)
    return box, T(scene, id .. ":detail", detail, x, y - 0.18, 13, C.mute)
end
local jobDetail
local box = {
    clicks = node("clicks", "Clicks", "web", -4.2, 1.6),
    orders = node("orders", "Orders", "api", -4.2, 0),
    sensors = node("sensors", "Sensors", "iot", -4.2, -1.6),
    log = node("log", "Event log", "3 partitions", -1.3, 0, 1.6, 1.3),
    warehouse = node("warehouse", "Warehouse", "hourly", 4.2, 1.2),
    dashboard = node("dashboard", "Dashboard", "live", 4.2, -1.2),
}
box.job, jobDetail = node("job", "Stream job", "1 worker", 1.4, 0, 1.6, 0.9)
-- every edge is a dashed route; moving its dash offset makes the dashes travel toward the tip
local function edge(id, points)
    return scene:route {id = id, points = points, stroke = C.line, width = 2, tip = 10, dash = {9, 7}, layer = L.edge}
end
local E = {}
for _, s in ipairs({{"clicks", 1.6}, {"orders", 0}, {"sensors", -1.6}}) do
    E[s[1]] = edge("e:" .. s[1], {{-3.45, s[2]}, {-2.75, s[2]}, {-2.75, s[2] * 0.3}, {-2.1, s[2] * 0.3}})
end
E.read = edge("e:read", {{-0.5, 0}, {0.6, 0}})
E.warehouse = edge("e:warehouse", {{2.2, 0.2}, {2.8, 0.2}, {2.8, 1.2}, {3.45, 1.2}})
E.dashboard = edge("e:dashboard", {{2.2, -0.2}, {2.8, -0.2}, {2.8, -1.2}, {3.45, -1.2}})
local ORDER = {"clicks", "orders", "sensors", "read", "warehouse", "dashboard"}

-- run every edge's dashes for dur seconds; speed[id] is pixels per second (default 48)
local offset = {}
local function flow(dur, speed)
    local tracks = {}
    for _, id in ipairs(ORDER) do
        offset[id] = (offset[id] or 0) - (speed and speed[id] or 48) * dur
        local o = offset[id]
        tracks[#tracks + 1] = function() scene:tween({target = E[id], dash_offset = o}, {duration = dur, curve = "linear"}) end
    end
    together(table.unpack(tracks))
end
local function tint(ids, color)
    local tracks = {}
    for _, id in ipairs(ids) do
        tracks[#tracks + 1] = function() scene:stroke(E[id], color, {duration = 0.3}) end
    end
    together(table.unpack(tracks))
end

scene:wait(0.6)
-- 1. events stream in at a steady rate
say("events stream in", 0, 2.55, C.accent)
tint(ORDER, C.accent)
flow(3)
-- 2. a spike: sources speed up, but the single worker cannot keep pace with the dashboard
say("traffic spike  dashboard lags", 0, 2.55, C.warn)
together(function() tint({"dashboard"}, C.warn) end, function() scene:stroke(box.dashboard, C.warn, {duration = 0.3}) end)
flow(3, {clicks = 96, orders = 96, sensors = 96, read = 60, warehouse = 48, dashboard = 14})
-- 3. a second worker joins and the whole path runs fast
together(function() scene:fade_out(jobDetail, {duration = 0.2}) end,
         function() scene:fade_in(T(scene, "job:detail2", "2 workers", 1.4, -0.18, 13, C.ok), {duration = 0.3}) end,
         function() scene:stroke(box.job, C.ok, {duration = 0.3}) end,
         function() say("second worker  caught up", 0, 2.55, C.ok) end)
together(function() tint({"dashboard"}, C.accent) end, function() scene:stroke(box.dashboard, C.line, {duration = 0.3}) end)
flow(3.4, {clicks = 96, orders = 96, sensors = 96, read = 96, warehouse = 72, dashboard = 72})
return scene`;

export const DIAGRAMS = {
  id: "diagrams",
  title: "Diagrams",
  episodes: [
    {id: "http-lifecycle", title: "HTTP request lifecycle", code: HTTP,
     blurb: "One GET travels client → DNS → load balancer → server.\nA cache miss sends it to the database; the row fills the cache, and the repeat request hits."},
    {id: "signup-swimlane", title: "Sign-up swimlane", code: SIGNUP,
     blurb: "Follow the form from user to browser to API.\nThe API hands a verification link to the email service; clicking it marks the row as verified."},
    {id: "event-loop", title: "The event loop", code: EVENTLOOP,
     blurb: "See why this prints A D C B.\nsetTimeout parks its callback in the task queue, Promise.then in the microtask queue, and the loop drains microtasks before the next task."},
    {id: "git-merge", title: "Branch and merge", code: GIT,
     blurb: "Branches are moving pointers.\nCommits on feature advance only feature; a hotfix makes main diverge, and git merge adds one commit with two parents."},
    {id: "ci-pipeline", title: "CI pipeline", code: CI,
     blurb: "A commit walks push → build → test → package → deploy.\nA failing test stops it and notifies the author; the fix goes all the way to staging."},
    {id: "data-flow", title: "Streaming pipeline", code: FLOW,
     blurb: "Dashed edges flow at the rate data moves.\nA traffic spike outruns the single stream worker and the dashboard feed slows to a crawl; a second worker brings it back up to speed."},
  ],
};
