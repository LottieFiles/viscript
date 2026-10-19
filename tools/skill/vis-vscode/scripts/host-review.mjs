// Renders a Lua Scene the way the VS Code Viewer does: the same WASM runtime, the same fonts
// (Source Serif 4 for the adaptive_vscode heading and body roles), and the extension's fallback
// palettes, which approximate the default Light and Dark themes; a workbench theme's own colors
// can differ. Writes one PNG per time and prints one line per time with the text that overlaps,
// is clipped, or leaves its box.
// Usage: node host-review.mjs <scene.lua> [--time SEC|end]... [--theme light|dark|both]
//        [--contain TEXT PANEL [INSET]]... [--padding PX] [--out DIR] [--runtime DIR]
import {mkdirSync, readFileSync, writeFileSync} from "node:fs";
import {tmpdir} from "node:os";
import {basename, extname, join, resolve} from "node:path";
import {fileURLToPath, pathToFileURL} from "node:url";
import {deflateSync} from "node:zlib";

// The extension's fallback palettes (tools/vscode/media/adaptive-theme.mjs); the skill check
// keeps them equal.
export const PALETTES = {
    light: {
        background: "#ffffff",
        foreground: "#202124",
        muted: "#555b64",
        accent: "#005fb8",
        secondary: "#6750a4",
        success: "#2e7d32",
        warning: "#8a5d00",
        danger: "#b3261e",
        info: "#00639b",
        surface: "#f5f6f8",
        line: "#d8dadd",
        result: "#8a6500",
        focus: "#315f8c",
        objects: ["#b45f06", "#b63a3c", "#2e7f7a", "#815681"],
    },
    dark: {
        background: "#1e1e1e",
        foreground: "#cccccc",
        muted: "#9d9d9d",
        accent: "#4daafc",
        secondary: "#c8a7ff",
        success: "#7ee787",
        warning: "#e3b341",
        danger: "#ff7b72",
        info: "#79c0ff",
        surface: "#252526",
        line: "#454545",
        result: "#edc948",
        focus: "#6ea8fe",
        objects: ["#f28e2b", "#ff6b6b", "#7bc96f", "#c29bc0"],
    },
};

// Font families and files, registered as the Viewer registers them.
const FONTS = [
    ["Pretendard", "Pretendard.ttf"],
    ["JetBrainsMono", "JetBrainsMono.ttf"],
    ["IBM Plex Sans KR", "IBMPlexSansKR-SemiBold.ttf"],
    ["Source Serif 4", "SourceSerif4-Semibold.ttf"],
];

const USAGE = `Usage: node host-review.mjs <scene.lua> [--time SEC|end]... [--theme light|dark|both]
       [--contain TEXT PANEL [INSET]]... [--padding PX] [--out DIR] [--runtime DIR]
--time defaults to 0 and end. --contain checks that each Text whose id matches TEXT stays
INSET pixels inside PANEL; a * in TEXT matches any run of characters and fills the * in PANEL.
--padding is the smallest gap between two Texts (default 4). PNGs go to --out, by default a
directory under the system temp directory, so review renders stay out of .vscode/vis.`;

