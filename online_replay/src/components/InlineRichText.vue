<script setup>
import { computed } from "vue";
import katex from "katex";

const props = defineProps({
  text: {
    type: String,
    required: true
  },
  tag: {
    type: String,
    default: "p"
  }
});

function escapeHtml(value) {
  return value
    .replaceAll("&", "&amp;")
    .replaceAll("<", "&lt;")
    .replaceAll(">", "&gt;")
    .replaceAll('"', "&quot;")
    .replaceAll("'", "&#39;");
}

function looksLikeCodeSnippet(value) {
  if (typeof value !== "string") {
    return false;
  }

  const trimmed = value.trim();
  if (!trimmed) {
    return false;
  }

  return /::|\.json\b|\b(?:true|false|null)\b|[A-Za-z]+(?:_[A-Za-z0-9]+){1,}|[A-Za-z][A-Za-z0-9_]*\.[A-Za-z_][A-Za-z0-9_.-]*/.test(trimmed);
}

function looksLikeSimpleMathIdentifier(value) {
  if (typeof value !== "string") {
    return false;
  }

  const trimmed = value.trim();
  if (!trimmed) {
    return false;
  }

  return /^[A-Za-z](?:_(?:[A-Za-z0-9]+|\{[^{}]+\})|\^(?:[A-Za-z0-9]+|\{[^{}]+\}))+$/u.test(trimmed);
}

function looksLikeStandaloneMath(value, options = {}) {
  const { codeChunk = false } = options;

  if (typeof value !== "string") {
    return false;
  }

  const trimmed = value.trim();
  if (!trimmed || trimmed.includes("`") || trimmed.includes("**")) {
    return false;
  }

  const hasExplicitLatexCommand = /\\[a-zA-Z]+/.test(trimmed);
  const hasLatexSignal = hasExplicitLatexCommand
    || /[_^]|\\\{|\\\}|\\left|\\right|\\frac|\\min|\\max|\\cdot|\\mid|\\mathcal|\\mathrm/.test(trimmed);

  if (!hasLatexSignal) {
    return false;
  }

  if (codeChunk && !hasExplicitLatexCommand && !looksLikeSimpleMathIdentifier(trimmed) && looksLikeCodeSnippet(trimmed)) {
    return false;
  }

  if (hasExplicitLatexCommand) {
    return true;
  }

  const alphaTokens = trimmed.match(/[A-Za-zÀ-ÿ]+/g) || [];
  const knownMathTokens = new Set([
    "alpha",
    "argmax",
    "argmin",
    "beta",
    "bigl",
    "bigr",
    "ceil",
    "clip",
    "dens",
    "delta",
    "dist",
    "dots",
    "d",
    "edge",
    "epsilon",
    "eta",
    "eq",
    "frac",
    "gamma",
    "inf",
    "kappa",
    "lambda",
    "left",
    "mathcal",
    "max",
    "mid",
    "min",
    "mu",
    "omega",
    "phi",
    "pi",
    "psi",
    "rho",
    "mathrm",
    "right",
    "round",
    "shape",
    "sigma",
    "sim",
    "sup",
    "tau",
    "theta",
    "text",
    "top"
  ]);
  const longPlainWords = alphaTokens.filter((token) => {
    const normalized = token.toLowerCase();
    return token.length > 5 && !knownMathTokens.has(normalized);
  });

  return longPlainWords.length === 0;
}

function renderMath(value, displayMode) {
  const renderedMath = katex.renderToString(value, {
    displayMode,
    throwOnError: false,
    strict: "ignore"
  });

  const className = displayMode
    ? "math-formula math-formula--display inline-rich-text__math"
    : "math-formula inline-rich-text__math";

  return `<span class="${className}">${renderedMath}</span>`;
}

function renderPlainChunk(value) {
  if (looksLikeStandaloneMath(value)) {
    return renderMath(value.trim(), true);
  }

  return escapeHtml(value).replace(/\*\*([\s\S]+?)\*\*/g, "<strong>$1</strong>");
}

const rendered = computed(() => {
  const chunks = props.text.split(/`([^`]+)`/g);
  return chunks
    .map((chunk, index) => {
      if (index % 2 === 1) {
        if (looksLikeStandaloneMath(chunk, { codeChunk: true })) {
          return renderMath(chunk.trim(), false);
        }

        const escaped = escapeHtml(chunk);
        return `<code class="inline-token">${escaped}</code>`;
      }

      return renderPlainChunk(chunk);
    })
    .join("");
});
</script>

<template>
  <component :is="tag" class="inline-rich-text" v-html="rendered" />
</template>