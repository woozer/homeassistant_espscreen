#pragma once

// Stable boundary between the shared LVGL renderer and its hardware host.
// Keep rendering and navigation outside this header; this is only the data and
// operations needed by a display/input adapter.
#define ESP_SCREEN_RENDERER_ABI 1

#ifdef __cplusplus
extern "C" {
#endif

typedef struct esp_screen_renderer_profile {
  int width;
  int height;
  int dpi;
  int columns;
  int rows;
  unsigned capabilities;
} esp_screen_renderer_profile;

typedef struct esp_screen_renderer_touch_event {
  int x;
  int y;
  int pressed;
  int released;
} esp_screen_renderer_touch_event;

// Host implementations provide these operations. The renderer owns the
// framebuffer and returns a pointer valid until the next render call.
void esp_screen_renderer_init(const esp_screen_renderer_profile *profile);
void esp_screen_renderer_set_state(const char *state_json);
void esp_screen_renderer_touch(const esp_screen_renderer_touch_event *touch);
const unsigned *esp_screen_renderer_frame(void);
void esp_screen_renderer_render(void);

#ifdef __cplusplus
}
#endif
