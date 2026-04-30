import { createApp } from "vue";
import "katex/dist/katex.min.css";
import "../styles.css";
import "./theme.css";
import App from "./App.vue";
import router from "./router/index.js";

createApp(App).use(router).mount("#app");