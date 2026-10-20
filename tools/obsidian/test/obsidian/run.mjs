// Runs the built plugin in the Obsidian installed on this computer, with a profile and a vault made
// for the run, and checks scenes in reading view, Live Preview, an embed, their own tab and the
// settings, in both themes, in the render worker and on the main thread. Your own vaults and
// settings are never opened: Obsidian gets a profile folder of its own, and the run stops before
// changing anything if the vault Obsidian opens is not the one made for it.
//
//   npm run test:obsidian          (builds first)
//
// OBSIDIAN is the app's executable when it is not in the usual place. E2E_DIR keeps the profile,
// the vault and screenshots in that folder; otherwise they go in a temporary folder that is removed.
import assert from "node:assert/strict";
import {spawn} from "node:child_process";
import {randomBytes} from "node:crypto";
import {existsSync} from "node:fs";
import {cp, mkdir, mkdtemp, readFile, realpath, rm, stat, writeFile} from "node:fs/promises";
import {createServer} from "node:net";
import {tmpdir} from "node:os";
import {join} from "node:path";
import test, {after, before} from "node:test";
import {fileURLToPath} from "node:url";
import {chromium} from "playwright-core";

const here = fileURLToPath(new URL(".", import.meta.url));
const plugin = fileURLToPath(new URL("../../", import.meta.url));
const repo = fileURLToPath(new URL("../../../../", import.meta.url));
const docs = join(repo, "docs/page");
const runtime = join(repo, "tools/skill/vis-skills/assets/wasm");

const EXECUTABLES = {
    darwin: ["/Applications/Obsidian.app/Contents/MacOS/Obsidian"],
    win32: [join(process.env.LOCALAPPDATA ?? "", "Programs/Obsidian/Obsidian.exe")],
    linux: ["/usr/bin/obsidian", "/opt/Obsidian/obsidian", "/snap/bin/obsidian"],
};
const executable = process.env.OBSIDIAN ?? (EXECUTABLES[process.platform] ?? []).find((path) => existsSync(path));
const skip = executable ? false : "Obsidian is not installed here; set OBSIDIAN to its executable";
const options = {skip, timeout: 90_000};

let root, vault, child, browser, app, sink, log = "";
const errors = [];
// Whatever scene code manages to send out arrives here.
const hits = [];
const listener = createServer((socket) => {
    hits.push("connection");
    socket.once("data", (data) => {
        hits.push(String(data).split("\r\n")[0]);
        socket.end("HTTP/1.1 200 OK\r\nAccess-Control-Allow-Origin: *\r\nContent-Length: 0\r\nConnection: close\r\n\r\n");
    });
    socket.on("error", () => undefined);
});

const delay = (ms) => new Promise((resolve) => setTimeout(resolve, ms));

/** Calls `check` until it returns something, or fails after `timeout`. */
async function until(check, what, timeout = 20_000) {
    const deadline = Date.now() + timeout;
    for (;;) {
        const value = await check();
        if (value) return value;
        if (Date.now() > deadline) throw new Error(`Gave up waiting for ${what}`);
        await delay(250);
    }
}

const pages = () => browser.contexts().flatMap((context) => context.pages());

async function freePort() {
    const server = createServer();
    await new Promise((resolve) => server.listen(0, "127.0.0.1", resolve));
    const {port} = server.address();
    await new Promise((resolve) => server.close(resolve));
    return port;
}

async function makeVault() {
    vault = join(root, "vault");
    await cp(join(here, "vault"), vault, {recursive: true});
    // A font scenes name that the plugin does not ship.
    await mkdir(join(vault, "Fonts"), {recursive: true});
    await cp(join(repo, "src/examples/assets/PretendardStd-Regular.ttf"), join(vault, "Fonts/PretendardStd-Regular.ttf"));
    await cp(join(docs, "assets/earth.png"), join(vault, "earth.png"));
    // A deterministic, decoded sound for the audio cue examples.
    const rate = 8000, samples = rate, tone = Buffer.alloc(44 + samples * 2);
    tone.write("RIFF"); tone.writeUInt32LE(36 + samples * 2, 4); tone.write("WAVEfmt ", 8);
    tone.writeUInt32LE(16, 16); tone.writeUInt16LE(1, 20); tone.writeUInt16LE(1, 22);
    tone.writeUInt32LE(rate, 24); tone.writeUInt32LE(rate * 2, 28); tone.writeUInt16LE(2, 32); tone.writeUInt16LE(16, 34);
    tone.write("data", 36); tone.writeUInt32LE(samples * 2, 40);
    for (let i = 0; i < samples; i++) {
        // Keep the loop seam quiet so quality checks measure the gain envelope, not an abrupt
        // source boundary after resampling the 8 kHz fixture to the device's sample rate.
        const edge = Math.min(1, i / (rate * 0.01), (samples - 1 - i) / (rate * 0.01));
        tone.writeInt16LE(Math.round(Math.sin(2 * Math.PI * 440 * i / rate) * 8000 * edge), 44 + i * 2);
    }
    await mkdir(join(vault, "assets"), {recursive: true});
    await writeFile(join(vault, "assets/tone.wav"), tone);
    const dir = join(vault, ".obsidian/plugins/viscript");
    await mkdir(dir, {recursive: true});
    for (const name of ["main.js", "manifest.json", "styles.css"]) await cp(join(plugin, name), join(dir, name));
    await writeFile(join(dir, "data.json"), JSON.stringify({fontFolder: "Fonts"}));
    await writeFile(join(vault, ".obsidian/community-plugins.json"), JSON.stringify(["viscript"]));
}

async function launch() {
    const profile = join(root, "profile");
    await mkdir(profile, {recursive: true});
    // Obsidian's list of vaults, with only the test vault, open at start.
    await writeFile(join(profile, "obsidian.json"), JSON.stringify({
        vaults: {[randomBytes(8).toString("hex")]: {path: vault, ts: Date.now(), open: true}}, updateDisabled: true,
    }));
    const port = await freePort();
    child = spawn(executable, [`--user-data-dir=${profile}`, `--remote-debugging-port=${port}`], {stdio: ["ignore", "pipe", "pipe"]});
    for (const stream of [child.stdout, child.stderr]) stream.on("data", (data) => { log += data; });
    browser = await until(() => {
        if (child.exitCode !== null) throw new Error(`Obsidian quit:\n${log}`);
        return chromium.connectOverCDP(`http://127.0.0.1:${port}`).catch(() => null);
    }, "Obsidian to start", 60_000);
    app = await until(() => pages().find((page) => page.url().includes("index.html")), "the vault's window", 60_000);
    app.on("pageerror", (error) => errors.push(error.message));
    app.on("console", (message) => { if (message.type() === "error") errors.push(message.text()); });
    const opened = await (await app.waitForFunction(() => window.app?.vault?.adapter?.basePath, null, {timeout: 60_000})).jsonValue();
    if (await realpath(opened) !== await realpath(vault)) {
        throw new Error(`Obsidian opened ${opened}, not the test vault; stopped before changing anything`);
    }
}

async function enablePlugin() {
    // The vault comes with the plugin, so Obsidian asks whether to trust its author.
    const trust = app.getByRole("button", {name: "Trust author and enable plugins"});
    const loaded = () => Boolean(window.app?.plugins?.plugins?.viscript?._loaded && window.app.workspace?.layoutReady);
    await until(async () => {
        if (await app.evaluate(loaded).catch(() => false)) return true;
        if (await trust.isVisible().catch(() => false)) await trust.click();
        return false;
    }, "the plugin to load", 30_000);
}

