<script setup lang="ts">
import { computed, nextTick, onBeforeUnmount, onMounted, ref, watch } from "vue";
import createModule from "../wasm/firmware_preview.js";
import { clock24, entityName, liveOf, screenBuiltinName, state } from "../store";
import { clockText } from "../model/topbar";
import { getJson } from "../api";
import type { Tile } from "../types";

const props = defineProps<{ width: number; height: number; dpi?: number; columns: number; rows: number; pages?: number; target?: number; room?: number; mode?: string; tiles?: Tile[] }>();
const canvas = ref<HTMLCanvasElement | null>(null);
let module: any = null;
let raf = 0;
const page = ref(0);
const pointerStart = ref<number | null>(null);
const pageCount = computed(() => Math.max(1, props.pages || 1));
const pageTiles = computed(() => (props.tiles || []).filter((tile) => Math.floor(tile.slot / (props.columns * props.rows)) === page.value));
const visualTiles = computed(() => pageTiles.value.filter((tile) => !(domainOf(tile) === "weather" && displayOf(tile) === "forecast")));
const forecasts = ref<Record<string, { d?: string; c?: string; h?: number; l?: number; p?: number }[]>>({});
const domainOf = (tile: Tile) => tile.entity.split(".", 1)[0];
const stateOf = (tile: Tile) => liveOf(tile.entity);
const labelOf = (tile: Tile) => tile.name || (domainOf(tile) === "screen" ? screenBuiltinName(tile.entity) : undefined) || entityName(tile.entity);
const displayOf = (tile: Tile) => tile.options?.display || "standard";
function valueOf(tile: Tile) {
  const live = stateOf(tile), domain = domainOf(tile);
  if (domain === "screen") {
    if (displayOf(tile) === "analog") return "◷";
    if (displayOf(tile) === "digital") return clockText(clock24.value, new Date(state.now));
    return labelOf(tile);
  }
  if (!live) return "—";
  const a = live.a || {};
  if (domain === "climate") return `${a.temperature ?? "—"}°  ·  now ${a.current_temperature ?? "—"}°`;
  if (domain === "weather") return `${live.state}${a.temperature !== undefined ? `  ·  ${a.temperature}°` : ""}`;
  if (domain === "media_player") return a.media_title || live.state;
  if (["sensor", "number", "input_number", "counter"].includes(domain)) return `${live.state}${a.unit_of_measurement || ""}`;
  return live.state;
}
const modeOf = (tile: Tile) => stateOf(tile)?.state || "";
const forecastOf = (tile: Tile) => forecasts.value[tile.entity] || [];
const weatherIcon = (condition?: string) => ({ sunny: "☀", "clear-night": "☾", cloudy: "☁", partlycloudy: "◐", rainy: "☂", pouring: "☂", snowy: "❄", fog: "≋", windy: "≋" } as Record<string, string>)[condition || ""] || "·";
const weatherTemp = (tile: Tile) => stateOf(tile)?.a?.temperature;
const weatherCondition = (tile: Tile) => stateOf(tile)?.state || "";
async function loadForecasts() {
  const weather = (props.tiles || []).filter((tile) => domainOf(tile) === "weather");
  await Promise.all(weather.map(async (tile) => {
    try { forecasts.value[tile.entity] = (await getJson<{ days?: any[] }>(`forecast?entity=${encodeURIComponent(tile.entity)}`)).days || []; }
    catch { forecasts.value[tile.entity] = []; }
  }));
}
function swipeStart(event: PointerEvent) { pointerStart.value = event.clientX; }
function swipeEnd(event: PointerEvent) {
  if (pointerStart.value === null) return;
  const delta = event.clientX - pointerStart.value; pointerStart.value = null;
  if (Math.abs(delta) < 35) return;
  page.value = Math.max(0, Math.min(pageCount.value - 1, page.value + (delta < 0 ? 1 : -1)));
}
watch(pageCount, (count) => { if (page.value >= count) page.value = count - 1; });

