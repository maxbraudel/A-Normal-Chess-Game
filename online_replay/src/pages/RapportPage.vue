<script setup>
import { computed, nextTick, onBeforeUnmount, onMounted, ref } from "vue";

import InlineRichText from "../components/InlineRichText.vue";
import MathFormula from "../components/MathFormula.vue";
import RapportLawSection from "../components/RapportLawSection.vue";
import { randomnessReport } from "../content/randomnessReportContent.js";
import { reportText } from "../utils/reportText.js";

const activeTocId = ref("cadre");

const numberedLawSections = computed(() =>
  randomnessReport.lawSections.map((section, index) => ({
    ...section,
    number: `4.${index + 1}`
  }))
);

const tocItems = computed(() => [
  { id: "cadre", number: "1", label: "Cadre probabiliste" },
  { id: "sorties", number: "2", label: "Sorties statistiques" },
  { id: "patterns", number: "3", label: "Schémas de simulation" },
  ...numberedLawSections.value.map((section) => ({
    id: section.id,
    number: section.number,
    label: reportText(section.title)
  })),
  { id: "dependances", number: "5", label: "Dépendances" },
  { id: "difficultes", number: "6", label: "Difficultés" },
  { id: "perspectives", number: "7", label: "Perspectives" }
]);

let sectionObserver;

onMounted(async () => {
  await nextTick();

  sectionObserver = new IntersectionObserver(
    (entries) => {
      const visibleEntries = entries
        .filter((entry) => entry.isIntersecting)
        .sort((left, right) => left.boundingClientRect.top - right.boundingClientRect.top);

      if (visibleEntries.length > 0) {
        activeTocId.value = visibleEntries[0].target.id;
      }
    },
    {
      rootMargin: "-18% 0px -64% 0px",
      threshold: [0, 0.15, 0.4, 0.7]
    }
  );

  for (const item of tocItems.value) {
    const element = document.getElementById(item.id);
    if (element) {
      sectionObserver.observe(element);
    }
  }
});

onBeforeUnmount(() => {
  sectionObserver?.disconnect();
});
</script>