before(async () => {
    if (skip) return;
    root = process.env.E2E_DIR ?? await mkdtemp(join(tmpdir(), "viscript-obsidian-"));
    await mkdir(root, {recursive: true});
    await new Promise((resolve) => listener.listen(0, "127.0.0.1", resolve));
    sink = `127.0.0.1:${listener.address().port}`;
    await makeVault();
    await launch();
    await enablePlugin();
});

after(async () => {
    if (skip) return;
    listener.close();
    await browser?.close().catch(() => undefined);
    if (child && child.exitCode === null) {
        const exited = new Promise((resolve) => child.once("exit", resolve));
        child.kill();
        await Promise.race([exited, delay(5000)]);
        if (child.exitCode === null) child.kill("SIGKILL");
    }
    if (!process.env.E2E_DIR) await rm(root, {recursive: true, force: true, maxRetries: 5}).catch(() => undefined);
});

const READING = ".markdown-reading-view .viscript-scene";
const LIVE = ".markdown-source-view .viscript-scene";
const VIEW = ".workspace-leaf.mod-active .viscript-view";
const ACTIVE_READING = `.workspace-leaf.mod-active ${READING}`;
const ACTIVE_LIVE = `.workspace-leaf.mod-active ${LIVE}`;
const SHOWN = ["playing", "paused", "stopped"];

async function openNote(path, mode = "preview") {
    await app.evaluate(async ([path, mode]) => {
        await window.app.workspace.getLeaf(false).openFile(window.app.vault.getFileByPath(path), {state: {mode}});
    }, [path, mode]);
}

// The scenes `selector` finds: their state, and the error or notice each shows.
function scenes(selector, page = app) {
    return page.evaluate((selector) => [...document.querySelectorAll(selector)].map((scene) => {
        const error = scene.querySelector(".viscript-error"), notice = scene.querySelector(".viscript-notice");
        return {
            state: scene.dataset.state,
            error: error?.isShown() ? error.textContent : null,
            notice: notice?.isShown() ? notice.textContent : null,
        };
    }), selector);
}

// Scrolls to each scene in turn and waits for it to draw: players load only once seen.
async function showEach(selector) {
    const count = await app.locator(selector).count();
    for (let at = 0; at < count; at++) {
        await app.locator(selector).nth(at).scrollIntoViewIfNeeded();
        await app.waitForFunction(([selector, at, shown]) => {
            const scene = document.querySelectorAll(selector)[at];
            return scene && (scene.classList.contains("is-empty") || shown.includes(scene.dataset.state)
                || scene.querySelector(".viscript-error")?.isShown());
        }, [selector, at, SHOWN], {timeout: 20_000});
    }
}

async function shot(name, page = app) {
    if (process.env.E2E_DIR) await page.screenshot({path: join(root, `${name}.png`)});
}

// The settings, in their own window in Obsidian 1.14 and over the notes before.
const settingsPage = () => until(() => pages().find((page) => page.url() === "about:blank"), "the settings window", 3000)
    .catch(() => app);

// The scene's background color, read halfway down its left edge, where these scenes draw nothing.
async function backgroundColor(canvas) {
    const png = (await canvas.screenshot()).toString("base64");
    return app.evaluate(async (png) => {
        const image = new Image();
        image.src = `data:image/png;base64,${png}`;
        await image.decode();
        const context = document.createElement("canvas").getContext("2d");
        [context.canvas.width, context.canvas.height] = [image.width, image.height];
        context.drawImage(image, 0, 0);
        return [...context.getImageData(Math.round(image.width * 0.04), Math.round(image.height / 2), 1, 1).data.slice(0, 3)];
    }, png);
}

const hostMode = () => app.evaluate(async () => (await window.app.plugins.plugins.viscript.host()).mode);

// Loads a JavaScript scene in the plugin's render host: its width, or why it did not load.
const probe = (source) => app.evaluate(async (source) => {
    const host = await window.app.plugins.plugins.viscript.host();
    const session = host.open(document.createElement("canvas"), {framed() {}, failed() {}, lost() {}});
    try {
        return (await session.load({source, language: "js", name: "probe.js", resources: {assets: []}})).width;
    } catch (error) {
        return error.message;
    } finally {
        session.close();
    }
}, source);

// A scene that tries each way out to the listener, and whose width tells whether its code was kept
// from Node, the page and Obsidian's storage.
const SCOPE = () => `const attempt = (run) => { try { const result = run(); if (result && result.catch) result.catch(() => {}); } catch {} };
attempt(() => fetch("http://${sink}/fetch"));
attempt(() => { const request = new XMLHttpRequest(); request.open("GET", "http://${sink}/xhr"); request.send(); });
attempt(() => new WebSocket("ws://${sink}/websocket"));
attempt(() => new EventSource("http://${sink}/eventsource"));
attempt(() => import("http://${sink}/import.js"));
attempt(() => importScripts("http://${sink}/importScripts.js"));
const storage = (() => { try { indexedDB.open("probe"); return true; } catch { return false; } })();
const apart = typeof require === "undefined" && typeof process === "undefined" && typeof document === "undefined" && !storage;
return vis.scene({width: apart ? 64 : 32, height: 16});`;

// What the scene sent out, after giving it time to.
async function sent() {
    await delay(1500);
    return hits.splice(0);
}

test("scenes play in reading view: code blocks, an embed and JavaScript", options, async () => {
    await app.evaluate(async () => {
        const file = window.app.vault.getFileByPath("Scenes.md");
        await window.app.workspace.getLeaf(false).openFile(file, {state: {mode: "preview"}});
    });
    await app.waitForSelector(READING);
    await showEach(READING);
    const shown = await scenes(READING);
    assert.equal(shown.length, 5);
    assert.deepEqual(shown.filter((scene) => scene.error), []);
    assert.match(shown.at(-1).notice, /is not a viscript scene/);
    assert.equal(await hostMode(), "worker");
    await shot("reading");
});

test("a click on a scene pauses it, and another plays it again", options, async () => {
    const scene = app.locator(READING).first();
    const canvas = scene.locator(".viscript-canvas");
    const reaches = (wanted) => until(async () => await scene.getAttribute("data-state") === wanted, `the scene to be ${wanted}`, 5000);
    await canvas.scrollIntoViewIfNeeded();
    await reaches("playing");
    await canvas.click();
    await reaches("paused");
    await canvas.click();
    await reaches("playing");
});

test("fonts by name: the theme's from the plugin, one the scene names from the folder, the rest in the text font", options, async () => {
    const found = await app.evaluate(async () => {
        const fonts = window.app.plugins.plugins.viscript.fonts;
        const found = {};
        for (const name of ["Pretendard", "JetBrainsMono", "Pretendard Std", "No Such Sans", "No Such Mono"]) {
            const font = await fonts.fetch(name);
            // A bundled font arrives whole; one from the folder as the face the engine reads.
            found[name] = font.source.startsWith("bundled:") ? [font.source, font.data.byteLength] : font.source;
        }
        return found;
    });
    const bundled = async (role, file) => [`bundled:${role}`, (await stat(join(runtime, file))).size];
    assert.deepEqual(found, {
        // The files VS Code ships, Korean included.
        Pretendard: await bundled("text", "Pretendard.ttf"),
        JetBrainsMono: await bundled("code", "JetBrainsMono.ttf"),
        "Pretendard Std": "folder:Fonts/PretendardStd-Regular.ttf",
        "No Such Sans": await bundled("text", "Pretendard.ttf"),
        "No Such Mono": await bundled("code", "JetBrainsMono.ttf"),
    });
});

