<script setup>
import * as echarts from "echarts";
import { onBeforeUnmount, onMounted, ref, watch } from "vue";

const props = defineProps({
  option: {
    type: Object,
    required: true
  },
  height: {
    type: Number,
    default: 300
  },
  ariaLabel: {
    type: String,
    default: "Graphique statistique"
  }
});

const chartRoot = ref(null);

let chartInstance;
let resizeObserver;

function renderChart(option) {
  if (!chartInstance) {
    return;
  }

  chartInstance.setOption(option, true);
  chartInstance.resize();
}

onMounted(() => {
  if (!chartRoot.value) {
    return;
  }

  chartInstance = echarts.init(chartRoot.value, null, { renderer: "svg" });
  renderChart(props.option);

  resizeObserver = new ResizeObserver(() => {
    chartInstance?.resize();
  });
  resizeObserver.observe(chartRoot.value);
});

watch(
  () => props.option,
  (option) => {
    renderChart(option);
  },
  { deep: true }
);

onBeforeUnmount(() => {
  resizeObserver?.disconnect();
  chartInstance?.dispose();
  chartInstance = undefined;
});
</script>

<template>
  <div
    ref="chartRoot"
    class="rapport-stat-chart"
    :style="{ height: `${height}px` }"
    role="img"
    :aria-label="ariaLabel"
  />
</template>