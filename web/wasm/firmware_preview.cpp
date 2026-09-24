// First browser renderer target.  This deliberately has no ESPHome or board driver dependency: the
// profile arrives at runtime, while the LVGL framebuffer and the climate-card geometry are shared with firmware.
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <array>
#include <string>
#include "lvgl.h"
#include "../../components/smart_display/climate_card.h"
#include "../../components/smart_display/weather_card.h"
#include "../../components/smart_display/renderer_host_api.h"
#include "generated/firmware_renderer_manifest.h"
#include "generated/esphome_weather_26.c"

static_assert(ESP_SCREEN_RENDERER_ABI == 1, "Update the WebAssembly host adapter for the renderer ABI change");

namespace {
lv_display_t *display = nullptr;
lv_obj_t *root = nullptr;
uint32_t *frame = nullptr;
int width = 720, height = 720, dpi = 170, columns = 3, rows = 3;
float target = 21.0f, current = 20.5f;
std::string mode = "heat";
struct WeatherDay { std::string day, condition; float high = NAN, low = NAN, rain = NAN; };
std::array<WeatherDay, 5> weather_days{};
int weather_count = 0;
float weather_current = NAN;
std::string weather_condition;
bool has_weather = false;
bool has_climate = false;

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
void card(lv_obj_t *parent, int x, int y, int w, int h) {
  auto *item = lv_obj_create(parent); lv_obj_set_pos(item, x, y); lv_obj_set_size(item, w, h);
  lv_obj_set_style_radius(item, std::max(12, width / 30), 0); lv_obj_set_style_bg_color(item, lv_color_hex(0xFFFFFF), 0);
  lv_obj_set_style_bg_opa(item, LV_OPA_COVER, 0); lv_obj_set_style_border_width(item, 0, 0);
}
const char *weather_glyph(const std::string &condition) {
  if (condition == "sunny") return "\U000F0599"; if (condition == "clear-night") return "\U000F0594";
  if (condition == "cloudy") return "\U000F0590"; if (condition == "partlycloudy") return "\U000F0595";
  if (condition == "rainy") return "\U000F0597"; if (condition == "pouring") return "\U000F0596";
  if (condition == "snowy") return "\U000F0598"; if (condition == "fog") return "\U000F0591";
  if (condition == "lightning") return "\U000F0593"; if (condition == "windy") return "\U000F059D";
  return "\U000F0595";
}
const char *weather_text(const std::string &condition) {
  if (condition == "partlycloudy") return "Partly cloudy"; if (condition == "clear-night") return "Clear night";
  if (condition == "sunny") return "Sunny"; if (condition == "cloudy") return "Cloudy";
  if (condition == "rainy") return "Rainy"; if (condition == "pouring") return "Pouring";
  if (condition == "snowy") return "Snowy"; if (condition == "fog") return "Fog";
  return condition.c_str();
}
const char *weather_draw_glyph(const std::string &condition) {
  return weather_glyph(condition);
}
void weather_render() {
  const int gap = std::max(4, width / 90), grid_top = 58;
  const int cell_w = (width - gap * (columns + 1)) / std::max(1, columns);
  const int cell_h = (height - grid_top - gap * (rows + 1)) / std::max(1, rows);
  const int x = gap, y = grid_top + gap + cell_h + gap, w = width - 2 * gap, h = cell_h;
  card(root, x, y, w, h);
  const int pad = std::max(12, width / 45), current_w = std::max(110, w * 28 / 100);
  char value[24]; std::snprintf(value, sizeof(value), "%.0f°", weather_current);
  const char *current_glyph = weather_draw_glyph(weather_condition);
  label(root, current_glyph, x + pad, y + h / 2 - 24, 48, 42, &esphome_weather_26, lv_color_hex(0x1B1B1B));
  label(root, value, x + pad + 48, y + h / 2 - 24, current_w - 48, 42, &lv_font_montserrat_28, lv_color_hex(0x1B1B1B));
  label(root, weather_text(weather_condition), x + pad, y + h / 2 + 20, current_w - pad, 24, &lv_font_montserrat_14, lv_color_hex(0x5A5F66));
  const int count = std::min(5, weather_count), days_x = x + current_w, days_w = w - current_w - pad;
  const int column = count ? days_w / count : days_w;
  for (int i = 0; i < count; ++i) {
    const auto &day = weather_days[i]; const int dx = days_x + i * column;
    label(root, day.day.c_str(), dx, y + 24, column, 24, &lv_font_montserrat_14, lv_color_hex(0x1B1B1B));
    const char *day_glyph = weather_draw_glyph(day.condition);
    label(root, day_glyph, dx, y + 48, column, 30, &esphome_weather_26, lv_color_hex(0x1B1B1B));
    char temps[32]; std::snprintf(temps, sizeof(temps), "%.0f/%.0f", day.high, day.low);
    label(root, temps, dx, y + 78, column, 24, &lv_font_montserrat_14, lv_color_hex(0x5A5F66));
    if (std::isfinite(day.rain)) { char rain[16]; std::snprintf(rain, sizeof(rain), "%.0f%%", day.rain); label(root, rain, dx, y + 101, column, 18, &lv_font_montserrat_14, lv_color_hex(0x4D8FC2)); }
  }
}

void render() {
  if (!root) return;
  lv_obj_clean(root);
  lv_obj_set_style_bg_color(root, lv_color_hex(0xE7E7E7), 0);
  lv_obj_set_style_bg_opa(root, LV_OPA_COVER, 0);
  if (has_weather) weather_render();
  if (has_climate && !has_weather) {
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
  }
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
  target = setpoint; current = room; mode = hvac_mode ? hvac_mode : "heat"; has_climate = true; render();
}
void preview_clear_climate() { has_climate = false; }
void preview_clear_weather() { weather_count = 0; has_weather = false; weather_condition.clear(); }
void preview_set_weather_current(float temperature, const char *condition) { weather_current = temperature; weather_condition = condition ? condition : ""; has_weather = true; }
void preview_set_weather_day(int index, const char *day, const char *condition, float high, float low, float rain) {
  if (index < 0 || index >= (int)weather_days.size()) return;
  weather_days[index] = {day ? day : "", condition ? condition : "", high, low, rain}; weather_count = std::max(weather_count, index + 1); has_weather = true;
}
void preview_set_profile(int cols, int lines) {
  columns = std::clamp(cols, 1, 12); rows = std::clamp(lines, 1, 12); render();
}
void preview_render() { if (display) { render(); lv_refr_now(display); lv_timer_handler(); } }
const uint32_t *preview_frame() { return frame; }
int preview_width() { return width; }
int preview_height() { return height; }
}