test("an API example has one player with Lua and JavaScript, while unrelated examples keep their players", options, async () => {
    await openNote("Paired.md");
    await app.waitForSelector(ACTIVE_READING);
    await showEach(ACTIVE_READING);
    assert.equal(await app.locator(ACTIVE_READING).count(), 1);
    assert.equal(await app.locator(`${ACTIVE_READING} .viscript-source-language`).count(), 2);
    assert.deepEqual((await scenes(ACTIVE_READING)).filter((scene) => scene.error), []);

    await openNote("Unpaired.md");
    await app.waitForSelector(ACTIVE_READING);
    await showEach(ACTIVE_READING);
    assert.equal(await app.locator(ACTIVE_READING).count(), 3, "unmarked and separately explained scenes stay separate");
    assert.deepEqual((await scenes(ACTIVE_READING)).filter((scene) => scene.error), []);
});

test("audio cues produce decoded WebAudio samples in Lua and JavaScript, and pausing silences them", options, async (t) => {
    const cdp = await app.context().newCDPSession(app);
    await cdp.send("Emulation.setFocusEmulationEnabled", {enabled: true});
    t.after(async () => {
        await openNote("Scenes.md").catch(() => undefined);
        await app.evaluate(() => {
            for (const probe of window.e2eAudioProbes ?? []) probe.node.removeEventListener("audioprocess", probe.listen);
            delete window.e2eAudioProbes;
            delete window.e2eAudioBlock;
            delete window.e2eAudioCanvas;
        }).catch(() => undefined);
        await cdp.send("Emulation.setFocusEmulationEnabled", {enabled: false}).catch(() => undefined);
        await cdp.detach().catch(() => undefined);
    });
    await openNote("Audio.md");
    await app.waitForSelector(ACTIVE_READING);
    await showEach(ACTIVE_READING);
    assert.equal(await app.locator(ACTIVE_READING).count(), 1);
    assert.deepEqual((await scenes(ACTIVE_READING)).filter((scene) => scene.error), []);
    const assets = await app.evaluate((selector) => {
        const source = document.querySelector(`${selector} .viscript-source-panel code`).textContent;
        return window.app.plugins.plugins.viscript.resources({source, language: "lua", path: "Audio.md", name: "audio.lua"}).assets;
    }, ACTIVE_READING);
    assert.equal(assets.length, 1, "a basename resolves the vault's audio attachment");
    assert.match(assets[0].id, /^assets\/tone\.wav@/);
    assert.equal(assets[0].name, "tone.wav");
    assert.equal(assets[0].mime, "audio/wav");
    const scene = app.locator(ACTIVE_READING), play = scene.locator(".viscript-play");
    const playerAction = (action) => app.evaluate(([selector, action]) => {
        const canvas = document.querySelector(`${selector} canvas`);
        const player = [...window.app.plugins.plugins.viscript.blocks].find((block) => block.player?.canvas === canvas)?.player;
        if (action === "pause") { player.pause(); player.seek(0); }
        return {state: player.state, duration: player.duration};
    }, [ACTIVE_READING, action]);
    // The real miniaudio callback fills these buffers from decoded WAV bytes. Observing it after
    // its callback, rather than replacing the audio API, catches a missing asset or silent mixer.
    const observe = () => app.evaluate(() => {
        const probes = window.e2eAudioProbes ??= [];
        for (const device of window.miniaudio?.devices ?? []) {
            if (!device?.scriptNode || probes.some((probe) => probe.node === device.scriptNode)) continue;
            const probe = {device, node: device.scriptNode, frames: 0, nonzero: 0, peak: 0};
            probe.listen = (event) => {
                probe.frames++;
                for (let channel = 0; channel < event.outputBuffer.numberOfChannels; channel++) {
                    const samples = event.outputBuffer.getChannelData(channel);
                    for (const sample of samples) {
                        const level = Math.abs(sample);
                        if (level > 0.00001) probe.nonzero++;
                        if (level > probe.peak) probe.peak = level;
                    }
                }
            };
            probe.node.addEventListener("audioprocess", probe.listen);
            probes.push(probe);
        }
        return probes.map((probe) => ({state: probe.device.webaudio?.state, frames: probe.frames, nonzero: probe.nonzero, peak: probe.peak}));
    });
    const reset = () => app.evaluate(() => {
        for (const probe of window.e2eAudioProbes ?? []) probe.frames = probe.nonzero = probe.peak = 0;
    });
    for (const language of ["lua", "js"]) {
        if (language === "js") {
            await scene.locator('.viscript-source-language[data-language="js"]').click();
            await until(async () => (await playerAction("inspect")).duration === 5 &&
                (await scenes(ACTIVE_READING)).every((scene) => SHOWN.includes(scene.state) && !scene.error), "the JavaScript audio scene");
        }
        await playerAction("pause");
        await until(async () => (await playerAction("inspect")).state === "paused", "the audio scene paused");
        await reset();
        await play.click();
        const emitted = await until(async () => (await observe()).some((probe) => probe.state === "running" && probe.nonzero > 0),
            `${language} audio to emit nonzero decoded PCM`, 6000).catch(async (error) => {
                const evidence = await app.evaluate((selector) => {
                    const source = document.querySelector(`${selector} .viscript-source-panel code`)?.textContent ?? "";
                    const plugin = window.app.plugins.plugins.viscript;
                    return {resources: plugin.resources({source, language: "lua", path: "Audio.md", name: "audio.lua"}),
                        audioDevices: (window.miniaudio?.devices ?? []).map((device) => device && {state: device.state, context: device.webaudio?.state}),
                        scene: document.querySelector(selector)?.dataset.state};
                }, ACTIVE_READING);
                assert.fail(`${error.message}\n${JSON.stringify(evidence)}\n${JSON.stringify(await observe())}`);
            });
        assert.ok(emitted);
        await play.click();
        await until(async () => (await playerAction("inspect")).state === "paused", "the cue paused with the scene");
        await delay(350); // Let the last queued device buffer finish.
        await reset();
        await delay(300);
        assert.ok((await observe()).every((probe) => probe.nonzero === 0), "paused scenes output no audio samples");
        await play.click();
        await until(async () => (await observe()).some((probe) => probe.state === "running" && probe.nonzero > 0),
            `${language} audio to resume with playback`, 6000);
        await play.click();
    }
    // Editing a source can add or remove sound without changing the block. A transferred canvas
    // belongs to the worker; an inline canvas keeps its main-thread 2D context.
    const js = (await readFile(join(vault, "Audio.md"), "utf8")).match(/```viscript-js loop\n([^]*?)\n```/)[1];
    await app.evaluate((selector) => {
        const canvas = document.querySelector(`${selector} canvas`);
        window.e2eAudioBlock = [...window.app.plugins.plugins.viscript.blocks].find((block) => block.player?.canvas === canvas);
        window.e2eAudioCanvas = canvas;
    }, ACTIVE_READING);
    const replaceSource = (source) => app.evaluate((source) => {
        window.e2eAudioBlock.show({source, language: "js", name: "audio.js", path: "Audio.md"});
    }, source);
    const surface = () => app.evaluate(() => {
        const player = window.e2eAudioBlock.player, canvas = player?.canvas;
        let inline = false;
        try { inline = Boolean(canvas?.getContext("2d")); } catch {}
        return {changed: Boolean(canvas && canvas !== window.e2eAudioCanvas), state: player?.state, inline};
    });
    await replaceSource(js.replace(/^scene\.sound\([^\n]+\);\n/m, ""));
    await until(async () => {
        const seen = await surface();
        return seen.changed && SHOWN.includes(seen.state) && !seen.inline;
    }, "an edited silent scene to return to the worker");
    await app.evaluate(() => { window.e2eAudioCanvas = window.e2eAudioBlock.player.canvas; });
    await replaceSource(js);
    await until(async () => {
        const seen = await surface();
        return seen.changed && SHOWN.includes(seen.state) && seen.inline;
    }, "an edited sound scene to return to inline rendering");
    await playerAction("pause");
    await observe();
    await reset();
    await play.click();
    await until(async () => (await observe()).some((probe) => probe.state === "running" && probe.nonzero > 0),
        "the edited source to play its sound again", 6000);
    await play.click();
    await openNote("Runtime Audio.md");
    await showEach(ACTIVE_READING);
    const runtime = app.locator(`${ACTIVE_READING} canvas`);
    await runtime.click();
    await observe();
    await reset();
    await runtime.press("Space");
    await until(async () => (await observe()).some((probe) => probe.state === "running" && probe.nonzero > 0),
        "a retained runtime sound to emit nonzero decoded PCM after Space", 6000);
    assert.equal(await hostMode(), "worker", "audio scenes leave the ordinary render worker enabled");
    await shot("audio");
});

