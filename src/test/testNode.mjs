// What the Node tests share: the example fonts, and runs of the viscript CLI that must end with
// the exit status a test expects.
import assert from "node:assert/strict";
import { spawnSync } from "node:child_process";
import { fileURLToPath } from "node:url";

const asset = (name) => fileURLToPath(new URL(`../examples/assets/${name}`, import.meta.url));

export const font = asset("Pretendard.ttf");
export const monoFont = asset("JetBrainsMono.ttf");
// The options that load both fonts.
export const fontOptions = ["--font", font, "--font", monoFont];

// Runs the CLI; a status other than the expected one fails with what the CLI printed.
export function run(cli, args, expected = 0, options = {}) {
    const result = spawnSync(cli, args, { encoding: "utf8", maxBuffer: 32 * 1024 * 1024, ...options });
    assert.equal(result.status, expected, result.stderr || result.stdout);
    return result;
}

// Audits a scene with these font options: (scene, further options, expected status) => result.
export const auditor = (cli, fonts) => (scene, extra = [], expected = 0) =>
    run(cli, ["audit", scene, ...fonts, ...extra], expected);