function parse(argv) {
    const options = {times: [], themes: ["light", "dark"], contain: [], padding: 4};
    const number = (value, flag) => {
        const result = Number(value);
        if (value === undefined || !Number.isFinite(result) || result < 0) throw new Error(`${flag} needs a number`);
        return result;
    };
    for (let i = 0; i < argv.length; i++) {
        const arg = argv[i];
        if (arg === "--help" || arg === "-h") {
            options.help = true;
        } else if (arg === "--time") {
            const value = argv[++i];
            options.times.push(value === "end" ? "end" : number(value, "--time"));
        } else if (arg === "--theme") {
            const value = argv[++i];
            if (!["light", "dark", "both"].includes(value)) throw new Error("--theme is light, dark, or both");
            options.themes = value === "both" ? ["light", "dark"] : [value];
        } else if (arg === "--contain") {
            const [text, panel] = [argv[++i], argv[++i]];
            if (!text || !panel) throw new Error("--contain needs TEXT and PANEL");
            const inset = argv[i + 1] !== undefined && /^\d+(\.\d+)?$/.test(argv[i + 1]) ? Number(argv[++i]) : 0;
            options.contain.push({text, panel, inset});
        } else if (arg === "--padding") {
            options.padding = number(argv[++i], "--padding");
        } else if (arg === "--out") {
            options.out = argv[++i];
        } else if (arg === "--runtime") {
            options.runtime = argv[++i];
        } else if (!arg.startsWith("--") && !options.scene) {
            options.scene = arg;
        } else {
            throw new Error(`unknown argument ${arg}`);
        }
    }
    if (!options.times.length) options.times = [0, "end"];
    return options;
}

const CRC = new Uint32Array(256).map((_, n) => {
    let c = n;
    for (let k = 0; k < 8; k++) c = c & 1 ? 0xedb88320 ^ (c >>> 1) : c >>> 1;
    return c >>> 0;
});

function chunk(type, data) {
    const out = Buffer.alloc(12 + data.length);
    out.writeUInt32BE(data.length, 0);
    out.write(type, 4, "ascii");
    data.copy(out, 8);
    let crc = 0xffffffff;
    for (const byte of out.subarray(4, 8 + data.length)) crc = CRC[(crc ^ byte) & 0xff] ^ (crc >>> 8);
    out.writeUInt32BE((crc ^ 0xffffffff) >>> 0, 8 + data.length);
    return out;
}

// An 8-bit RGBA PNG of the rendered pixels.
function png(width, height, rgba) {
    const header = Buffer.alloc(13);
    header.writeUInt32BE(width, 0);
    header.writeUInt32BE(height, 4);
    header[8] = 8;
    header[9] = 6;
    const stride = width * 4;
    const rows = Buffer.alloc((stride + 1) * height);
    const pixels = Buffer.from(rgba.buffer, rgba.byteOffset, rgba.byteLength);
    for (let y = 0; y < height; y++) pixels.copy(rows, y * (stride + 1) + 1, y * stride, (y + 1) * stride);
    return Buffer.concat([
        Buffer.from([137, 80, 78, 71, 13, 10, 26, 10]),
        chunk("IHDR", header),
        chunk("IDAT", deflateSync(rows)),
        chunk("IEND", Buffer.alloc(0)),
    ]);
}

const seconds = (time) => String(Number(time.toFixed(3)));

function review(report, contain) {
    const issues = [];
    const objects = report.objects;
    const name = (object) => (object.scenePath === "root" ? "" : `${object.scenePath} `)
        + (object.id ?? `${object.type}#${object.index}`);
    const text = (object) => object?.type === "text" && object.visible && object.visibleBounds;
    const ancestors = (object) => {
        const result = [];
        for (let parent = object.parent; parent !== null; parent = objects[parent].parent) result.push(parent);
        return result;
    };
    // Texts of one typeset block, such as the tokens of a code line, sit close by design.
    const typeset = (first, second) => {
        const blocks = new Set(ancestors(second).filter((index) => objects[index].typeset));
        return ancestors(first).some((index) => blocks.has(index));
    };
    for (const collision of report.collisions) {
        const [first, second] = [objects[collision.first], objects[collision.second]];
        if (!text(first) || !text(second) || typeset(first, second)) continue;
        const kind = collision.kind === "overlap" ? "text overlap" : "text gap";
        issues.push(`${kind} ${name(first)} x ${name(second)}`);
    }
    for (const object of objects) {
        if (text(object) && (object.clipped || object.outside)) issues.push(`clipped ${name(object)}`);
    }
    for (const rule of contain) {
        const escape = (part) => part.replace(/[.+?^${}()|[\]\\]/g, "\\$&");
        const pattern = new RegExp(`^${rule.text.split("*").map(escape).join("(.+?)")}$`);
        let matched = false;
        for (const object of objects) {
            const match = object.id && pattern.exec(object.id);
            if (!match) continue;
            matched = true;
            if (!text(object)) continue;
            let group = 0;
            const panelId = rule.panel.replace(/\*/g, () => match[++group] ?? "");
            const panel = objects.find((other) => other.scene === object.scene && other.id === panelId);
            if (!panel?.visible || !panel.visibleBounds) {
                issues.push(`${name(object)} has no visible ${panelId}`);
                continue;
            }
            const [inner, outer] = [object.visibleBounds, panel.visibleBounds];
            const overflow = Math.max(
                outer.x + rule.inset - inner.x,
                outer.y + rule.inset - inner.y,
                inner.x + inner.width - (outer.x + outer.width - rule.inset),
                inner.y + inner.height - (outer.y + outer.height - rule.inset),
            );
            if (overflow > 0.5) issues.push(`${name(object)} leaves ${panelId} by ${overflow.toFixed(1)} px`);
        }
        if (!matched) issues.push(`no object matches ${rule.text}`);
    }
    return issues;
}

