// Plays scenes in Chromium through the built player: in the render worker and on the main thread,
// with fonts asked for by name, input and clicks, a worker that hangs, players scrolled out of
// view, and JavaScript scenes compiled in the sandbox.
// Usage: npm run test:browser (builds first). Set CHROMIUM to use a browser other than Playwright's.
import assert from "node:assert/strict";
import {createServer} from "node:http";
import {readFile} from "node:fs/promises";
import {extname, join, normalize} from "node:path";
import test, {after, before} from "node:test";
import {fileURLToPath} from "node:url";
import {chromium} from "playwright-core";

const root = fileURLToPath(new URL("../../../../", import.meta.url));
const TYPES = {".html": "text/html", ".js": "text/javascript", ".wasm": "application/wasm", ".ttf": "font/ttf",
    ".png": "image/png", ".json": "application/json"};
// Requests scene code sent out, which only scenes outside the sandbox should get to make.
const leaks = [];
const server = createServer(async (request, response) => {
    if (request.url.startsWith("/leak/")) {
        leaks.push(request.url);
        return response.writeHead(200, {"access-control-allow-origin": "*"}).end("ok");
    }
    const path = normalize(join(root, decodeURIComponent(new URL(request.url, "http://localhost").pathname)));
    if (!path.startsWith(root)) return response.writeHead(403).end();
    try {
        const data = await readFile(path);
        response.writeHead(200, {"content-type": TYPES[extname(path)] ?? "application/octet-stream"}).end(data);
    } catch {
        response.writeHead(404).end();
    }
});
server.on("upgrade", (request, socket) => {
    leaks.push(request.url);
    socket.destroy();
});
let browser, base, origin;

before(async () => {
    await new Promise((resolve) => server.listen(0, "127.0.0.1", resolve));
    origin = `http://127.0.0.1:${server.address().port}`;
    base = `${origin}/tools/player/test/browser/index.html`;
    browser = await chromium.launch({executablePath: process.env.CHROMIUM, headless: true});
});
after(async () => {
    await browser?.close();
    server.close();
});

// `init` runs in the page before its own scripts.
async function open(query = "", init) {
    const page = await browser.newPage({viewport: {width: 800, height: 600}});
    if (init) await page.addInitScript(init);
    const errors = [];
    page.on("pageerror", (error) => errors.push(error.message));
    page.on("console", (message) => { if (message.type() === "error") errors.push(message.text()); });
    await page.goto(base + query);
    await page.waitForFunction(() => window.e2e);
    return {page, errors};
}

const state = (page, id) => page.evaluate((id) => window.e2e.players[id]?.state, id);
const until = (page, id, wanted, timeout = 15000) =>
    page.waitForFunction(([id, wanted]) => window.e2e.players[id]?.state === wanted, [id, wanted], {timeout});
// Distinct colors in a screenshot of the player's canvas, as a test that it shows a frame.
async function shot(page, id) {
    const canvas = await page.evaluateHandle((id) => window.e2e.players[id].canvas, id);
    return canvas.screenshot();
}
const distinct = (png) => new Set(Array.from({length: Math.min(png.length, 20000)}, (_, i) => png[i])).size;

test("a scene plays in the render worker", async () => {
    const {page, errors} = await open();
    await page.evaluate(() => window.e2e.make("a", {source: window.e2e.scenes.HERO, name: "hero.lua"}));
    await until(page, "a", "playing");
    assert.equal(await page.evaluate(() => window.e2e.host.mode), "worker");
    if (process.env.SHOTS) await page.screenshot({path: join(process.env.SHOTS, "worker.png")});
    const t0 = await page.evaluate(() => window.e2e.players.a.time);
    await page.waitForTimeout(600);
    const t1 = await page.evaluate(() => window.e2e.players.a.time);
    assert.ok(t1 > t0, `time moves: ${t0} → ${t1}`);
    const first = await shot(page, "a");
    assert.ok(distinct(first) > 40, "the canvas shows a frame");
    await page.waitForTimeout(400);
    assert.ok(!first.equals(await shot(page, "a")), "frames change");
    assert.deepEqual(errors, []);
    await page.close();
});

