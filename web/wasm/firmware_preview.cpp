// First browser renderer target.  This deliberately has no ESPHome or board driver dependency: the
// profile arrives at runtime, while the LVGL framebuffer and the climate-card geometry are shared with firmware.
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <string>
#include "lvgl.h"
#include "../../components/smart_display/climate_card.h"

namespace {
lv_display_t *display = nullptr;
lv_obj_t *root = nullptr;
uint32_t *frame = nullptr;
int width = 720, height = 720, dpi = 170, columns = 3, rows = 3;
float target = 21.0f, current = 20.5f;
std::string mode = "heat";

void flush(lv_display_t *, const lv_area_t *area, uint8_t *px) {
  const int w = area->x2 - area->x1 + 1;
  const int h = area->y2 - area->y1 + 1;
  const auto *src = reinterpret_cast<const uint32_t *>(px);
  for (int y = 0; y < h; ++y)
    std::copy(src + y * w, src + (y + 1) * w, frame + (area->y1 + y) * width + area->x1);
  lv_display_flush_ready(display);
}

void label(lv_obj_t *parent, const char *text, int x, int y, int w, int h, const lv_font_t *font, lv_color_t color) {
  auto *item = lv_label_create(parent);
  lv_label_set_text(item, text);
  lv_obj_set_pos(item, x, y); lv_obj_set_size(item, w, h);
  lv_obj_set_style_text_font(item, font, 0); lv_obj_set_style_text_color(item, color, 0);
  lv_obj_set_style_text_align(item, LV_TEXT_ALIGN_CENTER, 0);
}

void render() {
  if (!root) return;
  lv_obj_clean(root);
  lv_obj_set_style_bg_color(root, lv_color_hex(0xE7E7E7), 0);
  lv_obj_set_style_bg_opa(root, LV_OPA_COVER, 0);
  const int grid_top = 58, gap = std::max(4, width / 90);
  const int cell_w = (width - gap * (columns + 1)) / std::max(1, columns);
  const int cell_h = (height - grid_top - gap * (rows + 1)) / std::max(1, rows);
  for (int row = 0; row < rows; ++row) for (int col = 0; col < columns; ++col) {
    auto *tile = lv_obj_create(root);
    lv_obj_set_pos(tile, gap + col * (cell_w + gap), grid_top + gap + row * (cell_h + gap));
    lv_obj_set_size(tile, cell_w, cell_h);
    lv_obj_set_style_radius(tile, std::max(8, width / 45), 0);
    lv_obj_set_style_bg_color(tile, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_border_width(tile, 1, 0);
    lv_obj_set_style_border_color(tile, lv_color_hex(0xDDDDDD), 0);
  }
  climate_card::Metrics metrics;
  ui::configure(dpi, "standard");
  const auto layout = climate_card::layout(metrics, width, metrics.bar + 12, height - 18, 2, 0, 1);
  auto *dial = lv_arc_create(root);
  const int size = std::min(width - 80, height - 160);
  lv_obj_set_size(dial, size, size); lv_obj_center(dial);
  lv_arc_set_range(dial, 0, 1000); lv_arc_set_bg_angles(dial, 135, 405); lv_arc_set_rotation(dial, 0);
  lv_arc_set_value(dial, static_cast<int>(std::clamp((target - 5.0f) / 30.0f, 0.0f, 1.0f) * 1000));
  lv_obj_set_style_arc_width(dial, 14, LV_PART_MAIN); lv_obj_set_style_arc_width(dial, 14, LV_PART_INDICATOR);
  lv_obj_set_style_arc_color(dial, lv_color_hex(0xD6D9DE), LV_PART_MAIN);
  lv_obj_set_style_arc_color(dial, lv_color_hex(0xF0A51A), LV_PART_INDICATOR);
  lv_obj_set_style_bg_opa(dial, LV_OPA_TRANSP, 0); lv_obj_set_style_border_width(dial, 0, 0);
  lv_obj_set_style_bg_color(dial, lv_color_hex(0xF0A51A), LV_PART_KNOB);
  char value[32]; std::snprintf(value, sizeof(value), "%.1f°", target);
  label(root, value, width / 2 - 90, height / 2 - 34, 180, 42, &lv_font_montserrat_28, lv_color_hex(0x1B1B1B));
  char now[32]; std::snprintf(now, sizeof(now), "now %.1f°", current);
  label(root, now, width / 2 - 90, height / 2 + 18, 180, 24, &lv_font_montserrat_14, lv_color_hex(0x5A5F66));
  label(root, mode.c_str(), width / 2 - 90, height - 42, 180, 24, &lv_font_montserrat_14, lv_color_hex(0x5A5F66));
  lv_obj_invalidate(root); lv_timer_handler();
}
}

extern "C" {
void preview_init(int w, int h, int display_dpi) {
  width = std::max(1, w); height = std::max(1, h); dpi = display_dpi > 0 ? display_dpi : 170;
  lv_init(); frame = new uint32_t[static_cast<size_t>(width) * height]{};
  display = lv_display_create(width, height); lv_display_set_flush_cb(display, flush);
  lv_display_set_buffers(display, frame, nullptr, static_cast<uint32_t>(width * height * sizeof(uint32_t)), LV_DISPLAY_RENDER_MODE_FULL);
  root = lv_screen_active(); render();
}
void preview_set_climate(float setpoint, float room, const char *hvac_mode) {
  target = setpoint; current = room; mode = hvac_mode ? hvac_mode : "heat"; render();
}
void preview_set_profile(int cols, int lines) {
  columns = std::clamp(cols, 1, 12); rows = std::clamp(lines, 1, 12); render();
}
void preview_render() { if (display) { lv_timer_handler(); render(); } }
const uint32_t *preview_frame() { return frame; }
int preview_width() { return width; }
int preview_height() { return height; }
}
