<script setup>
import { computed, nextTick, onBeforeUnmount, onMounted, ref, watch } from "vue";
import { mountReplayViewer } from "../../app.js";
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
  }
});

const viewerRoot = ref(null);
let viewerInstance = null;

const watchedProps = computed(() => [
  props.replayData,
  props.replayUrl,
  props.assetRoot,
  props.masterConfigUrl,
  props.autoplayIntervalMs,
  props.autoplayOnMount,
  props.loopPlayback,
  props.initialZoom,
  props.minTurn,
  props.maxTurn,
  props.initialTurn,
  props.enableCellDebug,
  props.enablePerspective,
  props.perspectiveKingdom,
  props.trackedTarget
]);

function buildMountOptions() {
  const options = {
    autoplayOnMount: props.autoplayOnMount,
    loopPlayback: props.loopPlayback,
    enableCellDebug: props.enableCellDebug,
    perspectiveEnabled: props.enablePerspective,
    perspectiveKingdom: props.perspectiveKingdom,
    trackedTarget: props.trackedTarget,
    recenterTrackedTargetOnFrameChange: false
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

function destroyViewer() {
  if (viewerRoot.value) {
    unregisterViewerVisibility(viewerRoot.value);
  }

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
  <div class="illustration-map">
    <div
      ref="viewerRoot"
      class="replay-root replay-root--hide-timeline replay-root--hide-turn-overlay illustration-map__viewer"
    >
      <canvas class="replay-canvas" data-replay-ref="replayCanvas" aria-label="Carte d'illustration"></canvas>

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