test("the render worker asks for each font a scene sets text in once, and plays on without one the host lacks", async () => {
    const {page, errors} = await open();
    const asked = () => page.evaluate(() => window.e2e.fontsAsked.slice());
    await page.evaluate(() => window.e2e.make("a", {source: window.e2e.scenes.HERO, name: "hero.lua"}));
    await until(page, "a", "playing");
    assert.deepEqual((await asked()).sort(), ["JetBrainsMono", "Pretendard"]);
    const scene = (...fonts) => `local s = vis.scene {width = 160, height = 96}
${fonts.map((font, i) => `s:text {text = "Fonts", font = "${font}", point = {0, ${i}}}`).join("\n")}
return s`;
    const show = async (source) => {
        await page.evaluate((source) => window.e2e.players.b ? window.e2e.players.b.load({source, name: "fonts.lua"})
            : window.e2e.make("b", {source, name: "fonts.lua"}), source);
        await until(page, "b", "playing");
        await page.waitForTimeout(200);
        return shot(page, "b");
    };
    // A font the host has none for is asked for once, and its text is drawn in the default font.
    const blank = await show(scene());
    const missing = await show(scene("Nowhere Sans"));
    assert.deepEqual((await asked()).slice(2), ["Nowhere Sans"]);
    assert.ok(!missing.equals(blank), "the text in the missing font is drawn");
    await show(scene("Nowhere Sans"));
    // Nor are fonts the worker has asked for again.
    await show(scene("Pretendard"));
    assert.equal((await asked()).length, 3);
    // Once the host's fonts change, scenes ask for theirs again as they load.
    await page.evaluate(() => window.e2e.host.refreshFonts());
    await show(scene("Pretendard", "JetBrainsMono"));
    assert.deepEqual((await asked()).slice(3).sort(), ["JetBrainsMono", "Pretendard"]);
    assert.deepEqual(errors, []);
    await page.close();
});

test("a JavaScript scene in the worker cannot start workers or reach the page's storage", async () => {
    const {page, errors} = await open();
    // The scene's width tells what its code could see.
    const source = `const hidden = ["Worker", "SharedWorker", "indexedDB", "caches"].every((name) => !(name in globalThis));
return vis.scene({width: hidden ? 64 : 32, height: 16});`;
    await page.evaluate((source) => window.e2e.make("a", {source, language: "js", name: "probe.js"}), source);
    await page.waitForFunction(() => window.e2e.players.a.info);
    assert.equal(await page.evaluate(() => window.e2e.host.mode), "worker");
    assert.equal(await page.evaluate(() => window.e2e.players.a.info.width), 64);
    assert.deepEqual(errors, []);
    await page.close();
});

// A scene that tries each way out to the test server, and whose width tells whether its code was
// kept from Node, the page and the page's storage.
const LEAKY = () => `const attempt = (run) => { try { const result = run(); if (result && result.catch) result.catch(() => {}); } catch {} };
attempt(() => fetch("${origin}/leak/fetch"));
attempt(() => { const request = new XMLHttpRequest(); request.open("GET", "${origin}/leak/xhr"); request.send(); });
attempt(() => new WebSocket("${origin.replace("http", "ws")}/leak/websocket"));
attempt(() => new EventSource("${origin}/leak/eventsource"));
attempt(() => import("${origin}/leak/import.js"));
attempt(() => importScripts("${origin}/leak/importScripts.js"));
const storage = (() => { try { indexedDB.open("probe"); return true; } catch { return false; } })();
const apart = typeof require === "undefined" && typeof document === "undefined" && !storage;
return vis.scene({width: apart ? 64 : 32, height: 16});`;

test("a JavaScript scene compiled in the sandbox plays, and its code reaches neither the network nor the page", async () => {
    for (const mode of ["auto", "inline"]) {
        const {page, errors} = await open(`?sandbox&mode=${mode}`);
        await page.evaluate((source) => window.e2e.make("a", {source, language: "js", name: "leaky.js"}), LEAKY());
        await page.waitForFunction(() => window.e2e.players.a.info);
        assert.equal(await page.evaluate(() => window.e2e.players.a.info.width), 64, mode);
        await page.waitForTimeout(1000);
        assert.deepEqual(leaks.splice(0), [], mode);
        // The sandbox's refusals are logged; nothing else should be.
        assert.deepEqual(errors.filter((message) => !/Content Security Policy/.test(message)), [], mode);
        await page.close();
    }
    // The same scene in the render worker gets out, which shows the test above can see a leak.
    const {page} = await open();
    await page.evaluate((source) => window.e2e.make("a", {source, language: "js", name: "leaky.js"}), LEAKY());
    await page.waitForFunction(() => window.e2e.players.a.info);
    await page.waitForTimeout(1000);
    assert.ok(leaks.splice(0).includes("/leak/fetch"));
    await page.close();
});

