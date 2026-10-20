#!/usr/bin/env node
// Install the self-contained plugin without npm, a source checkout, or a build.
import {copyFile, mkdir, readFile, stat, writeFile} from "node:fs/promises";
import {dirname, join, resolve} from "node:path";
import {fileURLToPath} from "node:url";

const usage = "Usage: node scripts/install.mjs <vault-folder> [--show-source]";
const args = process.argv.slice(2);

async function readJSON(path, fallback) {
    try {
        return JSON.parse(await readFile(path, "utf8"));
    } catch (error) {
        if (error.code === "ENOENT") return fallback;
        throw new Error(`Cannot read ${path}: ${error.message}`);
    }
}

async function install() {
    if (args.length === 1 && args[0] === "--help") {
        console.log(usage);
        return;
    }
    const positional = args.filter((arg) => !arg.startsWith("--"));
    if (positional.length !== 1 || !positional[0].trim()
        || args.some((arg) => arg.startsWith("--") && arg !== "--show-source")) {
        throw new Error(usage);
    }
    const vault = resolve(positional[0]);
    if (!(await stat(vault)).isDirectory()) throw new Error(`Vault must be an existing directory: ${vault}`);

    const config = join(vault, ".obsidian");
    const destination = join(config, "plugins/viscript");
    const pluginsPath = join(config, "community-plugins.json");
    const plugins = await readJSON(pluginsPath, []);
    if (!Array.isArray(plugins) || plugins.some((id) => typeof id !== "string")) {
        throw new Error(`Expected a list of plugin IDs in ${pluginsPath}`);
    }
    const showSource = args.includes("--show-source");
    const dataPath = join(destination, "data.json");
    const data = showSource ? await readJSON(dataPath, {}) : null;
    if (showSource && (!data || typeof data !== "object" || Array.isArray(data))) {
        throw new Error(`Expected plugin settings in ${dataPath}`);
    }

    const source = resolve(dirname(fileURLToPath(import.meta.url)), "../assets/plugin");
    const files = ["main.js", "manifest.json", "styles.css"];
    // Check the portable bundle before touching a vault.
    await Promise.all(files.map((name) => stat(join(source, name))));
    await mkdir(destination, {recursive: true});
    await Promise.all(files.map((name) => copyFile(join(source, name), join(destination, name))));
    if (!plugins.includes("viscript")) {
        await writeFile(pluginsPath, `${JSON.stringify([...plugins, "viscript"], null, 2)}\n`);
    }
    if (showSource && data.showSource !== true) {
        await writeFile(dataPath, `${JSON.stringify({...data, showSource: true}, null, 2)}\n`);
    }
    console.log(`Installed viscript in ${destination}`);
    console.log("Reload Obsidian or toggle viscript off and on. If Restricted Mode is on, enable community plugins in Obsidian settings.");
}

install().catch((error) => {
    console.error(error.message);
    process.exitCode = 1;
});
