<script setup>
import InlineRichText from "./InlineRichText.vue";
import ReplayViewer from "./ReplayViewer.vue";
import StatChart from "./StatChart.vue";

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
      <p v-if="block.eyebrow" class="rapport-stats-block__eyebrow">{{ block.eyebrow }}</p>
      <div class="rapport-stats-block__title-row">
        <span
          v-if="sourceTag"
          class="rapport-source-tag"
          :class="sourceKind ? `rapport-source-tag--${sourceKind}` : ''"
        >
          {{ sourceTag }}
        </span>
        <h3>{{ block.title }}</h3>
      </div>
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
      :height="400"
      :aria-label="block.chartLabel || block.title"
    />

    <InlineRichText
      v-if="block.postChartInterpretation"
      class="rapport-stats-block__post-chart-interpretation"
      :text="block.postChartInterpretation"
    />

    <section v-if="block.exampleReplay" class="rapport-stats-example">
      <div
        v-if="block.exampleReplay.label || block.exampleReplay.sourceTag"
        class="rapport-stats-example__header"
      >
        <span
          v-if="block.exampleReplay.sourceTag"
          class="rapport-source-tag"
          :class="block.exampleReplay.sourceKind ? `rapport-source-tag--${block.exampleReplay.sourceKind}` : ''"
        >
          {{ block.exampleReplay.sourceTag }}
        </span>
        <p v-if="block.exampleReplay.label" class="rapport-stats-example__label">
          {{ block.exampleReplay.label }}
        </p>
      </div>
      <InlineRichText
        v-if="block.exampleReplay.description"
        class="rapport-stats-example__description"
        :text="block.exampleReplay.description"
      />
      <div class="rapport-replay-frame rapport-stats-example__frame">
        <ReplayViewer class="rapport-replay rapport-stats-example__viewer" v-bind="block.exampleReplay.viewer || {}" />
      </div>
    </section>
  </article>
</template>