test("a JavaScript scene that never returns is stopped in the sandbox, and the other players play on", async () => {
    const {page} = await open("?sandbox&compile=1000");
    await page.evaluate(() => window.e2e.make("a", {source: window.e2e.scenes.HERO}));
    await until(page, "a", "playing");
    const canvas = await page.evaluateHandle(() => window.e2e.players.a.canvas);
    await page.evaluate(() => window.e2e.make("b", {source: "for (;;) {}", language: "js", name: "loop.js"}));
    await until(page, "b", "error", 10000);
    assert.deepEqual(await page.evaluate(() => window.e2e.players.b.error),
        {phase: "timeout", message: "This scene's JavaScript ran for more than 1 second, so it was stopped"});
    assert.equal(await page.evaluate((old) => window.e2e.players.a.canvas === old, canvas), true, "the render worker was not replaced");
    assert.equal(await state(page, "a"), "playing");
    // It runs again when asked.
    await page.evaluate(() => window.e2e.players.b.load({source: "return vis.scene({width: 32, height: 32});", language: "js", name: "fixed.js"}));
    await page.waitForFunction(() => window.e2e.players.b.info?.width === 32);
    await page.close();
});

test("a scene plays on the main thread when asked to", async () => {
    const {page, errors} = await open("?mode=inline");
    await page.evaluate(() => window.e2e.make("a", {source: window.e2e.scenes.HERO}));
    await until(page, "a", "playing");
    assert.equal(await page.evaluate(() => window.e2e.host.mode), "inline");
    await page.waitForTimeout(300);
    const opaque = await page.evaluate(() => {
        const canvas = window.e2e.players.a.canvas;
        const data = canvas.getContext("2d").getImageData(0, 0, canvas.width, canvas.height).data;
        let count = 0;
        for (let i = 3; i < data.length; i += 4) if (data[i] > 0) count++;
        return count / (data.length / 4);
    });
    assert.ok(opaque > 0.5, `the canvas is drawn (${opaque})`);
    assert.deepEqual(errors, []);
    await page.close();
});

test("pointer input reaches a live scene in the worker", async () => {
    const {page, errors} = await open();
    await page.evaluate(() => window.e2e.make("a", {source: window.e2e.scenes.PATH, name: "path.lua"}, {interactive: true}));
    await until(page, "a", "playing");
    assert.equal(await page.evaluate(() => window.e2e.players.a.live), true);
    await page.waitForTimeout(300);
    const before = await shot(page, "a");
    const box = await (await page.evaluateHandle(() => window.e2e.players.a.canvas)).boundingBox();
    await page.mouse.move(box.x + box.width * 0.7, box.y + box.height * 0.6);
    await page.mouse.down();
    await page.mouse.up();
    await page.waitForTimeout(300);
    assert.ok(!before.equals(await shot(page, "a")), "the scene answers the pointer");
    assert.deepEqual(errors, []);
    await page.close();
});

test("a click pauses or plays a scene that takes no input, and is input to one that does", async () => {
    const {page, errors} = await open();
    await page.evaluate(() => window.e2e.make("a", {source: window.e2e.scenes.HERO}));
    await until(page, "a", "playing");
    assert.equal(await page.evaluate(() => window.e2e.players.a.live), false);
    const canvas = await page.evaluateHandle(() => window.e2e.players.a.canvas);
    await canvas.click();
    await until(page, "a", "paused");
    const t0 = await page.evaluate(() => window.e2e.players.a.time);
    await page.waitForTimeout(300);
    assert.equal(await page.evaluate(() => window.e2e.players.a.time), t0, "a paused scene holds its time");
    await canvas.click();
    await until(page, "a", "playing");
    await page.evaluate(() => window.e2e.make("b", {source: window.e2e.scenes.PATH, name: "path.lua"}, {interactive: true}));
    await until(page, "b", "playing");
    await (await page.evaluateHandle(() => window.e2e.players.b.canvas)).click();
    await page.waitForTimeout(300);
    assert.equal(await state(page, "b"), "playing");
    assert.deepEqual(await page.evaluate(() => window.e2e.events.filter((event) => /^[ab] (play|pause)$/.test(event))),
        ["a pause", "a play"], "each click on the first scene toggled it once, and none the live one");
    assert.deepEqual(errors, []);
    await page.close();
});

