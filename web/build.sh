#!/usr/bin/env bash
# Compiles the allocator to WebAssembly and assembles the demo site in web/dist.
# Needs emcc on PATH (https://emscripten.org). CI runs this for GitHub Pages.
set -euo pipefail
cd "$(dirname "$0")"
rm -rf dist && mkdir -p dist
emcc -O2 -Wall -Wextra -I../include \
  ../src/my_allocator.c alloc_web.c \
  -sMODULARIZE=1 -sEXPORT_NAME=Allocator -sENVIRONMENT=web \
  -sEXPORTED_FUNCTIONS=_my_malloc,_my_free,_my_calloc,_print_heap_metadata,_bench_run,_bench_failed,_bench_corrupt,_malloc,_free \
  -sEXPORTED_RUNTIME_METHODS=HEAPU8 -sALLOW_MEMORY_GROWTH=1 -sINITIAL_MEMORY=33554432 \
  -o dist/allocator.js
cp index.html app.js demo.css og.jpg dist/
echo "built web/dist"