function paint() {
  if (!module || !canvas.value) return;
  module._preview_set_profile(props.columns, props.rows);
  module._preview_clear_climate();
  const climate = (props.tiles || []).find((tile) => domainOf(tile) === "climate");
  if (climate) module._preview_set_climate(props.target ?? 21, props.room ?? 20, props.mode || "heat");
  module._preview_clear_weather();
  const weather = (props.tiles || []).find((tile) => domainOf(tile) === "weather");
  if (weather) {
    const live = stateOf(weather);
    module.ccall("preview_set_weather_current", "void", ["number", "string"], [Number(live?.a?.temperature ?? 0), weatherCondition(weather)]);
    forecastOf(weather).slice(0, 5).forEach((day, index) => module.ccall("preview_set_weather_day", "void", ["number", "string", "string", "number", "number", "number"], [index, day.d || "", day.c || "", Number(day.h ?? NaN), Number(day.l ?? NaN), Number(day.p ?? NaN)]));
  }
  module._preview_render();
  const context = canvas.value.getContext("2d");
  if (!context) return;
  const pixels = module.HEAPU8.subarray(module._preview_frame(), module._preview_frame() + props.width * props.height * 4);
  context.putImageData(new ImageData(new Uint8ClampedArray(pixels), props.width, props.height), 0, 0);
}
async function start() {
  module = await createModule();
  module._preview_init(props.width, props.height, props.dpi ?? 170);
  await nextTick(); paint();
}
watch(() => [props.width, props.height, props.dpi, props.columns, props.rows, props.target, props.room, props.mode], () => {
  cancelAnimationFrame(raf); raf = requestAnimationFrame(paint);
});
watch(() => props.tiles, loadForecasts, { immediate: true, deep: true });
watch(forecasts, () => {
  cancelAnimationFrame(raf); raf = requestAnimationFrame(paint);
}, { deep: true });
onMounted(start);
onBeforeUnmount(() => cancelAnimationFrame(raf));
</script>

<template>
  <div class="firmware-preview" :style="{ aspectRatio: `${width} / ${height}` }" @pointerdown="swipeStart" @pointerup="swipeEnd">
    <canvas ref="canvas" :width="width" :height="height" aria-label="LVGL firmware preview"></canvas>
    <div class="firmware-overlay" :style="{ gridTemplateColumns: `repeat(${columns}, 1fr)`, gridTemplateRows: `repeat(${rows}, 1fr)` }" aria-label="Configured tiles">
      <div v-for="tile in visualTiles" :key="`${tile.entity}-${tile.slot}`"
        class="firmware-tile" :class="{ 'firmware-weather-tile': domainOf(tile) === 'weather' && displayOf(tile) === 'forecast' }"
        :style="{ gridColumn: `${(tile.slot % columns) + 1} / span ${tile.options?.size === 'wide' || (domainOf(tile) === 'weather' && displayOf(tile) === 'forecast') ? Math.min(2, columns - (tile.slot % columns)) : tile.options?.size === 'full' ? columns : 1}`, gridRow: `${Math.floor((tile.slot % (columns * rows)) / columns) + 1}` }">
        <span v-if="!(domainOf(tile) === 'weather' && displayOf(tile) === 'forecast')" class="firmware-tile-name">{{ labelOf(tile) }}</span>
        <span v-if="domainOf(tile) !== 'weather' || displayOf(tile) !== 'forecast'" class="firmware-tile-value">{{ valueOf(tile) }}</span>
        <span v-if="domainOf(tile) === 'climate'" class="firmware-tile-mode">{{ modeOf(tile) }}</span>
        <span v-if="domainOf(tile) === 'weather' && displayOf(tile) === 'forecast'" class="weather-forecast">
          <span class="weather-current"><b>{{ weatherIcon(weatherCondition(tile)) }}</b><strong>{{ weatherTemp(tile) ?? "—" }}°</strong><span>{{ weatherCondition(tile) }}</span></span>
          <span class="weather-days"><span v-for="(day, index) in forecastOf(tile).slice(0, 5)" :key="`${tile.entity}-${index}`" class="weather-day">
            <b>{{ day.d || "—" }}</b><span class="weather-icon">{{ weatherIcon(day.c) }}</span><strong>{{ day.h ?? "—" }}° / {{ day.l ?? "—" }}°</strong><small v-if="day.p !== undefined">{{ day.p }}%</small>
          </span></span>
          <span v-if="!forecastOf(tile).length" class="weather-empty">No forecast</span>
        </span>
      </div>
    </div>
    <div v-if="pageCount > 1" class="firmware-dots" aria-label="Preview pages">
      <button v-for="index in pageCount" :key="index" type="button" :class="{ active: index - 1 === page }" :aria-label="`Page ${index}`" @click.stop="page = index - 1"></button>
    </div>
  </div>
</template>