<template>
  <main class="rapport-page">
    <section class="rapport-hero">
      <div class="rapport-hero__copy">
        <p class="landing-kicker">{{ reportText(randomnessReport.hero.kicker) }}</p>
        <h1>{{ reportText(randomnessReport.hero.title) }}</h1>
        <InlineRichText class="landing-lead" :text="randomnessReport.hero.lead" />
        <InlineRichText class="rapport-hero__source" :text="randomnessReport.hero.source" />
      </div>

      <div class="rapport-summary-grid">
        <article v-for="stat in randomnessReport.summaryStats" :key="stat.label" class="rapport-summary-card">
          <p class="rapport-summary-card__value">{{ stat.value }}</p>
          <p class="rapport-summary-card__label">{{ reportText(stat.label) }}</p>
          <p class="rapport-summary-card__detail">{{ reportText(stat.detail) }}</p>
        </article>
      </div>
    </section>

    <div class="rapport-layout">
      <aside class="rapport-toc">
        <div class="rapport-toc__inner">
          <p class="rapport-toc__eyebrow">Table des matières</p>
          <a
            v-for="item in tocItems"
            :key="item.id"
            class="rapport-toc__link"
            :class="{ 'rapport-toc__link--active': activeTocId === item.id }"
            :href="`#${item.id}`"
          >
            <span class="rapport-toc__number">{{ item.number }}</span>
            <span class="rapport-toc__text">{{ item.label }}</span>
          </a>
        </div>
      </aside>

      <div class="rapport-content">
        <section id="cadre" class="rapport-panel">
          <header class="rapport-section__header">
            <p class="rapport-panel__eyebrow">Cadre</p>
            <h2>
              <span class="rapport-section__number">1.</span>
              Lecture probabiliste du runtime
            </h2>
          </header>

          <div class="rapport-richtext">
            <InlineRichText
              v-for="paragraph in randomnessReport.methodology.paragraphs"
              :key="paragraph"
              :text="paragraph"
            />
          </div>

          <div class="rapport-formula-grid">
            <article v-for="formula in randomnessReport.methodology.formulas" :key="formula.label" class="rapport-formula-card">
              <p class="rapport-formula-card__label">{{ reportText(formula.label) }}</p>
              <MathFormula :formula="formula.latex" :display="true" />
            </article>
          </div>

          <ul class="rapport-note-list">
            <li v-for="highlight in randomnessReport.methodology.highlights" :key="highlight">
              <InlineRichText :text="highlight" tag="span" />
            </li>
          </ul>
        </section>

        <section id="sorties" class="rapport-panel">
          <header class="rapport-section__header">
            <p class="rapport-panel__eyebrow">Observation</p>
            <h2>
              <span class="rapport-section__number">2.</span>
              Sorties statistiques déjà disponibles
            </h2>
          </header>

          <div class="rapport-output-grid">
            <article v-for="output in randomnessReport.outputStats" :key="output.title" class="rapport-output-card">
              <h3>{{ reportText(output.title) }}</h3>
              <InlineRichText :text="output.text" />
              <ul class="rapport-note-list rapport-note-list--compact">
                <li v-for="bullet in output.bullets" :key="bullet">
                  <InlineRichText :text="bullet" tag="span" />
                </li>
              </ul>
            </article>
          </div>
        </section>

        <section id="patterns" class="rapport-panel">
          <header class="rapport-section__header">
            <p class="rapport-panel__eyebrow">Simulation</p>
            <h2>
              <span class="rapport-section__number">3.</span>
              Schémas de simulation récurrente
            </h2>
          </header>

          <div class="rapport-code-grid">
            <article v-for="pattern in randomnessReport.codePatterns" :key="pattern.title" class="rapport-code-card">
              <h3>{{ reportText(pattern.title) }}</h3>
              <InlineRichText :text="pattern.description" />
              <pre class="rapport-code"><code>{{ pattern.code }}</code></pre>
            </article>
          </div>
        </section>

        <RapportLawSection
          v-for="section in numberedLawSections"
          :key="section.id"
          :section="section"
          :section-number="section.number"
        />

        <section id="dependances" class="rapport-panel">
          <header class="rapport-section__header">
            <p class="rapport-panel__eyebrow">Dépendances</p>
            <h2>
              <span class="rapport-section__number">5.</span>
              Structures de corrélation et de dépendance
            </h2>
          </header>

          <ul class="rapport-note-list">
            <li v-for="note in randomnessReport.dependenceNotes" :key="note">
              <InlineRichText :text="note" tag="span" />
            </li>
          </ul>
        </section>

        <section id="difficultes" class="rapport-panel">
          <header class="rapport-section__header">
            <p class="rapport-panel__eyebrow">Analyse</p>
            <h2>
              <span class="rapport-section__number">6.</span>
              Difficultés mathématiques réelles
            </h2>
          </header>

          <div class="rapport-output-grid">
            <article v-for="difficulty in randomnessReport.difficulties" :key="difficulty.title" class="rapport-output-card">
              <h3>{{ reportText(difficulty.title) }}</h3>
              <InlineRichText :text="difficulty.text" />
            </article>
          </div>
        </section>

        <section id="perspectives" class="rapport-panel">
          <header class="rapport-section__header">
            <p class="rapport-panel__eyebrow">Suite</p>
            <h2>
              <span class="rapport-section__number">7.</span>
              Perspectives de mesure et d'amélioration
            </h2>
          </header>

          <ul class="rapport-note-list">
            <li v-for="perspective in randomnessReport.perspectives" :key="perspective">
              <InlineRichText :text="perspective" tag="span" />
            </li>
          </ul>
        </section>
      </div>
    </div>
  </main>
</template>