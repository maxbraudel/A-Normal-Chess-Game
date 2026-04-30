<script setup>
import InlineRichText from "./InlineRichText.vue";
import MathFormula from "./MathFormula.vue";
import RapportStatsBlock from "./RapportStatsBlock.vue";
import { reportText } from "../utils/reportText.js";

defineProps({
  item: {
    type: Object,
    required: true
  },
  observedData: {
    type: Array,
    default: () => []
  },
  observedDataLabel: {
    type: String,
    default: "Donnees observees"
  }
});
</script>

<template>
  <article class="rapport-process-card">
    <header class="rapport-process-card__header">
      <div>
        <p class="rapport-process-card__system">{{ reportText(item.system) }}</p>
        <h3>{{ reportText(item.title) }}</h3>
      </div>
      <p class="rapport-process-card__law">{{ reportText(item.lawUse) }}</p>
    </header>

    <div v-if="item.variable" class="rapport-process-card__field rapport-process-card__field--math">
      <span class="rapport-process-card__label">Variable</span>
      <MathFormula :formula="item.variable" :display="true" />
    </div>

    <div class="rapport-process-card__field">
      <span class="rapport-process-card__label">Phénomène</span>
      <InlineRichText :text="item.phenomenon" />
    </div>

    <div class="rapport-process-card__field">
      <span class="rapport-process-card__label">Pourquoi cette loi</span>
      <InlineRichText :text="item.why" />
    </div>

    <div class="rapport-process-card__field">
      <span class="rapport-process-card__label">Simulation</span>
      <InlineRichText :text="item.simulation" />
    </div>

    <div class="rapport-process-card__field">
      <span class="rapport-process-card__label">Choix des paramètres</span>
      <InlineRichText :text="item.parameterChoice" />
    </div>

    <div v-if="item.parameters?.length" class="rapport-process-card__field">
      <span class="rapport-process-card__label">Paramètres</span>
      <ul class="rapport-process-card__list">
        <li v-for="parameter in item.parameters" :key="parameter">
          <InlineRichText :text="parameter" tag="span" />
        </li>
      </ul>
    </div>

    <div class="rapport-process-card__field">
      <span class="rapport-process-card__label">Structure de dépendance</span>
      <InlineRichText :text="item.dependence" />
    </div>

    <div v-if="observedData.length" class="rapport-process-card__field">
      <span class="rapport-process-card__label">{{ reportText(observedDataLabel) }}</span>
      <div class="rapport-process-card__observed">
        <RapportStatsBlock v-for="block in observedData" :key="block.title" :block="block" embedded />
      </div>
    </div>
  </article>
</template>