test("cue gain fades stay bounded and continuous through repeated Lua and JavaScript playback", options, async (t) => {
    const cdp = await app.context().newCDPSession(app);
    await cdp.send("Emulation.setFocusEmulationEnabled", {enabled: true});
    const clear = () => app.evaluate(() => {
        for (const probe of window.e2eGainProbes ?? []) probe.node.removeEventListener("audioprocess", probe.listen);
        delete window.e2eGainProbes;
    });
    t.after(async () => {
        await clear().catch(() => undefined);
        await app.evaluate(() => {
            for (const block of window.app.plugins.plugins.viscript.blocks) block.player?.pause();
        }).catch(() => undefined);
        await openNote("Scenes.md").catch(() => undefined);
        await cdp.send("Emulation.setFocusEmulationEnabled", {enabled: false}).catch(() => undefined);
        await cdp.detach().catch(() => undefined);
    });
    await app.evaluate(() => {
        for (const block of window.app.plugins.plugins.viscript.blocks) block.player?.pause();
    });
    await openNote("Gain.md");
    await app.waitForSelector(ACTIVE_READING);
    await showEach(ACTIVE_READING);
    const scene = app.locator(ACTIVE_READING);
    assert.equal(await scene.count(), 1);
    assert.deepEqual((await scenes(ACTIVE_READING)).filter((scene) => scene.error), []);

    for (const language of ["lua", "js"]) {
        if (language === "js") {
            await scene.locator('.viscript-source-language[data-language="js"]').click();
            await until(async () => (await scenes(ACTIVE_READING)).every((scene) => SHOWN.includes(scene.state) && !scene.error),
                "the JavaScript gain scene");
        }
        await app.evaluate((selector) => {
            const plugin = window.app.plugins.plugins.viscript;
            const canvas = document.querySelector(`${selector} canvas`);
            const player = [...plugin.blocks].find((block) => block.player?.canvas === canvas).player;
            player.pause();
            player.seek(0);
        }, ACTIVE_READING);
        await delay(200);
        await clear();
        // Listen after miniaudio's actual mixer, without substituting a browser audio API. A
        // 0.244-full-scale sine at gain 0..1 must stay bounded, including after a loop restarts it.
        await app.evaluate((selector) => {
            const plugin = window.app.plugins.plugins.viscript;
            const canvas = document.querySelector(`${selector} canvas`);
            const player = [...plugin.blocks].find((block) => block.player?.canvas === canvas).player;
            window.e2eGainProbes = [];
            for (const device of window.miniaudio?.devices ?? []) {
                if (!device?.scriptNode) continue;
                const probe = {node: device.scriptNode, buffers: 0, nonzero: 0, peak: 0, maxStep: 0, middleStep: 0,
                    loops: 0, lastTime: 0, lastSamples: [], maxStepAt: null};
                probe.listen = (event) => {
                    probe.buffers++;
                    const time = player.time;
                    if (time < probe.lastTime - 0.5) probe.loops++;
                    probe.lastTime = time;
                    for (let channel = 0; channel < event.outputBuffer.numberOfChannels; channel++) {
                        const samples = event.outputBuffer.getChannelData(channel);
                        let previous = probe.lastSamples[channel] ?? 0;
                        for (let i = 0; i < samples.length; i++) {
                            const sample = samples[i];
                            const level = Math.abs(sample);
                            if (level > 0.00001) probe.nonzero++;
                            probe.peak = Math.max(probe.peak, level);
                            const step = Math.abs(sample - previous);
                            if (step > probe.maxStep) {
                                probe.maxStep = step;
                                probe.maxStepAt = {buffer: probe.buffers, channel, offset: i, time, previous, sample};
                            }
                            previous = sample;
                        }
                        probe.lastSamples[channel] = previous;
                        // No transport operation occurs inside a callback. Its middle must be as
                        // continuous as the decoded sine, even when a fade spans mixer chunks.
                        const middle = Math.floor(samples.length / 2);
                        for (let i = Math.max(1, middle - 4); i < Math.min(samples.length, middle + 5); i++) {
                            probe.middleStep = Math.max(probe.middleStep, Math.abs(samples[i] - samples[i - 1]));
                        }
                    }
                };
                probe.node.addEventListener("audioprocess", probe.listen);
                window.e2eGainProbes.push(probe);
            }
        }, ACTIVE_READING);
        await scene.locator(".viscript-play").click();
        await delay(3600);
        const heard = await app.evaluate(() => (window.e2eGainProbes ?? []).map(({buffers, nonzero, peak, maxStep, middleStep, loops, maxStepAt}) =>
            ({buffers, nonzero, peak, maxStep, middleStep, loops, maxStepAt})));
        if (process.env.E2E_DIR) await writeFile(join(root, `gain-${language}.json`), JSON.stringify(heard));
        assert.ok(heard.some((probe) => probe.nonzero > 10_000 && probe.loops >= 2),
            `${language} must emit decoded samples across two scene restarts: ${JSON.stringify(heard)}`);
        assert.ok(heard.every((probe) => probe.peak <= 0.28),
            `${language} gain must not amplify the 0.244-full-scale asset: ${JSON.stringify(heard)}`);
        assert.ok(heard.every((probe) => probe.middleStep <= 0.04),
            `${language} gain fades must not click inside audio buffers: ${JSON.stringify(heard)}`);
        assert.ok(heard.every((probe) => probe.maxStep <= 0.04),
            `${language} cue looping must preserve sample continuity: ${JSON.stringify(heard)}`);
        await scene.locator(".viscript-play").click();
        await clear();
    }
    await shot("gain");
});

