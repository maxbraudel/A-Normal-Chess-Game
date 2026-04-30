<script setup>
import InlineRichText from "../components/InlineRichText.vue";
import MathFormula from "../components/MathFormula.vue";
import RapportLawSection from "../components/RapportLawSection.vue";
import { randomnessReport } from "../content/randomnessReportContent.js";

const tocItems = [
  { id: "cadre", label: "Cadre probabiliste" },
  { id: "sorties", label: "Sorties statistiques" },
  { id: "patterns", label: "Schemas de simulation" },
  ...randomnessReport.lawSections.map((section) => ({
    id: section.id,
    label: section.title
  })),
  { id: "dependances", label: "Dependances" },
  { id: "difficultes", label: "Difficultes" },
  { id: "perspectives", label: "Perspectives" }
];
</script>

<template>
  <main class="rapport-page">
    <section class="rapport-hero">
      <div class="rapport-hero__copy">
        <p class="landing-kicker">{{ randomnessReport.hero.kicker }}</p>
        <h1>{{ randomnessReport.hero.title }}</h1>
        <InlineRichText class="landing-lead" :text="randomnessReport.hero.lead" />
        <InlineRichText class="rapport-hero__source" :text="randomnessReport.hero.source" />
      </div>

      <div class="rapport-summary-grid">
        <article v-for="stat in randomnessReport.summaryStats" :key="stat.label" class="rapport-summary-card">
          <p class="rapport-summary-card__value">{{ stat.value }}</p>
          <p class="rapport-summary-card__label">{{ stat.label }}</p>
          <p class="rapport-summary-card__detail">{{ stat.detail }}</p>
        </article>
      </div>
    </section>

    <div class="rapport-layout">
      <aside class="rapport-toc">
        <div class="rapport-toc__inner">
          <p class="rapport-toc__eyebrow">Table des matieres</p>
          <a v-for="item in tocItems" :key="item.id" class="rapport-toc__link" :href="`#${item.id}`">
            {{ item.label }}
          </a>
        </div>
      </aside>

      <div class="rapport-content">
        <section id="cadre" class="rapport-panel">
          <header class="rapport-section__header">
            <p class="rapport-panel__eyebrow">Cadre</p>
            <h2>Lecture probabiliste du runtime</h2>
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
              <p class="rapport-formula-card__label">{{ formula.label }}</p>
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
            <h2>Sorties statistiques deja disponibles</h2>
          </header>

          <div class="rapport-output-grid">
            <article v-for="output in randomnessReport.outputStats" :key="output.title" class="rapport-output-card">
              <h3>{{ output.title }}</h3>
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
            <h2>Schemas de simulation recurrente</h2>
          </header>

          <div class="rapport-code-grid">
            <article v-for="pattern in randomnessReport.codePatterns" :key="pattern.title" class="rapport-code-card">
              <h3>{{ pattern.title }}</h3>
              <InlineRichText :text="pattern.description" />
              <pre class="rapport-code"><code>{{ pattern.code }}</code></pre>
            </article>
          </div>
        </section>

        <RapportLawSection v-for="section in randomnessReport.lawSections" :key="section.id" :section="section" />

        <section id="dependances" class="rapport-panel">
          <header class="rapport-section__header">
            <p class="rapport-panel__eyebrow">Dependances</p>
            <h2>Structures de correlation et de dependance</h2>
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
            <h2>Difficultes mathematiques reelles</h2>
          </header>

          <div class="rapport-output-grid">
            <article v-for="difficulty in randomnessReport.difficulties" :key="difficulty.title" class="rapport-output-card">
              <h3>{{ difficulty.title }}</h3>
              <InlineRichText :text="difficulty.text" />
            </article>
          </div>
        </section>

        <section id="perspectives" class="rapport-panel">
          <header class="rapport-section__header">
            <p class="rapport-panel__eyebrow">Suite</p>
            <h2>Perspectives de mesure et d'amelioration</h2>
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