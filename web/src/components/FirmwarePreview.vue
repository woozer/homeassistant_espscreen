<script setup lang="ts">
import { nextTick, onBeforeUnmount, onMounted, ref, watch } from "vue";
import createModule from "../wasm/firmware_preview.js";

const props = defineProps<{ width: number; height: number; dpi?: number; columns: number; rows: number; target?: number; room?: number; mode?: string }>();
const canvas = ref<HTMLCanvasElement | null>(null);
let module: any = null;
let raf = 0;

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
onMounted(start);
onBeforeUnmount(() => cancelAnimationFrame(raf));
</script>

<template>
  <div class="firmware-preview" :style="{ aspectRatio: `${width} / ${height}` }">
    <canvas ref="canvas" :width="width" :height="height" aria-label="LVGL firmware preview"></canvas>
  </div>
</template>
