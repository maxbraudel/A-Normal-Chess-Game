<script setup>
import { computed, nextTick, onBeforeUnmount, onMounted, ref, watch } from "vue";
import { mountReplayViewer } from "../../app.js";

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
let viewerInstance = null;

const rootClasses = computed(() => ({
  "replay-root--hide-timeline": !props.showTimeline,
  "replay-root--hide-turn-overlay": !props.showTurnOverlay,
  "replay-root--hide-status-overlay": !props.showStatusOverlay
}));

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

  options.enableCellDebug = props.enableCellDebug;
  options.perspectiveEnabled = props.enablePerspective;
  options.perspectiveKingdom = props.perspectiveKingdom;
  options.trackedTarget = props.trackedTarget;

  return options;
}

function destroyViewer() {
  if (!viewerInstance) {
    return;
  }

  viewerInstance.destroy();
  viewerInstance = null;
}

function mountViewer() {
  if (!viewerRoot.value) {
    return;
  }

  viewerInstance = mountReplayViewer(viewerRoot.value, buildMountOptions());
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
    props.enableCellDebug,
    props.enablePerspective,
    props.perspectiveKingdom,
    props.trackedTarget
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
    <canvas class="replay-canvas" data-replay-ref="replayCanvas" aria-label="Carte du replay"></canvas>

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
          <span class="overlay-label">Au trait</span>
          <strong class="overlay-value" data-replay-ref="activeKingdomValue">-</strong>
        </div>
      </div>
    </div>

    <div class="status-overlay status-overlay-right status-overlay-status" data-replay-ref="statusOverlay" hidden>
      <strong class="status-overlay-value" data-replay-ref="statusText">-</strong>
    </div>

    <div class="zoom-controls" aria-label="Controles de zoom">
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
        title="Zoom arriere"
        aria-label="Zoom arriere"
      >
        -
      </button>
    </div>

    <div class="timeline-overlay">
      <div class="timeline-controls">
        <div class="playback-group">
          <button type="button" class="action-button" data-replay-ref="firstTurnButton" title="Aller au debut">|&lt;</button>
          <button type="button" class="action-button" data-replay-ref="prevTurnButton" title="Tour precedent">&lt;</button>
          <button type="button" class="action-button primary-action" data-replay-ref="playPauseButton" title="Lancer la lecture">Lire</button>
          <button type="button" class="action-button" data-replay-ref="nextTurnButton" title="Tour suivant">&gt;</button>
          <button type="button" class="action-button" data-replay-ref="lastTurnButton" title="Aller a la fin">&gt;|</button>
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