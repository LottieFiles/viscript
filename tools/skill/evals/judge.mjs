// Rate the contact sheets of one case's rounds blind, with Codex.
//   node judge.mjs <out.json> <case-dir>...
// Each case dir holds the sheet.png and grade.json that grade.mjs wrote for the same case, in
// different rounds or with different skills. Codex sees the sheets in a shuffled order under
// letters; out.json keeps each score with its case dir, and the table prints best first.
import {spawnSync} from "node:child_process";
import {existsSync, readFileSync, writeFileSync} from "node:fs";
import {isolated} from "./codex.mjs";

const [out, ...dirs] = process.argv.slice(2);
if (!out || dirs.length < 2 || dirs.length > 10) {
    console.error("usage: node judge.mjs <out.json> <case-dir>... (2 to 10 case dirs)");
    process.exit(2);
}
for (const dir of dirs) if (!existsSync(`${dir}/sheet.png`)) throw new Error(`${dir} has no sheet.png`);
const keys = ["explains", "visual_carries", "text_economy", "layout", "motion", "overall"];
const order = dirs.map((dir) => ({dir, key: Math.random()})).sort((a, b) => a.key - b.key).map((e) => e.dir);
const letters = order.map((_, i) => String.fromCharCode(65 + i));
const request = JSON.parse(readFileSync(`${dirs[0]}/grade.json`, "utf8")).prompt;
const prompt = `You are judging short explanatory animations made for this request:

${request}

Each attached image is a contact sheet of one clip: 12 frames at evenly spaced times, read left to right, top to bottom. The clips may come from different tools; judge only what a first-time viewer would see.
${letters.map((letter, i) => `- Image ${i + 1} is clip ${letter}`).join("\n")}

Score every clip from 1 (poor) to 5 (excellent) on:
- explains: a first-time viewer understands the idea from the clip alone;
- visual_carries: shapes and motion carry the explanation rather than text;
- text_economy: every word earns its place; no titles, slogans, persistent caption bars, metadata, or text that repeats what the picture shows;
- layout: nothing overlaps, clips, or crowds; the subject fills the frame; legible at the clip's size;
- motion: the change between frames shows the mechanism step by step, and identities persist across steps;
- overall: your preference as a teacher.
In issues, name the most important defects in one or two sentences. Be strict and consistent across clips.`;
const clip = {type: "object", additionalProperties: false, required: ["clip", ...keys, "issues"],
    properties: Object.fromEntries([["clip", {type: "string"}], ...keys.map((k) => [k, {type: "integer"}]), ["issues", {type: "string"}]])};
writeFileSync(`${out}.schema.json`, JSON.stringify({type: "object", additionalProperties: false, required: ["clips"],
    properties: {clips: {type: "array", items: clip}}}));
const run = spawnSync("codex", ["exec", ...isolated(), "-s", "read-only", "--output-schema", `${out}.schema.json`,
    "-o", `${out}.raw`, ...order.flatMap((dir) => ["-i", `${dir}/sheet.png`]), "--", prompt],
    {stdio: ["ignore", "ignore", "pipe"], encoding: "utf8"});
if (run.status !== 0) {
    process.stderr.write(run.stderr ?? "");
    process.exit(run.status ?? 1);
}
const scores = JSON.parse(readFileSync(`${out}.raw`, "utf8")).clips
    .map(({clip: letter, ...score}) => ({dir: order[letters.indexOf(letter)], ...score}))
    .sort((a, b) => b.overall - a.overall || keys.reduce((s, k) => s + b[k] - a[k], 0));
writeFileSync(out, JSON.stringify(scores, null, 2));
console.log(`| case dir | ${keys.join(" | ")} | issues |\n|---|${keys.map(() => "---|").join("")}---|`);
for (const s of scores) console.log(`| ${s.dir} | ${keys.map((k) => s[k]).join(" | ")} | ${s.issues} |`);
