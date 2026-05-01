<script setup>
import InlineRichText from "./InlineRichText.vue";
import StatChart from "./StatChart.vue";
import { reportText } from "../utils/reportText.js";

defineProps({
  block: {
    type: Object,
    required: true
  },
  sourceKind: {
    type: String,
    default: ""
  },
  sourceTag: {
    type: String,
    default: ""
  },
  embedded: {
    type: Boolean,
    default: false
  }
});
</script>

<template>
  <article class="rapport-stats-block" :class="{ 'rapport-stats-block--embedded': embedded }">
    <header class="rapport-stats-block__header">
      <p v-if="block.eyebrow" class="rapport-stats-block__eyebrow">{{ reportText(block.eyebrow) }}</p>
      <div class="rapport-stats-block__title-row">
        <span
          v-if="sourceTag"
          class="rapport-source-tag"
          :class="sourceKind ? `rapport-source-tag--${sourceKind}` : ''"
        >
          {{ reportText(sourceTag) }}
        </span>
        <h3>{{ reportText(block.title) }}</h3>
      </div>
      <InlineRichText v-if="block.description" class="rapport-stats-block__description" :text="block.description" />
    </header>

    <div v-if="block.metrics?.length" class="rapport-stats-block__metrics">
      <article v-for="metric in block.metrics" :key="metric.label" class="rapport-stats-metric">
        <p class="rapport-stats-metric__value">{{ reportText(metric.value) }}</p>
        <p class="rapport-stats-metric__label">{{ reportText(metric.label) }}</p>
      </article>
    </div>

    <ul v-if="block.insights?.length" class="rapport-note-list rapport-note-list--compact rapport-stats-block__insights">
      <li v-for="insight in block.insights" :key="insight">
        <InlineRichText :text="insight" tag="span" />
      </li>
    </ul>

    <StatChart
      v-if="block.chartOption"
      :option="block.chartOption"
      :height="400"
      :aria-label="reportText(block.chartLabel || block.title)"
    />
  </article>
</template>