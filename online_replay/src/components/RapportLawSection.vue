<script setup>
import InlineRichText from "./InlineRichText.vue";
import MathFormula from "./MathFormula.vue";
import RapportProcessCard from "./RapportProcessCard.vue";

defineProps({
  section: {
    type: Object,
    required: true
  }
});
</script>

<template>
  <section :id="section.id" class="rapport-section rapport-panel">
    <header class="rapport-section__header">
      <p class="rapport-panel__eyebrow">{{ section.badge }}</p>
      <h2>{{ section.title }}</h2>
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
      <RapportProcessCard v-for="item in section.processes" :key="item.title" :item="item" />
    </div>
  </section>
</template>