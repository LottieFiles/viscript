// Codex for the evaluation: runs with the user's config, plugins, and every installed skill
// turned off, so a case sees only the skill copied into its folder.
//   node codex.mjs <case-dir> <request> [model]   runs codex exec in <case-dir>/work
import {spawnSync} from "node:child_process";
import {closeSync, existsSync, openSync, readdirSync, statSync} from "node:fs";
import {homedir} from "node:os";
import {join} from "node:path";
import {fileURLToPath} from "node:url";

// The skills.config value that turns off each SKILL.md in the user's and the system skill folders.
function skillsOff() {
    const roots = [join(process.env.CODEX_HOME || join(homedir(), ".codex"), "skills"), join(homedir(), ".agents", "skills")];
    const found = [];
    const walk = (dir, depth) => {
        if (!existsSync(dir)) return;
        for (const name of readdirSync(dir)) {
            const path = join(dir, name);
            if (name === "SKILL.md") found.push(path);
            else if (depth < 3) { try { if (statSync(path).isDirectory()) walk(path, depth + 1); } catch {} }
        }
    };
    for (const root of roots) walk(root, 0);
    return `skills.config=[${found.map((path) => `{path=${JSON.stringify(path)},enabled=false}`).join(",")}]`;
}

// The options every evaluation run of codex exec starts with.
export const isolated = () => [
    "--ignore-user-config", "-c", "features.plugins=false", "-c", skillsOff(), "--skip-git-repo-check", "--ephemeral",
];

if (process.argv[1] === fileURLToPath(import.meta.url)) {
    const [dir, request, model] = process.argv.slice(2);
    const work = join(dir, "work");
    const out = openSync(join(dir, "run.jsonl"), "w"), err = openSync(join(dir, "run.err"), "w");
    // codex exec waits for stdin unless it is closed.
    const run = spawnSync("codex", ["exec", ...isolated(), "-C", work, "-s", "workspace-write",
        ...(model ? ["-m", model] : []), "--json", "-o", join(dir, "last.txt"), request], {cwd: work, stdio: ["ignore", out, err]});
    closeSync(out); closeSync(err);
    process.exit(run.status ?? 1);
}
