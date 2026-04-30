<script setup>
import InlineRichText from "./InlineRichText.vue";
import StatChart from "./StatChart.vue";

defineProps({
  block: {
    type: Object,
    required: true
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
      <p v-if="block.eyebrow" class="rapport-stats-block__eyebrow">{{ block.eyebrow }}</p>
      <h3>{{ block.title }}</h3>
      <InlineRichText v-if="block.description" class="rapport-stats-block__description" :text="block.description" />
    </header>

    <div v-if="block.metrics?.length" class="rapport-stats-block__metrics">
      <article v-for="metric in block.metrics" :key="metric.label" class="rapport-stats-metric">
        <p class="rapport-stats-metric__value">{{ metric.value }}</p>
        <p class="rapport-stats-metric__label">{{ metric.label }}</p>
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
      :height="block.chartHeight || 320"
      :aria-label="block.chartLabel || block.title"
    />
  </article>
</template>