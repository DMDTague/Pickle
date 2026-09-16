#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
BUILD_DIR="$ROOT/build-wasm"
OUT_DIR="$ROOT/web/public/engine"

rm -rf "$BUILD_DIR"
mkdir -p "$OUT_DIR"

emcmake cmake -S "$ROOT" -B "$BUILD_DIR" -DCMAKE_BUILD_TYPE=Release
cmake --build "$BUILD_DIR" --target pickle_web --parallel 2

cp "$BUILD_DIR/pickle_web.js" "$OUT_DIR/pickle.js"
cp "$BUILD_DIR/pickle_web.wasm" "$OUT_DIR/pickle_web.wasm"

echo "Built:"
ls -lh "$OUT_DIR/pickle.js" "$OUT_DIR/pickle_web.wasm"
