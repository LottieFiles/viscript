import assert from "node:assert/strict";
import test, {afterEach, mock} from "node:test";
import type {VisModule, VisScene} from "vis-wasm-bundle";
import type {AssetRef} from "../src/protocol.ts";
import {InstancePool, type FontAnswer} from "../src/server/pool.ts";

interface FakeEngine {
    destroyed: boolean;
    fonts: [string, ArrayBuffer, string][];
    assets: [string, ArrayBuffer][];
    sounds: string[];
    audio: {load(name: string): void};
    font(name: string, data: ArrayBuffer, type: string): void;
    asset(name: string, data: ArrayBuffer): void;
    destroy(): void;
}

interface FakeModule {
    onAbort: () => void;
    // engines[0] is the instance's anchor, which holds its fonts.
    engines: FakeEngine[];
}

interface Faults {
    failModule?: boolean;
    failEngine?: boolean;
    trapDestroy?: boolean;
    /** How the engine takes a font: "throw" refuses it, "trap" traps. */
    font?: "throw" | "trap";
}

// Disposed after each test, so idle timers do not keep the run alive.
const pools: InstancePool[] = [];
afterEach(() => {
    for (const pool of pools.splice(0)) pool.dispose();
});

// Fonts: a name in `fonts` has one, "Missing" none, and "Offline" fails to arrive once.
function fixture({failModule = false, failEngine = false, trapDestroy = false, font}: Faults = {}) {
    const state = {calls: [] as string[], modules: [] as FakeModule[], failModule, offline: true};
    const pool = new InstancePool({
        async createModule(onAbort) {
            state.calls.push("module");
            await Promise.resolve();
            if (state.failModule) {
                state.failModule = false;
                throw new Error("module failed");
            }
            const module: FakeModule = {onAbort, engines: []};
            state.modules.push(module);
            return module as unknown as VisModule;
        },
        async createEngine(visModule) {
            const module = visModule as unknown as FakeModule;
            state.calls.push("engine");
            await Promise.resolve();
            if (failEngine && module.engines.length) throw new Error("engine failed");
            const engine: FakeEngine = {
                destroyed: false, fonts: [], assets: [], sounds: [],
                audio: {load: (name) => engine.sounds.push(name)},
                font(name, data, type) {
                    if (font === "throw") throw new Error("could not load the font");
                    if (font === "trap") throw new WebAssembly.RuntimeError("unreachable");
                    this.fonts.push([name, data, type]);
                },
                asset(name, data) {
                    this.assets.push([name, data]);
                },
                destroy() {
                    if (trapDestroy && module.engines[0] !== this) throw new WebAssembly.RuntimeError("unreachable");
                    this.destroyed = true;
                },
            };
            module.engines.push(engine);
            return engine as unknown as VisScene;
        },
        async font(name): Promise<FontAnswer | null> {
            state.calls.push(`font:${name}`);
            await Promise.resolve();
            if (name === "Missing") return null;
            if (name === "Offline" && state.offline) {
                state.offline = false;
                throw new Error("Could not load the font (503)");
            }
            return {data: new ArrayBuffer(16), type: name.endsWith("otf") ? "otf" : "ttf"};
        },
        async assetBytes(asset) {
            state.calls.push(`asset:${asset.id}`);
            return new ArrayBuffer(16);
        },
    });
    pools.push(pool);
    return {pool, state};
}

const EARTH: AssetRef = {id: "earth.png", name: "earth.png", mime: "png"};
const THEME = ["Pretendard", "JetBrainsMono"];

const count = (calls: string[], call: string) => calls.filter((c) => c === call).length;
const engineOf = (lease: {engine: VisScene}) => lease.engine as unknown as FakeEngine;
const anchorOf = (state: {modules: FakeModule[]}, index = 0) => state.modules[index]!.engines[0]!;

