<script setup>
import * as echarts from "echarts";
import { computed, onBeforeUnmount, onMounted, ref, watch } from "vue";

import { reportText } from "../utils/reportText.js";

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

function normalizeOptionText(value) {
  if (typeof value === "string") {
    return reportText(value);
  }

  if (Array.isArray(value)) {
    return value.map(normalizeOptionText);
  }

  if (value && typeof value === "object") {
    return Object.entries(value).reduce((result, [key, nestedValue]) => {
      result[key] = normalizeOptionText(nestedValue);
      return result;
    }, {});
  }

  return value;
}

const normalizedOption = computed(() => normalizeOptionText(props.option));

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
  renderChart(normalizedOption.value);

  resizeObserver = new ResizeObserver(() => {
    chartInstance?.resize();
  });
  resizeObserver.observe(chartRoot.value);
});

watch(
  normalizedOption,
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