<script setup>
import { computed, nextTick, onBeforeUnmount, onMounted, ref, watch } from "vue";
import { mountReplayViewer } from "../../app.js";
import ReplayToast from "./ReplayToast.vue";
import { registerViewerVisibility, unregisterViewerVisibility } from "../utils/viewerVisibilityController.js";

const props = defineProps({
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
  initialZoom: {
    type: Number,
    default: undefined
  },
  toastCooldownMs: {
    type: Number,
    default: 0
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
  updateCameraOnEveryTick: {
    type: Boolean,
    default: false
  },
  lockCamera: {
    type: Boolean,
    default: false
  },
  showTimeline: {
    type: Boolean,
    default: true
  },
  showTurnOverlay: {
    type: Boolean,
    default: true
  },
  showStatusOverlay: {
    type: Boolean,
    default: true
  }
});

const viewerRoot = ref(null);
const toastItems = ref([]);
const showInteractionTooltip = ref(false);
let viewerInstance = null;

const rootClasses = computed(() => ({
  "replay-root--hide-timeline": !props.showTimeline,
  "replay-root--hide-turn-overlay": !props.showTurnOverlay
}));

const interactionTooltipItems = computed(() => {
  const items = [
    { key: "Ctrl + molette", value: "Zoom" },
    { key: "Double-clic", value: "Recadrer" }
  ];

  if (!props.lockCamera) {
    items.splice(1, 0, { key: "Glisser", value: "Caméra" });
  }

  if (props.enableCellDebug) {
    items.push({ key: "Clic", value: "Debug cellule" });
  }

  return items;
});

function showReplayTooltip() {
  if (!interactionTooltipItems.value.length) {
    return;
  }

  showInteractionTooltip.value = true;
}

function hideReplayTooltip() {
  showInteractionTooltip.value = false;
}

function handleToastStateChange(nextToasts) {
  toastItems.value = Array.isArray(nextToasts) ? nextToasts : [];
}

function buildMountOptions() {
  const options = {};

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
  if (typeof props.initialZoom === "number") {
    options.initialZoom = props.initialZoom;
  }
  if (typeof props.toastCooldownMs === "number") {
    options.toastCooldownMs = props.toastCooldownMs;
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

  options.autoplayOnMount = props.autoplayOnMount;
  options.loopPlayback = props.loopPlayback;
  options.enableCellDebug = props.enableCellDebug;
  options.perspectiveEnabled = props.enablePerspective;
  options.perspectiveKingdom = props.perspectiveKingdom;
  options.trackedTarget = props.trackedTarget;
  options.updateCameraOnEveryTick = props.updateCameraOnEveryTick;
  options.lockCamera = props.lockCamera;
  options.onToastStateChange = handleToastStateChange;

  return options;
}

function destroyViewer() {
  if (viewerRoot.value) {
    unregisterViewerVisibility(viewerRoot.value);
  }

  if (!viewerInstance) {
    return;
  }

  viewerInstance.destroy();
  viewerInstance = null;
  toastItems.value = [];
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

watch(
  () => [
    props.replayUrl,
    props.assetRoot,
    props.masterConfigUrl,
    props.autoplayIntervalMs,
    props.autoplayOnMount,
    props.loopPlayback,
    props.initialZoom,
    props.toastCooldownMs,
    props.minTurn,
    props.maxTurn,
    props.initialTurn,
    props.enableCellDebug,
    props.enablePerspective,
    props.perspectiveKingdom,
    props.trackedTarget,
    props.updateCameraOnEveryTick,
    props.lockCamera
  ],
  async () => {
    destroyViewer();
    await nextTick();
    mountViewer();
  }
);
</script>

<template>
  <div ref="viewerRoot" :class="['replay-root', rootClasses]">
    <canvas
      class="replay-canvas"
      data-replay-ref="replayCanvas"
      aria-label="Carte du replay"
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

    <TransitionGroup
      v-if="props.showStatusOverlay"
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

    <div class="timeline-overlay">
      <div class="timeline-controls">
        <div class="playback-group">
          <button type="button" class="action-button" data-replay-ref="firstTurnButton" title="Aller au début">|&lt;</button>
          <button type="button" class="action-button" data-replay-ref="prevTurnButton" title="Tour précédent">&lt;</button>
          <button type="button" class="action-button primary-action" data-replay-ref="playPauseButton" title="Lancer la lecture">Lire</button>
          <button type="button" class="action-button" data-replay-ref="nextTurnButton" title="Tour suivant">&gt;</button>
          <button type="button" class="action-button" data-replay-ref="lastTurnButton" title="Aller à la fin">&gt;|</button>
        </div>

        <input
          class="timeline-slider"
          type="range"
          data-replay-ref="turnSlider"
          min="0"
          max="0"
          step="1"
          value="0"
          aria-label="Tour du replay"
        >
      </div>
    </div>
  </div>
</template>