test("source settings and each player's toggle keep its canvas and time, and language tabs show the exact source", options, async (t) => {
    const note = await readFile(join(vault, "Paired.md"), "utf8");
    const lua = note.match(/```viscript loop\n([^]*?)\n```/)[1];
    const js = note.match(/```viscript-js loop\n([^]*?)\n```/)[1];
    t.after(async () => {
        await app.evaluate(async () => {
            delete window.e2eSourcePlayer;
            await window.app.plugins.plugins.viscript.update({showSource: false});
        }).catch(() => undefined);
        await openNote("Scenes.md");
    });
    await openNote("Paired.md");
    await app.waitForSelector(ACTIVE_READING);
    await showEach(ACTIVE_READING);
    const scene = app.locator(ACTIVE_READING);
    const panel = scene.locator(".viscript-source-panel");
    const toggle = scene.locator(".viscript-source-toggle");
    assert.equal(await scene.locator(".viscript-source-tools").count(), 1);
    assert.equal(await panel.isVisible(), false, "a new vault keeps source hidden until asked for");

    await app.evaluate(() => window.app.plugins.plugins.viscript.update({showSource: true}));
    await panel.waitFor({state: "visible"});
    assert.equal((await panel.locator("code").textContent()).trimEnd(), lua);
    await until(async () => await panel.locator("code.language-lua .token").count(), "highlighted Lua");
    await app.evaluate((selector) => {
        const canvas = document.querySelector(`${selector} canvas`);
        const player = [...window.app.plugins.plugins.viscript.blocks].find((block) => block.player?.canvas === canvas).player;
        canvas.dataset.e2eSource = "kept";
        window.e2eSourcePlayer = player;
        player.pause();
        player.seek(1.25);
    }, ACTIVE_READING);
    const kept = () => app.evaluate((selector) => {
        const canvas = document.querySelector(`${selector} canvas`);
        const player = [...window.app.plugins.plugins.viscript.blocks].find((block) => block.player?.canvas === canvas)?.player;
        return player === window.e2eSourcePlayer && canvas?.dataset.e2eSource === "kept"
            && player.state === "paused" && Math.abs(player.time - 1.25) < 0.05;
    }, ACTIVE_READING);
    await until(kept, "the scene paused at the chosen time");
    await toggle.click();
    assert.equal(await toggle.getAttribute("aria-expanded"), "false");
    assert.equal(await panel.isVisible(), false);
    assert.ok(await kept(), "hiding source kept the canvas, player and time");
    await toggle.click();
    assert.equal(await toggle.getAttribute("aria-expanded"), "true");
    assert.ok(await kept(), "showing source kept the canvas, player and time");

    await app.evaluate(() => window.app.plugins.plugins.viscript.update({showSource: false}));
    assert.equal(await panel.isVisible(), false);
    assert.ok(await kept(), "the default setting only changes presentation");
    await toggle.click();
    assert.equal(await panel.isVisible(), true);
    assert.equal(await app.evaluate(() => window.app.plugins.plugins.viscript.settings.showSource), false,
        "a player's toggle does not change the vault setting");
    await app.evaluate(() => window.app.plugins.plugins.viscript.update({showSource: true}));
    assert.equal(await panel.isVisible(), true);
    assert.ok(await kept());

    const javascript = scene.locator('.viscript-source-language[data-language="js"]');
    await javascript.click();
    assert.equal(await javascript.getAttribute("aria-pressed"), "true");
    assert.equal((await panel.locator("code").textContent()).trimEnd(), js);
    await until(async () => await panel.locator("code.language-javascript .token").count(), "highlighted JavaScript");
    await until(() => app.evaluate((selector) => {
        const canvas = document.querySelector(`${selector} canvas`);
        return [...window.app.plugins.plugins.viscript.blocks].find((block) => block.player?.canvas === canvas)?.player?.duration === 6;
    }, ACTIVE_READING), "the JavaScript scene to load");
    assert.equal(await app.locator(ACTIVE_READING).count(), 1);

    // Automated runs leave the operating system clipboard alone.
    assert.equal(await scene.getByRole("button", {name: "Copy source", exact: true}).isVisible(), true);
    assert.equal(await scene.getByRole("button", {name: "Copy source", exact: true}).isEnabled(), true);
    await shot("source-panel");
});

test("editing either language keeps one paired player above the original source without a second source panel", options, async (t) => {
    const original = await readFile(join(vault, "Paired.md"), "utf8");
    const cdp = await app.context().newCDPSession(app);
    await cdp.send("Emulation.setFocusEmulationEnabled", {enabled: true});
    const editing = ".workspace-leaf.mod-active .viscript-editing";
    const select = (text, offset = 0, length = 0) => app.evaluate(([text, offset, length]) => {
        const editor = window.app.workspace.activeEditor.editor;
        const at = editor.getValue().indexOf(text) + offset;
        editor.focus();
        editor.setSelection(editor.offsetToPos(at), editor.offsetToPos(at + length));
    }, [text, offset, length]);
    const version = (selector, duration) => app.evaluate(([selector, duration, shown]) => {
        const canvas = [...document.querySelectorAll(selector)].find((scene) => scene.isShown())?.querySelector("canvas");
        const player = [...window.app.plugins.plugins.viscript.blocks].find((block) => block.player?.canvas === canvas)?.player;
        return player && shown.includes(player.state) && player.duration === duration;
    }, [selector, duration, SHOWN]);
    const setMode = (source) => app.evaluate(async (source) => {
        const leaf = window.app.workspace.getMostRecentLeaf();
        await leaf.setViewState({type: "markdown", state: {file: "Paired.md", mode: "source", source}});
        window.app.workspace.setActiveLeaf(leaf, {focus: true});
    }, source);
    t.after(async () => {
        await app.evaluate((source) => window.app.vault.modify(window.app.vault.getFileByPath("Paired.md"), source), original)
            .catch(() => undefined);
        await app.evaluate(() => window.app.plugins.plugins.viscript.update({showSource: false})).catch(() => undefined);
        await openNote("Scenes.md");
        await cdp.send("Emulation.setFocusEmulationEnabled", {enabled: false}).catch(() => undefined);
        await cdp.detach().catch(() => undefined);
    });
    await openNote("Paired.md");
    await app.evaluate(() => window.app.plugins.plugins.viscript.update({showSource: true}));
    await setMode(false);
    for (const [source, duration] of [["local scene = vis.scene", 4], ["const scene = vis.scene", 6]]) {
        await select(source);
        await until(async () => await app.locator(`${editing}:visible`).count() === 1, "the edited example's scene");
        await app.locator(`${editing}:visible`).scrollIntoViewIfNeeded();
        await until(async () => await app.locator(`${ACTIVE_LIVE}:visible`).count() === 1, "one player for the edited Lua/JavaScript pair");
        await until(() => version(editing, duration), `the ${duration}-second version to play`);
        assert.equal(await app.locator(`${editing} .viscript-source-tools, ${editing} .viscript-source-panel`).count(), 0);
        assert.ok(await app.locator(".workspace-leaf.mod-active .cm-line", {hasText: source}).count(), "the original Markdown source stays visible");
        assert.deepEqual((await scenes(ACTIVE_LIVE)).filter((scene) => scene.error), []);
    }
    await select("scene.wait(6)", "scene.wait(".length, 1);
    await app.keyboard.type("7");
    await until(() => version(editing, 7), "the edited JavaScript version to play for seven seconds");
    await until(async () => (await readFile(join(vault, "Paired.md"), "utf8")).includes("scene.wait(7)"), "the paired edit to be saved");
    await select("# Paired example");
    await until(async () => await app.locator(`${editing}:visible`).count() === 0 && await app.locator(`${ACTIVE_LIVE}:visible`).count() === 1,
        "the single native preview after editing");
    await app.locator(`${ACTIVE_LIVE}:visible .viscript-source-panel`).waitFor({state: "visible"});
    await app.locator(`${ACTIVE_LIVE}:visible .viscript-source-language[data-language="js"]`).click();
    await until(async () => (await app.locator(`${ACTIVE_LIVE}:visible .viscript-source-panel code`).textContent()).includes("scene.wait(7)"),
        "the JavaScript tab to show the edited source");
    await until(() => version(ACTIVE_LIVE, 7), "the native player to use the edited JavaScript version");
    await setMode(true);
    assert.equal(await app.locator(ACTIVE_LIVE).count(), 0, "source mode has only the original code");
});

