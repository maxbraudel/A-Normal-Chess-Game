<script setup>
import IllustrationMap from "./IllustrationMap.vue";
import InlineRichText from "./InlineRichText.vue";
import MathFormula from "./MathFormula.vue";
import RapportStatsBlock from "./RapportStatsBlock.vue";
import { reportText } from "../utils/reportText.js";

function getObservedSourceMeta(label) {
  const normalizedLabel = reportText(label || "");
  const loweredLabel = normalizedLabel.toLowerCase();
  const simulatedMatch = loweredLabel.match(/(\d+)\s+parties?\s+simul/);

  if (simulatedMatch) {
    return {
      kind: "simulated",
      tag: `${simulatedMatch[1]} parties simulées`
    };
  }

  if (loweredLabel.includes("partie réelle") || loweredLabel.includes("partie reelle")) {
    return {
      kind: "real",
      tag: "Partie réelle avec joueur"
    };
  }

  return {
    kind: "",
    tag: normalizedLabel || "Données observées"
  };
}

defineProps({
  item: {
    type: Object,
    required: true
  },
  observedSections: {
    type: Array,
    default: () => []
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

    <div v-if="item.theory" class="rapport-process-card__field rapport-process-card__field--theory">
      <span class="rapport-process-card__label">Cadre theorique explicite</span>
      <div class="rapport-process-card__theory-grid">
        <article class="rapport-process-card__theory-item">
          <p class="rapport-process-card__theory-label">Support / espace d'etat</p>
          <MathFormula :formula="item.theory.support" :display="true" />
        </article>
        <article class="rapport-process-card__theory-item">
          <p class="rapport-process-card__theory-label">Formule de loi</p>
          <MathFormula :formula="item.theory.law" :display="true" />
        </article>
        <article class="rapport-process-card__theory-item">
          <p class="rapport-process-card__theory-label">Esperance</p>
          <MathFormula :formula="item.theory.expectation" :display="true" />
        </article>
        <article class="rapport-process-card__theory-item">
          <p class="rapport-process-card__theory-label">Variance</p>
          <MathFormula :formula="item.theory.variance" :display="true" />
        </article>
      </div>
      <InlineRichText
        v-if="item.theory.note"
        class="rapport-process-card__theory-note"
        :text="item.theory.note"
      />
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

    <div v-if="item.illustration" class="rapport-process-card__field rapport-process-card__field--illustration">
      <span class="rapport-process-card__label">Illustration</span>
      <div class="rapport-process-illustration">
        <IllustrationMap v-bind="item.illustration" />
      </div>
    </div>

    <div
      v-if="observedSections.length || observedData.length"
      class="rapport-process-card__field"
    >
      <div
        v-for="section in (observedSections.length ? observedSections : [{ label: observedDataLabel, blocks: observedData }])"
        :key="section.label"
        class="rapport-process-card__observed-section"
      >
        <div class="rapport-process-card__observed">
          <RapportStatsBlock
            v-for="block in (section.blocks || [])"
            :key="`${section.label}-${block.title}`"
            :block="block"
            :source-kind="getObservedSourceMeta(section.label).kind"
            :source-tag="getObservedSourceMeta(section.label).tag"
            embedded
          />
        </div>
      </div>
    </div>
  </article>
</template>