<script setup>
import { computed, nextTick, onBeforeUnmount, onMounted, ref, watch } from "vue";
import { mountReplayViewer } from "../../app.js";
import ReplayToast from "./ReplayToast.vue";
import { registerViewerVisibility, unregisterViewerVisibility } from "../utils/viewerVisibilityController.js";

const props = defineProps({
  replayData: {
    type: Object,
    default: null
  },
  replayUrl: {
    type: String,
    default: undefined
  },
  assetRoot: {
    type: String,
    default: undefined
  },
  masterConfigUrl: {
    type: String,
    default: undefined
  },
  autoplayIntervalMs: {
    type: Number,
    default: 1000
  },
  autoplayOnMount: {
    type: Boolean,
    default: true
  },
  toastCooldownMs: {
    type: Number,
    default: 0
  },
  loopPlayback: {
    type: Boolean,
    default: true
  },
  initialZoom: {
    type: Number,
    default: undefined
  },
  minTurn: {
    type: Number,
    default: undefined
  },
  maxTurn: {
    type: Number,
    default: undefined
  },
  initialTurn: {
    type: Number,
    default: undefined
  },
  enableCellDebug: {
    type: Boolean,
    default: false
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
  trackedTarget: {
    type: Object,
    default: null
  },
  showToasts: {
    type: Boolean,
    default: false
  },
  showStatusOverlay: {
    type: Boolean,
    default: false
  },
  statusOverlay: {
    type: Object,
    default: undefined
  },
  description: {
    type: String,
    default: undefined
  }
});

const viewerRoot = ref(null);
const toastItems = ref([]);
const currentFrameIndex = ref(0);
const showInteractionTooltip = ref(false);
let viewerInstance = null;

const interactionTooltipItems = computed(() => {
  const items = [
    { key: "Ctrl + molette", value: "Zoom" },
    { key: "Glisser", value: "Caméra" },
    { key: "Double-clic", value: "Recadrer" }
  ];

  if (props.enableCellDebug) {
    items.push({ key: "Clic", value: "Debug cellule" });
  }

  return items;
});

const customStatusOverlay = computed(() => {
  if (!props.statusOverlay || typeof props.statusOverlay !== "object") {
    return null;
  }

  const values = Array.isArray(props.statusOverlay.values) ? props.statusOverlay.values : [];
  const fallbackValue = props.statusOverlay.initialValue ?? null;
  const safeIndex = Math.min(currentFrameIndex.value, Math.max(0, values.length - 1));
  const value = values.length ? values[safeIndex] : fallbackValue;
  const kingdomKey = props.statusOverlay.kingdom === "black" ? "black" : "white";
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

const rootClasses = computed(() => ({
  "illustration-map--show-toasts": props.showToasts,
  "illustration-map--show-status-overlay": props.showStatusOverlay && !!customStatusOverlay.value
}));

const watchedProps = computed(() => [
  props.replayData,
  props.replayUrl,
  props.assetRoot,
  props.masterConfigUrl,
  props.autoplayIntervalMs,
  props.autoplayOnMount,
  props.toastCooldownMs,
  props.loopPlayback,
  props.initialZoom,
  props.minTurn,
  props.maxTurn,
  props.initialTurn,
  props.enableCellDebug,
  props.enablePerspective,
  props.perspectiveKingdom,
  props.trackedTarget,
  props.showToasts,
  props.showStatusOverlay,
  props.statusOverlay
]);

function buildMountOptions() {
  const options = {
    autoplayOnMount: props.autoplayOnMount,
    loopPlayback: props.loopPlayback,
    enableCellDebug: props.enableCellDebug,
    perspectiveEnabled: props.enablePerspective,
    perspectiveKingdom: props.perspectiveKingdom,
    trackedTarget: props.trackedTarget,
    recenterTrackedTargetOnFrameChange: false,
    onToastStateChange: handleToastStateChange,
    onFrameStateChange: handleFrameStateChange
  };

  if (props.replayData) {
    options.replayData = props.replayData;
  }
  if (props.replayUrl) {
    options.replayUrl = props.replayUrl;
  }
  if (props.assetRoot) {
    options.assetRoot = props.assetRoot;
  }
  if (props.masterConfigUrl) {
    options.masterConfigUrl = props.masterConfigUrl;
  }
  if (typeof props.autoplayIntervalMs === "number") {
    options.autoplayIntervalMs = props.autoplayIntervalMs;
  }
  if (typeof props.toastCooldownMs === "number") {
    options.toastCooldownMs = props.toastCooldownMs;
  }
  if (typeof props.initialZoom === "number") {
    options.initialZoom = props.initialZoom;
  }
  if (typeof props.minTurn === "number") {
    options.minTurn = props.minTurn;
  }
  if (typeof props.maxTurn === "number") {
    options.maxTurn = props.maxTurn;
  }
  if (typeof props.initialTurn === "number") {
    options.initialTurn = props.initialTurn;
  }

  return options;
}

function handleToastStateChange(nextToasts) {
  toastItems.value = props.showToasts && Array.isArray(nextToasts) ? nextToasts : [];
}

function handleFrameStateChange(nextFrameState) {
  currentFrameIndex.value = Number.isFinite(nextFrameState && nextFrameState.frameIndex)
    ? Math.max(0, Math.trunc(nextFrameState.frameIndex))
    : 0;
}

function showReplayTooltip() {
  showInteractionTooltip.value = true;
}

function hideReplayTooltip() {
  showInteractionTooltip.value = false;
}

function destroyViewer() {
  if (viewerRoot.value) {
    unregisterViewerVisibility(viewerRoot.value);
  }

  if (!viewerInstance) {
    toastItems.value = [];
    currentFrameIndex.value = 0;
    showInteractionTooltip.value = false;
    return;
  }

  viewerInstance.destroy();
  viewerInstance = null;
  toastItems.value = [];
  currentFrameIndex.value = 0;
  showInteractionTooltip.value = false;
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
});

onBeforeUnmount(() => {
  destroyViewer();
});

watch(watchedProps, async () => {
  destroyViewer();
  await nextTick();
  mountViewer();
});
</script>

<template>
  <div :class="['illustration-map', rootClasses]">
    <div
      ref="viewerRoot"
      class="replay-root replay-root--hide-timeline replay-root--hide-turn-overlay illustration-map__viewer"
    >
      <canvas
        class="replay-canvas"
        data-replay-ref="replayCanvas"
        aria-label="Carte d'illustration"
        @pointerenter="showReplayTooltip"
        @pointerleave="hideReplayTooltip"
      ></canvas>

      <div
        :class="[
          'replay-help-tooltip',
          {
            'replay-help-tooltip--visible': showInteractionTooltip
          }
        ]"
        aria-hidden="true"
      >
        <div
          v-for="item in interactionTooltipItems"
          :key="item.key"
          class="replay-help-tooltip__row"
        >
          <span class="replay-help-tooltip__key">{{ item.key }}</span>
          <span class="replay-help-tooltip__value">{{ item.value }}</span>
        </div>
      </div>

      <div class="status-overlay-group status-overlay-group-left" hidden>
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

      <div
        v-if="customStatusOverlay"
        class="status-overlay-group status-overlay-group-left illustration-map__custom-overlay-group"
        aria-hidden="true"
      >
        <div class="status-overlay illustration-map__custom-overlay">
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
          title="Zoom avant"
          aria-label="Zoom avant"
        >
          +
        </button>
        <button
          type="button"
          class="action-button zoom-action-button"
          data-replay-ref="zoomOutButton"
          title="Zoom arrière"
          aria-label="Zoom arrière"
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