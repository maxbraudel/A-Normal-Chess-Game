import { createRouter, createWebHistory } from "vue-router";

import RapportPage from "../pages/RapportPage.vue";

const router = createRouter({
  history: createWebHistory(),
  routes: [
    {
      path: "/",
      name: "rapport",
      component: RapportPage
    }
  ],
  scrollBehavior(to, from, savedPosition) {
    if (savedPosition) {
      return savedPosition;
    }

    if (to.hash) {
      return {
        el: to.hash,
        top: 92,
        behavior: "smooth"
      };
    }

    return {
      top: 0
    };
  }
});

export default router;