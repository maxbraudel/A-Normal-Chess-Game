<script setup>
import { computed, nextTick, onBeforeUnmount, onMounted, ref, watch } from "vue";

import { mountReplayViewer } from "../../app.js";
import ReplayToast from "./ReplayToast.vue";
import { registerViewerVisibility, unregisterViewerVisibility } from "../utils/viewerVisibilityController.js";

const PERSPECTIVE_KINGDOM_KEYS = Object.freeze(["white", "black"]);

const props = defineProps({
  replayData: {
    type: Object,
    required: true
  },
  assetRoot: {
    type: String,
    default: undefined
  },
  masterConfigUrl: {
    type: String,
    default: undefined
  },
  ariaLabel: {
    type: String,
    default: "Illustration statique du plateau"
  },
  autoplayIntervalMs: {
    type: Number,
    default: undefined
  },
  autoplayOnMount: {
    type: Boolean,
    default: false
  },
  loopPlayback: {
    type: Boolean,
    default: false
  },
  toastCooldownMs: {
    type: Number,
    default: 0
  },
  enablePerspective: {
    type: Boolean,
    default: false
  },
  perspectiveKingdom: {
    type: String,
    default: "white",
    validator(value) {
      return ["white", "black"].includes(value);
    }
  },
  perspectiveSequence: {
    type: Array,
    default: undefined
  },
  perspectiveSwapIntervalMs: {
    type: Number,
    default: 2000
  },
  showPerspectiveOverlay: {
    type: Boolean,
    default: false
  },
  showToasts: {
    type: Boolean,
    default: false
  },
  statusOverlay: {
    type: Object,
    default: undefined
  }
});

const viewerRoot = ref(null);
const toastItems = ref([]);
const activePerspectiveIndex = ref(0);
const currentFrameIndex = ref(0);
let viewerInstance = null;
let perspectiveTimer = null;

const normalizedPerspectiveSequence = computed(() => {
  const rawSequence = Array.isArray(props.perspectiveSequence) && props.perspectiveSequence.length
    ? props.perspectiveSequence
    : [props.perspectiveKingdom];
  const sequence = rawSequence.filter((value) => PERSPECTIVE_KINGDOM_KEYS.includes(value));

  return sequence.length ? Array.from(new Set(sequence)) : [props.perspectiveKingdom];
});

const activePerspectiveKingdom = computed(() => {
  const sequence = normalizedPerspectiveSequence.value;
  const index = Math.min(activePerspectiveIndex.value, sequence.length - 1);
  return sequence[Math.max(0, index)] || props.perspectiveKingdom;
});

const perspectiveSequenceKey = computed(() => normalizedPerspectiveSequence.value.join("|"));

const rootClasses = computed(() => ({
  "replay-root--hide-timeline": true,
  "replay-root--hide-turn-overlay": true
}));

const vignetteClasses = computed(() => ({
  "static-board-vignette--show-perspective": props.showPerspectiveOverlay,
  "static-board-vignette--show-toasts": props.showToasts,
  "static-board-vignette--show-status-overlay": Boolean(props.statusOverlay)
}));

const customStatusOverlay = computed(() => {
  if (!props.statusOverlay || typeof props.statusOverlay !== "object") {
    return null;
  }

  const kingdomKey = props.statusOverlay.kingdom === "black" ? "black" : "white";
  const values = Array.isArray(props.statusOverlay.values) ? props.statusOverlay.values : [];
  const fallbackValue = props.statusOverlay.initialValue ?? null;
  const index = Math.min(currentFrameIndex.value, Math.max(0, values.length - 1));
  const value = values.length ? values[index] : fallbackValue;
  const assetRoot = props.assetRoot || "/assets";
  const showShield = props.statusOverlay.showShield !== false;

  return {
    label: typeof props.statusOverlay.label === "string" ? props.statusOverlay.label : "Statut",
    value: value ?? "-",
    showShield,
    shieldAlt: kingdomKey === "black" ? "Bouclier Noirs" : "Bouclier Blancs",
    shieldSrc: showShield
      ? `${assetRoot}/textures/ui/${kingdomKey === "black" ? "shield_black" : "shield_white"}.png`
      : null
  };
});

function handleToastStateChange(nextToasts) {
  toastItems.value = props.showToasts && Array.isArray(nextToasts) ? nextToasts : [];
}

function handleFrameStateChange(nextFrameState) {
  currentFrameIndex.value = Number.isFinite(nextFrameState && nextFrameState.frameIndex)
    ? Math.max(0, Math.trunc(nextFrameState.frameIndex))
    : 0;
}

function buildMountOptions() {
  const options = {
    replayData: props.replayData,
    autoplayOnMount: props.autoplayOnMount,
    loopPlayback: props.loopPlayback,
    interactionEnabled: false,
    perspectiveEnabled: props.enablePerspective,
    perspectiveKingdom: activePerspectiveKingdom.value,
    onToastStateChange: handleToastStateChange,
    onFrameStateChange: handleFrameStateChange
  };

  if (typeof props.autoplayIntervalMs === "number") {
    options.autoplayIntervalMs = props.autoplayIntervalMs;
  }

  if (typeof props.toastCooldownMs === "number") {
    options.toastCooldownMs = props.toastCooldownMs;
  }

  if (props.assetRoot) {
    options.assetRoot = props.assetRoot;
  }

  if (props.masterConfigUrl) {
    options.masterConfigUrl = props.masterConfigUrl;
  }

  return options;
}

function destroyViewer() {
  if (viewerRoot.value) {
    unregisterViewerVisibility(viewerRoot.value);
  }

  if (!viewerInstance) {
    toastItems.value = [];
    currentFrameIndex.value = 0;
    return;
  }

  viewerInstance.destroy();
  viewerInstance = null;
  toastItems.value = [];
  currentFrameIndex.value = 0;
}

