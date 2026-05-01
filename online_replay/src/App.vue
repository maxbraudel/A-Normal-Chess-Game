<script setup>
import { computed } from "vue";
import { RouterLink, RouterView, useRoute } from "vue-router";

const route = useRoute();
const isReportRoute = computed(() => route.matched.some((record) => record.meta.layout === "rapport"));

const navigationItems = [
  {
    label: "Rapport",
    to: "/"
  }
];
</script>

<template>
  <div class="site-shell" :class="{ 'site-shell--rapport': isReportRoute }">
    <header v-if="!isReportRoute" class="site-header">
      <div class="site-header__inner">
        <RouterLink class="site-brand" to="/">
          <span class="site-brand__eyebrow">ANCG</span>
          <strong class="site-brand__title">Online Replay</strong>
        </RouterLink>

        <nav class="site-nav" aria-label="Navigation principale">
          <RouterLink
            v-for="item in navigationItems"
            :key="item.to"
            :to="item.to"
            class="site-nav__link"
          >
            {{ item.label }}
          </RouterLink>
        </nav>
      </div>
    </header>

    <RouterView />
  </div>
</template>