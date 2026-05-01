<script setup>
import { computed, nextTick, onBeforeUnmount, onMounted, ref, watch } from "vue";

import { mountReplayViewer } from "../../app.js";
import ReplayToast from "./ReplayToast.vue";
import { registerViewerVisibility, unregisterViewerVisibility } from "../utils/viewerVisibilityController.js";

const PERSPECTIVE_KINGDOM_KEYS = Object.freeze(["white", "black"]);
const BUILDING_LABELS = Object.freeze({
  barracks: "Caserne",
  church: "Église",
  farm: "Ferme",
  mine: "Mine"
});
const PIECE_LABELS = Object.freeze({
  bishop: "Fou",
  king: "Roi",
  knight: "Cavalier",
  pawn: "Pion",
  queen: "Reine",
  rook: "Tour"
});

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
  allowZoom: {
    type: Boolean,
    default: true
  },
  showBuildingLabels: {
    type: Boolean,
    default: false
  },
  hiddenBuildingLabelKeys: {
    type: Array,
    default: () => []
  },
  productionOverlay: {
    type: Object,
    default: undefined
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

const replayGridSize = computed(() => {
  const grid = props.replayData && props.replayData.initialSnapshot && props.replayData.initialSnapshot.grid;
  return Array.isArray(grid) && grid.length ? grid.length : 9;
});

const buildingTypeKeyById = computed(() => {
  const entries = props.replayData && props.replayData.referenceData && Array.isArray(props.replayData.referenceData.buildingTypes)
    ? props.replayData.referenceData.buildingTypes
    : [];
  const byId = new Map();

  for (const entry of entries) {
    if (typeof entry?.id === "number" && typeof entry?.key === "string") {
      byId.set(entry.id, entry.key);
    }
  }

  return byId;
});

const initialSnapshotBuildings = computed(() => {
  const snapshot = props.replayData && props.replayData.initialSnapshot;
  if (!snapshot) {
    return [];
  }

  return [
    ...(Array.isArray(snapshot.whiteKingdom?.buildings) ? snapshot.whiteKingdom.buildings : []),
    ...(Array.isArray(snapshot.blackKingdom?.buildings) ? snapshot.blackKingdom.buildings : []),
    ...(Array.isArray(snapshot.publicBuildings) ? snapshot.publicBuildings : [])
  ];
});

const buildingLabels = computed(() => {
  if (!props.showBuildingLabels) {
    return [];
  }

  const hiddenLabelKeys = new Set(
    Array.isArray(props.hiddenBuildingLabelKeys)
      ? props.hiddenBuildingLabelKeys.map((value) => String(value).toLowerCase())
      : []
  );

  return initialSnapshotBuildings.value
    .map((building, index) => {
      const buildingKey = buildingTypeKeyById.value.get(building.type);
      const label = BUILDING_LABELS[buildingKey] || null;
      if (!label || hiddenLabelKeys.has(buildingKey)) {
        return null;
      }

      return {
        key: `building-label-${building.id ?? index}`,
        label,
        style: resolveBuildingLabelStyle(building, replayGridSize.value)
      };
    })
    .filter(Boolean);
});

const activeProductionOverlay = computed(() => {
  if (!props.productionOverlay || typeof props.productionOverlay !== "object") {
    return null;
  }

  const building = initialSnapshotBuildings.value.find((entry) => entry.id === props.productionOverlay.buildingId);
  if (!building) {
    return null;
  }

  const kingdomKey = props.productionOverlay.kingdom === "black" ? "black" : "white";
  const pieceType = typeof props.productionOverlay.pieceType === "string" ? props.productionOverlay.pieceType : null;
  const assetRoot = props.assetRoot || "/assets";
  const pieceLabel = pieceType ? (PIECE_LABELS[pieceType] || pieceType) : null;
  const progressValue = resolveFrameOverlayValue(
    props.productionOverlay.progressValues,
    currentFrameIndex.value,
    props.productionOverlay.initialProgress ?? 0
  );
  const turnsRemainingValue = resolveFrameOverlayValue(
    props.productionOverlay.values,
    currentFrameIndex.value,
    props.productionOverlay.initialValue ?? "0"
  );
  const turnsRemainingNumber = Number(turnsRemainingValue);

  if (Number.isFinite(turnsRemainingNumber) && turnsRemainingNumber <= 0) {
    return null;
  }

  return {
    label: typeof props.productionOverlay.label === "string" ? props.productionOverlay.label : "Tours restants",
    value: turnsRemainingValue,
    iconAlt: pieceLabel ? `${pieceLabel} ${kingdomKey === "black" ? "noir" : "blanc"}` : "",
    iconSrc: pieceType ? `${assetRoot}/textures/pieces/${kingdomKey}/${pieceType}.png` : null,
    progressStyle: {
      transform: `scaleX(${clampOverlayRatio(progressValue)})`
    },
    placement: props.productionOverlay.anchorPlacement === "below" ? "below" : "above",
    style: resolveBuildingOverlayStyle(building, replayGridSize.value, {
      anchorYOffsetCells: props.productionOverlay.anchorYOffsetCells,
      anchorPlacement: props.productionOverlay.anchorPlacement
    })
  };
});

const vignetteClasses = computed(() => ({
  "static-board-vignette--show-perspective": props.showPerspectiveOverlay,
  "static-board-vignette--show-toasts": props.showToasts,
  "static-board-vignette--show-status-overlay": Boolean(props.statusOverlay || props.showBuildingLabels)
}));

function clampOverlayRatio(value) {
  return Math.min(1, Math.max(0, Number.isFinite(Number(value)) ? Number(value) : 0));
}

function resolveBuildingOverlayStyle(building, gridSize, options = {}) {
  const safeGridSize = Math.max(1, Number(gridSize) || 9);
  const buildingWidth = Number.isFinite(building?.w) ? building.w : 1;
  const buildingHeight = Number.isFinite(building?.h) ? building.h : 1;
  const offsetX = Number.isFinite(building?.ox) ? building.ox : 0;
  const offsetY = Number.isFinite(building?.oy) ? building.oy : 0;
  const verticalOffset = Number.isFinite(Number(options.anchorYOffsetCells)) ? Number(options.anchorYOffsetCells) : 0.4;
  const placement = options.anchorPlacement === "below" ? "below" : "above";
  const left = clampPercentage(((offsetX + (buildingWidth / 2)) / safeGridSize) * 100, 14, 86);
  const anchorY = placement === "below"
    ? offsetY + buildingHeight + verticalOffset
    : offsetY + verticalOffset;
  const top = clampPercentage((anchorY / safeGridSize) * 100, placement === "below" ? 12 : 16, placement === "below" ? 90 : 82);

  return {
    left: `${left}%`,
    top: `${top}%`
  };
}

function resolveBuildingLabelStyle(building, gridSize) {
  const safeGridSize = Math.max(1, Number(gridSize) || 9);
  const buildingWidth = Number.isFinite(building?.w) ? building.w : 1;
  const offsetX = Number.isFinite(building?.ox) ? building.ox : 0;
  const offsetY = Number.isFinite(building?.oy) ? building.oy : 0;
  const left = clampPercentage(((offsetX + (buildingWidth / 2)) / safeGridSize) * 100, 10, 90);
  const top = clampPercentage(((offsetY - 0.14) / safeGridSize) * 100, 8, 82);

  return {
    left: `${left}%`,
    top: `${top}%`
  };
}

function clampPercentage(value, min, max) {
  return Math.min(max, Math.max(min, value));
}

function resolveFrameOverlayValue(values, frameIndex, fallbackValue) {
  if (!Array.isArray(values) || !values.length) {
    return fallbackValue;
  }

  const safeIndex = Math.min(Math.max(0, frameIndex), values.length - 1);
  return values[safeIndex];
}

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

      <div v-if="buildingLabels.length" class="static-board-vignette__board-overlay-layer" aria-hidden="true">
        <div
          v-for="buildingLabel in buildingLabels"
          :key="buildingLabel.key"
          class="status-overlay static-board-vignette__board-overlay static-board-vignette__board-overlay--building-label"
          :style="buildingLabel.style"
        >
          <div class="overlay-meta">
            <span class="overlay-label">{{ buildingLabel.label }}</span>
          </div>
        </div>
      </div>

      <div v-if="activeProductionOverlay" class="static-board-vignette__board-overlay-layer" aria-hidden="true">
        <div
          :class="[
            'status-overlay',
            'static-board-vignette__board-overlay',
            'static-board-vignette__board-overlay--production',
            {
              'static-board-vignette__board-overlay--production-below': activeProductionOverlay.placement === 'below'
            }
          ]"
          :style="activeProductionOverlay.style"
        >
          <img
            v-if="activeProductionOverlay.iconSrc"
            class="turn-indicator-icon static-board-vignette__board-overlay-icon"
            :src="activeProductionOverlay.iconSrc"
            :alt="activeProductionOverlay.iconAlt"
          >
          <div class="overlay-meta static-board-vignette__board-overlay-meta">
            <span class="overlay-label">{{ activeProductionOverlay.label }}</span>
            <div class="static-board-vignette__progress-bar">
              <span class="static-board-vignette__progress-fill" :style="activeProductionOverlay.progressStyle"></span>
              <span class="static-board-vignette__progress-value">{{ activeProductionOverlay.value }}</span>
            </div>
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

      <div v-show="props.allowZoom" class="zoom-controls" aria-label="Contrôles de zoom">
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