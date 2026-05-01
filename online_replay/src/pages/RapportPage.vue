<script setup>
import { computed, nextTick, onBeforeUnmount, onMounted, ref } from "vue";

import InlineRichText from "../components/InlineRichText.vue";
import MathFormula from "../components/MathFormula.vue";
import RapportLawSection from "../components/RapportLawSection.vue";
import RapportStatsBlock from "../components/RapportStatsBlock.vue";
import ReplayViewer from "../components/ReplayViewer.vue";
import { randomnessReport } from "../content/randomnessReportContent.js";
import { loadRealGameStatsReport } from "../content/realGameStatsContent.js";
import { randomnessStatsReport } from "../content/randomnessStatsContent.js";
import { reportText } from "../utils/reportText.js";

const REPORT_INTRO_REPLAY = Object.freeze({
  autoplayOnMount: true,
  autoplayIntervalMs: 220,
  loopPlayback: true,
  toastCooldownMs: 2400,
  enablePerspective: true,
  perspectiveKingdom: "white"
});

const activeTocId = ref("cadre");
const realGameStatsReport = ref({
  observedDataLabel: "Donnees observees sur la partie reelle",
  processStatsByTitle: {}
});

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

const processStatsByTitle = randomnessStatsReport.processStatsByTitle;
const observedSectionsByTitle = computed(() =>
  Object.fromEntries(
    Array.from(new Set([
      ...Object.keys(randomnessStatsReport.processStatsByTitle),
      ...Object.keys(realGameStatsReport.value.processStatsByTitle || {})
    ])).map((title) => {
      const simulatedBlocks = randomnessStatsReport.processStatsByTitle[title] || [];
      const realBlocks = (realGameStatsReport.value.processStatsByTitle || {})[title] || [];

      return [
        title,
        [
          ...(simulatedBlocks.length
            ? [{ label: randomnessStatsReport.observedDataLabel, blocks: simulatedBlocks }]
            : []),
          ...(realBlocks.length
            ? [{ label: realGameStatsReport.value.observedDataLabel, blocks: realBlocks }]
            : [])
        ]
      ];
    })
  )
);

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

  try {
    realGameStatsReport.value = await loadRealGameStatsReport();
  } catch (error) {
    console.error("Impossible de charger les stats de la partie reelle pour le rapport.", error);
  }
});

onBeforeUnmount(() => {
  sectionObserver?.disconnect();
});
</script>

<template>
  <main class="rapport-page">
    <section class="rapport-hero">
      <div class="rapport-hero__copy rapport-hero__copy--full">
        <p v-if="randomnessReport.hero.kicker" class="landing-kicker">{{ reportText(randomnessReport.hero.kicker) }}</p>
        <h1>{{ reportText(randomnessReport.hero.title) }}</h1>
        <InlineRichText v-if="randomnessReport.hero.lead" class="landing-lead" :text="randomnessReport.hero.lead" />
        <InlineRichText v-if="randomnessReport.hero.source" class="rapport-hero__source" :text="randomnessReport.hero.source" />
      </div>
    </section>

    <section class="rapport-panel rapport-panel--intro">
      <header class="rapport-section__header">
        <p class="rapport-panel__eyebrow">Présentation</p>
        <h2>Introduction au jeu</h2>
      </header>

      <div class="rapport-richtext">
        <InlineRichText
          v-for="paragraph in randomnessReport.gameIntroduction.paragraphs"
          :key="paragraph"
          :text="paragraph"
        />
      </div>

      <div class="rapport-output-grid">
        <article v-for="section in randomnessReport.gameIntroduction.sections" :key="section.title" class="rapport-output-card">
          <h3>{{ reportText(section.title) }}</h3>
          <div class="rapport-richtext rapport-richtext--compact">
            <InlineRichText
              v-for="paragraph in section.paragraphs"
              :key="`${section.title}-${paragraph}`"
              :text="paragraph"
            />
          </div>
        </article>
      </div>
    </section>

    <section class="rapport-panel rapport-panel--intro">
      <header class="rapport-section__header">
        <p class="rapport-panel__eyebrow">Lecture stratégique</p>
        <h2>Lien avec les processus aléatoires</h2>
      </header>

      <div class="rapport-richtext">
        <InlineRichText
          v-for="paragraph in randomnessReport.randomnessLink.paragraphs"
          :key="paragraph"
          :text="paragraph"
        />
      </div>

      <div class="rapport-output-grid">
        <article v-for="section in randomnessReport.randomnessLink.sections" :key="section.title" class="rapport-output-card">
          <h3>{{ reportText(section.title) }}</h3>
          <InlineRichText :text="section.text" />
        </article>
      </div>

      <div class="rapport-subsection">
        <h3 class="rapport-subsection__title">{{ reportText(randomnessReport.randomnessLink.reportDimensionsTitle) }}</h3>

        <div class="rapport-output-grid">
          <article
            v-for="dimension in randomnessReport.randomnessLink.reportDimensions"
            :key="dimension.title"
            class="rapport-output-card"
          >
            <h3>{{ reportText(dimension.title) }}</h3>
            <InlineRichText :text="dimension.text" />
          </article>
        </div>

        <div class="rapport-summary-row" aria-label="Résumé des processus aléatoires">
          <article v-for="stat in randomnessReport.summaryStats" :key="stat.label" class="rapport-summary-card rapport-summary-card--inline">
            <p class="rapport-summary-card__value">{{ stat.value }}</p>
            <p class="rapport-summary-card__label">{{ reportText(stat.label) }}</p>
            <p class="rapport-summary-card__detail">{{ reportText(stat.detail) }}</p>
          </article>
        </div>
      </div>

      <div class="rapport-subsection rapport-subsection--replay">
        <h3 class="rapport-subsection__title">{{ reportText(randomnessReport.randomnessLink.replayTitle) }}</h3>
        <InlineRichText class="rapport-subsection__lead" :text="randomnessReport.randomnessLink.replayText" />

        <div class="rapport-replay-frame">
          <ReplayViewer class="rapport-replay" v-bind="REPORT_INTRO_REPLAY" />
        </div>
      </div>
    </section>

    <section class="rapport-panel rapport-panel--intro">
      <header class="rapport-section__header">
        <p class="rapport-panel__eyebrow">Rapport</p>
        <h2>Rapport des processus aléatoires</h2>
      </header>

      <div class="rapport-richtext">
        <InlineRichText
          v-for="paragraph in randomnessReport.reportPrelude.paragraphs"
          :key="paragraph"
          :text="paragraph"
        />
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

          <div class="rapport-stats-collection rapport-stats-collection--overview">
            <RapportStatsBlock
              v-for="block in randomnessStatsReport.overviewBlocks"
              :key="block.title"
              :block="block"
            />
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
          :process-stats-by-title="processStatsByTitle"
          :observed-sections-by-title="observedSectionsByTitle"
          :observed-data-label="randomnessStatsReport.observedDataLabel"
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