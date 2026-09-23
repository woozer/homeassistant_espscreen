<script setup lang="ts">
import { nextTick, onBeforeUnmount, onMounted, ref, watch } from "vue";
import createModule from "../wasm/firmware_preview.js";
import { clock24, entityName, liveOf, screenBuiltinName, state } from "../store";
import { clockText } from "../model/topbar";
import { getJson } from "../api";
import type { Tile } from "../types";

const props = defineProps<{ width: number; height: number; dpi?: number; columns: number; rows: number; target?: number; room?: number; mode?: string; tiles?: Tile[] }>();
const canvas = ref<HTMLCanvasElement | null>(null);
let module: any = null;
let raf = 0;
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
async function loadForecasts() {
  const weather = (props.tiles || []).filter((tile) => domainOf(tile) === "weather" && displayOf(tile) === "forecast");
  await Promise.all(weather.map(async (tile) => {
    try { forecasts.value[tile.entity] = (await getJson<{ days?: any[] }>(`forecast?entity=${encodeURIComponent(tile.entity)}`)).days || []; }
    catch { forecasts.value[tile.entity] = []; }
  }));
}

function paint() {
  if (!module || !canvas.value) return;
  module._preview_set_profile(props.columns, props.rows);
  module._preview_set_climate(props.target ?? 21, props.room ?? 20, props.mode || "heat");
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
onMounted(start);
onBeforeUnmount(() => cancelAnimationFrame(raf));
</script>

<template>
  <div class="firmware-preview" :style="{ aspectRatio: `${width} / ${height}` }">
    <canvas ref="canvas" :width="width" :height="height" aria-label="LVGL firmware preview"></canvas>
    <div class="firmware-overlay" :style="{ gridTemplateColumns: `repeat(${columns}, 1fr)`, gridTemplateRows: `repeat(${rows}, 1fr)` }" aria-label="Configured tiles">
      <div v-for="tile in (tiles || []).filter((item) => item.slot < columns * rows)" :key="`${tile.entity}-${tile.slot}`"
        class="firmware-tile" :style="{ gridColumn: `${(tile.slot % columns) + 1} / span ${tile.options?.size === 'wide' ? Math.min(2, columns - (tile.slot % columns)) : tile.options?.size === 'full' ? columns : 1}`, gridRow: `${Math.floor(tile.slot / columns) + 1}` }">
        <span class="firmware-tile-name">{{ labelOf(tile) }}</span>
        <span v-if="domainOf(tile) !== 'weather' || displayOf(tile) !== 'forecast'" class="firmware-tile-value">{{ valueOf(tile) }}</span>
        <span v-if="domainOf(tile) === 'climate'" class="firmware-tile-mode">{{ modeOf(tile) }}</span>
        <span v-if="domainOf(tile) === 'weather' && displayOf(tile) === 'forecast'" class="weather-forecast">
          <span v-for="(day, index) in forecastOf(tile).slice(0, 5)" :key="`${tile.entity}-${index}`" class="weather-day">
            <b>{{ day.d || "—" }}</b><span>{{ day.c || "—" }}</span><strong>{{ day.h ?? "—" }}° / {{ day.l ?? "—" }}°</strong><small v-if="day.p !== undefined">{{ day.p }}% rain</small>
          </span>
          <span v-if="!forecastOf(tile).length" class="weather-empty">No forecast</span>
        </span>
      </div>
    </div>
  </div>
</template>
