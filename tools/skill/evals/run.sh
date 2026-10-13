#!/bin/sh
# Run the vis-skills evaluation cases with headless Claude Code.
# Usage: [EVAL_SKILL=dir] [EVAL_MODEL=opus] tools/skill/evals/run.sh <out-dir> <viscript-cli> [case-id...]
# Each case runs in an empty folder holding only a copy of vis-skills, so the
# agent sees the portable skill and nothing from this repository. Then grade.mjs
# renders, inspects, and audits whatever scene.lua the agent produced.
set -eu

here=$(cd "$(dirname "$0")" && pwd)
skill=${EVAL_SKILL:-"$here/../vis-skills"}
out=$(mkdir -p "$1" && cd "$1" && pwd)
cli=$(cd "$(dirname "$2")" && pwd)/$(basename "$2")
shift 2
model=${EVAL_MODEL:-opus}

cases=$(node -e 'for (const c of require(process.argv[1])) console.log(c.id)' "$here/cases.json")
[ $# -gt 0 ] && cases="$*"

rm -rf "$out/skill" && cp -R "$skill" "$out/skill"
font="$out/skill/assets/wasm/Pretendard.ttf"
code_font="$out/skill/assets/wasm/JetBrainsMono.ttf"

run_case() {
    id=$1
    dir="$out/$id"
    rm -rf "$dir" && mkdir -p "$dir/work/.claude/skills" "$dir/work/bin"
    cp -R "$out/skill" "$dir/work/.claude/skills/vis-skills"
    ln -s "$cli" "$dir/work/bin/viscript"
    prompt=$(node -e 'const c=require(process.argv[1]).find(c=>c.id===process.argv[2]);console.log(c.prompt)' "$here/cases.json" "$id")
    request="$prompt

Save the Lua scene as ./scene.lua. The viscript CLI is on PATH and the fonts are \$VIS_FONT (body) and \$VIS_CODE_FONT (code)."
    printf '%s\n' "$request" > "$dir/prompt.txt"
    start=$(date +%s)
    (cd "$dir/work" && PATH="$dir/work/bin:$PATH" VIS_FONT="$font" VIS_CODE_FONT="$code_font" \
        claude -p "$request" --model "$model" --setting-sources project \
            --permission-mode bypassPermissions --no-session-persistence \
            --output-format json > "$dir/run.json" 2> "$dir/run.err") || true
    echo $(( $(date +%s) - start )) > "$dir/seconds.txt"
    node "$here/grade.mjs" "$dir" "$cli" "$font" || true
}

for id in $cases; do run_case "$id" & done
wait
node "$here/grade.mjs" --index "$out"
echo "results: $out/index.html"