test("leases share one instance, whose anchor registers each font once, even concurrently", async () => {
    const {pool, state} = fixture();
    const leases = await Promise.all([pool.acquire(), pool.acquire(), pool.acquire()]);
    assert.equal(state.calls[0], "module");
    assert.deepEqual(await Promise.all(leases.map((lease) => pool.registerFonts(lease, THEME))), [true, true, true]);
    assert.equal(state.modules.length, 1);
    const [anchor, ...engines] = state.modules[0]!.engines;
    assert.deepEqual(engines, leases.map(engineOf));
    assert.deepEqual(anchor!.fonts.map(([name]) => name), THEME);
    assert.equal(count(state.calls, "font:Pretendard"), 1);
    for (const engine of engines) assert.equal(engine.fonts.length, 0);
    // Registered once, a font is not asked for again.
    assert.equal(await pool.registerFonts(leases[0]!, THEME), true);
    assert.equal(count(state.calls, "font:Pretendard"), 1);
});

test("a font goes in as the type it was given", async () => {
    const {pool, state} = fixture();
    const lease = await pool.acquire();
    await pool.registerFonts(lease, ["Serif.otf", "Sans"]);
    assert.deepEqual(anchorOf(state).fonts.map(([name, , type]) => [name, type]), [["Serif.otf", "otf"], ["Sans", "ttf"]]);
});

test("a name without a font is asked for once and registers nothing", async () => {
    const {pool, state} = fixture();
    const lease = await pool.acquire();
    assert.equal(await pool.registerFonts(lease, ["Missing"]), false);
    assert.equal(await pool.registerFonts(lease, ["Missing"]), false);
    assert.equal(count(state.calls, "font:Missing"), 1);
    assert.equal(anchorOf(state).fonts.length, 0);
    // Any font going in is enough for the scene to load again.
    assert.equal(await pool.registerFonts(lease, ["Missing", "Pretendard"]), true);
});

test("a font that did not arrive is asked for again; one the engine refuses is taken as none", async () => {
    const {pool, state} = fixture();
    const lease = await pool.acquire();
    assert.equal(await pool.registerFonts(lease, ["Offline"]), false);
    assert.equal(await pool.registerFonts(lease, ["Offline"]), true);
    assert.equal(count(state.calls, "font:Offline"), 2);

    const refusing = fixture({font: "throw"});
    const other = await refusing.pool.acquire();
    assert.equal(await refusing.pool.registerFonts(other, ["Broken"]), false);
    assert.equal(await refusing.pool.registerFonts(other, ["Broken"]), false);
    assert.equal(count(refusing.state.calls, "font:Broken"), 1);
    assert.equal(refusing.pool.usable(other), true);
});

test("a trap while registering a font faults the instance", async () => {
    const {pool, state} = fixture({font: "trap"});
    const lease = await pool.acquire();
    await assert.rejects(pool.registerFonts(lease, ["Pretendard"]), /stopped after an error/);
    assert.equal(pool.usable(lease), false);
    await pool.acquire();
    assert.equal(state.modules.length, 2);
});

test("fonts outlive the leases that brought them; images stay with each engine", async () => {
    const {pool, state} = fixture();
    const first = await pool.acquire();
    await pool.registerFonts(first, THEME);
    await pool.ensureAssets(first, [EARTH]);
    pool.release(first);
    const second = await pool.acquire();
    assert.equal(await pool.registerFonts(second, THEME), true);
    await pool.ensureAssets(second, [EARTH]);
    assert.equal(state.modules.length, 1);
    assert.equal(anchorOf(state).destroyed, false);
    assert.deepEqual(anchorOf(state).fonts.map(([name]) => name), THEME);
    assert.equal(engineOf(first).destroyed, true);
    assert.equal(engineOf(first).assets.length, 1);
    assert.equal(engineOf(second).assets.length, 1);
});

test("ensureAssets registers only what is missing, once, even concurrently", async () => {
    const {pool} = fixture();
    const lease = await pool.acquire();
    await Promise.all([pool.ensureAssets(lease, [EARTH]), pool.ensureAssets(lease, [EARTH])]);
    assert.equal(engineOf(lease).assets.length, 1);
    await pool.ensureAssets(lease, [EARTH]);
    assert.equal(engineOf(lease).assets.length, 1);
});

test("an image under a known name but with other bytes replaces the one registered", async () => {
    const {pool} = fixture();
    const lease = await pool.acquire();
    await pool.ensureAssets(lease, [EARTH]);
    await pool.ensureAssets(lease, [{...EARTH, id: "other/earth.png"}]);
    assert.deepEqual(engineOf(lease).assets.map(([name]) => name), ["earth.png", "earth.png"]);
});

