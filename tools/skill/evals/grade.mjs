// Grade one evaluation case, or build the round index.
//   node grade.mjs <case-dir> <viscript-cli> <font>
//   node grade.mjs --index <round-dir>
import {execFileSync} from "node:child_process";
import {copyFileSync, existsSync, readFileSync, readdirSync, writeFileSync} from "node:fs";
import {basename, dirname, join} from "node:path";

const read = (path, fallback = "") => (existsSync(path) ? readFileSync(path, "utf8") : fallback);
const json = (path) => { try { return JSON.parse(read(path)); } catch { return null; } };

if (process.argv[2] === "--index") {
    const round = process.argv[3];
    const rows = readdirSync(round).filter((id) => existsSync(join(round, id, "grade.json")))
        .map((id) => ({id, ...json(join(round, id, "grade.json"))}));
    const cell = (r) => `
<section><h2>${r.id} ${r.pass ? "✅" : "❌"}</h2>
<p class="prompt">${r.prompt.replace(/</g, "&lt;")}</p>
<p>${r.duration ?? "-"} s · ${r.objects ?? "-"} objects · ${r.texts ?? "-"} texts (${r.titles ?? "-"} title/caption) ·
tracks ${r.trackers ?? "-"} · looks ${r.looks ?? "-"} · audit errors ${r.auditErrors ?? "-"} ·
${r.turns ?? "-"} turns · ${r.tokens ? `${Math.round(r.tokens.input / 1000)}k in (${Math.round(r.tokens.cached / 1000)}k cached) · ` : ""}${r.seconds}s${r.cost ? ` · $${r.cost.toFixed(2)}` : ""}${r.error ? ` · <b>${r.error}</b>` : ""}</p>
${r.gif ? `<img src="${r.id}/out.gif"><img src="${r.id}/final.png">
<img class="sheet" src="${r.id}/sheet.png">` : ""}
<p><a href="${r.id}/scene.lua">scene.lua</a> · <a href="${r.id}/run.json">run.json</a></p></section>`;
    const passed = rows.filter((r) => r.pass).length;
    const cost = rows.reduce((s, r) => s + (r.cost ?? 0), 0);
    const tokens = rows.reduce((s, r) => s + (r.tokens?.input ?? 0), 0);
    writeFileSync(join(round, "index.html"), `<!doctype html><meta charset="utf-8"><title>${basename(round)}</title>
<style>body{font:14px system-ui;margin:16px;background:#f6f6f4;color:#222}section{background:#fff;padding:12px;margin:12px 0;border-radius:6px}
img{max-width:48%;}img.sheet{max-width:97%}img{margin-right:1%;vertical-align:top;border:1px solid #ddd}.prompt{color:#555}</style>
<h1>${basename(round)}: ${passed}/${rows.length} pass · ${cost ? `$${cost.toFixed(2)}` : `${Math.round(tokens / 1000)}k input tokens`}</h1>${rows.map(cell).join("")}`);
    writeFileSync(join(round, "summary.json"), JSON.stringify(rows, null, 2));
    process.exit(0);
}

const [dir, cli, font] = process.argv.slice(2);
const run = json(join(dir, "run.json")) ?? {};
const grade = {
    prompt: read(join(dir, "prompt.txt")).split("\n")[0],
    seconds: Number(read(join(dir, "seconds.txt"), "0")),
    turns: run.num_turns, cost: run.total_cost_usd, pass: false,
};
// Token use: Claude Code reports its usage in run.json; Codex streams events to run.jsonl, where
// the turn's usage comes last and each shell command is one item.
if (run.usage) {
    const u = run.usage;
    grade.tokens = {input: u.input_tokens + (u.cache_creation_input_tokens ?? 0) + (u.cache_read_input_tokens ?? 0),
        cached: u.cache_read_input_tokens ?? 0, output: u.output_tokens};
}
if (existsSync(join(dir, "run.jsonl"))) {
    grade.turns = 0;
    for (const line of read(join(dir, "run.jsonl")).split("\n")) {
        let event; try { event = JSON.parse(line); } catch { continue; }
        if (event.type === "item.completed" && event.item?.type === "command_execution") grade.turns++;
        if (event.type === "turn.completed") {
            const u = event.usage;
            grade.tokens = {input: u.input_tokens, cached: u.cached_input_tokens ?? 0, output: u.output_tokens};
        }
    }
}
const vis = (...args) => execFileSync(cli, args, {encoding: "utf8", stdio: ["ignore", "pipe", "pipe"]});
const scene = join(dir, "work", "scene.lua");
const fonts = ["--font", font, "--font", join(dirname(font), "JetBrainsMono.ttf")];
try {
    if (!existsSync(scene)) throw new Error("no scene.lua");
    copyFileSync(scene, join(dir, "scene.lua"));
    const info = JSON.parse(vis("inspect", scene, ...fonts));
    grade.duration = info.duration;
    grade.objects = info.objects.length;
    // Slide-ness signals: how much of the scene is text, and whether it carries titles or captions.
    const texts = info.objects.filter((o) => o.type === "text");
    grade.texts = texts.length;
    grade.titles = texts.filter((o) => /title|caption|heading|subtitle|takeaway/i.test(o.id ?? "")).length;
    grade.trackers = (read(scene).match(/:track\(/g) ?? []).length;
    grade.looks = (read(scene).match(/:(look|focus)\(/g) ?? []).length;
    vis("render", scene, ...fonts, "--time", String(info.duration), "-o", join(dir, "final.png"));
    vis("render", scene, ...fonts, "--fps", "15", "-o", join(dir, "out.gif"));
    // One contact sheet of 12 evenly spaced frames, which judge.mjs rates.
    const times = Array.from({length: 11}, (_, k) => (info.duration * k / 11).toFixed(3)).concat("end");
    vis("render", scene, ...fonts, "--times", times.join(","), "-o", join(dir, "sheet.png"));
    grade.gif = true;
    let audit;
    try { audit = vis("audit", scene, ...fonts, "--format", "json"); } catch (e) { audit = e.stdout; }
    const report = JSON.parse(audit);
    grade.auditErrors = report.summary.errors;
    grade.auditIssues = report.issues.slice(0, 8).map((i) => i.message ?? i.code ?? JSON.stringify(i));
    grade.pass = grade.auditErrors === 0 && info.duration > 0;
} catch (e) {
    grade.error = String(e.stderr || e.message).trim().split("\n")[0].slice(0, 200);
}
writeFileSync(join(dir, "grade.json"), JSON.stringify(grade, null, 2));
console.log(basename(dir), grade.pass ? "PASS" : "FAIL", grade.error ?? `audit=${grade.auditErrors}`);
