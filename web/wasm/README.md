# LVGL browser preview

`build.sh` compiles LVGL and the browser adapter to `web/src/wasm/firmware_preview.{js,wasm}`. The adapter accepts a
screen profile at runtime (`preview_init(width, height, dpi)` plus `preview_set_profile(columns, rows)`), so the
same module can serve square 3×3 previews, wide 2×3 panels, portrait screens, and future board shapes.

The adapter is intentionally independent of ESPHome hardware drivers. It is the first extraction seam for the
browser renderer; the editor will pass the complete layout and entity state through this seam as the remaining shared
runtime tile code is moved behind it.

Run `./web/wasm/build.sh` from the repository root. Emscripten is required; the script downloads pinned LVGL 9.5.0
source into the local cache when it is not present.

The forecast icons are extracted from ESPHome's generated `materialdesign_icons_mini` font, the same 4-bpp font
used by the firmware. After changing the ESPHome version, `packages/core.yaml`, or the firmware icon list, compile
the board once and regenerate the host asset before building the preview:

```sh
docker run --rm --entrypoint esphome \
  -v "$PWD:/config" -v "$PWD/../.cache/esphome-font:/config/.esphome" \
  ghcr.io/esphome/esphome:2026.9.0 compile /config/waveshare-esp32s3-4b.yaml
ESPHOME_GENERATED_MAIN="$PWD/../.cache/esphome-font/build/waveshare4b-new/src/main.cpp" ./web/wasm/build.sh
```

`generate_esphome_weather_font.py` reads the generated ESPHome `Font/Glyph` arrays and writes the small LVGL
descriptor consumed by the browser. There is no second hand-maintained icon font.