export async function main(argv) {
    const options = parse(argv);
    if (options.help || !options.scene) {
        console.log(USAGE);
        return options.help ? 0 : 2;
    }
    const runtimeDir = resolve(options.runtime
        ?? fileURLToPath(new URL("../../vis-skills/assets/wasm/", import.meta.url)));
    const {createVis} = await import(pathToFileURL(join(runtimeDir, "client.js")).href);
    const wasmBinary = readFileSync(join(runtimeDir, "vis-wasm.wasm"));
    const fonts = FONTS.map(([family, file]) => [family, readFileSync(join(runtimeDir, file))]);
    const source = readFileSync(options.scene, "utf8");
    const stem = basename(options.scene, extname(options.scene));
    const out = resolve(options.out ?? join(tmpdir(), "viscript-host-review", stem));
    mkdirSync(out, {recursive: true});

    let problems = 0;
    for (const mode of options.themes) {
        const runtime = await createVis("return vis.scene {width = 16, height = 16}", "bootstrap.lua", {wasmBinary});
        try {
            runtime.hostTheme({dark: mode === "dark", ...PALETTES[mode]});
            for (const [family, bytes] of fonts) runtime.font(family, bytes, "ttf");
            runtime.loadLua(source, basename(options.scene));
            const [width, height] = runtime.size();
            if (mode === options.themes[0]) {
                const theme = runtime.adaptiveTheme ? "adaptive_vscode" : "fixed theme, palette unchanged";
                console.log(`${basename(options.scene)}  ${width}x${height}  ${seconds(runtime.duration)} s`
                    + `${runtime.loop ? "  loop" : ""}  ${theme}`);
            }
            for (const time of options.times) {
                const at = time === "end" ? runtime.duration : time;
                if (at > runtime.duration + 1e-6) {
                    throw new Error(`--time ${seconds(at)} is past the end (${seconds(runtime.duration)} s)`);
                }
                const file = join(out, `${stem}-${mode}-${seconds(at)}.png`);
                writeFileSync(file, png(width, height, runtime.render(at, true)));
                const issues = review(runtime.layoutReport(at, options.padding), options.contain);
                problems += issues.length;
                console.log(`${mode} t=${seconds(at)}  ${issues.length ? issues.join("; ") : "ok"}  ${file}`);
            }
        } finally {
            runtime.destroy();
        }
    }
    return problems ? 1 : 0;
}

if (process.argv[1] && resolve(process.argv[1]) === fileURLToPath(import.meta.url)) {
    try {
        process.exitCode = await main(process.argv.slice(2));
    } catch (error) {
        console.error(`host-review: ${error.message}`);
        process.exitCode = 2;
    }
}