test("Live Preview draws the same scenes", options, async () => {
    await app.evaluate(async () => {
        const leaf = window.app.workspace.getMostRecentLeaf();
        await leaf.setViewState({type: "markdown", state: {file: "Scenes.md", mode: "source", source: false}});
    });
    await app.waitForSelector(LIVE);
    await showEach(LIVE);
    const shown = await scenes(LIVE);
    assert.ok(shown.length >= 3, `${shown.length} scenes`);
    assert.deepEqual(shown.filter((scene) => scene.error), []);
    await shot("live-preview");
});

test("a code block being edited in Live Preview keeps its scene above the source, which follows the typing", options, async (t) => {
    // Obsidian shows a block's source only to an editor with focus, which a window behind others lacks.
    const cdp = await app.context().newCDPSession(app);
    await cdp.send("Emulation.setFocusEmulationEnabled", {enabled: true});
    const EDITING = ".markdown-source-view .viscript-editing";
    const setMode = (source) => app.evaluate(async (source) => {
        const leaf = window.app.workspace.getLeavesOfType("markdown").find((leaf) => leaf.view.file?.path === "Scenes.md");
        await leaf.setViewState({type: "markdown", state: {file: "Scenes.md", mode: "source", source}});
        window.app.workspace.setActiveLeaf(leaf, {focus: true});
    }, source);
    // Puts the cursor at `offset` characters into the first `text` in the note, in an editor with focus.
    const select = (text, offset = 0, length = 0) => app.evaluate(([text, offset, length]) => {
        const editor = window.app.workspace.activeEditor.editor;
        const at = editor.getValue().indexOf(text) + offset;
        editor.focus();
        editor.setSelection(editor.offsetToPos(at), editor.offsetToPos(at + length));
    }, [text, offset, length]);
    // The editing scene, and whether the first block's opening fence shows as source just below it.
    const editing = () => app.evaluate((selector) => {
        const el = document.querySelector(selector), scene = el?.querySelector(".viscript-scene");
        const fence = [...document.querySelectorAll(".markdown-source-view .cm-line")].find((line) => line.textContent.includes("viscript loop"));
        return {
            count: document.querySelectorAll(selector).length,
            state: scene?.dataset.state ?? null,
            error: el?.querySelector(".viscript-error")?.isShown() ? el.querySelector(".viscript-error").textContent : null,
            source: Boolean(fence),
            above: Boolean(el && fence && el.nextElementSibling === fence),
            kept: el?.dataset.kept === "yes",
            focused: document.activeElement?.matches(".markdown-source-view .cm-content") ?? false,
            time: el?.querySelector(".viscript-time")?.textContent ?? null,
        };
    }, EDITING);
    const reaches = (check, what) => until(async () => check(await editing()), what, 10_000).catch(async (error) => {
        assert.fail(`${error.message}\n${JSON.stringify(await editing())}`);
    });
    // The scene is above the line the editor scrolled to, and plays only once seen.
    const reveal = () => app.locator(EDITING).scrollIntoViewIfNeeded();
    t.after(async () => {
        await setMode(false).catch(() => undefined);
        await cdp.send("Emulation.setFocusEmulationEnabled", {enabled: false}).catch(() => undefined);
        await cdp.detach().catch(() => undefined);
    });

    await setMode(false);
    await select("local scene = vis.scene", 0);
    await reaches((seen) => seen.count === 1 && seen.above && seen.focused, "the scene above the source");
    await reveal();
    await reaches((seen) => SHOWN.includes(seen.state) && !seen.error, "the scene to draw");
    await app.evaluate((selector) => { document.querySelector(selector).dataset.kept = "yes"; }, EDITING);
    await shot("editing");

    // Typing changes the scene without making a new one.
    await select("scene:wait(0.5)", "scene:".length, "wait".length);
    await app.keyboard.type("wiat");
    await reveal();
    await reaches((seen) => /scene\.lua:\d+: .*wiat/.test(seen.error ?? "") && seen.kept && seen.focused, "the typed error");
    await select("scene:wiat(0.5)", "scene:".length, "wiat".length);
    await app.keyboard.type("wait");
    await reveal();
    await reaches((seen) => !seen.error && seen.kept && SHOWN.includes(seen.state), "the fixed scene");

    // A click pauses the scene and a drag on the timeline seeks, and neither ends the editing.
    const canvas = app.locator(`${EDITING} .viscript-canvas`);
    await reaches((seen) => seen.state === "playing", "the scene to play");
    await canvas.click();
    await reaches((seen) => seen.state === "paused" && seen.focused && seen.source && seen.kept, "a click to pause the scene");
    await canvas.hover();
    const box = await app.locator(`${EDITING} .viscript-scrub`).boundingBox();
    await app.mouse.move(box.x + box.width * 0.2, box.y + box.height / 2);
    await app.mouse.down();
    await app.mouse.move(box.x + box.width * 0.75, box.y + box.height / 2, {steps: 4});
    await app.mouse.up();
    await reaches((seen) => {
        const [time, duration] = (seen.time ?? "").split(" / ").map(Number);
        return Math.abs(time - duration * 0.75) < 0.25 && seen.focused && seen.source && seen.kept;
    }, "a drag to seek");

    // Leaving the block, or the editor, gives the block back to Obsidian's own scene.
    await select("# Scenes", 0);
    await reaches((seen) => seen.count === 0 && !seen.source, "the block to be drawn by Obsidian again");
    await select("local scene = vis.scene", 0);
    await reaches((seen) => seen.count === 1 && seen.above, "the scene above the source again");
    await app.evaluate(() => document.activeElement.blur());
    await reaches((seen) => seen.count === 0 && !seen.source, "the block to be drawn when the editor has no focus");

    // Source mode shows no scenes.
    await setMode(true);
    await select("local scene = vis.scene", 0);
    await delay(1000);
    assert.equal((await editing()).count, 0);
});

test("a scene file opens in its own tab, where typing saves the file and redraws", options, async () => {
    await app.evaluate(async () => {
        await window.app.workspace.getLeaf("tab").openFile(window.app.vault.getFileByPath("orbit.lua"));
    });
    const drawn = () => app.waitForFunction(([view, shown]) => shown.includes(document.querySelector(`${view} .viscript-scene`)?.dataset.state)
        && !document.querySelector(`${view} .viscript-error`).isShown(), [VIEW, SHOWN], {timeout: 20_000});
    await drawn();
    await app.evaluate(() => window.app.plugins.plugins.viscript.update({showSource: true}));
    assert.equal(await app.locator(`${VIEW} .viscript-source-tools, ${VIEW} .viscript-source-panel`).count(), 0,
        "the scene tab already has its own editor");
    // Select the label's text in the source and type over it.
    await app.locator(`${VIEW} .cm-line`, {hasText: "text = \"orbit.lua\""}).first().click();
    await app.evaluate((view) => {
        const tile = document.querySelector(`${view} .cm-content`).cmTile;
        const editor = tile.view ?? Object.values(tile).find((value) => value?.state && value?.dispatch);
        const at = editor.state.doc.toString().indexOf("orbit.lua\"");
        editor.dispatch({selection: {anchor: at, head: at + "orbit.lua".length}});
    }, VIEW);
    await app.keyboard.type("edited here");
    await until(async () => (await readFile(join(vault, "orbit.lua"), "utf8")).includes("\"edited here\""), "the edit to be saved", 10_000);
    await drawn();
    await shot("scene-view");
});