test("sounds go to the engine's audio when the scene plays them, and are left out when it does not", async () => {
    const {pool, state} = fixture();
    const lease = await pool.acquire();
    const chime: AssetRef = {id: "chime.wav", name: "chime.wav", mime: "audio/wav"};
    await pool.ensureAssets(lease, [EARTH, chime]);
    assert.deepEqual(engineOf(lease).assets.map(([name]) => name), ["earth.png"]);
    assert.deepEqual(engineOf(lease).sounds, []);
    assert.equal(count(state.calls, "asset:chime.wav"), 0);
    await pool.ensureAssets(lease, [EARTH, chime], true);
    assert.deepEqual(engineOf(lease).sounds, ["chime.wav"]);
    assert.equal(engineOf(lease).assets.length, 1);
});

test("retired instances take no new sessions; the idle go at once, the others when their sessions leave", async () => {
    const {pool, state} = fixture();
    const kept = await pool.acquire();
    const idle = await pool.acquire();
    pool.release(idle);
    await pool.registerFonts(kept, THEME);
    pool.retire();
    assert.equal(pool.fits(kept), false);
    assert.equal(pool.usable(kept), true, "a retired instance still draws its scenes");
    assert.equal(anchorOf(state).destroyed, false);
    const fresh = await pool.acquire();
    assert.equal(state.modules.length, 2);
    assert.equal(pool.fits(fresh), true);
    // The new instance asks for its fonts again.
    await pool.registerFonts(fresh, THEME);
    assert.equal(count(state.calls, "font:Pretendard"), 2);
    pool.release(kept);
    assert.equal(anchorOf(state).destroyed, true);

    const spare = fixture();
    spare.pool.release(await spare.pool.acquire());
    spare.pool.retire();
    assert.equal(anchorOf(spare.state).destroyed, true, "an instance without sessions goes at once");
});

test("an instance without leases goes after the idle time, its anchor with it", async () => {
    mock.timers.enable({apis: ["setTimeout"]});
    try {
        const {pool, state} = fixture();
        const lease = await pool.acquire();
        pool.release(lease);
        pool.release(lease);
        mock.timers.tick(29000);
        pool.release(await pool.acquire());
        mock.timers.tick(29000);
        assert.equal(anchorOf(state).destroyed, false);
        mock.timers.tick(1000);
        assert.equal(anchorOf(state).destroyed, true);
        await pool.acquire();
        assert.equal(state.modules.length, 2);
    } finally {
        mock.timers.reset();
    }
});

test("a trap or abort faults the instance: its leases stop fitting and new ones get a fresh instance", async () => {
    const {pool, state} = fixture();
    const lease = await pool.acquire();
    pool.report(lease, new Error("Lua error"));
    assert.equal(pool.fits(lease), true);
    assert.equal(pool.usable(lease), true);
    pool.report(lease, new WebAssembly.RuntimeError("unreachable"));
    assert.equal(pool.fits(lease), false);
    assert.equal(pool.usable(lease), false);
    const next = await pool.acquire();
    assert.equal(state.modules.length, 2);
    state.modules[1]!.onAbort();
    assert.equal(pool.fits(next), false);
    await pool.acquire();
    assert.equal(state.modules.length, 3);
});

test("a lease whose destroy traps faults its instance and is still released", async () => {
    mock.timers.enable({apis: ["setTimeout"]});
    try {
        const {pool, state} = fixture({trapDestroy: true});
        const lease = await pool.acquire();
        pool.release(lease);
        assert.equal(pool.usable(lease), false);
        await pool.acquire();
        assert.equal(state.modules.length, 2);
        mock.timers.tick(30000);
        assert.equal(anchorOf(state).destroyed, false, "a faulted instance is not called into");
    } finally {
        mock.timers.reset();
    }
});

test("an instance that fails to start leaves the pool, so the next acquire starts another", async () => {
    const {pool, state} = fixture({failModule: true});
    await assert.rejects(pool.acquire(), /module failed/);
    await pool.acquire();
    assert.equal(state.modules.length, 1);
});

test("an engine that fails to start rejects the acquire", async () => {
    const {pool} = fixture({failEngine: true});
    await assert.rejects(pool.acquire(), /engine failed/);
});