test("a scene that hangs the worker is halted, and the other players come back", async () => {
    const {page} = await open("?hang=1500");
    await page.evaluate(() => window.e2e.make("a", {source: window.e2e.scenes.HERO}));
    await until(page, "a", "playing");
    const oldCanvas = await page.evaluateHandle(() => window.e2e.players.a.canvas);
    await page.evaluate(() => window.e2e.make("b", {source: "while (true) {}", language: "js", name: "loop.js"}));
    await until(page, "b", "error", 10000);
    assert.equal(await page.evaluate(() => window.e2e.players.b.error.phase), "timeout");
    await until(page, "a", "playing");
    assert.equal(await page.evaluate((old) => window.e2e.players.a.canvas !== old && !old.isConnected, oldCanvas), true,
        "the player's canvas was replaced");
    const t0 = await page.evaluate(() => window.e2e.players.a.time);
    await page.waitForTimeout(400);
    assert.ok(await page.evaluate(() => window.e2e.players.a.time) > t0, "the other player plays on");
    // The halted one loads again when asked.
    await page.evaluate(() => window.e2e.players.b.load({source: "return vis.scene {width = 64, height = 64}"}));
    await until(page, "b", "playing");
    await page.close();
});

test("a player out of view freezes, frees its engine, and loads again when shown", async () => {
    const {page, errors} = await open();
    await page.evaluate(() => window.e2e.make("a", {source: window.e2e.scenes.HERO}));
    await until(page, "a", "playing");
    await page.evaluate(() => scrollTo(0, 3000));
    await until(page, "a", "frozen");
    await page.waitForTimeout(2300);
    await page.evaluate(() => scrollTo(0, 0));
    await until(page, "a", "playing");
    const t0 = await page.evaluate(() => window.e2e.players.a.time);
    await page.waitForTimeout(400);
    assert.ok(await page.evaluate(() => window.e2e.players.a.time) > t0);
    const events = await page.evaluate(() => window.e2e.events.filter((event) => event.startsWith("a ")));
    assert.deepEqual(events.filter((event) => / (load|freeze)$/.test(event)), ["a load", "a freeze"],
        "loading again after being freed is not a new load");
    assert.deepEqual(errors, []);
    await page.close();
});

test("playback ends, loops and plays ranges as asked", async () => {
    const {page, errors} = await open();
    const scene = "local s = vis.scene {width = 64, height = 64}\nlocal c = s:circle {center = {0, 0}, radius = 0.5}\ns:wait(0.4)\nreturn s";
    await page.evaluate((source) => window.e2e.make("a", {source}, {end: "stop"}), scene);
    await until(page, "a", "stopped", 5000);
    assert.equal(await page.evaluate(() => window.e2e.players.a.time), await page.evaluate(() => window.e2e.players.a.duration));
    await page.evaluate(() => window.e2e.players.a.play());
    await until(page, "a", "playing");
    await page.evaluate(() => window.e2e.players.a.playRange(0.1, 0.2));
    await until(page, "a", "stopped", 5000);
    assert.equal(await page.evaluate(() => window.e2e.players.a.time), 0.2);
    await page.evaluate(() => {
        window.e2e.players.a.clearRange();
        window.e2e.players.a.configure({end: "loop"});
        window.e2e.players.a.play();
    });
    await page.waitForFunction(() => window.e2e.events.includes("a loop"), null, {timeout: 5000});
    assert.equal(await state(page, "a"), "playing");
    assert.deepEqual(errors, []);
    await page.close();
});

test("playback runs at the rate asked, within the rates the runtime's audio takes", async () => {
    const {page, errors} = await open();
    const scene = "local s = vis.scene {width = 64, height = 64}\ns:circle {center = {0, 0}, radius = 0.5}\ns:wait(60)\nreturn s";
    await page.evaluate((source) => window.e2e.make("a", {source}, {rate: 4}), scene);
    await until(page, "a", "playing");
    const time = () => page.evaluate(() => window.e2e.players.a.time);
    const t0 = await time();
    await page.waitForTimeout(500);
    const t1 = await time();
    assert.ok(t1 - t0 > 1.4 && t1 - t0 < 3, `four seconds a second: ${t0} → ${t1}`);
    assert.deepEqual(await page.evaluate(() => [100, 0.01, -1].map((rate) => {
        window.e2e.players.a.configure({rate});
        return window.e2e.players.a.rate;
    })), [8, 0.125, 0.125]);
    assert.deepEqual(errors, []);
    await page.close();
});

