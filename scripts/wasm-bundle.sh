#!/bin/sh
# Build the optimized WASM runtime and copy it into every in-repo consumer:
#   tools/vscode/runtime/            (VS Code Preview)
#   tools/skill/vis-skills/assets/wasm/  (portable skill bundle)
#   docs/page/runtime/                 (GitHub Pages journal)
# Usage: scripts/wasm-bundle.sh [build-dir]   (requires emcc on PATH)
set -eu

root=$(cd "$(dirname "$0")/.." && pwd)
build=${1:-"$root/build-wasm"}

if [ ! -f "$build/build.ninja" ]; then
    meson setup "$build" "$root" \
        --cross-file "$root/scripts/cross/wasm32.txt" \
        --buildtype=minsize -Ddebug=false -Db_lto=true -Db_ndebug=true \
        -Dtests=false -Dexamples=false \
        -Dmodules=ui,input,runtime,audio,motion,diagram,tex
fi
ninja -C "$build" vis-wasm.js

# extra size pass: emcc runs wasm-opt once; --converge repeats -Oz until the module stops shrinking
binaryen=$(em-config BINARYEN_ROOT 2>/dev/null || echo "$(dirname "$(command -v emcc)")/../..")
wasm_opt="$binaryen/bin/wasm-opt"
if [ -x "$wasm_opt" ]; then
    "$wasm_opt" "$build/vis-wasm.wasm" -Oz --converge \
        --enable-bulk-memory --enable-nontrapping-float-to-int --enable-sign-ext --enable-mutable-globals \
        -o "$build/vis-wasm.opt.wasm"
    mv "$build/vis-wasm.opt.wasm" "$build/vis-wasm.wasm"
else
    echo "wasm-opt not found under $binaryen; skipping the extra size pass" >&2
fi

node "$root/scripts/sync-wasm.mjs" "$build" \
    "$root/tools/vscode/runtime" \
    "$root/tools/skill/vis-skills/assets/wasm" \
    "$root/docs/page/runtime"
