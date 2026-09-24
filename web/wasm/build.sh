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
# When supplied, regenerate the weather font from ESPHome's generated C++ before
# compiling. This keeps the browser preview tied to the same font output as the
# firmware after an ESPHome/core/font update.
if [ -n "${ESPHOME_GENERATED_MAIN:-}" ]; then
  python3 "$ROOT/web/wasm/generate_esphome_weather_font.py" \
    "$ESPHOME_GENERATED_MAIN" "$ROOT/web/wasm/generated/esphome_weather_26.c"
fi
python3 "$ROOT/web/wasm/generate_renderer_manifest.py" "$ROOT"
COMMON_FLAGS="-DLV_CONF_INCLUDE_SIMPLE -DLV_FONT_FMT_TXT_LARGE=1 -I$ROOT/web/wasm -I$LVGL -I$LVGL/src"
em++ -O2 -std=c++17 $COMMON_FLAGS -c \
  "$ROOT/web/wasm/firmware_preview.cpp" -o "$ROOT/../.cache/firmware_preview-large.o"
OBJECTS="$ROOT/../.cache/firmware_preview-large.o"
for source in $(find "$LVGL/src" -name '*.c' -print); do
  object="$ROOT/../.cache/wasm-large-$(basename "$source").o"
  if [ ! -f "$object" ]; then
    emcc -O2 $COMMON_FLAGS -c "$source" -o "$object"
  fi
  OBJECTS="$OBJECTS $object"
done
em++ -O2 $OBJECTS \
  -sWASM=0 -sALLOW_MEMORY_GROWTH=1 -sEXPORTED_FUNCTIONS='["_preview_init","_preview_set_profile","_preview_set_climate","_preview_clear_climate","_preview_clear_weather","_preview_set_weather_current","_preview_set_weather_day","_preview_touch","_preview_back","_preview_render","_preview_frame","_preview_width","_preview_height"]' \
  -sEXPORTED_RUNTIME_METHODS='["ccall","cwrap","HEAPU8"]' -sENVIRONMENT=web -sMODULARIZE=1 -sEXPORT_ES6=1 \
  -o "$OUT/firmware_preview.js"
