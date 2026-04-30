<script setup>
const props = defineProps({
  shieldSrc: {
    type: String,
    required: true
  },
  shieldAlt: {
    type: String,
    default: ""
  },
  label: {
    type: String,
    required: true
  },
  message: {
    type: String,
    required: true
  },
  tone: {
    type: String,
    default: "warning",
    validator(value) {
      return ["danger", "neutral", "success", "warning"].includes(value);
    }
  }
});
</script>

<template>
  <article :class="['replay-toast', `replay-toast--${props.tone}`]">
    <img class="turn-indicator-icon" :src="props.shieldSrc" :alt="props.shieldAlt">
    <div class="overlay-meta replay-toast__meta">
      <span class="overlay-label">{{ props.label }}</span>
      <strong class="overlay-value">{{ props.message }}</strong>
    </div>
  </article>
</template>