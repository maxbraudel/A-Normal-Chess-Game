<script setup>
import hljs from "highlight.js/lib/core";
import cpp from "highlight.js/lib/languages/cpp";
import "highlight.js/styles/atom-one-dark.css";
hljs.registerLanguage("cpp", cpp);

import IllustrationMap from "./IllustrationMap.vue";
import InlineRichText from "./InlineRichText.vue";
import MathFormula from "./MathFormula.vue";
import RapportStatsBlock from "./RapportStatsBlock.vue";

function highlightCpp(code) {
  return hljs.highlight(code, { language: "cpp" }).value;
}

function getObservedSourceMeta(label) {
  const normalizedLabel = label || "";
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

const FULL_WIDTH_THEORY_LAYOUTS = Object.freeze({
  "champ spatial de l'eau": ["expectation", "variance"],
  "champ spatial de la terre": ["expectation", "variance"],
  "position d'entree le long du bord d'un brouillard": ["support", "law"],
  "densite locale d'un brouillard": ["support", "law"],
  "montant d'or d'un coffre": ["support", "law", "expectation", "variance"],
  "recompenses d'xp": ["expectation", "variance"]
});

function normalizeProcessTitle(title) {
  return (title || "")
    .normalize("NFD")
    .replace(/\p{Diacritic}/gu, "")
    .trim()
    .toLowerCase();
}

function getTheoryItemClasses(title, slot) {
  const fullWidthSlots = FULL_WIDTH_THEORY_LAYOUTS[normalizeProcessTitle(title)] || [];

  return [
    "rapport-process-card__theory-item",
    fullWidthSlots.includes(slot) ? "rapport-process-card__theory-item--full" : ""
  ];
}

function resolveRandomnessKind(itemKind, isDensity, defaultKind) {
  if (itemKind) {
    return itemKind;
  }

  if (isDensity) {
    return "density";
  }

  return defaultKind || "";
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

function getRelatedProcessKey(entry, index) {
  if (!entry) {
    return `related-${index}`;
  }

  return entry.title || entry.href || `related-${index}`;
}

defineProps({
  item: {
    type: Object,
    required: true
  },
  processAnchorId: {
    type: String,
    default: ""
  },
  defaultRandomnessKind: {
    type: String,
    default: ""
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
    default: "Données observées"
  }
});
</script>

<template>
  <article :id="processAnchorId || undefined" class="rapport-process-card">
    <header class="rapport-process-card__header">
      <div>
        <p class="rapport-process-card__system">{{ item.system }}</p>
        <h3 class="rapport-process-card__title">
          <span>{{ item.title }}</span>
          <span
            v-if="resolveRandomnessKind(item.randomnessKind, item.isDensity, defaultRandomnessKind)"
            :class="getRandomnessBadgeClasses(resolveRandomnessKind(item.randomnessKind, item.isDensity, defaultRandomnessKind))"
          >
            {{ getRandomnessBadgeLabel(resolveRandomnessKind(item.randomnessKind, item.isDensity, defaultRandomnessKind)) }}
          </span>
        </h3>
      </div>
      <p class="rapport-process-card__law">{{ item.lawUse }}</p>
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
      <span class="rapport-process-card__label">Cadre théorique explicite</span>
      <div class="rapport-process-card__theory-grid">
        <article :class="getTheoryItemClasses(item.title, 'support')">
          <p class="rapport-process-card__theory-label">Support / espace d'état</p>
          <MathFormula :formula="item.theory.support" :display="true" />
        </article>
        <article :class="getTheoryItemClasses(item.title, 'law')">
          <p class="rapport-process-card__theory-label">Formule de loi</p>
          <MathFormula :formula="item.theory.law" :display="true" />
        </article>
        <article :class="getTheoryItemClasses(item.title, 'expectation')">
          <p class="rapport-process-card__theory-label">Espérance</p>
          <MathFormula :formula="item.theory.expectation" :display="true" />
        </article>
        <article :class="getTheoryItemClasses(item.title, 'variance')">
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

    <div v-if="item.codeSnippet" class="rapport-process-card__field">
      <span class="rapport-process-card__label">Extrait de code (C++)</span>
      <pre class="rapport-process-card__code"><code class="hljs language-cpp" v-html="highlightCpp(item.codeSnippet)"></code></pre>
    </div>

    <div v-if="item.simulationFromUniform" class="rapport-process-card__field">
      <span class="rapport-process-card__label">Construction depuis U[0,1]</span>
      <InlineRichText :text="item.simulationFromUniform" />
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

    <div v-if="item.relatedProcesses?.length" class="rapport-process-card__field">
      <span class="rapport-process-card__label">Processus alimentés</span>
      <ul class="rapport-process-card__list">
        <li v-for="(relatedProcess, index) in item.relatedProcesses" :key="getRelatedProcessKey(relatedProcess, index)">
          <span>{{ relatedProcess.seedLabel }} :</span>
          <a :href="relatedProcess.href">{{ relatedProcess.title }}</a>
          <span v-if="relatedProcess.summary">{{ ` — ${relatedProcess.summary}` }}</span>
        </li>
      </ul>
    </div>

    <div v-if="item.illustration" class="rapport-process-card__field rapport-process-card__field--illustration">
      <span class="rapport-process-card__label">Illustration</span>
      <div class="rapport-process-illustration">
        <IllustrationMap v-bind="item.illustration" />
      </div>
      <InlineRichText
        v-if="item.illustration.description"
        class="rapport-process-illustration__description"
        :text="item.illustration.description"
      />
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