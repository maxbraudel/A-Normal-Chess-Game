<script setup>
import * as echarts from "echarts";
import { computed, onBeforeUnmount, onMounted, ref, watch } from "vue";

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

function resolveGridInset(value, fallback) {
  return typeof value === "number" ? value : fallback;
}

function buildGraphicTextFont(style = {}) {
  const fontStyle = style.fontStyle || "normal";
  const fontWeight = style.fontWeight || 600;
  const fontSize = style.fontSize || 13;
  const fontFamily = style.fontFamily || 'Georgia, "Times New Roman", serif';

  return `${fontStyle} ${fontWeight} ${fontSize}px ${fontFamily}`;
}

function normalizeChartAxes(option) {
  if (!option || typeof option !== "object" || !option.yAxis) {
    return option;
  }

  const grid = Array.isArray(option.grid) ? option.grid[0] || {} : option.grid || {};
  const axes = Array.isArray(option.yAxis) ? option.yAxis : [option.yAxis];
  const graphicEntries = [];
  const nextAxes = axes.map((axis, index) => {
    if (!axis || typeof axis !== "object" || !axis.name) {
      return axis;
    }

    const position = axis.position || (index === 0 ? "left" : "right");
    const axisOffset = typeof axis.offset === "number" ? axis.offset : 0;
    const axisNameTextStyle = axis.nameTextStyle || {};
    const top = Math.max(10, resolveGridInset(grid.top, 46) - 28);

    graphicEntries.push({
      type: "text",
      silent: true,
      z: 100,
      top,
      left: position === "right" ? undefined : resolveGridInset(grid.left, 74) + axisOffset,
      right: position === "right" ? resolveGridInset(grid.right, 18) + axisOffset : undefined,
      style: {
        text: axis.name,
        fill: axisNameTextStyle.color || "#1f1f1f",
        font: buildGraphicTextFont(axisNameTextStyle),
        textAlign: position === "right" ? "right" : "left",
        textVerticalAlign: "top"
      }
    });

    return {
      ...axis,
      name: "",
      nameRotate: 0,
      nameGap: 0
    };
  });

  if (!graphicEntries.length) {
    return option;
  }

  const existingGraphic = Array.isArray(option.graphic)
    ? option.graphic
    : option.graphic
      ? [option.graphic]
      : [];

  return {
    ...option,
    yAxis: Array.isArray(option.yAxis) ? nextAxes : nextAxes[0],
    graphic: [...existingGraphic, ...graphicEntries]
  };
}

const normalizedOption = computed(() => normalizeChartAxes(props.option));

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