// A circle in a space, whose camera drags, the wheel and keys may move.
const CAMERA = `local s = vis.scene {width = 320, height = 180, camera = {mode = "interactive", view = "2d", target = {0, 0}, height = 6}}
local space = s:space {x = {-3, 3, 1}, y = {-2, 2, 1}}
space:circle {center = {0, 0}, radius = 1}
s:wait(60)
return s`;

test("drags, the wheel and keys move an interactive camera when the player lets them, and a drag is not a click", async () => {
    const {page, errors} = await open();
    await page.evaluate((source) => window.e2e.make("a", {source, name: "camera.lua"}, {camera: true, autoplay: false}), CAMERA);
    await until(page, "a", "paused");
    await page.waitForTimeout(200);
    const canvas = await page.evaluateHandle(() => window.e2e.players.a.canvas);
    const box = await canvas.boundingBox();
    const drag = async (dx) => {
        await page.mouse.move(box.x + box.width / 2, box.y + box.height / 2);
        await page.mouse.down();
        await page.mouse.move(box.x + box.width / 2 + dx, box.y + box.height / 2, {steps: 6});
        await page.mouse.up();
        await page.waitForTimeout(300);
    };
    const before = await shot(page, "a");
    await drag(60);
    const panned = await shot(page, "a");
    assert.ok(!before.equals(panned), "the drag panned the camera");
    assert.equal(await state(page, "a"), "paused", "the drag did not play the scene");
    // The drag focused the canvas, so its keys reach the camera.
    await page.keyboard.press("3");
    await page.waitForFunction(() => window.e2e.players.a.cameraView === "3d");
    await page.keyboard.press("r");
    await page.waitForTimeout(300);
    const turned = await shot(page, "a");
    await page.mouse.wheel(0, 300);
    await page.waitForTimeout(300);
    assert.ok(!turned.equals(await shot(page, "a")), "the wheel zoomed");
    // With the camera off, a drag moves nothing: it is a click, which plays the scene.
    await page.evaluate(() => window.e2e.players.a.configure({camera: false}));
    const still = await shot(page, "a");
    await drag(60);
    await until(page, "a", "playing");
    assert.ok(still.equals(await shot(page, "a")), "the camera stayed");
    assert.equal(await page.evaluate(() => window.e2e.players.a.moveCamera("view2d")), true, "the camera moves when asked");
    await page.waitForFunction(() => window.e2e.players.a.cameraView === "2d");
    assert.deepEqual(errors, []);
    await page.close();
});

test("a frame is captured apart from the canvas, in the worker and on the main thread", async () => {
    for (const query of ["", "?mode=inline"]) {
        const {page, errors} = await open(query);
        await page.evaluate(() => window.e2e.make("a", {source: window.e2e.scenes.HERO, name: "hero.lua"}));
        await until(page, "a", "playing");
        const captured = await page.evaluate(async () => {
            const player = window.e2e.players.a;
            const [one, again, two] = [await player.capture({time: 1}), await player.capture({time: 1}), await player.capture({time: 1, ratio: 2})];
            let opaque = 0;
            for (let i = 3; i < one.data.length; i += 4) if (one.data[i] === 255) opaque++;
            return {
                size: [player.info.width, player.info.height], one: [one.width, one.height], two: [two.width, two.height],
                opaque: opaque / (one.width * one.height), same: one.data.every((value, i) => value === again.data[i]),
            };
        });
        assert.deepEqual(captured.one, captured.size, query);
        assert.deepEqual(captured.two, captured.size.map((side) => side * 2), query);
        assert.ok(captured.opaque > 0.9, `the frame is drawn (${captured.opaque})`);
        assert.ok(captured.same, "a frame captured twice is the same");
        assert.equal(await state(page, "a"), "playing");
        assert.deepEqual(errors, []);
        await page.close();
    }
});

