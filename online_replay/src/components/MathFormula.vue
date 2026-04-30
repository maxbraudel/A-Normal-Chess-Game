<script setup>
import { computed } from "vue";
import katex from "katex";

const props = defineProps({
  formula: {
    type: String,
    required: true
  },
  display: {
    type: Boolean,
    default: false
  },
  tag: {
    type: String,
    default: ""
  }
});

const rootTag = computed(() => props.tag || (props.display ? "div" : "span"));

const renderedMath = computed(() =>
  katex.renderToString(props.formula, {
    displayMode: props.display,
    throwOnError: false,
    strict: "ignore"
  })
);
</script>

<template>
  <component
    :is="rootTag"
    class="math-formula"
    :class="{ 'math-formula--display': display }"
    v-html="renderedMath"
  />
</template>