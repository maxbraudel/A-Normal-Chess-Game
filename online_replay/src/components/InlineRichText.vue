<script setup>
import { computed } from "vue";

import { reportText } from "../utils/reportText.js";

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

const rendered = computed(() => {
  const chunks = props.text.split(/`([^`]+)`/g);
  return chunks
    .map((chunk, index) => {
      if (index % 2 === 1) {
        const escaped = escapeHtml(chunk);
        return `<code class="inline-token">${escaped}</code>`;
      }

      return escapeHtml(reportText(chunk));
    })
    .join("");
});
</script>

<template>
  <component :is="tag" class="inline-rich-text" v-html="rendered" />
</template>