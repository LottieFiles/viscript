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
const SHOWN = ["playing", "paused", "stopped"];

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

test("fonts by name: the theme's from the plugin, one the scene names from the folder, the rest in the text font", options, async () => {
    const found = await app.evaluate(async () => {
        const fonts = window.app.plugins.plugins.viscript.fonts;
        const found = {};
        for (const name of ["Pretendard", "JetBrainsMono", "Source Serif 4", "IBM Plex Sans KR", "Pretendard Std", "No Such Sans", "No Such Mono"]) {
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
        "Source Serif 4": await bundled("serif", "SourceSerif4-Semibold.ttf"),
        "IBM Plex Sans KR": await bundled("serifKorean", "IBMPlexSansKR-SemiBold.ttf"),
        "Pretendard Std": "folder:Fonts/PretendardStd-Regular.ttf",
        "No Such Sans": await bundled("text", "Pretendard.ttf"),
        "No Such Mono": await bundled("code", "JetBrainsMono.ttf"),
    });
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

test("a scene file opens in its own tab, where typing saves the file and redraws", options, async () => {
    await app.evaluate(async () => {
        await window.app.workspace.getLeaf("tab").openFile(window.app.vault.getFileByPath("orbit.lua"));
    });
    const drawn = () => app.waitForFunction(([view, shown]) => shown.includes(document.querySelector(`${view} .viscript-scene`)?.dataset.state)
        && !document.querySelector(`${view} .viscript-error`).isShown(), [VIEW, SHOWN], {timeout: 20_000});
    await drawn();
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

test("scenes with the adaptive theme take Obsidian's colors, light and dark", options, async () => {
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
    assert.ok(["Text font", "Code font", "Font folder", "Download fonts"].every((name) => names.includes(name)), names.join(", "));
    assert.ok(!names.includes("Text font name"), "the font's name is asked for only with another font chosen");
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
            "```viscript", "local scene = vis.scene {width = 480, height = 160, theme = \"adaptive_vscode\"}",
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
            "```viscript", "local scene = vis.scene {width = 480, height = 160, theme = \"adaptive_vscode\"}",
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

test("the plugin logged no errors", options, (t) => {
    for (const message of errors) t.diagnostic(message);
    assert.deepEqual(errors.filter((message) => /viscript|vis-wasm|render server/i.test(message)), []);
});
