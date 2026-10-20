import assert from "node:assert/strict";
import {spawnSync} from "node:child_process";
import {createHash} from "node:crypto";
import {cp, mkdtemp, mkdir, readFile, rm, writeFile} from "node:fs/promises";
import {tmpdir} from "node:os";
import {dirname, join, resolve} from "node:path";
import {fileURLToPath} from "node:url";
import {test} from "node:test";

const skill = resolve(dirname(fileURLToPath(import.meta.url)), "..");
const hash = (bytes) => createHash("sha256").update(bytes).digest("hex");

async function fixture(t) {
    const dir = await mkdtemp(join(tmpdir(), "vis-obsidian-install-"));
    t.after(() => rm(dir, {recursive: true, force: true}));
    const portable = join(dir, "portable skill");
    // A copied skill outside the checkout must be sufficient to install the plugin.
    await cp(skill, portable, {recursive: true, filter: (path) => path !== join(skill, "test")});
    const vault = join(dir, "My Vault");
    await mkdir(vault);
    return {
        vault,
        run: (...args) => spawnSync(process.execPath, [join(portable, "scripts/install.mjs"), ...args], {encoding: "utf8"}),
    };
}

test("portable install and reinstall preserve notes, settings, and other plugins", async (t) => {
    const {vault, run} = await fixture(t);
    const config = join(vault, ".obsidian");
    const plugin = join(config, "plugins/viscript");
    await mkdir(plugin, {recursive: true});
    const preserved = {
        [join(vault, "Keep.md")]: "# My note\n",
        [join(config, "app.json")]: '{"safeMode":true}\n',
        [join(config, "workspace.json")]: '{"open":"Keep.md"}\n',
        [join(plugin, "data.json")]: '{ "fontFolder": "Fonts", "showSource": false }\n',
    };
    await Promise.all(Object.entries(preserved).map(([path, text]) => writeFile(path, text)));
    await writeFile(join(config, "community-plugins.json"), '["other-plugin"]\n');
    for (let pass = 0; pass < 2; pass++) {
        const result = run(vault);
        assert.equal(result.status, 0, result.stderr);
        for (const [path, text] of Object.entries(preserved)) assert.equal(await readFile(path, "utf8"), text);
        assert.deepEqual(JSON.parse(await readFile(join(config, "community-plugins.json"), "utf8")), ["other-plugin", "viscript"]);
        for (const name of ["main.js", "manifest.json", "styles.css"]) {
            assert.equal(hash(await readFile(join(plugin, name))), hash(await readFile(join(skill, "assets/plugin", name))));
        }
    }
});

test("source display is opt-in and preserves other preferences", async (t) => {
    const {vault, run} = await fixture(t);
    assert.equal(run(vault).status, 0);
    const dataPath = join(vault, ".obsidian/plugins/viscript/data.json");
    await assert.rejects(readFile(dataPath), {code: "ENOENT"});
    await writeFile(dataPath, '{"fontFolder":"Fonts","showSource":false}\n');
    const result = run(vault, "--show-source");
    assert.equal(result.status, 0, result.stderr);
    assert.deepEqual(JSON.parse(await readFile(dataPath, "utf8")), {fontFolder: "Fonts", showSource: true});
});

test("invalid targets or settings fail before changing the vault", async (t) => {
    const {vault, run} = await fixture(t);
    assert.notEqual(run().status, 0);
    assert.notEqual(run("").status, 0);
    assert.notEqual(run(join(vault, "missing")).status, 0);
    assert.notEqual(run(vault, "--unknown").status, 0);
    const config = join(vault, ".obsidian");
    await mkdir(config);
    const pluginsPath = join(config, "community-plugins.json");
    await writeFile(pluginsPath, "not json\n");
    assert.notEqual(run(vault).status, 0);
    assert.equal(await readFile(pluginsPath, "utf8"), "not json\n");
    await assert.rejects(readFile(join(config, "plugins/viscript/main.js")), {code: "ENOENT"});
});
