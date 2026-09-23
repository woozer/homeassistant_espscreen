#!/usr/bin/env python3
"""Extract the weather glyphs emitted by ESPHome's generated LVGL font.

ESPHome remains the source of truth: run ESPHome's code generation first, then
this script converts its Font/Glyph arrays into an LVGL host-font descriptor.
"""
from pathlib import Path
import re
import sys

if len(sys.argv) != 3:
    raise SystemExit(f"usage: {sys.argv[0]} GENERATED_MAIN.C OUTPUT.C")
src, out = map(Path, sys.argv[1:])
text = src.read_text()
array_match = re.search(r"static constexpr uint8_t uint8_t_id_10\[\] PROGMEM = \{(.*?)\};", text, re.S)
glyph_match = re.search(r"static const font::Glyph font_glyph_id_10\[\] = \{(.*?)\};", text, re.S)
if not array_match or not glyph_match:
    raise SystemExit("ESPHome materialdesign_icons_mini arrays were not found")
raw = [int(value, 16) for value in re.findall(r"0x([0-9a-fA-F]+)", array_match.group(1))]
glyphs = [tuple(map(int, row)) for row in re.findall(r"\{(\d+), \(uint8_t_id_10 \+ (\d+)\), (\d+), (\d+), (\d+), (\d+), (\d+)\}", glyph_match.group(1))]
want = set(range(0xF0590, 0xF059E)) | {0xF067E, 0xF067F, 0xF05D6}
selected = [g for g in glyphs if g[0] in want]
if not selected:
    raise SystemExit("No weather glyphs found in generated ESPHome font")
missing = sorted(want - {g[0] for g in selected})
# F059A is not present in the current MDI font and is not used by firmware.
if missing and missing != [0xF059A]:
    raise SystemExit("Missing weather glyphs: " + ", ".join(hex(x) for x in missing))
selected.sort()
bitmap = []
metrics = []
for index, (cp, offset, advance, ox, oy, width, height) in enumerate(selected):
    byte_count = (width * height + 1) // 2  # ESPHome bpp=4, two pixels/byte
    data = raw[offset:offset + byte_count]
    if len(data) != byte_count:
        raise SystemExit(f"short bitmap for U+{cp:04X}")
    metrics.append((len(bitmap), advance * 16, width, height, ox, oy))
    bitmap.extend(data)
base = selected[0][0]
unicode_offsets = [cp - base for cp, *_ in selected]
lines = [
    "/* Generated from ESPHome materialdesign_icons_mini (packages/core.yaml). */",
    '#include "lvgl.h"',
    "static const uint8_t esphome_weather_bitmap[] = {",
]
for i in range(0, len(bitmap), 16):
    lines.append("    " + ", ".join(f"0x{x:02x}" for x in bitmap[i:i+16]) + ",")
lines += [
    "};",
    "static const lv_font_fmt_txt_glyph_dsc_t esphome_weather_glyphs[] = {",
    "    { .bitmap_index = 0, .adv_w = 0, .box_w = 0, .box_h = 0, .ofs_x = 0, .ofs_y = 0 },",
]
lines += [f"    {{ .bitmap_index = {i}, .adv_w = {a}, .box_w = {w}, .box_h = {h}, .ofs_x = {ox}, .ofs_y = {oy} }}," for i, a, w, h, ox, oy in metrics]
lines += [
    "};",
    "static const uint16_t esphome_weather_unicode[] = {",
    "    " + ", ".join(str(x) for x in unicode_offsets) + ",",
    "};",
    "static const lv_font_fmt_txt_cmap_t esphome_weather_cmap[] = {",
    f"    {{ .range_start = {base}, .range_length = {max(unicode_offsets)+1}, .glyph_id_start = 1, .unicode_list = esphome_weather_unicode, .glyph_id_ofs_list = NULL, .list_length = {len(selected)}, .type = LV_FONT_FMT_TXT_CMAP_SPARSE_TINY }},",
    "};",
    "static const lv_font_fmt_txt_dsc_t esphome_weather_dsc = {",
    "    .glyph_bitmap = esphome_weather_bitmap, .glyph_dsc = esphome_weather_glyphs, .cmaps = esphome_weather_cmap,",
    "    .kern_dsc = NULL, .kern_scale = 0, .cmap_num = 1, .bpp = 4, .kern_classes = 0, .bitmap_format = 0",
    "};",
    "const lv_font_t esphome_weather_26 = {",
    "    .get_glyph_dsc = lv_font_get_glyph_dsc_fmt_txt, .get_glyph_bitmap = lv_font_get_bitmap_fmt_txt,",
    "    .line_height = 26, .base_line = 0, .subpx = LV_FONT_SUBPX_NONE, .underline_position = 0, .underline_thickness = 0,",
    "    .dsc = &esphome_weather_dsc, .fallback = NULL, .user_data = NULL",
    "};",
]
out.write_text("\n".join(lines) + "\n")
print(f"wrote {out} from {src}: {len(selected)} glyphs, {len(bitmap)} bytes")
