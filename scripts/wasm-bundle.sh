#!/bin/sh
# Build the optimized WASM runtime and copy it into every in-repo consumer:
#   tools/vscode/runtime/            (VS Code Preview)
#   tools/skill/vis-skills/assets/wasm/  (portable skill bundle)
# Usage: scripts/wasm-bundle.sh [build-dir]   (requires emcc on PATH)
set -eu

root=$(cd "$(dirname "$0")/.." && pwd)
build=${1:-"$root/build-wasm"}

if [ ! -f "$build/build.ninja" ]; then
    meson setup "$build" "$root" \
        --cross-file "$root/scripts/cross/wasm32.txt" \
        --buildtype=minsize -Db_lto=true \
        -Dtests=false -Dexamples=false \
        -Dmodules=ui,input,runtime,audio,motion,diagram,tex
fi
ninja -C "$build" vis-wasm.js

node "$root/scripts/sync-wasm.mjs" "$build" \
    "$root/tools/vscode/runtime" \
    "$root/tools/skill/vis-skills/assets/wasm"
