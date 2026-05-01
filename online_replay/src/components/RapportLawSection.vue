<script setup>
import InlineRichText from "./InlineRichText.vue";
import MathFormula from "./MathFormula.vue";
import RapportProcessCard from "./RapportProcessCard.vue";

function resolveRandomnessKind(kind, isDensity) {
  if (kind) {
    return kind;
  }

  return isDensity ? "density" : "";
}

function getRandomnessBadgeLabel(kind) {
  if (kind === "density") {
    return "loi continue";
  }

  if (kind === "discrete") {
    return "loi discrète";
  }

  return "";
}

function getRandomnessBadgeClasses(kind) {
  return [
    "rapport-source-tag",
    "rapport-randomness-badge",
    kind ? `rapport-randomness-badge--${kind}` : ""
  ];
}

function getDefaultProcessRandomnessKind(section) {
  if (section.randomnessKind) {
    return section.randomnessKind;
  }

  if (section.id === "uniformes") {
    return "discrete";
  }

  return "";
}

defineProps({
  section: {
    type: Object,
    required: true
  },
  sectionNumber: {
    type: String,
    required: true
  },
  processStatsByTitle: {
    type: Object,
    default: () => ({})
  },
  observedSectionsByTitle: {
    type: Object,
    default: () => ({})
  },
  observedDataLabel: {
    type: String,
    default: "Données observées"
  }
});
</script>

<template>
  <section :id="section.id" class="rapport-section rapport-panel">
    <header class="rapport-section__header">
      <p class="rapport-panel__eyebrow">{{ section.badge }}</p>
      <h2 class="rapport-section__title">
        <span class="rapport-section__title-text">
          <span class="rapport-section__number">{{ sectionNumber }}</span>
          {{ section.title }}
        </span>
        <span
          v-if="resolveRandomnessKind(section.randomnessKind, section.isDensity)"
          :class="getRandomnessBadgeClasses(resolveRandomnessKind(section.randomnessKind, section.isDensity))"
        >
          {{ getRandomnessBadgeLabel(resolveRandomnessKind(section.randomnessKind, section.isDensity)) }}
        </span>
      </h2>
    </header>

    <div class="rapport-richtext">
      <InlineRichText v-for="paragraph in section.description" :key="paragraph" :text="paragraph" />
    </div>

    <div class="rapport-formula-grid">
      <article v-for="formula in section.formulaCards" :key="formula.label" class="rapport-formula-card">
        <p class="rapport-formula-card__label">{{ formula.label }}</p>
        <MathFormula :formula="formula.latex" :display="true" />
        <p v-if="formula.note" class="rapport-formula-card__note">{{ formula.note }}</p>
      </article>
    </div>

    <ul v-if="section.notes?.length" class="rapport-note-list">
      <li v-for="note in section.notes" :key="note">
        <InlineRichText :text="note" tag="span" />
      </li>
    </ul>

    <div class="rapport-process-grid">
      <RapportProcessCard
        v-for="item in section.processes"
        :key="item.title"
        :item="item"
        :default-randomness-kind="getDefaultProcessRandomnessKind(section)"
        :observed-sections="observedSectionsByTitle[item.title] || []"
        :observed-data="processStatsByTitle[item.title] || []"
        :observed-data-label="observedDataLabel"
      />
    </div>
  </section>
</template>