test("with interactive set to timeline, a scene's UI controls take input as its timeline plays", async () => {
    const {page, errors} = await open();
    const source = await (await fetch(`${origin}/tools/skill/vis-interactive/templates/button-state-motion.lua`)).text();
    // The first scene sets its own, in place of the player's.
    await page.evaluate((source) => {
        window.e2e.make("a", {source, name: "buttons.lua", interactive: "timeline"}, {interactive: false});
        window.e2e.make("b", {source, name: "buttons.lua"}, {interactive: true});
    }, source);
    await until(page, "a", "playing");
    await until(page, "b", "playing");
    await page.waitForTimeout(400);
    assert.deepEqual(await page.evaluate(() => [window.e2e.players.a.live, window.e2e.players.b.live]), [true, true]);
    assert.ok(await page.evaluate(() => window.e2e.players.a.time) > 0.2, "the timeline plays");
    assert.equal(await page.evaluate(() => window.e2e.players.b.time), 0, "the timeline stays at 0");
    // "Set running" at (145, -198) from the centre of the 960 by 540 scene moves the indicator over 0.48 seconds.
    const box = await (await page.evaluateHandle(() => window.e2e.players.a.canvas)).boundingBox();
    const before = await shot(page, "a");
    await page.mouse.click(box.x + (480 + 145) / 960 * box.width, box.y + (270 + 198) / 540 * box.height);
    await page.mouse.move(box.x + 5, box.y + 5);
    await page.waitForTimeout(800);
    assert.ok(!before.equals(await shot(page, "a")), "the button moved the indicator");
    assert.deepEqual(errors, []);
    await page.close();
});

test("a retained runtime steps while its scene plays, and holds while it is paused", async () => {
    const {page, errors} = await open();
    const source = `local s = vis.scene {width = 160, height = 90, camera = {view = "2d", target = {0, 0}, height = 9}}
local dot = s:circle {center = {0, 0}, radius = 1, fill = "accent"}
vis.runtime(s, {fixed_step = 1 / 60, update = function(ctx, dt, time)
    ctx:update(dot, {shift = {math.sin(time * 6) * 3, 0, 0}})
end})
return s`;
    await page.evaluate((source) => window.e2e.make("a", {source, name: "runtime.lua"}), source);
    await until(page, "a", "playing");
    assert.equal(await page.evaluate(() => window.e2e.players.a.live), true);
    const moves = async () => {
        const first = await shot(page, "a");
        await page.waitForTimeout(250);
        return !first.equals(await shot(page, "a"));
    };
    assert.ok(await moves(), "the runtime steps as the scene plays");
    await page.evaluate(() => window.e2e.players.a.pause());
    await page.waitForTimeout(150);
    assert.equal(await moves(), false, "a paused scene's runtime holds");
    await page.evaluate(() => window.e2e.players.a.play());
    await page.waitForTimeout(100);
    assert.ok(await moves(), "it steps again once played");
    assert.deepEqual(errors, []);
    await page.close();
});

test("on the main thread a scene's sound starts once the page has had a click", async () => {
    // Playwright's calls count as the user's, so the page is told it has had none.
    const {page, errors} = await open("?mode=inline", () => Object.defineProperty(Navigator.prototype, "userActivation",
        {get: () => ({hasBeenActive: false, isActive: false})}));
    const source = `local s = vis.scene {width = 64, height = 64}
s:circle {center = {0, 0}, radius = 0.5}
vis.audio.cue(s, {asset = "tone.wav", bus = "music", ["end"] = 2, loop = true})
s:wait(2)
return s`;
    await page.evaluate((source) => window.e2e.make("a", {source, name: "tone.lua"}, {audio: true, end: "loop"}), source);
    await until(page, "a", "playing");
    // miniaudio marks the device the runtime's audio starts.
    const started = () => (window.miniaudio?.devices ?? []).some((device) => device?.state === window.miniaudio.device_state.started);
    await page.waitForTimeout(300);
    assert.equal(await page.evaluate(started), false, "no sound before a click");
    await page.mouse.click(4, 4);
    await page.waitForFunction(started, null, {timeout: 5000});
    await page.evaluate(() => window.e2e.players.a.pause());
    await page.waitForTimeout(200);
    assert.equal(await state(page, "a"), "paused");
    assert.deepEqual(errors, []);
    await page.close();
});