test("an error shows under the scene, and goes once the file is fixed", options, async () => {
    const original = await readFile(join(vault, "orbit.lua"), "utf8");
    const write = (text) => app.evaluate((text) => window.app.vault.modify(window.app.vault.getFileByPath("orbit.lua"), text), text);
    await write(original.replace("scene:wait(0.4)", "scene:wiat(0.4)"));
    await app.waitForFunction((view) => document.querySelector(`${view} .viscript-error`)?.isShown(), VIEW, {timeout: 10_000});
    assert.match(await app.locator(`${VIEW} .viscript-error`).textContent(), /orbit\.lua:\d+: .*wiat/);
    await write(original);
    await app.waitForFunction((view) => !document.querySelector(`${view} .viscript-error`).isShown(), VIEW, {timeout: 10_000});
});

test("scenes that name no theme take Obsidian's colors, light and dark", options, async () => {
    const canvas = app.locator(`${VIEW} canvas`);
    const near = (a, b) => a.every((value, at) => Math.abs(value - b[at]) <= 3);
    for (const theme of ["moonstone", "obsidian"]) {
        await app.evaluate((theme) => window.app.changeTheme(theme), theme);
        const wanted = await app.evaluate(() => {
            const probe = document.body.createDiv();
            probe.style.color = "var(--background-primary)";
            const color = getComputedStyle(probe).color.match(/\d+/g).slice(0, 3).map(Number);
            probe.remove();
            return color;
        });
        let seen;
        await until(async () => near(seen = await backgroundColor(canvas), wanted), `the ${theme} theme`, 10_000)
            .catch(() => assert.fail(`${theme}: the scene's background is ${seen}, the note's ${wanted}`));
        await shot(theme);
    }
    await app.evaluate(() => window.app.changeTheme("moonstone"));
});

test("the settings show the plugin's tab with a font preview", options, async () => {
    await app.evaluate(() => window.app.setting.open());
    const settings = await settingsPage();
    await settings.getByText("viscript", {exact: true}).first().click();
    await settings.locator(".viscript-preview").scrollIntoViewIfNeeded();
    await settings.waitForFunction((shown) => shown.includes(document.querySelector(".viscript-preview .viscript-scene")?.dataset.state),
        SHOWN, {timeout: 20_000});
    assert.deepEqual((await scenes(".viscript-preview .viscript-scene", settings)).filter((scene) => scene.error), []);
    const names = await settings.evaluate(() => [...document.querySelectorAll(".setting-item")]
        .filter((item) => item.offsetParent !== null).map((item) => item.querySelector(".setting-item-name")?.textContent));
    assert.ok(["Text font", "Code font", "Font folder", "Download fonts", "Show source"].every((name) => names.includes(name)), names.join(", "));
    assert.ok(!names.includes("Text font name"), "the font's name is asked for only with another font chosen");
    assert.equal(await settings.locator(".viscript-preview .viscript-source-tools, .viscript-preview .viscript-source-panel").count(), 0,
        "the font preview does not repeat its sample source");
    const sourceToggle = settings.locator(".setting-item").filter({has: settings.locator(".setting-item-name", {hasText: /^Show source$/})})
        .locator(".checkbox-container");
    const initialSource = await app.evaluate(() => window.app.plugins.plugins.viscript.settings.showSource);
    for (const enabled of [!initialSource, initialSource]) {
        await sourceToggle.click();
        await until(async () => await app.evaluate(() => window.app.plugins.plugins.viscript.settings.showSource) === enabled,
            `the source setting to be ${enabled}`);
        await until(async () => JSON.parse(await readFile(join(vault, ".obsidian/plugins/viscript/data.json"), "utf8")).showSource === enabled,
            "the source setting to be saved");
    }
    await shot("settings", settings);
    await app.evaluate(() => window.app.setting.close());
});

test("a font a scene names that is nowhere here is downloaded from Fontsource into the font folder", options, async (t) => {
    const api = "https://api.fontsource.org/v1/fonts/lobster";
    if (!await fetch(api, {signal: AbortSignal.timeout(8000)}).then((response) => response.ok, () => false)) {
        t.skip("Fontsource cannot be reached from here");
        return;
    }
    // The vault's news of the saved file must not send every scene back to load again.
    await app.evaluate(() => {
        window.e2eReloads = 0;
        for (const block of window.app.plugins.plugins.viscript.blocks) {
            const reload = block.reload.bind(block);
            block.reload = () => { window.e2eReloads++; reload(); };
        }
    });
    await app.evaluate(async () => {
        const note = await window.app.vault.create("Download.md", [
            "```viscript", "local scene = vis.scene {width = 480, height = 160, theme = \"adaptive\"}",
            "scene:text {text = \"Lobster, from Fontsource\", font = \"Lobster\", size = 28}", "return scene", "```", "",
        ].join("\n"));
        await window.app.workspace.getLeaf("tab").openFile(note, {state: {mode: "preview"}});
    });
    const scene = `.workspace-leaf.mod-active ${READING}`;
    await app.waitForFunction(([selector, shown]) => shown.includes(document.querySelector(selector)?.dataset.state), [scene, SHOWN],
        {timeout: 30_000});
    const saved = join(vault, "Fonts/Lobster.ttf");
    await until(() => existsSync(saved), "the font to be saved", 10_000);
    assert.equal((await readFile(saved)).readUInt32BE(0), 0x00010000, "a TrueType font");
    assert.equal(await app.evaluate(async () => (await window.app.plugins.plugins.viscript.fonts.fetch("Lobster")).source),
        "folder:Fonts/Lobster.ttf");
    await delay(1500);
    assert.equal(await app.evaluate(() => window.e2eReloads), 0, "saving the font reloaded no scene");
    assert.deepEqual((await scenes(scene)).filter((shown) => shown.error), []);
    await shot("fontsource");
    await app.evaluate(() => window.app.workspace.getMostRecentLeaf()?.detach());
});

test("with no font folder set, the first font downloaded makes Fonts the font folder", options, async (t) => {
    const api = "https://api.fontsource.org/v1/fonts/pacifico";
    if (!await fetch(api, {signal: AbortSignal.timeout(8000)}).then((response) => response.ok, () => false)) {
        t.skip("Fontsource cannot be reached from here");
        return;
    }
    // As a new vault starts. Setting the folder then has the render worker ask for its fonts again,
    // once, and the scene that waited for the font settles with it.
    await app.evaluate(async () => {
        const plugin = window.app.plugins.plugins.viscript;
        await plugin.update({fontFolder: ""});
        const host = await plugin.host();
        const refresh = host.refreshFonts.bind(host);
        window.e2eRefreshes = 0;
        host.refreshFonts = () => { window.e2eRefreshes++; refresh(); };
        const note = await window.app.vault.create("First download.md", [
            "```viscript", "local scene = vis.scene {width = 480, height = 160, theme = \"adaptive\"}",
            "scene:text {text = \"Pacifico, the first download\", font = \"Pacifico\", size = 28}", "return scene", "```", "",
        ].join("\n"));
        await window.app.workspace.getLeaf("tab").openFile(note, {state: {mode: "preview"}});
    });
    const scene = `.workspace-leaf.mod-active ${READING}`;
    await app.waitForFunction(([selector, shown]) => shown.includes(document.querySelector(selector)?.dataset.state), [scene, SHOWN],
        {timeout: 30_000});
    const saved = join(vault, "Fonts/Pacifico.ttf");
    await until(() => existsSync(saved), "the font to be saved", 10_000);
    await until(() => app.evaluate(() => window.app.plugins.plugins.viscript.settings.fontFolder === "Fonts"), "the font folder", 10_000);
    assert.equal(await app.evaluate(async () => (await window.app.plugins.plugins.viscript.fonts.fetch("Pacifico")).source),
        "folder:Fonts/Pacifico.ttf");
    await delay(1500);
    assert.equal(await app.evaluate(() => window.e2eRefreshes), 1, "the fonts were asked for again once");
    await app.waitForFunction(([selector, shown]) => shown.includes(document.querySelector(selector)?.dataset.state), [scene, SHOWN],
        {timeout: 30_000});
    assert.deepEqual((await scenes(scene)).filter((shown) => shown.error), []);
    await shot("first-download");
    await app.evaluate(() => window.app.workspace.getMostRecentLeaf()?.detach());
});

