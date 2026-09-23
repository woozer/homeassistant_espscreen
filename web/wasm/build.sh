#!/bin/sh
set -eu
ROOT=$(CDPATH= cd -- "$(dirname "$0")/../.." && pwd)
LVGL="$ROOT/../.cache/lvgl-9.5.0"
OUT="$ROOT/web/src/wasm"
mkdir -p "$OUT" "$ROOT/../.cache/emscripten"
if [ ! -f "$LVGL/src/lv_init.c" ]; then
  archive="$ROOT/../.cache/lvgl-9.5.0.tar.gz"
  curl --fail --silent --show-error --location "https://github.com/lvgl/lvgl/archive/refs/tags/v9.5.0.tar.gz" -o "$archive"
  rm -rf "$ROOT/../.cache/lvgl-9.5.0.tmp"
  mkdir "$ROOT/../.cache/lvgl-9.5.0.tmp"
  tar -xzf "$archive" -C "$ROOT/../.cache/lvgl-9.5.0.tmp"
  mv "$ROOT/../.cache/lvgl-9.5.0.tmp/lvgl-9.5.0" "$LVGL"
  rmdir "$ROOT/../.cache/lvgl-9.5.0.tmp"
fi
export PATH=/opt/homebrew/bin:/opt/homebrew/opt/python@3.14/bin:$PATH
export EMSDK_PYTHON=/opt/homebrew/opt/python@3.14/bin/python3.14
export EM_CACHE="$ROOT/../.cache/emscripten"
em++ -O2 -std=c++17 -DLV_CONF_INCLUDE_SIMPLE -I"$ROOT/web/wasm" -I"$LVGL" -I"$LVGL/src" -c \
  "$ROOT/web/wasm/firmware_preview.cpp" -o "$ROOT/../.cache/firmware_preview.o"
OBJECTS="$ROOT/../.cache/firmware_preview.o"
for source in $(find "$LVGL/src" -name '*.c' -print); do
  object="$ROOT/../.cache/wasm-$(basename "$source").o"
  if [ ! -f "$object" ]; then
    emcc -O2 -DLV_CONF_INCLUDE_SIMPLE -I"$ROOT/web/wasm" -I"$LVGL" -I"$LVGL/src" -c "$source" -o "$object"
  fi
  OBJECTS="$OBJECTS $object"
done
em++ -O2 $OBJECTS \
  -sWASM=0 -sALLOW_MEMORY_GROWTH=1 -sEXPORTED_FUNCTIONS='["_preview_init","_preview_set_profile","_preview_set_climate","_preview_clear_weather","_preview_set_weather_current","_preview_set_weather_day","_preview_render","_preview_frame","_preview_width","_preview_height"]' \
  -sEXPORTED_RUNTIME_METHODS='["ccall","cwrap","HEAPU8"]' -sENVIRONMENT=web -sMODULARIZE=1 -sEXPORT_ES6=1 \
  -o "$OUT/firmware_preview.js"
