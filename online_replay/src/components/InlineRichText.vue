<script setup>
import { computed } from "vue";

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
      const escaped = escapeHtml(chunk);
      if (index % 2 === 1) {
        return `<code class="inline-token">${escaped}</code>`;
      }

      return escaped;
    })
    .join("");
});
</script>

<template>
  <component :is="tag" class="inline-rich-text" v-html="rendered" />
</template>