test("a JavaScript scene reaches neither Node, Obsidian's storage nor the network", options, async () => {
    assert.equal(await hostMode(), "worker");
    // The page itself reaches the listener, so the scene's silence is the sandbox's doing.
    hits.splice(0);
    await app.evaluate((sink) => fetch(`http://${sink}/page`).then(() => undefined, () => undefined), sink);
    assert.deepEqual(await sent(), ["connection", "GET /page HTTP/1.1"]);
    assert.equal(await probe(SCOPE()), 64);
    assert.deepEqual(await sent(), []);
});

test("a JavaScript scene that never returns is stopped, and the other scenes play on", options, async () => {
    // A worker stuck in scene code would be replaced, and every scene would get a new canvas.
    await app.evaluate(() => {
        for (const block of window.app.plugins.plugins.viscript.blocks) if (block.player) block.player.canvas.dataset.e2e = "kept";
    });
    assert.equal(await probe("for (;;) {}"), "This scene's JavaScript ran for more than 10 seconds, so it was stopped");
    const kept = await app.evaluate(() => [...window.app.plugins.plugins.viscript.blocks]
        .filter((block) => block.player).map((block) => block.player.canvas.dataset.e2e === "kept"));
    assert.ok(kept.length && kept.every(Boolean), `canvases kept: ${kept}`);
});

test("with the worker turned off, scenes play on the main thread, JavaScript ones too", options, async () => {
    await app.evaluate(() => window.app.plugins.plugins.viscript.update({worker: false}));
    assert.equal(await hostMode(), "inline");
    hits.splice(0);
    assert.equal(await probe(SCOPE()), 64);
    assert.deepEqual(await sent(), []);
    await app.waitForFunction((view) => document.querySelector(`${view} .viscript-scene`)?.dataset.state === "playing"
        || [...window.app.plugins.plugins.viscript.blocks].some((block) => block.player?.state === "playing"), VIEW, {timeout: 20_000});
    await app.evaluate(() => window.app.plugins.plugins.viscript.update({worker: true}));
    assert.equal(await hostMode(), "worker");
});

test("turning the plugin off removes every scene, and turning it on draws them again", options, async () => {
    await app.evaluate(() => window.app.plugins.disablePlugin("viscript"));
    for (const page of pages()) assert.equal(await page.locator(".viscript-scene").count(), 0, page.url());
    await app.evaluate(() => window.app.plugins.enablePlugin("viscript"));
    await app.evaluate(async () => {
        const leaf = window.app.workspace.getLeavesOfType("markdown").find((leaf) => leaf.getViewState().state?.file === "Scenes.md");
        await window.app.workspace.revealLeaf(leaf);
        window.app.workspace.setActiveLeaf(leaf, {focus: true});
    });
    const active = ".workspace-leaf.mod-active .viscript-scene";
    await app.waitForFunction(([selector, shown]) => [...document.querySelectorAll(selector)].some((scene) => shown.includes(scene.dataset.state)),
        [active, SHOWN], {timeout: 20_000}).catch(async (error) => {
        const leaf = await app.evaluate(() => window.app.workspace.getMostRecentLeaf()?.getViewState());
        assert.fail(`${error.message}\nactive leaf: ${JSON.stringify(leaf)}\nscenes: ${JSON.stringify(await scenes(".viscript-scene"))}`);
    });
    await shot("enabled-again");
});

test("a folder's menu makes a scene file beside New canvas, which opens with its name to type", options, async () => {
    await app.evaluate(async () => {
        // Obsidian's own menus, which the page holds, rather than the system's on macOS.
        window.app.vault.setConfig("nativeMenus", false);
        await window.app.vault.createFolder("Drafts");
        await window.app.workspace.revealLeaf(window.app.workspace.getLeavesOfType("file-explorer")[0]);
    });
    // Opens the folder's menu, and gives its items in order with "|" between sections.
    const menu = async () => {
        await app.locator(".nav-folder-title[data-path=\"Drafts\"]").click({button: "right"});
        await app.locator(".menu").waitFor();
        return app.evaluate(() => [...document.querySelectorAll(".menu .menu-item-title, .menu .menu-separator")]
            .map((item) => item.classList.contains("menu-separator") ? "|" : item.textContent));
    };
    const newScene = async (name) => {
        const items = await menu();
        const section = items.join("\n").split("|").find((part) => part.includes("New canvas")) ?? "";
        assert.deepEqual(items.filter((item) => item === "New viscript"), ["New viscript"], items.join(", "));
        assert.ok(section.split("\n").includes("New viscript"), items.join(", "));
        await shot("new-scene-menu");
        await app.locator(".menu .menu-item", {hasText: "New viscript"}).click();
        await until(() => existsSync(join(vault, "Drafts", name)), `Drafts/${name}`, 10_000);
        await until(() => app.evaluate((path) => window.app.workspace.getActiveFile()?.path === path, `Drafts/${name}`),
            `Drafts/${name} to open`, 10_000);
        await app.waitForFunction(([view, shown]) => shown.includes(document.querySelector(`${view} .viscript-scene`)?.dataset.state)
            && !document.querySelector(`${view} .viscript-error`).isShown(), [VIEW, SHOWN], {timeout: 20_000});
    };

    await newScene("Untitled.lua");
    assert.match(await readFile(join(vault, "Drafts/Untitled.lua"), "utf8"), /^local scene = vis\.scene \{[^]*\nreturn scene\n$/);
    // The tab's title holds the name, selected, so typing names the file.
    const title = await app.evaluate(() => ({
        focused: document.activeElement?.matches(".workspace-leaf.mod-active .view-header-title") ?? false,
        selected: window.getSelection()?.toString(),
    }));
    assert.deepEqual(title, {focused: true, selected: "Untitled"});
    await shot("new-scene");
    await app.keyboard.type("Bounce");
    await app.keyboard.press("Enter");
    await until(() => existsSync(join(vault, "Drafts/Bounce.lua")) && !existsSync(join(vault, "Drafts/Untitled.lua")),
        "the file to take its new name", 10_000);

    // A name the folder holds is passed over.
    await newScene("Untitled.lua");
    await newScene("Untitled 1.lua");
});

test("the plugin logged no errors", options, (t) => {
    for (const message of errors) t.diagnostic(message);
    assert.deepEqual(errors.filter((message) => /viscript|vis-wasm|render server/i.test(message)), []);
});