function stopPerspectiveTimer() {
  if (perspectiveTimer !== null) {
    window.clearInterval(perspectiveTimer);
    perspectiveTimer = null;
  }
}

function startPerspectiveTimer() {
  stopPerspectiveTimer();

  if (!props.enablePerspective || normalizedPerspectiveSequence.value.length < 2) {
    return;
  }

  const intervalMs = Math.max(250, Number(props.perspectiveSwapIntervalMs) || 0);
  perspectiveTimer = window.setInterval(() => {
    activePerspectiveIndex.value = (activePerspectiveIndex.value + 1) % normalizedPerspectiveSequence.value.length;
  }, intervalMs);
}

function mountViewer() {
  if (!viewerRoot.value) {
    return;
  }

  viewerInstance = mountReplayViewer(viewerRoot.value, buildMountOptions());
  registerViewerVisibility(viewerRoot.value, viewerInstance);
}

onMounted(() => {
  mountViewer();
  startPerspectiveTimer();
});

onBeforeUnmount(() => {
  stopPerspectiveTimer();
  destroyViewer();
});

watch(
  () => [
    props.replayData,
    props.assetRoot,
    props.masterConfigUrl,
    props.autoplayIntervalMs,
    props.autoplayOnMount,
    props.loopPlayback,
    props.toastCooldownMs,
    props.enablePerspective
  ],
  async () => {
    destroyViewer();
    await nextTick();
    mountViewer();
  }
);

watch(
  () => [
    props.enablePerspective,
    props.perspectiveKingdom,
    props.perspectiveSwapIntervalMs,
    perspectiveSequenceKey.value
  ],
  () => {
    activePerspectiveIndex.value = 0;
    startPerspectiveTimer();
  }
);

watch(
  () => activePerspectiveKingdom.value,
  (nextPerspectiveKingdom) => {
    if (!viewerInstance || !props.enablePerspective || typeof viewerInstance.setPerspectiveKingdom !== "function") {
      return;
    }

    viewerInstance.setPerspectiveKingdom(nextPerspectiveKingdom);
  }
);
</script>

<template>
  <div :class="['static-board-vignette', vignetteClasses]">
    <div
      ref="viewerRoot"
      :class="['replay-root', 'static-board-vignette__viewer', rootClasses]"
    >
      <canvas class="replay-canvas" data-replay-ref="replayCanvas" :aria-label="ariaLabel"></canvas>

      <div class="status-overlay-group status-overlay-group-left">
        <div class="status-overlay" data-replay-ref="perspectiveOverlay" hidden>
          <img class="turn-indicator-icon" data-replay-ref="perspectiveKingdomShield" alt="">
          <div class="overlay-meta">
            <span class="overlay-label">Point de vue</span>
            <strong class="overlay-value" data-replay-ref="perspectiveKingdomValue">-</strong>
          </div>
        </div>

        <div class="status-overlay" data-replay-ref="activeTurnOverlay" hidden>
          <img class="turn-indicator-icon" data-replay-ref="activeKingdomShield" alt="">
          <div class="overlay-meta">
            <span class="overlay-label" data-replay-ref="activeKingdomLabel">-</span>
            <strong class="overlay-value" data-replay-ref="activeTurnValue">Tour 0</strong>
          </div>
        </div>
      </div>

      <div v-if="customStatusOverlay" class="status-overlay-group status-overlay-group-left static-board-vignette__custom-overlay-group">
        <div class="status-overlay static-board-vignette__custom-overlay">
          <img
            v-if="customStatusOverlay.showShield"
            class="turn-indicator-icon"
            :src="customStatusOverlay.shieldSrc"
            :alt="customStatusOverlay.shieldAlt"
          >
          <div class="overlay-meta">
            <span class="overlay-label">{{ customStatusOverlay.label }}</span>
            <strong class="overlay-value">{{ customStatusOverlay.value }}</strong>
          </div>
        </div>
      </div>

      <TransitionGroup
        v-if="props.showToasts"
        name="replay-toast-list"
        tag="div"
        class="replay-toast-stack"
        aria-live="polite"
      >
        <ReplayToast
          v-for="toast in toastItems"
          :key="toast.id"
          :shield-src="toast.shieldSrc"
          :shield-alt="toast.shieldAlt"
          :label="toast.label"
          :message="toast.message"
          :tone="toast.tone"
        />
      </TransitionGroup>

      <div class="zoom-controls" aria-label="Contrôles de zoom">
        <button
          type="button"
          class="action-button zoom-action-button"
          data-replay-ref="zoomInButton"
          aria-label="Zoom avant"
        >
          +
        </button>
        <button
          type="button"
          class="action-button zoom-action-button"
          data-replay-ref="zoomOutButton"
          aria-label="Zoom arriere"
        >
          -
        </button>
      </div>

      <div class="timeline-overlay" hidden>
        <div class="timeline-controls">
          <div class="playback-group">
            <button type="button" class="action-button" data-replay-ref="firstTurnButton">|&lt;</button>
            <button type="button" class="action-button" data-replay-ref="prevTurnButton">&lt;</button>
            <button type="button" class="action-button primary-action" data-replay-ref="playPauseButton">Lire</button>
            <button type="button" class="action-button" data-replay-ref="nextTurnButton">&gt;</button>
            <button type="button" class="action-button" data-replay-ref="lastTurnButton">&gt;|</button>
          </div>

          <input
            class="timeline-slider"
            type="range"
            data-replay-ref="turnSlider"
            min="0"
            max="0"
            step="1"
            value="0"
            aria-label="Tour de l'illustration"
          >
        </div>
      </div>
    </div>
  </div>
</template>