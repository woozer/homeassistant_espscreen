# Firmware-backed emulator

The browser preview must render the same LVGL tree as the ESP32 firmware. Vue owns the editor and the test-device
controls; it does not recreate firmware cards or detail pages.

The two targets share the renderer and differ only at the hardware boundary:

| Firmware target | Browser target |
| --- | --- |
| ESPHome display flush | WebAssembly framebuffer copied to canvas |
| ESPHome touchscreen | Pointer events converted to LVGL input events |
| Home Assistant API | Test-lab state and service-call adapter |
| GPIO, sensors and clock | Virtual device capabilities and simulated values |

Firmware code may use the host boundary for display, input, time, sensors and service calls. Rendering and navigation
must remain in the shared LVGL code. The host build is required to compile the same renderer sources as the firmware;
hand-maintained browser versions of firmware cards are not part of the contract.

## Update rule

Every firmware update runs both targets:

1. Compile the board firmware with ESPHome.
2. Compile the host renderer with the same firmware checkout and generated font assets.
3. Run the host renderer smoke test for the configured board profiles.

If the shared renderer interface changes, the host build must fail until its adapter is updated. It must not silently
fall back to an older renderer. The current WASM build already regenerates weather glyph data from ESPHome's generated
`materialdesign_icons_mini` font; the same source linkage will be used for the full renderer extraction.

## Host boundary

The host adapter will expose only these operations to the editor:

```text
init(profile)
set_device_state(state)
send_touch(x, y, pressed)
render_frame()
dispatch_service_call(call)
```

The profile carries resolution, DPI, grid dimensions and board capabilities. No 720 × 720 or 3 × 3 assumption belongs
inside the renderer.
