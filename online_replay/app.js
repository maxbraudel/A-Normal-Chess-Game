import { REPLAY_CONFIG as DEFAULT_REPLAY_CONFIG } from "./config.js";
import { loadMasterConfigData, loadReplayData } from "./src/stores/replayDataStore.js";

export function mountReplayViewer(rootElement, configOverrides = {}) {
if (!(rootElement instanceof HTMLElement)) {
  throw new Error("mountReplayViewer requires a root HTMLElement.");
}

const replayConfig = {
  ...DEFAULT_REPLAY_CONFIG,
  ...configOverrides
};
const managedListeners = [];
const abortController = new AbortController();
let isDestroyed = false;

const TARGET_SCHEMA_VERSION = 5;
const CONCEALING_FOG_ALPHA_THRESHOLD = 40;
const CLICK_DRAG_THRESHOLD_PX = 6;
const MIN_CAMERA_ZOOM = 0.08;
const MAX_CAMERA_ZOOM = 32;
const CAMERA_ZOOM_STEP_FACTOR = 1.18;
const CTRL_WHEEL_ZOOM_DELTA_STEP = 100;
const toastDismissDelayMs = normalizeToastCooldownMs(replayConfig.toastCooldownMs);

const DEFAULT_MASTER_CONFIG = {
  game: {
    map: {
      cell_size_px: 16
    },
    rendering: {
      damaged_structures: {
        opacity_percent: 70
      }
    }
  }
};

const CELL_TYPE_KEYS = ["void", "grass", "dirt", "water"];
const CELL_FALLBACK_COLORS = {
  void: "#0a0d0a",
  grass: "#607a42",
  dirt: "#70563c",
  water: "#2d5978"
};

const BUILDING_TEXTURE_PATHS = {
  church: "textures/cells/church.png",
  mine: "textures/cells/mine.png",
  farm: "textures/cells/farm.png",
  barracks: "textures/cells/barrak.png",
  wood_wall: "textures/cells/wall_wood.png",
  stone_wall: "textures/cells/wall_stone.png",
  bridge: "textures/cells/bridge.png",
  arena: "textures/cells/barrak.png"
};

const CHUNKED_BUILDINGS = {
  barracks: { id: "barracks", width: 4, height: 3 },
  church: { id: "church", width: 4, height: 3 },
  mine: { id: "mine", width: 6, height: 6 },
  farm: { id: "farm", width: 6, height: 4 },
  arena: { id: "arena", width: 4, height: 4 }
};

const KINGDOM_SHIELD_PATHS = {
  white: "textures/ui/shield_white.png",
  black: "textures/ui/shield_black.png"
};

const KINGDOM_PERSPECTIVE_LABELS = {
  white: "Blancs",
  black: "Noirs"
};

const WEATHER_FRONT_DIRECTION_KEYS = [
  "north",
  "south",
  "east",
  "west",
  "north_east",
  "north_west",
  "south_east",
  "south_west"
];

const BUILDING_FLIP_HORIZONTAL_MASK = 1;
const BUILDING_FLIP_VERTICAL_MASK = 2;

let configuredPerspectiveKingdomKey = resolveConfiguredPerspectiveKingdomKey(replayConfig);
const trackedTargetConfig = normalizeTrackedTargetConfig(replayConfig.trackedTarget);
const shouldUpdateCameraOnEveryTick = replayConfig.updateCameraOnEveryTick === true
  || replayConfig.recenterTrackedTargetOnFrameChange === true;
const isCameraPanLocked = replayConfig.lockCamera === true;
const isInteractionEnabled = replayConfig.interactionEnabled !== false;
const isCellDebugEnabled = Boolean(replayConfig.enableCellDebug);
const onToastStateChange = typeof replayConfig.onToastStateChange === "function"
  ? replayConfig.onToastStateChange
  : null;
const onFrameStateChange = typeof replayConfig.onFrameStateChange === "function"
  ? replayConfig.onFrameStateChange
  : null;

const state = {
  replay: null,
  frameIndex: 0,
  playbackWindow: {
    minFrameIndex: 0,
    maxFrameIndex: 0,
    initialFrameIndex: 0,
    autoplayOnMount: false
  },
  masterConfig: DEFAULT_MASTER_CONFIG,
  textures: new Map(),
  autoPlayHandle: null,
  statusMessage: "Chargement du replay...",
  toast: {
    slots: {
      chest: createToastSlotState(),
      status: createToastSlotState()
    }
  },
  suspension: {
    reasons: new Set(),
    resumeAutoplayOnUnsuspend: false,
    pendingRender: false
  },
  camera: {
    zoom: 1,
    centerWorldX: 0,
    centerWorldY: 0,
    initialized: false,
    boardKey: null,
    isDragging: false,
    pointerId: null,
    lastClientX: 0,
    lastClientY: 0,
    dragStartClientX: 0,
    dragStartClientY: 0,
    didDrag: false,
    pendingRuleCameraUpdate: Boolean(trackedTargetConfig)
  }
};

const refs = resolveRefs(rootElement);

bindEvents();
renderIdle();
void bootstrap();

return {
  destroy() {
    isDestroyed = true;
    abortController.abort();
    state.suspension.reasons.clear();
    state.suspension.resumeAutoplayOnUnsuspend = false;
    state.suspension.pendingRender = false;
    stopAutoplay();
    clearAllToastSlots();
    state.textures.clear();
    while (managedListeners.length) {
      const dispose = managedListeners.pop();
      if (dispose) {
        dispose();
      }
    }
  },

  setSuspended(reason, suspended) {
    setViewerSuspended(reason, suspended);
  },

  setPerspectiveKingdom(nextPerspectiveKingdomKey) {
    const normalizedPerspectiveKingdomKey = normalizePerspectiveKingdomKey(nextPerspectiveKingdomKey);
    if (configuredPerspectiveKingdomKey === normalizedPerspectiveKingdomKey) {
      return;
    }

    configuredPerspectiveKingdomKey = normalizedPerspectiveKingdomKey;
    renderCurrentFrame({ force: true });
  }
};

function isViewerSuspended() {
  return state.suspension.reasons.size > 0;
}

function setViewerSuspended(reason, suspended) {
  if (isDestroyed || typeof reason !== "string" || !reason) {
    return;
  }

  const reasons = state.suspension.reasons;
  const wasSuspended = reasons.size > 0;

  if (suspended) {
    if (reasons.has(reason)) {
      return;
    }

    reasons.add(reason);
    if (!wasSuspended) {
      state.suspension.resumeAutoplayOnUnsuspend = Boolean(state.autoPlayHandle);
      pauseViewerForSuspension();
    }
    return;
  }

  if (!reasons.has(reason)) {
    return;
  }

  reasons.delete(reason);
  if (reasons.size === 0) {
    resumeViewerAfterSuspension();
  }
}

function pauseViewerForSuspension() {
  clearAllToastDismissTimers();
  cancelCameraInteraction();
  stopAutoplay();
}

function resumeViewerAfterSuspension() {
  const shouldResumeAutoplay = state.suspension.resumeAutoplayOnUnsuspend;
  const hasPendingRender = state.suspension.pendingRender;

  state.suspension.resumeAutoplayOnUnsuspend = false;
  state.suspension.pendingRender = false;

  if (shouldResumeAutoplay) {
    startAutoplay();
    return;
  }

  if (state.replay || hasPendingRender) {
    renderCurrentFrame({ force: true });
    return;
  }

  renderCanvasMessage(state.statusMessage);
}

function cancelCameraInteraction() {
  state.camera.isDragging = false;
  state.camera.pointerId = null;
  state.camera.didDrag = false;
  refs.replayCanvas.classList.remove("is-dragging");
}

async function bootstrap() {
  if (window.location.protocol === "file:") {
    setError(
      "Le viewer est ouvert via file://. Servir le dossier en HTTP pour autoriser fetch sur le companion et les assets."
    );
    return;
  }

  setStatus("Chargement du replay...", describeReplaySource(replayConfig));

  try {
    const [masterConfig, rawReplay] = await Promise.all([
      loadMasterConfig(replayConfig.masterConfigUrl),
      loadReplaySource(replayConfig)
    ]);

    if (isDestroyed) {
      return;
    }

    state.masterConfig = masterConfig;
    state.replay = buildReplayModel(rawReplay, masterConfig);
    state.playbackWindow = resolvePlaybackWindow(state.replay.frames, replayConfig);
    state.frameIndex = state.playbackWindow.initialFrameIndex;
    state.statusMessage = "";
    await primeTextureCatalog(state.replay);

    if (isDestroyed) {
      return;
    }

    resetCameraToConfiguredInitialView(state.replay.frames[state.frameIndex]);
    syncTimelineBounds();
    renderCurrentFrame();
    if (state.playbackWindow.autoplayOnMount) {
      startAutoplay();
    }
    setStatus(
      "Replay chargé.",
      `${state.replay.meta.title} · ${state.replay.frames.length} frames disponibles`
    );
  } catch (error) {
    if (isDestroyed || isAbortError(error)) {
      return;
    }
    setError(error instanceof Error ? error.message : String(error));
  }
}

function resolveRefs(root) {
  return {
    replayCanvas: mustGet(root, "replayCanvas"),
    activeTurnOverlay: mustGet(root, "activeTurnOverlay"),
    activeKingdomLabel: mustGet(root, "activeKingdomLabel"),
    activeKingdomShield: mustGet(root, "activeKingdomShield"),
    activeTurnValue: mustGet(root, "activeTurnValue"),
    perspectiveOverlay: mustGet(root, "perspectiveOverlay"),
    perspectiveKingdomShield: mustGet(root, "perspectiveKingdomShield"),
    perspectiveKingdomValue: mustGet(root, "perspectiveKingdomValue"),
    firstTurnButton: mustGet(root, "firstTurnButton"),
    prevTurnButton: mustGet(root, "prevTurnButton"),
    playPauseButton: mustGet(root, "playPauseButton"),
    nextTurnButton: mustGet(root, "nextTurnButton"),
    lastTurnButton: mustGet(root, "lastTurnButton"),
    zoomInButton: mustGet(root, "zoomInButton"),
    zoomOutButton: mustGet(root, "zoomOutButton"),
    turnSlider: mustGet(root, "turnSlider")
  };
}

function mustGet(root, refName) {
  const element = root.querySelector(`[data-replay-ref="${refName}"]`);
  if (!element) {
    throw new Error(`Missing required replay ref ${refName}`);
  }
  return element;
}

function addManagedListener(target, type, listener, options) {
  target.addEventListener(type, listener, options);
  managedListeners.push(function () {
    target.removeEventListener(type, listener, options);
  });
}

function bindEvents() {
  if (isInteractionEnabled && !rootElement.hasAttribute("tabindex")) {
    rootElement.tabIndex = 0;
  } else if (!isInteractionEnabled) {
    rootElement.removeAttribute("tabindex");
  }

  refs.replayCanvas.removeAttribute("title");

  addManagedListener(refs.firstTurnButton, "click", function () {
    stopAutoplay();
    setFrameIndex(currentPlaybackWindow().minFrameIndex);
  });

  addManagedListener(refs.prevTurnButton, "click", function () {
    stopAutoplay();
    setFrameIndex(state.frameIndex - 1);
  });

  addManagedListener(refs.nextTurnButton, "click", function () {
    stopAutoplay();
    setFrameIndex(state.frameIndex + 1);
  });

  addManagedListener(refs.lastTurnButton, "click", function () {
    stopAutoplay();
    if (!state.replay) {
      return;
    }
    setFrameIndex(currentPlaybackWindow().maxFrameIndex);
  });

  addManagedListener(refs.playPauseButton, "click", function () {
    if (state.autoPlayHandle) {
      stopAutoplay();
    } else {
      startAutoplay();
    }
  });

  addManagedListener(refs.zoomInButton, "click", function () {
    zoomCameraFromKeyboard(CAMERA_ZOOM_STEP_FACTOR);
  });

  addManagedListener(refs.zoomOutButton, "click", function () {
    zoomCameraFromKeyboard(1 / CAMERA_ZOOM_STEP_FACTOR);
  });

  addManagedListener(refs.turnSlider, "input", function (event) {
    stopAutoplay();
    const nextIndex = Number(event.target.value);
    setFrameIndex(nextIndex);
  });

  if (isInteractionEnabled && !isCameraPanLocked) {
    addManagedListener(refs.replayCanvas, "pointerdown", onCanvasPointerDown);
    addManagedListener(refs.replayCanvas, "pointermove", onCanvasPointerMove);
    addManagedListener(refs.replayCanvas, "pointerup", onCanvasPointerUp);
    addManagedListener(refs.replayCanvas, "pointercancel", onCanvasPointerUp);
  }

  if (isInteractionEnabled) {
    addManagedListener(refs.replayCanvas, "wheel", onCanvasWheel, { passive: false });
    addManagedListener(refs.replayCanvas, "dblclick", function () {
      const frame = currentFrame();
      if (!frame) {
        return;
      }

      resetCameraToFit(frame);
      renderCurrentFrame();
    });
  }

  addManagedListener(window, "resize", function () {
    if (state.replay) {
      renderCurrentFrame();
      return;
    }

    renderCanvasMessage(state.statusMessage);
  });

  if (isInteractionEnabled) {
    addManagedListener(rootElement, "keydown", function (event) {
      if (!state.replay) {
        return;
      }

      if (event.key === "ArrowLeft") {
        stopAutoplay();
        setFrameIndex(state.frameIndex - 1);
      }

      if (event.key === "ArrowRight") {
        stopAutoplay();
        setFrameIndex(state.frameIndex + 1);
      }

      if (event.key === " ") {
        event.preventDefault();
        if (state.autoPlayHandle) {
          stopAutoplay();
        } else {
          startAutoplay();
        }
      }

      if (event.key === "+" || event.key === "=") {
        event.preventDefault();
        zoomCameraFromKeyboard(CAMERA_ZOOM_STEP_FACTOR);
      }

      if (event.key === "-" || event.key === "_") {
        event.preventDefault();
        zoomCameraFromKeyboard(1 / CAMERA_ZOOM_STEP_FACTOR);
      }

      if (event.key === "0") {
        event.preventDefault();
        const frame = currentFrame();
        if (!frame) {
          return;
        }

        resetCameraToFit(frame);
        renderCurrentFrame();
      }
    });
  }
}

function renderIdle() {
  syncTimelineBounds();
  syncControlsState();
  syncOverlays(null);
  syncToastState(null);
  renderCanvasMessage(state.statusMessage);
}

function setStatus(label, detail) {
  state.statusMessage = detail || label;
  console.info(label, detail || "");
}

function setError(message) {
  stopAutoplay();
  state.statusMessage = message;
  syncTimelineBounds();
  syncControlsState();
  syncOverlays(null);
  renderCanvasMessage(message);
  console.error(message);
}

async function loadMasterConfig(url) {
  return loadMasterConfigData(url);
}

async function loadReplay(url) {
  return loadReplayData(url);
}

async function loadReplaySource(config) {
  if (config && config.replayData && typeof config.replayData === "object") {
    validateReplay(config.replayData);
    return config.replayData;
  }

  if (!config || !config.replayUrl) {
    throw new Error("Aucune source de replay fournie au viewer.");
  }

  return loadReplay(config.replayUrl);
}

function describeReplaySource(config) {
  if (config && typeof config.replayUrl === "string" && config.replayUrl) {
    return config.replayUrl;
  }

  if (config && config.replayData && typeof config.replayData === "object") {
    return config.replayData.saveName
      || (config.replayData.initialSnapshot && config.replayData.initialSnapshot.gameName)
      || "Scene integree";
  }

  return "Replay";
}

function validateReplay(data) {
  if (!data || typeof data !== "object" || Array.isArray(data)) {
    throw new Error("Le companion chargé n'est pas un objet JSON valide.");
  }

  if (Number(data.schemaVersion) !== TARGET_SCHEMA_VERSION) {
    throw new Error("Ce viewer attend un companion Data schema v5.");
  }

  if (!data.initialSnapshot && !Array.isArray(data.turnHistory) && !data.currentStateSummary) {
    throw new Error("Le companion n'expose aucun snapshot exploitable.");
  }
}

function buildReplayModel(data, masterConfig) {
  const frames = [];
  const turnHistory = Array.isArray(data.turnHistory) ? data.turnHistory : [];
  const sharedGrid = resolveSharedReplayGrid(data);

  if (data.initialSnapshot) {
    frames.push(normalizeFrame({
      snapshot: resolveReplaySnapshot(data.initialSnapshot, sharedGrid),
      analytics: data.initialAnalytics || null,
      committedTurnNumber: 0,
      capturedAtUnix: data.createdAtUnix || null,
      gameOver: false,
      winner: null,
      activeKingdom: data.initialSnapshot.activeKingdom,
      activeValidation: data.initialActiveValidation || null,
      nextTurnValidation: data.initialNextTurnValidation || null,
      label: "État initial",
      rawEvents: Array.isArray(data.initialSnapshot.events) ? data.initialSnapshot.events : [],
      notifications: []
    }, data.referenceData, masterConfig));
  }

  for (const record of turnHistory) {
    if (!record || !record.snapshot) {
      continue;
    }

    frames.push(normalizeFrame({
      snapshot: resolveReplaySnapshot(record.snapshot, sharedGrid),
      analytics: record.analytics || null,
      committedTurnNumber: record.committedTurnNumber,
      capturedAtUnix: record.capturedAtUnix || null,
      gameOver: Boolean(record.gameOver),
      winner: typeof record.winner === "number" ? record.winner : null,
      activeKingdom: record.committedActiveKingdom,
      activeValidation: record.activeValidation || null,
      nextTurnValidation: record.nextTurnValidation || null,
      label: `Tour ${record.committedTurnNumber}`,
      rawEvents: Array.isArray(record.newEvents) ? record.newEvents : [],
      notifications: Array.isArray(record.notifications) ? record.notifications : []
    }, data.referenceData, masterConfig));
  }

  if (!frames.length && data.currentStateSummary) {
    frames.push(normalizeFrame({
      snapshot: resolveReplaySnapshot(data.currentStateSummary, sharedGrid),
      analytics: data.currentAnalytics || null,
      committedTurnNumber: data.currentStateSummary.turnNumber || 0,
      capturedAtUnix: data.lastUpdatedAtUnix || null,
      gameOver: false,
      winner: null,
      activeKingdom: data.currentStateSummary.activeKingdom,
      activeValidation: data.currentActiveValidation || data.currentStateSummary.activeValidation || null,
      nextTurnValidation: data.currentNextTurnValidation || data.currentStateSummary.nextTurnValidation || null,
      label: `Tour ${data.currentStateSummary.turnNumber || 0}`,
      rawEvents: Array.isArray(data.currentStateSummary.events) ? data.currentStateSummary.events : [],
      notifications: []
    }, data.referenceData, masterConfig));
  }

  if (!frames.length) {
    throw new Error("Aucune frame replayable n'a été construite depuis le companion.");
  }

  return {
    raw: data,
    referenceData: data.referenceData || {},
    frames,
    meta: {
      title: data.saveName || frames[0].snapshot.gameName || "Replay",
      continuity: Boolean(data.historyContinuityComplete),
      worldSeed: data.sessionContext && typeof data.sessionContext.worldSeed === "number"
        ? data.sessionContext.worldSeed
        : null,
      cellSize: getCellSize(masterConfig)
    }
  };
}

function resolveSharedReplayGrid(data) {
  if (Array.isArray(data && data.sharedGrid) && data.sharedGrid.length) {
    return data.sharedGrid;
  }
  if (Array.isArray(data && data.initialSnapshot && data.initialSnapshot.grid) && data.initialSnapshot.grid.length) {
    return data.initialSnapshot.grid;
  }
  if (Array.isArray(data && data.currentStateSummary && data.currentStateSummary.grid) && data.currentStateSummary.grid.length) {
    return data.currentStateSummary.grid;
  }

  const turnHistory = Array.isArray(data && data.turnHistory) ? data.turnHistory : [];
  const firstRecordWithGrid = turnHistory.find(function (record) {
    return Boolean(record && record.snapshot && Array.isArray(record.snapshot.grid) && record.snapshot.grid.length);
  });

  return firstRecordWithGrid ? firstRecordWithGrid.snapshot.grid : [];
}

function resolveReplaySnapshot(snapshot, sharedGrid) {
  if (!snapshot || typeof snapshot !== "object") {
    return snapshot;
  }

  if (Array.isArray(snapshot.grid)) {
    return snapshot;
  }

  if (!Array.isArray(sharedGrid) || !sharedGrid.length) {
    return snapshot;
  }

  return {
    ...snapshot,
    grid: sharedGrid
  };
}

function resolvePlaybackWindow(frames, config) {
  if (!Array.isArray(frames) || !frames.length) {
    return {
      minFrameIndex: 0,
      maxFrameIndex: 0,
      initialFrameIndex: 0,
      autoplayOnMount: false
    };
  }

  const frameCount = frames.length;
  const requestedInitialTurn = normalizeConfiguredTurn(config && config.initialTurn);
  let requestedMinTurn = normalizeConfiguredTurn(config && config.minTurn);
  let requestedMaxTurn = normalizeConfiguredTurn(config && config.maxTurn);

  if (requestedMinTurn !== null && requestedMaxTurn !== null && requestedMinTurn > requestedMaxTurn) {
    console.warn("[Replay config] minTurn est superieur a maxTurn. La plage demandee a ete reordonnee.");
    const swappedTurn = requestedMinTurn;
    requestedMinTurn = requestedMaxTurn;
    requestedMaxTurn = swappedTurn;
  }

  const minFrameIndex = requestedMinTurn === null
    ? 0
    : (findFirstFrameIndexAtOrAfterTurn(frames, requestedMinTurn) ?? (frameCount - 1));
  const maxFrameIndex = requestedMaxTurn === null
    ? (frameCount - 1)
    : (findLastFrameIndexAtOrBeforeTurn(frames, requestedMaxTurn) ?? 0);

  if (minFrameIndex > maxFrameIndex) {
    console.warn("[Replay config] La plage de tours demandee ne correspond a aucune frame exploitable. Le replay complet est utilise.");
    return {
      minFrameIndex: 0,
      maxFrameIndex: frameCount - 1,
      initialFrameIndex: resolveInitialPlaybackFrameIndex(frames, requestedInitialTurn, 0, frameCount - 1),
      autoplayOnMount: Boolean(config && config.autoplayOnMount)
    };
  }

  return {
    minFrameIndex,
    maxFrameIndex,
    initialFrameIndex: resolveInitialPlaybackFrameIndex(frames, requestedInitialTurn, minFrameIndex, maxFrameIndex),
    autoplayOnMount: Boolean(config && config.autoplayOnMount)
  };
}

function currentPlaybackWindow() {
  return state.playbackWindow || {
    minFrameIndex: 0,
    maxFrameIndex: 0,
    initialFrameIndex: 0,
    autoplayOnMount: false
  };
}

function normalizeConfiguredTurn(value) {
  return Number.isFinite(value) ? Math.trunc(value) : null;
}

function readFrameTurnNumber(frame) {
  return Number.isFinite(frame && frame.committedTurnNumber)
    ? Math.trunc(frame.committedTurnNumber)
    : 0;
}

function findFirstFrameIndexAtOrAfterTurn(frames, turnNumber) {
  for (let index = 0; index < frames.length; index += 1) {
    if (readFrameTurnNumber(frames[index]) >= turnNumber) {
      return index;
    }
  }

  return null;
}

function findLastFrameIndexAtOrBeforeTurn(frames, turnNumber) {
  for (let index = frames.length - 1; index >= 0; index -= 1) {
    if (readFrameTurnNumber(frames[index]) <= turnNumber) {
      return index;
    }
  }

  return null;
}

function resolveInitialPlaybackFrameIndex(frames, requestedTurn, minFrameIndex, maxFrameIndex) {
  if (requestedTurn === null) {
    return minFrameIndex;
  }

  let closestFrameIndex = minFrameIndex;
  let closestDistance = Number.POSITIVE_INFINITY;

  for (let index = minFrameIndex; index <= maxFrameIndex; index += 1) {
    const distance = Math.abs(readFrameTurnNumber(frames[index]) - requestedTurn);
    if (distance < closestDistance) {
      closestDistance = distance;
      closestFrameIndex = index;
    }
  }

  return closestFrameIndex;
}

function normalizeFrame(frameInput, referenceData, masterConfig) {
  const snapshot = frameInput.snapshot;
  const whitePieces = toArray(snapshot.whiteKingdom && snapshot.whiteKingdom.pieces);
  const blackPieces = toArray(snapshot.blackKingdom && snapshot.blackKingdom.pieces);
  const whiteBuildings = toArray(snapshot.whiteKingdom && snapshot.whiteKingdom.buildings);
  const blackBuildings = toArray(snapshot.blackKingdom && snapshot.blackKingdom.buildings);
  const publicBuildings = toArray(snapshot.publicBuildings);
  const analyticsPieces = frameInput.analytics
    && frameInput.analytics.entities
    && Array.isArray(frameInput.analytics.entities.pieceIndex)
    ? frameInput.analytics.entities.pieceIndex
    : null;
  const analyticsBuildings = frameInput.analytics
    && frameInput.analytics.entities
    && Array.isArray(frameInput.analytics.entities.buildingIndex)
    ? frameInput.analytics.entities.buildingIndex
    : null;
  const sideToMoveKingdom = typeof snapshot.activeKingdom === "number"
    ? snapshot.activeKingdom
    : frameInput.activeKingdom;

  return {
    label: frameInput.label,
    committedTurnNumber: frameInput.committedTurnNumber,
    capturedAtUnix: frameInput.capturedAtUnix,
    gameOver: frameInput.gameOver,
    winner: frameInput.winner,
    activeKingdom: frameInput.activeKingdom,
    activeValidation: normalizeValidation(frameInput.activeValidation),
    nextTurnValidation: normalizeValidation(frameInput.nextTurnValidation),
    snapshot,
    analytics: frameInput.analytics,
    rawEvents: frameInput.rawEvents,
    notifications: toArray(frameInput.notifications),
    grid: toArray(snapshot.grid),
    pieces: whitePieces.concat(blackPieces),
    piecesAnalytics: analyticsPieces,
    buildings: whiteBuildings.concat(blackBuildings).concat(publicBuildings),
    buildingsAnalytics: analyticsBuildings,
    mapObjects: toArray(snapshot.mapObjects),
    autonomousUnits: toArray(snapshot.autonomousUnits),
    referenceData,
    masterConfig,
    derivedEventText: buildEventSummary(frameInput.rawEvents),
    sideToMoveKingdom,
    sideToMoveKey: kingdomKeyById(referenceData, sideToMoveKingdom),
    sideToMoveLabel: kingdomLabel(referenceData, sideToMoveKingdom),
    activeKingdomLabel: kingdomLabel(referenceData, frameInput.activeKingdom),
    winnerLabel: frameInput.winner === null ? "-" : kingdomLabel(referenceData, frameInput.winner)
  };
}

function normalizeValidation(validation) {
  if (!validation || typeof validation !== "object") {
    return null;
  }

  return {
    valid: Boolean(validation.valid),
    activeKingInCheck: Boolean(validation.activeKingInCheck),
    projectedKingInCheck: Boolean(validation.projectedKingInCheck),
    hasAnyLegalResponse: validation.hasAnyLegalResponse !== false,
    requiresSingleResponseMove: Boolean(validation.requiresSingleResponseMove),
    hasQueuedMove: Boolean(validation.hasQueuedMove),
    bankrupt: Boolean(validation.bankrupt),
    projectedEndingGold: Number(validation.projectedEndingGold) || 0,
    errorMessage: validation.errorMessage || ""
  };
}

function toArray(value) {
  return Array.isArray(value) ? value : [];
}

function buildEventSummary(events) {
  if (!Array.isArray(events) || !events.length) {
    return "Aucun événement marquant sur cette frame.";
  }

  return events
    .slice(-3)
    .map(function (entry) {
      if (typeof entry === "string") {
        return entry;
      }
      return entry.message || entry.msg || entry.kindLabel || entry.kindKey || "Événement";
    })
    .join(" · ");
}

function currentFrame() {
  return state.replay ? state.replay.frames[state.frameIndex] : null;
}

function prepareCanvas(canvas) {
  const rect = canvas.getBoundingClientRect();
  const devicePixelRatio = Math.max(1, window.devicePixelRatio || 1);
  const targetWidth = Math.max(1, Math.floor(rect.width * devicePixelRatio));
  const targetHeight = Math.max(1, Math.floor(rect.height * devicePixelRatio));

  if (canvas.width !== targetWidth || canvas.height !== targetHeight) {
    canvas.width = targetWidth;
    canvas.height = targetHeight;
  }

  return {
    rect,
    devicePixelRatio,
    width: canvas.width,
    height: canvas.height
  };
}

function renderCanvasMessage(message) {
  const canvasInfo = prepareCanvas(refs.replayCanvas);
  const context = refs.replayCanvas.getContext("2d");
  context.imageSmoothingEnabled = false;

  context.setTransform(1, 0, 0, 1, 0, 0);
  context.clearRect(0, 0, canvasInfo.width, canvasInfo.height);
  context.fillStyle = "#060806";
  context.fillRect(0, 0, canvasInfo.width, canvasInfo.height);

  if (!message) {
    return;
  }

  context.fillStyle = "rgba(239, 229, 199, 0.78)";
  context.textAlign = "center";
  context.textBaseline = "middle";
  context.font = `${Math.max(14, Math.floor(canvasInfo.height * 0.024))}px AncgDisplay, sans-serif`;
  context.fillText(message, canvasInfo.width / 2, canvasInfo.height / 2, canvasInfo.width * 0.78);
}

function syncTimelineBounds() {
  if (!state.replay) {
    refs.turnSlider.min = "0";
    refs.turnSlider.max = "0";
    refs.turnSlider.value = "0";
    return;
  }

  const playbackWindow = currentPlaybackWindow();
  refs.turnSlider.min = String(playbackWindow.minFrameIndex);
  refs.turnSlider.max = String(playbackWindow.maxFrameIndex);
  refs.turnSlider.value = String(state.frameIndex);
}

function syncControlsState() {
  const frameCount = state.replay ? state.replay.frames.length : 0;
  const playbackWindow = currentPlaybackWindow();
  const playbackFrameCount = state.replay
    ? Math.max(0, playbackWindow.maxFrameIndex - playbackWindow.minFrameIndex + 1)
    : 0;
  const atStart = state.frameIndex <= playbackWindow.minFrameIndex;
  const atEnd = !state.replay || state.frameIndex >= playbackWindow.maxFrameIndex;
  const canNavigate = playbackFrameCount > 1;
  const isAutoPlaying = Boolean(state.autoPlayHandle);

  refs.playPauseButton.textContent = isAutoPlaying ? "Pause" : "Lire";
  refs.playPauseButton.title = isAutoPlaying ? "Mettre en pause" : "Lancer la lecture";
  refs.playPauseButton.setAttribute("aria-label", isAutoPlaying ? "Mettre en pause" : "Lancer la lecture");
  refs.firstTurnButton.disabled = !canNavigate || atStart;
  refs.prevTurnButton.disabled = !canNavigate || atStart;
  refs.nextTurnButton.disabled = !canNavigate || atEnd;
  refs.lastTurnButton.disabled = !canNavigate || atEnd;
  refs.playPauseButton.disabled = !canNavigate;
  refs.zoomInButton.disabled = frameCount === 0;
  refs.zoomOutButton.disabled = frameCount === 0;
  refs.turnSlider.disabled = !canNavigate;
}

function setFrameIndex(nextIndex) {
  if (!state.replay) {
    return;
  }

  const playbackWindow = currentPlaybackWindow();
  const bounded = clamp(nextIndex, playbackWindow.minFrameIndex, playbackWindow.maxFrameIndex);
  const frameChanged = bounded !== state.frameIndex;
  state.frameIndex = bounded;
  if (frameChanged && shouldUpdateCameraOnEveryTick) {
    state.camera.pendingRuleCameraUpdate = true;
  }
  renderCurrentFrame();
}

function startAutoplay() {
  const playbackWindow = currentPlaybackWindow();
  const playbackFrameCount = playbackWindow.maxFrameIndex - playbackWindow.minFrameIndex + 1;
  if (!state.replay || state.autoPlayHandle || playbackFrameCount < 2) {
    return;
  }

  if (isViewerSuspended()) {
    state.suspension.resumeAutoplayOnUnsuspend = true;
    return;
  }

  if (state.frameIndex >= playbackWindow.maxFrameIndex) {
    state.frameIndex = playbackWindow.initialFrameIndex;
    if (shouldUpdateCameraOnEveryTick) {
      state.camera.pendingRuleCameraUpdate = true;
    }
  }

  state.autoPlayHandle = window.setInterval(function () {
    if (!state.replay) {
      stopAutoplay();
      return;
    }

    const nextIndex = state.frameIndex + 1;
    if (nextIndex > playbackWindow.maxFrameIndex) {
      if (replayConfig.loopPlayback) {
        setFrameIndex(playbackWindow.minFrameIndex);
        return;
      }
      stopAutoplay();
      return;
    }

    setFrameIndex(nextIndex);
  }, replayConfig.autoplayIntervalMs);

  syncControlsState();
  renderCurrentFrame();
}

function stopAutoplay() {
  if (state.autoPlayHandle) {
    window.clearInterval(state.autoPlayHandle);
    state.autoPlayHandle = null;
  }

  syncControlsState();
}

function renderCurrentFrame(options = {}) {
  if (!options.force && isViewerSuspended()) {
    state.suspension.pendingRender = true;
    return;
  }

  state.suspension.pendingRender = false;

  const frame = currentFrame();
  if (!frame) {
    renderCanvasMessage(state.statusMessage);
    emitFrameState(null);
    return;
  }

  refs.turnSlider.value = String(state.frameIndex);
  refs.turnSlider.setAttribute("aria-valuetext", frame.label);
  refs.turnSlider.title = frame.label;
  syncControlsState();
  syncConfiguredCamera(frame);
  syncOverlays(frame);
  syncToastState(frame);
  renderCanvasFrame(frame);
  emitFrameState(frame);
}

function syncConfiguredCamera(frame) {
  if (!frame || !state.camera.pendingRuleCameraUpdate) {
    return;
  }

  state.camera.pendingRuleCameraUpdate = false;

  if (!trackedTargetConfig) {
    resetCameraToConfiguredInitialView(frame);
    return;
  }

  if (isWeatherTrackedTarget(trackedTargetConfig)) {
    syncTrackedWeatherCamera(frame, trackedTargetConfig);
    return;
  }

  const trackedPoint = resolveTrackedTargetGridPoint(frame, trackedTargetConfig);

  if (!trackedPoint) {
    return;
  }

  centerCameraOnGridPoint(frame, trackedPoint.x, trackedPoint.y);
}

function isWeatherTrackedTarget(trackedTarget) {
  return Boolean(
    trackedTarget
    && (
      trackedTarget.kind === "cloud"
      || trackedTarget.kind === "front"
      || trackedTarget.kind === "weather-front"
    )
  );
}

function syncTrackedWeatherCamera(frame, trackedTarget) {
  const trackingState = resolveTrackedWeatherCameraState(frame, trackedTarget);

  if (!trackingState || !trackingState.center) {
    resetCameraToFit(frame);
    return;
  }

  centerCameraOnGridPoint(frame, trackingState.center.x, trackingState.center.y);
  state.camera.zoom = trackingState.zoom;
}

function resolveTrackedWeatherCameraState(frame, trackedTarget) {
  const fronts = getTrackedWeatherFronts(frame, trackedTarget);
  const boardCenter = resolveBoardCenterGridPoint(frame);

  if (!fronts.length) {
    return boardCenter
      ? {
          center: boardCenter,
          zoom: 1
        }
      : null;
  }

  const centers = fronts
    .map(function (front) {
      return resolveWeatherFrontCenter(front);
    })
    .filter(Boolean);

  if (!centers.length) {
    return boardCenter
      ? {
          center: boardCenter,
          zoom: 1
        }
      : null;
  }

  const total = centers.reduce(function (accumulator, center) {
    return {
      x: accumulator.x + center.x,
      y: accumulator.y + center.y
    };
  }, { x: 0, y: 0 });
  const focusCenter = {
    x: total.x / centers.length,
    y: total.y / centers.length
  };

  return {
    center: focusCenter,
    zoom: resolveTrackedWeatherZoom(frame, fronts, focusCenter)
  };
}

function getTrackedWeatherFronts(frame, trackedTarget) {
  const fronts = getActiveWeatherFronts(frame);
  if (!fronts.length) {
    return [];
  }

  const requestedIndex = typeof trackedTarget.index === "number"
    ? trackedTarget.index
    : typeof trackedTarget.id === "number"
      ? trackedTarget.id
      : null;

  if (requestedIndex === null) {
    return fronts;
  }

  const front = fronts[requestedIndex];
  return front ? [front] : [];
}

function resolveTrackedWeatherZoom(frame, fronts, focusCenter) {
  const canvasInfo = prepareCanvas(refs.replayCanvas);
  const metrics = getBoardMetrics(frame);
  const baseScale = Math.min(
    canvasInfo.width / Math.max(metrics.boardWidth, 1),
    canvasInfo.height / Math.max(metrics.boardHeight, 1)
  );
  const preferredZoom = resolvePreferredTrackedWeatherZoom();

  if (!Number.isFinite(baseScale) || baseScale <= 0 || canvasInfo.width <= 1 || canvasInfo.height <= 1) {
    return preferredZoom;
  }

  const bounds = fronts.reduce(function (accumulator, front) {
    const nextBounds = resolveWeatherFrontBounds(front);
    if (!nextBounds) {
      return accumulator;
    }

    return accumulator
      ? {
          minX: Math.min(accumulator.minX, nextBounds.minX),
          maxX: Math.max(accumulator.maxX, nextBounds.maxX),
          minY: Math.min(accumulator.minY, nextBounds.minY),
          maxY: Math.max(accumulator.maxY, nextBounds.maxY)
        }
      : nextBounds;
  }, null);

  if (!bounds) {
    return preferredZoom;
  }

  const paddingCells = 2;
  const halfWidth = Math.max(focusCenter.x - bounds.minX, bounds.maxX - focusCenter.x) + paddingCells;
  const halfHeight = Math.max(focusCenter.y - bounds.minY, bounds.maxY - focusCenter.y) + paddingCells;
  const requiredWorldWidth = Math.max(1, halfWidth * 2 * metrics.cellSize);
  const requiredWorldHeight = Math.max(1, halfHeight * 2 * metrics.cellSize);
  const fitZoom = Math.min(
    canvasInfo.width / Math.max(requiredWorldWidth * baseScale, 1),
    canvasInfo.height / Math.max(requiredWorldHeight * baseScale, 1)
  );

  return clamp(Math.min(preferredZoom, fitZoom), MIN_CAMERA_ZOOM, MAX_CAMERA_ZOOM);
}

function resolvePreferredTrackedWeatherZoom() {
  const configuredInitialZoom = Number(replayConfig.initialZoom);
  if (!Number.isFinite(configuredInitialZoom)) {
    return 1;
  }

  return clamp(configuredInitialZoom, MIN_CAMERA_ZOOM, MAX_CAMERA_ZOOM);
}

function resolveWeatherFrontBounds(front) {
  const center = resolveWeatherFrontCenter(front);
  if (!center) {
    return null;
  }

  const radii = resolveWeatherFrontAxisAlignedHalfExtents(front);
  return {
    minX: center.x - radii.halfWidth,
    maxX: center.x + radii.halfWidth,
    minY: center.y - radii.halfHeight,
    maxY: center.y + radii.halfHeight
  };
}

function resolveWeatherFrontAxisAlignedHalfExtents(front) {
  const radiusAlong = Math.max(0, (Number(front && front.radiusAlongTimes1000) || 0) / 1000);
  const radiusAcross = Math.max(0, (Number(front && front.radiusAcrossTimes1000) || 0) / 1000);
  const angle = resolveWeatherFrontAxisAngleRadians(front);

  if (angle === null) {
    const radius = Math.max(radiusAlong, radiusAcross);
    return {
      halfWidth: radius,
      halfHeight: radius
    };
  }

  const cosAngle = Math.cos(angle);
  const sinAngle = Math.sin(angle);

  return {
    halfWidth: Math.sqrt((radiusAlong * radiusAlong * cosAngle * cosAngle) + (radiusAcross * radiusAcross * sinAngle * sinAngle)),
    halfHeight: Math.sqrt((radiusAlong * radiusAlong * sinAngle * sinAngle) + (radiusAcross * radiusAcross * cosAngle * cosAngle))
  };
}

function resolveWeatherFrontAxisAngleRadians(front) {
  const directionKey = resolveWeatherFrontDirectionKey(front);
  switch (directionKey) {
    case "east":
    case "west":
      return 0;
    case "north":
    case "south":
      return Math.PI / 2;
    case "north_east":
    case "south_west":
      return Math.PI / 4;
    case "north_west":
    case "south_east":
      return (3 * Math.PI) / 4;
    default:
      return null;
  }
}

function syncOverlays(frame) {
  if (!frame) {
    refs.activeTurnOverlay.hidden = true;
    refs.perspectiveOverlay.hidden = true;
    return;
  }

  refs.activeTurnOverlay.hidden = false;
  refs.activeKingdomLabel.textContent = frame.sideToMoveLabel;
  refs.activeTurnValue.textContent = formatTurnOverlayLabel(frame);
  refs.activeKingdomShield.src = resolveUrl(`${replayConfig.assetRoot}/${KINGDOM_SHIELD_PATHS[frame.sideToMoveKey] || KINGDOM_SHIELD_PATHS.white}`);
  refs.activeKingdomShield.alt = `Bouclier ${frame.sideToMoveLabel}`;

  const perspective = resolvePerspectivePresentation(frame);
  if (!perspective) {
    refs.perspectiveOverlay.hidden = true;
  } else {
    refs.perspectiveOverlay.hidden = false;
    refs.perspectiveKingdomValue.textContent = perspective.label;
    refs.perspectiveKingdomShield.src = resolveUrl(
      `${replayConfig.assetRoot}/${KINGDOM_SHIELD_PATHS[perspective.kingdomKey] || KINGDOM_SHIELD_PATHS.white}`
    );
    refs.perspectiveKingdomShield.alt = `Bouclier ${perspective.label}`;
  }
}

function syncToastState(frame) {
  if (!frame) {
    clearAllToastSlots();
    emitToastState(null);
    return;
  }

  syncToastSlot("status", resolveStatusToast(frame), state.frameIndex);
  syncToastSlot("chest", resolveChestRewardToast(frame), state.frameIndex);

  emitToastState(frame);
}

function resolveChestRewardToast(frame) {
  const notification = toArray(frame.notifications).find(function (entry) {
    return entry
      && entry.kindKey === "chest_reward"
      && entry.chestReward
      && (typeof entry.kingdomKey === "string" || typeof entry.kingdom === "number");
  });

  if (!notification) {
    return null;
  }

  const kingdomKey = resolveNotificationKingdomKey(frame.referenceData, notification);
  const kingdomLabel = toastKingdomLabel(kingdomKey);
  return buildToastViewModel({
    id: buildChestToastId(notification, kingdomKey, state.frameIndex),
    slotKey: "chest",
    kingdomKey,
    kingdomLabel,
    message: `A gagné ${formatChestRewardToastText(notification)}.`,
    tone: "success",
    priority: 20
  });
}

function buildChestToastId(notification, kingdomKey, sourceFrameIndex) {
  const rewardTypeKey = notification && typeof notification.chestRewardTypeKey === "string"
    ? notification.chestRewardTypeKey
    : notification && notification.chestReward && typeof notification.chestReward.typeKey === "string"
      ? notification.chestReward.typeKey
      : "gold";
  const rewardAmount = notification && Number.isFinite(notification.chestRewardAmount)
    ? Math.trunc(notification.chestRewardAmount)
    : notification && notification.chestReward && Number.isFinite(notification.chestReward.amount)
      ? Math.trunc(notification.chestReward.amount)
      : 0;

  return `chest-reward:${sourceFrameIndex}:${kingdomKey}:${rewardTypeKey}:${rewardAmount}`;
}

function resolveNotificationKingdomKey(referenceData, notification) {
  if (notification && typeof notification.kingdomKey === "string" && KINGDOM_SHIELD_PATHS[notification.kingdomKey]) {
    return notification.kingdomKey;
  }

  const fallbackKingdomKey = kingdomKeyById(referenceData, notification && notification.kingdom);
  return KINGDOM_SHIELD_PATHS[fallbackKingdomKey] ? fallbackKingdomKey : "white";
}

function toastKingdomLabel(kingdomKey) {
  return kingdomKey === "black" ? "Noirs" : "Blancs";
}

function formatChestRewardToastText(notification) {
  const reward = notification && notification.chestReward ? notification.chestReward : null;
  const rewardTypeKey = reward && typeof reward.typeKey === "string"
    ? reward.typeKey
    : notification && typeof notification.chestRewardTypeKey === "string"
      ? notification.chestRewardTypeKey
      : "gold";
  const rewardAmount = Number(
    reward && Number.isFinite(reward.amount)
      ? reward.amount
      : notification && Number.isFinite(notification.chestRewardAmount)
        ? notification.chestRewardAmount
        : 0
  );
  const signedAmount = `${rewardAmount >= 0 ? "+" : ""}${Math.trunc(rewardAmount)}`;

  if (rewardTypeKey === "movement_points_max_bonus") {
    return `${signedAmount} point${Math.abs(Math.trunc(rewardAmount)) > 1 ? "s" : ""} de mouvement max par tour`;
  }

  if (rewardTypeKey === "build_points_max_bonus") {
    return `${signedAmount} point${Math.abs(Math.trunc(rewardAmount)) > 1 ? "s" : ""} de construction max par tour`;
  }

  return `${signedAmount} or`;
}

function buildToastViewModel({ id, kingdomKey, kingdomLabel, message, priority, tone }) {
  return {
    id,
    label: kingdomLabel,
    message,
    priority,
    shieldAlt: `Bouclier ${kingdomLabel}`,
    shieldSrc: resolveUrl(
      `${replayConfig.assetRoot}/${KINGDOM_SHIELD_PATHS[kingdomKey] || KINGDOM_SHIELD_PATHS.white}`
    ),
    tone
  };
}

function createToastSlotState() {
  return {
    current: null,
    sourceFrameIndex: null,
    hideHandle: null
  };
}

function syncToastSlot(slotKey, nextToast, sourceFrameIndex) {
  const slot = state.toast.slots[slotKey];
  if (!slot) {
    return;
  }

  if (nextToast) {
    clearToastDismissTimer(slot);
    slot.current = nextToast;
    slot.sourceFrameIndex = sourceFrameIndex;
    return;
  }

  if (!slot.current) {
    clearToastDismissTimer(slot);
    return;
  }

  if (slot.sourceFrameIndex === state.frameIndex) {
    clearToastDismissTimer(slot);
    return;
  }

  scheduleToastDismiss(slotKey);
}

function clearToastSlot(slotKey) {
  const slot = state.toast.slots[slotKey];
  if (!slot) {
    return;
  }

  clearToastDismissTimer(slot);
  slot.current = null;
  slot.sourceFrameIndex = null;
}

function clearAllToastSlots() {
  for (const slotKey of Object.keys(state.toast.slots)) {
    clearToastSlot(slotKey);
  }
}

function clearAllToastDismissTimers() {
  for (const slot of Object.values(state.toast.slots)) {
    clearToastDismissTimer(slot);
  }
}

function hideToastSlot(slotKey) {
  clearToastSlot(slotKey);
  emitToastState(currentFrame());
}

function scheduleToastDismiss(slotKey) {
  const slot = state.toast.slots[slotKey];
  if (!slot) {
    return;
  }

  if (toastDismissDelayMs <= 0) {
    hideToastSlot(slotKey);
    return;
  }

  if (slot.hideHandle) {
    return;
  }

  slot.hideHandle = window.setTimeout(function () {
    slot.hideHandle = null;
    if (!slot.current) {
      return;
    }

    if (slot.sourceFrameIndex === state.frameIndex) {
      return;
    }

    hideToastSlot(slotKey);
  }, toastDismissDelayMs);
}

function clearToastDismissTimer(slot) {
  if (!slot || !slot.hideHandle) {
    return;
  }

  window.clearTimeout(slot.hideHandle);
  slot.hideHandle = null;
}

function normalizeToastCooldownMs(value) {
  const parsedValue = Number(value);
  if (!Number.isFinite(parsedValue)) {
    return 0;
  }

  return Math.max(0, parsedValue);
}

function emitToastState(frame) {
  if (!onToastStateChange) {
    return;
  }

  onToastStateChange(buildToastStack(frame));
}

function buildToastStack(frame) {
  if (!frame) {
    return [];
  }

  const items = [];
  for (const slotKey of Object.keys(state.toast.slots)) {
    const slot = state.toast.slots[slotKey];
    if (slot && slot.current) {
      items.push(slot.current);
    }
  }

  return items.sort(function (left, right) {
    return left.priority - right.priority;
  });
}

function emitFrameState(frame) {
  if (!onFrameStateChange) {
    return;
  }

  if (!frame) {
    onFrameStateChange(null);
    return;
  }

  onFrameStateChange({
    frameIndex: state.frameIndex,
    committedTurnNumber: frame.committedTurnNumber,
    label: frame.label
  });
}

function resolveStatusToast(frame) {
  const validation = frame.nextTurnValidation || frame.activeValidation;
  const kingdomKey = KINGDOM_SHIELD_PATHS[frame.sideToMoveKey] ? frame.sideToMoveKey : "white";
  const kingdomLabel = frame.sideToMoveLabel || toastKingdomLabel(kingdomKey);

  if (frame.gameOver) {
    return buildToastViewModel({
      id: `status-checkmate:${kingdomKey}`,
      slotKey: "status",
      kingdomKey,
      kingdomLabel,
      message: "Échec et mat",
      tone: "danger",
      priority: 10
    });
  }

  if (validation && validation.activeKingInCheck) {
    return buildToastViewModel({
      id: `status-check:${kingdomKey}`,
      slotKey: "status",
      kingdomKey,
      kingdomLabel,
      message: "Échec",
      tone: "warning",
      priority: 10
    });
  }

  return null;
}

function formatTurnOverlayLabel(frame) {
  const committedTurnNumber = Number(frame && frame.committedTurnNumber);
  const safeTurnNumber = Number.isFinite(committedTurnNumber)
    ? Math.max(0, Math.trunc(committedTurnNumber))
    : 0;

  return `Tour ${safeTurnNumber}`;
}

function renderCanvasFrame(frame) {
  const canvasInfo = prepareCanvas(refs.replayCanvas);
  const canvas = refs.replayCanvas;
  const context = canvas.getContext("2d");
  context.imageSmoothingEnabled = false;

  context.setTransform(1, 0, 0, 1, 0, 0);
  context.clearRect(0, 0, canvas.width, canvas.height);

  const transform = computeBoardTransform(frame, canvasInfo.width, canvasInfo.height);

  context.fillStyle = "#090c0b";
  context.fillRect(0, 0, canvas.width, canvas.height);

  drawTerrain(context, frame, transform);
  drawBuildings(context, frame, transform);
  drawMapObjects(context, frame, transform);
  drawPieces(context, frame, transform);
  drawAutonomousUnits(context, frame, transform);
  drawWeather(context, frame, transform);
}

function computeBoardTransform(frame, canvasWidth, canvasHeight) {
  const metrics = ensureCameraForFrame(frame);
  const baseScale = Math.min(
    canvasWidth / Math.max(metrics.boardWidth, 1),
    canvasHeight / Math.max(metrics.boardHeight, 1)
  );
  const scale = Math.max(0.0001, baseScale * state.camera.zoom);

  const offsetX = (canvasWidth / 2) - (state.camera.centerWorldX * scale);
  const offsetY = (canvasHeight / 2) - (state.camera.centerWorldY * scale);

  return {
    ...metrics,
    baseScale,
    scaledCellSize: metrics.cellSize * scale,
    scale,
    offsetX,
    offsetY,
    canvasWidth,
    canvasHeight
  };
}

function getBoardMetrics(frame) {
  const gridHeight = frame.grid.length;
  const gridWidth = gridHeight ? frame.grid[0].length : 0;
  const cellSize = getCellSize(frame.masterConfig);

  return {
    cellSize,
    gridWidth,
    gridHeight,
    boardWidth: gridWidth * cellSize,
    boardHeight: gridHeight * cellSize,
    boardKey: `${gridWidth}x${gridHeight}:${cellSize}`
  };
}

function ensureCameraForFrame(frame) {
  const metrics = getBoardMetrics(frame);
  if (!state.camera.initialized || state.camera.boardKey !== metrics.boardKey) {
    resetCameraToFit(frame, metrics);
  }
  return metrics;
}

function resetCameraToFit(frame, metrics = getBoardMetrics(frame)) {
  state.camera.zoom = 1;
  state.camera.centerWorldX = metrics.boardWidth / 2;
  state.camera.centerWorldY = metrics.boardHeight / 2;
  state.camera.initialized = true;
  state.camera.boardKey = metrics.boardKey;
}

function resetCameraToConfiguredInitialView(frame, metrics = getBoardMetrics(frame)) {
  resetCameraToFit(frame, metrics);

  const configuredInitialZoom = Number(replayConfig.initialZoom);
  if (!Number.isFinite(configuredInitialZoom)) {
    return;
  }

  state.camera.zoom = clamp(configuredInitialZoom, MIN_CAMERA_ZOOM, MAX_CAMERA_ZOOM);
}

function centerCameraOnGridPoint(frame, gridX, gridY) {
  const metrics = getBoardMetrics(frame);
  state.camera.centerWorldX = gridX * metrics.cellSize;
  state.camera.centerWorldY = gridY * metrics.cellSize;
  state.camera.initialized = true;
  state.camera.boardKey = metrics.boardKey;
}

function centerCameraOnCell(frame, x, y) {
  centerCameraOnGridPoint(frame, x + 0.5, y + 0.5);
}

function screenToWorld(screenX, screenY, transform) {
  return {
    x: (screenX - transform.offsetX) / transform.scale,
    y: (screenY - transform.offsetY) / transform.scale
  };
}

function setCameraZoom(frame, nextZoom, anchorScreenX, anchorScreenY, canvasWidth, canvasHeight) {
  const transform = computeBoardTransform(frame, canvasWidth, canvasHeight);
  const clampedZoom = clamp(nextZoom, MIN_CAMERA_ZOOM, MAX_CAMERA_ZOOM);
  if (clampedZoom === state.camera.zoom) {
    return false;
  }

  const anchorWorld = screenToWorld(anchorScreenX, anchorScreenY, transform);
  state.camera.zoom = clampedZoom;

  const nextScale = transform.baseScale * clampedZoom;
  state.camera.centerWorldX = anchorWorld.x - ((anchorScreenX - (canvasWidth / 2)) / nextScale);
  state.camera.centerWorldY = anchorWorld.y - ((anchorScreenY - (canvasHeight / 2)) / nextScale);

  return true;
}

function zoomCameraFromKeyboard(factor) {
  const frame = currentFrame();
  if (!frame) {
    return;
  }

  const canvasInfo = prepareCanvas(refs.replayCanvas);
  const nextZoom = clamp(state.camera.zoom * factor, MIN_CAMERA_ZOOM, MAX_CAMERA_ZOOM);

  if (!setCameraZoom(
    frame,
    nextZoom,
    canvasInfo.width / 2,
    canvasInfo.height / 2,
    canvasInfo.width,
    canvasInfo.height
  )) {
    return;
  }

  renderCurrentFrame();
}

function onCanvasPointerDown(event) {
  if (!state.replay) {
    return;
  }

  if (event.pointerType !== "touch" && event.button !== 0) {
    return;
  }

  if (typeof rootElement.focus === "function") {
    rootElement.focus({ preventScroll: true });
  }

  state.camera.isDragging = true;
  state.camera.pointerId = event.pointerId;
  state.camera.lastClientX = event.clientX;
  state.camera.lastClientY = event.clientY;
  state.camera.dragStartClientX = event.clientX;
  state.camera.dragStartClientY = event.clientY;
  state.camera.didDrag = false;
  refs.replayCanvas.classList.add("is-dragging");
  refs.replayCanvas.setPointerCapture(event.pointerId);
  event.preventDefault();
}

function onCanvasPointerMove(event) {
  if (!state.camera.isDragging || state.camera.pointerId !== event.pointerId) {
    return;
  }

  const frame = currentFrame();
  if (!frame) {
    return;
  }

  const canvasInfo = prepareCanvas(refs.replayCanvas);
  const transform = computeBoardTransform(frame, canvasInfo.width, canvasInfo.height);
  const deltaX = (event.clientX - state.camera.lastClientX) * canvasInfo.devicePixelRatio;
  const deltaY = (event.clientY - state.camera.lastClientY) * canvasInfo.devicePixelRatio;
  const travelledX = event.clientX - state.camera.dragStartClientX;
  const travelledY = event.clientY - state.camera.dragStartClientY;

  if (Math.hypot(travelledX, travelledY) >= CLICK_DRAG_THRESHOLD_PX) {
    state.camera.didDrag = true;
  }

  state.camera.centerWorldX -= deltaX / transform.scale;
  state.camera.centerWorldY -= deltaY / transform.scale;
  state.camera.lastClientX = event.clientX;
  state.camera.lastClientY = event.clientY;
  renderCurrentFrame();
  event.preventDefault();
}

function onCanvasPointerUp(event) {
  if (state.camera.pointerId !== event.pointerId) {
    return;
  }

  const shouldInspectCell = event.type === "pointerup"
    && isCellDebugEnabled
    && !state.camera.didDrag;

  state.camera.isDragging = false;
  state.camera.pointerId = null;
  refs.replayCanvas.classList.remove("is-dragging");

  if (refs.replayCanvas.hasPointerCapture(event.pointerId)) {
    refs.replayCanvas.releasePointerCapture(event.pointerId);
  }

  if (shouldInspectCell) {
    inspectCellFromPointerEvent(event);
  }
}

function inspectCellFromPointerEvent(event) {
  const frame = currentFrame();
  if (!frame) {
    return;
  }

  const canvasInfo = prepareCanvas(refs.replayCanvas);
  const transform = computeBoardTransform(frame, canvasInfo.width, canvasInfo.height);
  const screenX = (event.clientX - canvasInfo.rect.left) * canvasInfo.devicePixelRatio;
  const screenY = (event.clientY - canvasInfo.rect.top) * canvasInfo.devicePixelRatio;
  const world = screenToWorld(screenX, screenY, transform);
  const cellX = Math.floor(world.x / transform.cellSize);
  const cellY = Math.floor(world.y / transform.cellSize);

  if (!isGridCoordinateInside(frame, cellX, cellY)) {
    console.info("[Replay debug] Clic hors de la grille.", {
      cell: { x: cellX, y: cellY },
      frame: frame.label
    });
    return;
  }

  logCellDebug(frame, cellX, cellY);
}

function logCellDebug(frame, cellX, cellY) {
  const payload = buildCellDebugPayload(frame, cellX, cellY);
  const title = `[Replay debug] Cellule (${cellX}, ${cellY}) · ${frame.label}`;

  if (typeof console.groupCollapsed === "function") {
    console.groupCollapsed(title);
    console.log("Resume", payload);
    console.log("Terrain", payload.terrain);
    console.log("Meteo", payload.weather);
    console.log("Pieces", payload.pieces);
    console.log("Batiments", payload.buildings);
    console.log("Cellules de batiment", payload.buildingCells);
    console.log("Objets", payload.mapObjects);
    console.log("Unites autonomes", payload.autonomousUnits);
    console.groupEnd();
    return;
  }

  console.log(title, payload);
}

function buildCellDebugPayload(frame, cellX, cellY) {
  const terrainCell = frame.grid[cellY] && frame.grid[cellY][cellX]
    ? frame.grid[cellY][cellX]
    : null;
  const buildingsAnalytics = toArray(frame.buildingsAnalytics);
  const matchingBuildingCells = [];
  const matchingBuildings = [];
  const seenBuildingIds = new Set();

  for (const building of buildingsAnalytics) {
    const matchingCells = toArray(building.cells).filter(function (cell) {
      const worldCell = cell && cell.worldCell;
      return worldCell && worldCell.x === cellX && worldCell.y === cellY;
    });

    if (!matchingCells.length) {
      continue;
    }

    if (!seenBuildingIds.has(building.id)) {
      seenBuildingIds.add(building.id);
      matchingBuildings.push(summarizeBuilding(building));
    }

    for (const cell of matchingCells) {
      matchingBuildingCells.push({
        buildingId: building.id,
        buildingTypeKey: building.buildingTypeKey || buildingTypeKey(frame.referenceData, building.buildingTypeId),
        ownerKingdomKey: building.ownerKingdomKey || kingdomKeyById(frame.referenceData, building.ownerKingdomId),
        destroyed: Boolean(cell.destroyed),
        breached: Boolean(cell.breached),
        hp: typeof cell.hp === "number" ? cell.hp : null,
        hiddenFromWhite: readPerspectiveHiddenFlag(cell, "white"),
        hiddenFromBlack: readPerspectiveHiddenFlag(cell, "black")
      });
    }
  }

  if (!matchingBuildings.length) {
    for (const building of toArray(frame.buildings)) {
      if (!legacyBuildingOccupiesCell(building, cellX, cellY)) {
        continue;
      }
      matchingBuildings.push(summarizeLegacyBuilding(frame.referenceData, building));
    }
  }

  return {
    cell: {
      x: cellX,
      y: cellY,
      frame: frame.label
    },
    terrain: summarizeTerrainCell(terrainCell),
    weather: {
      fogAlpha: weatherAlphaAtCell(frame, cellX, cellY),
      concealingFog: cellHasConcealingFog(frame, cellX, cellY),
      activeFronts: getActiveWeatherFronts(frame).map(function (front, index) {
        return summarizeWeatherFront(front, index);
      })
    },
    pieces: resolveDebugPieces(frame)
      .filter(function (piece) {
        const position = resolvePosition(piece);
        return position && position.x === cellX && position.y === cellY;
      })
      .map(function (piece) {
        return summarizePiece(frame.referenceData, piece);
      }),
    buildings: matchingBuildings,
    buildingCells: matchingBuildingCells,
    mapObjects: toArray(frame.mapObjects)
      .filter(function (object) {
        const position = resolvePosition(object);
        return position && position.x === cellX && position.y === cellY;
      })
      .map(summarizeMapObject),
    autonomousUnits: toArray(frame.autonomousUnits)
      .filter(function (unit) {
        const position = resolvePosition(unit);
        return position && position.x === cellX && position.y === cellY;
      })
      .map(function (unit) {
        return summarizeAutonomousUnit(frame.referenceData, unit);
      })
  };
}

function onCanvasWheel(event) {
  const frame = currentFrame();
  if (!frame) {
    return;
  }

  const zoomMultiplier = resolveWheelZoomMultiplier(event);
  if (!zoomMultiplier || zoomMultiplier === 1) {
    return;
  }

  event.preventDefault();

  const canvasInfo = prepareCanvas(refs.replayCanvas);
  const nextZoom = clamp(
    state.camera.zoom * zoomMultiplier,
    MIN_CAMERA_ZOOM,
    MAX_CAMERA_ZOOM
  );

  if (!setCameraZoom(
    frame,
    nextZoom,
    (event.clientX - canvasInfo.rect.left) * canvasInfo.devicePixelRatio,
    (event.clientY - canvasInfo.rect.top) * canvasInfo.devicePixelRatio,
    canvasInfo.width,
    canvasInfo.height
  )) {
    return;
  }

  renderCurrentFrame();
}

function resolveWheelZoomMultiplier(event) {
  if (!event || event.deltaY === 0) {
    return null;
  }

  if (isTrackpadPinchWheelEvent(event)) {
    return resolveRelativeZoomMultiplier(-event.deltaY / CTRL_WHEEL_ZOOM_DELTA_STEP);
  }

  if (isTrackpadScrollWheelEvent(event)) {
    return null;
  }

  return event.deltaY < 0 ? CAMERA_ZOOM_STEP_FACTOR : 1 / CAMERA_ZOOM_STEP_FACTOR;
}

function resolveRelativeZoomMultiplier(relativeZoomSteps) {
  if (!Number.isFinite(relativeZoomSteps) || relativeZoomSteps === 0) {
    return null;
  }

  return Math.pow(CAMERA_ZOOM_STEP_FACTOR, clamp(relativeZoomSteps, -1, 1));
}

function isTrackpadPinchWheelEvent(event) {
  return Boolean(event && event.ctrlKey);
}

function isTrackpadScrollWheelEvent(event) {
  if (!event || event.ctrlKey) {
    return false;
  }

  return event.deltaMode === WheelEvent.DOM_DELTA_PIXEL;
}

function drawTerrain(context, frame, transform) {
  for (let y = 0; y < frame.grid.length; y += 1) {
    const row = frame.grid[y];
    for (let x = 0; x < row.length; x += 1) {
      const cell = row[x];
      if (!cell || !cell.c) {
        continue;
      }

      const cellKey = CELL_TYPE_KEYS[cell.t] || "grass";
      const texturePath = `${replayConfig.assetRoot}/textures/cells/${cellKey}.png`;
      const image = state.textures.get(texturePath) || null;
      const brightness = typeof cell.b === "number" ? clamp(cell.b, 0, 255) / 255 : 1;
      const screen = cellRect(x, y, transform);

      if (image) {
        drawCellImage(context, image, screen, 0, cell.f || 0, brightness);
      } else {
        context.fillStyle = shadeColor(CELL_FALLBACK_COLORS[cellKey], brightness);
        context.fillRect(screen.x, screen.y, screen.width, screen.height);
      }
    }
  }
}

function drawBuildings(context, frame, transform) {
  const perspective = resolvePerspectivePresentation(frame);
  if (Array.isArray(frame.buildingsAnalytics) && frame.buildingsAnalytics.length) {
    const damagedOpacity = getDamagedStructureOpacity(frame.masterConfig);
    for (const building of frame.buildingsAnalytics) {
      const buildingKey = building.buildingTypeKey;
      const usesChunkedTextures = Boolean(CHUNKED_BUILDINGS[buildingKey]);
      const occupiedCells = usesChunkedTextures ? buildOccupiedBuildingCellSet(building.cells) : null;
      const rotationQuarterTurns = Number(building.rotationQuarterTurns) || 0;
      const flipMask = Number(building.flipMask) || 0;
      const isPublic = Boolean(building.isPublic);
      for (const cell of toArray(building.cells)) {
        if (shouldHideBuildingCellForPerspective(cell, building, perspective, frame)) {
          continue;
        }
        const worldCell = cell.worldCell;
        if (!worldCell) {
          continue;
        }
        let screen = cellRect(worldCell.x, worldCell.y, transform);
        if (occupiedCells) {
          screen = expandBuildingCellRect(screen, worldCell, occupiedCells);
        }
        const texturePath = resolveBuildingTexturePath(buildingKey, cell.sourceLocal);
        const image = texturePath ? state.textures.get(texturePath) || null : null;
        const breached = Boolean(cell.breached);
        const destroyed = Boolean(cell.destroyed);
        const cellOpacity = (!isPublic && (destroyed || breached)) ? damagedOpacity : 1;

        if (image) {
          drawCellImage(context, image, screen, rotationQuarterTurns, flipMask, cellOpacity);
        } else {
          context.fillStyle = fallbackBuildingColor(buildingKey, cellOpacity);
          context.fillRect(screen.x, screen.y, screen.width, screen.height);
        }
      }
    }
    return;
  }

  for (const building of frame.buildings) {
    const buildingKey = buildingTypeKey(frame.referenceData, building.type);
    const baseWidth = Number(building.w) || 1;
    const baseHeight = Number(building.h) || 1;
    const usesChunkedTextures = Boolean(CHUNKED_BUILDINGS[buildingKey]);
    const rotationQuarterTurns = Number(building.rot) || 0;
    const flipMask = Number(building.fm) || 0;
    const footprintWidth = getBuildingFootprintWidth(baseWidth, baseHeight, rotationQuarterTurns);
    const footprintHeight = getBuildingFootprintHeight(baseWidth, baseHeight, rotationQuarterTurns);
    const occupiedCells = usesChunkedTextures
      ? buildLegacyOccupiedBuildingCellSet(building, footprintWidth, footprintHeight)
      : null;
    for (let dy = 0; dy < footprintHeight; dy += 1) {
      for (let dx = 0; dx < footprintWidth; dx += 1) {
        const worldX = (Number(building.ox) || 0) + dx;
        const worldY = (Number(building.oy) || 0) + dy;
        if (shouldHideLegacyBuildingCellForPerspective(building, worldX, worldY, perspective, frame)) {
          continue;
        }
        let screen = cellRect(worldX, worldY, transform);
        if (occupiedCells) {
          screen = expandBuildingCellRect(screen, { x: worldX, y: worldY }, occupiedCells);
        }
        const sourceLocal = usesChunkedTextures
          ? mapFootprintToSourceLocalFor(dx, dy, baseWidth, baseHeight, rotationQuarterTurns, flipMask)
          : null;
        const texturePath = resolveBuildingTexturePath(buildingKey, sourceLocal);
        const image = texturePath ? state.textures.get(texturePath) || null : null;
        if (image) {
          drawCellImage(context, image, screen, rotationQuarterTurns, flipMask, 1);
        } else {
          context.fillStyle = fallbackBuildingColor(buildingKey, 1);
          context.fillRect(screen.x, screen.y, screen.width, screen.height);
        }
      }
    }
  }
}

function drawMapObjects(context, frame, transform) {
  for (const object of frame.mapObjects) {
    const position = resolvePosition(object);
    if (!position) {
      continue;
    }
    const screen = cellRect(position.x, position.y, transform);
    const chestPath = `${replayConfig.assetRoot}/textures/objects/chest.png`;
    const image = state.textures.get(chestPath) || null;
    if (image) {
      drawCellImage(context, image, screen, 0, 0, 1);
    } else {
      context.fillStyle = "#c59645";
      context.fillRect(
        screen.x + screen.width * 0.16,
        screen.y + screen.height * 0.16,
        screen.width * 0.68,
        screen.height * 0.68
      );
    }
  }
}

function drawPieces(context, frame, transform) {
  const perspective = resolvePerspectivePresentation(frame);
  const pieces = Array.isArray(frame.piecesAnalytics) && frame.piecesAnalytics.length
    ? frame.piecesAnalytics
    : frame.pieces;

  for (const piece of pieces) {
    if (shouldHidePieceForPerspective(piece, perspective, frame)) {
      continue;
    }

    const position = resolvePosition(piece);
    if (!position) {
      continue;
    }

    const typeKey = pieceTypeKey(frame.referenceData, resolvePieceTypeId(piece));
    const kingdomKey = resolveKingdomKey(frame.referenceData, piece);
    const texturePath = `${replayConfig.assetRoot}/textures/pieces/${kingdomKey}/${typeKey}.png`;
    const image = state.textures.get(texturePath) || null;
    const screen = cellRect(position.x, position.y, transform);

    if (image) {
      drawCellImage(context, image, screen, 0, 0, 1);
    } else {
      context.fillStyle = kingdomKey === "white" ? "#e8deca" : "#3f3122";
      context.beginPath();
      context.arc(screen.centerX, screen.centerY, Math.min(screen.width, screen.height) * 0.34, 0, Math.PI * 2);
      context.fill();
    }

    if (typeof piece.xp === "number" && piece.xp > 0) {
      context.fillStyle = "rgba(242, 217, 145, 0.88)";
      context.fillRect(
        screen.x + screen.width * 0.12,
        screen.y + screen.height * 0.76,
        Math.max(2, Math.min(screen.width, screen.height) * 0.1),
        Math.max(2, Math.min(screen.width, screen.height) * 0.1)
      );
    }
  }
}

function drawAutonomousUnits(context, frame, transform) {
  const perspective = resolvePerspectivePresentation(frame);
  for (const unit of frame.autonomousUnits) {
    if (shouldHideAutonomousUnitForPerspective(unit, perspective, frame)) {
      continue;
    }

    const position = resolvePosition(unit);
    if (!position) {
      continue;
    }

    const typeKey = resolveAutonomousUnitPieceTypeKey(frame.referenceData, unit);
    const texturePath = `${replayConfig.assetRoot}/textures/pieces/evil/${typeKey}.png`;
    const image = state.textures.get(texturePath) || null;
    const screen = cellRect(position.x, position.y, transform);

    if (image) {
      drawCellImage(context, image, screen, 0, 0, 1);
    } else {
      context.fillStyle = "#7d1d1d";
      context.beginPath();
      context.arc(screen.centerX, screen.centerY, Math.min(screen.width, screen.height) * 0.32, 0, Math.PI * 2);
      context.fill();
    }
  }
}

function drawWeather(context, frame, transform) {
  const weatherMask = getWeatherMask(frame);

  if (!weatherMask || !Array.isArray(weatherMask.alphaByCell) || !Array.isArray(weatherMask.shadeByCell)) {
    return;
  }

  const diameter = Number(weatherMask.diameter) || frame.grid.length;
  if (!diameter) {
    return;
  }

  for (let y = 0; y < diameter; y += 1) {
    for (let x = 0; x < diameter; x += 1) {
      const index = (y * diameter) + x;
      const alpha = Number(weatherMask.alphaByCell[index]) || 0;
      if (alpha <= 0) {
        continue;
      }
      const shade = clamp(Number(weatherMask.shadeByCell[index]) || 0, 0, 255);
      const screen = cellRect(x, y, transform);
      context.fillStyle = `rgba(${shade}, ${shade}, ${shade}, ${alpha / 255})`;
      context.fillRect(screen.x, screen.y, screen.width, screen.height);
    }
  }
}

function getWeatherMask(frame) {
  return frame
    && frame.snapshot
    && frame.snapshot.weatherState
    && frame.snapshot.weatherState.mask
    ? frame.snapshot.weatherState.mask
    : null;
}

function weatherAlphaAtCell(frame, x, y) {
  const weatherMask = getWeatherMask(frame);
  if (!weatherMask || !Array.isArray(weatherMask.alphaByCell) || !Array.isArray(weatherMask.shadeByCell)) {
    return 0;
  }

  const diameter = Number(weatherMask.diameter) || frame.grid.length;
  if (!Number.isFinite(diameter) || diameter <= 0 || x < 0 || y < 0 || x >= diameter || y >= diameter) {
    return 0;
  }

  const index = (y * diameter) + x;
  return clamp(Number(weatherMask.alphaByCell[index]) || 0, 0, 255);
}

function cellHasConcealingFog(frame, x, y) {
  return weatherAlphaAtCell(frame, x, y) >= CONCEALING_FOG_ALPHA_THRESHOLD;
}

function shouldHidePieceForPerspective(piece, perspective, frame) {
  if (!perspective) {
    return false;
  }

  const explicitHidden = readPerspectiveHiddenFlag(piece, perspective.kingdomKey);
  if (typeof explicitHidden === "boolean") {
    return explicitHidden;
  }

  const kingdomId = resolveKingdomId(piece);
  if (kingdomId === null || perspective.kingdomId === null || kingdomId === perspective.kingdomId) {
    return false;
  }

  const position = resolvePosition(piece);
  return Boolean(position) && cellHasConcealingFog(frame, position.x, position.y);
}

function shouldHideBuildingCellForPerspective(cell, building, perspective, frame) {
  if (!perspective) {
    return false;
  }

  const explicitCellHidden = readPerspectiveHiddenFlag(cell, perspective.kingdomKey);
  if (typeof explicitCellHidden === "boolean") {
    return explicitCellHidden;
  }

  const explicitBuildingHidden = readPerspectiveHiddenFlag(building, perspective.kingdomKey);
  if (typeof explicitBuildingHidden === "boolean" && !explicitBuildingHidden) {
    return false;
  }

  if (Boolean(building && building.isPublic)) {
    return false;
  }

  const ownerKingdomId = resolveOwnerKingdomId(building);
  if (ownerKingdomId === null || perspective.kingdomId === null || ownerKingdomId === perspective.kingdomId) {
    return false;
  }

  const position = resolvePosition(cell);
  return Boolean(position) && cellHasConcealingFog(frame, position.x, position.y);
}

function shouldHideLegacyBuildingCellForPerspective(building, x, y, perspective, frame) {
  if (!perspective) {
    return false;
  }

  if (Boolean(building && building.isPublic)) {
    return false;
  }

  const ownerKingdomId = resolveOwnerKingdomId(building);
  if (ownerKingdomId === null || perspective.kingdomId === null || ownerKingdomId === perspective.kingdomId) {
    return false;
  }

  return cellHasConcealingFog(frame, x, y);
}

function shouldHideAutonomousUnitForPerspective(unit, perspective, frame) {
  if (!perspective) {
    return false;
  }

  const explicitHidden = readPerspectiveHiddenFlag(unit, perspective.kingdomKey);
  if (typeof explicitHidden === "boolean") {
    return explicitHidden;
  }

  const position = resolvePosition(unit);
  return Boolean(position) && cellHasConcealingFog(frame, position.x, position.y);
}

function cellRect(x, y, transform) {
  const left = Math.round(transform.offsetX + (x * transform.scaledCellSize));
  const top = Math.round(transform.offsetY + (y * transform.scaledCellSize));
  const right = Math.round(transform.offsetX + ((x + 1) * transform.scaledCellSize));
  const bottom = Math.round(transform.offsetY + ((y + 1) * transform.scaledCellSize));
  return rectFromEdges(left, top, right, bottom);
}

function rectFromEdges(left, top, right, bottom) {
  const width = Math.max(1, right - left);
  const height = Math.max(1, bottom - top);

  return {
    x: left,
    y: top,
    width,
    height,
    size: Math.min(width, height),
    centerX: left + (width / 2),
    centerY: top + (height / 2)
  };
}

function buildOccupiedBuildingCellSet(cells) {
  const occupiedCells = new Set();
  for (const cell of toArray(cells)) {
    const worldCell = cell && cell.worldCell;
    if (!worldCell || typeof worldCell.x !== "number" || typeof worldCell.y !== "number") {
      continue;
    }
    occupiedCells.add(`${worldCell.x},${worldCell.y}`);
  }
  return occupiedCells;
}

function buildLegacyOccupiedBuildingCellSet(building, width, height) {
  const occupiedCells = new Set();
  const origin = resolveBuildingOrigin(building);
  if (!origin) {
    return occupiedCells;
  }

  for (let dy = 0; dy < height; dy += 1) {
    for (let dx = 0; dx < width; dx += 1) {
      occupiedCells.add(`${origin.x + dx},${origin.y + dy}`);
    }
  }

  return occupiedCells;
}

function normalizeBuildingRotationQuarterTurns(rotationQuarterTurns) {
  if (!Number.isFinite(rotationQuarterTurns) || rotationQuarterTurns < 0) {
    return 0;
  }

  return Math.trunc(rotationQuarterTurns) % 4;
}

function normalizeBuildingFlipMask(flipMask) {
  if (!Number.isFinite(flipMask) || flipMask < 0) {
    return 0;
  }

  return Math.trunc(flipMask) & (BUILDING_FLIP_HORIZONTAL_MASK | BUILDING_FLIP_VERTICAL_MASK);
}

function getBuildingFootprintWidth(baseWidth, baseHeight, rotationQuarterTurns) {
  const normalizedRotation = normalizeBuildingRotationQuarterTurns(rotationQuarterTurns);
  return normalizedRotation % 2 === 0 ? baseWidth : baseHeight;
}

function getBuildingFootprintHeight(baseWidth, baseHeight, rotationQuarterTurns) {
  const normalizedRotation = normalizeBuildingRotationQuarterTurns(rotationQuarterTurns);
  return normalizedRotation % 2 === 0 ? baseHeight : baseWidth;
}

function mapFootprintToSourceLocalFor(localX, localY, baseWidth, baseHeight, rotationQuarterTurns, flipMask) {
  const normalizedRotation = normalizeBuildingRotationQuarterTurns(rotationQuarterTurns);
  const footprintWidth = getBuildingFootprintWidth(baseWidth, baseHeight, normalizedRotation);
  const footprintHeight = getBuildingFootprintHeight(baseWidth, baseHeight, normalizedRotation);
  if (localX < 0 || localY < 0 || localX >= footprintWidth || localY >= footprintHeight) {
    return { x: -1, y: -1 };
  }

  let sourceX = 0;
  let sourceY = 0;
  switch (normalizedRotation) {
    case 0:
      sourceX = localX;
      sourceY = localY;
      break;
    case 1:
      sourceX = localY;
      sourceY = baseHeight - 1 - localX;
      break;
    case 2:
      sourceX = baseWidth - 1 - localX;
      sourceY = baseHeight - 1 - localY;
      break;
    case 3:
      sourceX = baseWidth - 1 - localY;
      sourceY = localX;
      break;
    default:
      break;
  }

  const normalizedFlipMask = normalizeBuildingFlipMask(flipMask);
  if ((normalizedFlipMask & BUILDING_FLIP_HORIZONTAL_MASK) !== 0) {
    sourceX = baseWidth - 1 - sourceX;
  }
  if ((normalizedFlipMask & BUILDING_FLIP_VERTICAL_MASK) !== 0) {
    sourceY = baseHeight - 1 - sourceY;
  }

  return { x: sourceX, y: sourceY };
}

function expandBuildingCellRect(screen, worldCell, occupiedCells) {
  const bleedX = Math.min(1, Math.floor((screen.width - 1) / 2));
  const bleedY = Math.min(1, Math.floor((screen.height - 1) / 2));
  if (bleedX <= 0 && bleedY <= 0) {
    return screen;
  }

  const hasLeftNeighbor = occupiedCells.has(`${worldCell.x - 1},${worldCell.y}`);
  const hasRightNeighbor = occupiedCells.has(`${worldCell.x + 1},${worldCell.y}`);
  const hasTopNeighbor = occupiedCells.has(`${worldCell.x},${worldCell.y - 1}`);
  const hasBottomNeighbor = occupiedCells.has(`${worldCell.x},${worldCell.y + 1}`);

  return rectFromEdges(
    screen.x - (hasLeftNeighbor ? bleedX : 0),
    screen.y - (hasTopNeighbor ? bleedY : 0),
    screen.x + screen.width + (hasRightNeighbor ? bleedX : 0),
    screen.y + screen.height + (hasBottomNeighbor ? bleedY : 0)
  );
}

function drawCellImage(context, image, screen, rotationQuarterTurns, flipMask, opacity) {
  context.save();
  context.globalAlpha = clamp(opacity, 0, 1);
  context.translate(screen.centerX, screen.centerY);
  context.rotate((rotationQuarterTurns % 4) * (Math.PI / 2));
  if (flipMask & 1) {
    context.scale(-1, 1);
  }
  if (flipMask & 2) {
    context.scale(1, -1);
  }
  context.drawImage(image, -(screen.width / 2), -(screen.height / 2), screen.width, screen.height);
  context.restore();
}

async function primeTextureCatalog(replay) {
  const texturePaths = new Set();

  for (const cellKey of ["grass", "dirt", "water"]) {
    texturePaths.add(`${replayConfig.assetRoot}/textures/cells/${cellKey}.png`);
  }

  texturePaths.add(`${replayConfig.assetRoot}/textures/objects/chest.png`);

  const pieceTypes = toArray(replay.referenceData && replay.referenceData.pieceTypes)
    .map(function (entry) {
      return entry.key;
    });

  for (const pieceKey of pieceTypes) {
    texturePaths.add(`${replayConfig.assetRoot}/textures/pieces/white/${pieceKey}.png`);
    texturePaths.add(`${replayConfig.assetRoot}/textures/pieces/black/${pieceKey}.png`);
    if (pieceKey !== "king") {
      texturePaths.add(`${replayConfig.assetRoot}/textures/pieces/evil/${pieceKey}.png`);
    }
  }

  const buildingTypesSeen = new Set();
  for (const frame of replay.frames) {
    for (const building of frame.buildings) {
      const key = buildingTypeKey(replay.referenceData, building.type);
      if (key) {
        buildingTypesSeen.add(key);
      }
    }
    for (const building of toArray(frame.buildingsAnalytics)) {
      if (building.buildingTypeKey) {
        buildingTypesSeen.add(building.buildingTypeKey);
      }
    }
  }

  for (const buildingKey of buildingTypesSeen) {
    const basePath = BUILDING_TEXTURE_PATHS[buildingKey];
    if (basePath) {
      texturePaths.add(`${replayConfig.assetRoot}/${basePath}`);
    }

    const chunkConfig = CHUNKED_BUILDINGS[buildingKey];
    if (chunkConfig) {
      for (let y = 0; y < chunkConfig.height; y += 1) {
        for (let x = 0; x < chunkConfig.width; x += 1) {
          texturePaths.add(
            `${replayConfig.assetRoot}/textures/cells/structures/${chunkConfig.id}/${chunkConfig.id}_${x + 1}_${y + 1}.png`
          );
        }
      }
    }
  }

  await Promise.all(Array.from(texturePaths, loadTexture));
}

async function loadTexture(path) {
  if (state.textures.has(path)) {
    return state.textures.get(path);
  }

  const image = await new Promise(function (resolve) {
    const nextImage = new Image();
    nextImage.onload = function () {
      resolve(nextImage);
    };
    nextImage.onerror = function () {
      resolve(null);
    };
    nextImage.src = resolveUrl(path);
  });

  state.textures.set(path, image);
  return image;
}

function resolveBuildingTexturePath(buildingKey, sourceLocal) {
  const chunkConfig = CHUNKED_BUILDINGS[buildingKey];
  if (chunkConfig && sourceLocal && typeof sourceLocal.x === "number" && typeof sourceLocal.y === "number") {
    return `${replayConfig.assetRoot}/textures/cells/structures/${chunkConfig.id}/${chunkConfig.id}_${sourceLocal.x + 1}_${sourceLocal.y + 1}.png`;
  }

  const directPath = BUILDING_TEXTURE_PATHS[buildingKey];
  return directPath ? `${replayConfig.assetRoot}/${directPath}` : null;
}

function resolvePosition(entity) {
  if (!entity || typeof entity !== "object") {
    return null;
  }

  if (typeof entity.x === "number" && typeof entity.y === "number") {
    return { x: entity.x, y: entity.y };
  }

  if (entity.position && typeof entity.position.x === "number" && typeof entity.position.y === "number") {
    return { x: entity.position.x, y: entity.position.y };
  }

  if (entity.worldCell && typeof entity.worldCell.x === "number" && typeof entity.worldCell.y === "number") {
    return { x: entity.worldCell.x, y: entity.worldCell.y };
  }

  return null;
}

function getCellSize(masterConfig) {
  const value = masterConfig
    && masterConfig.game
    && masterConfig.game.map
    && masterConfig.game.map.cell_size_px;
  return Number.isFinite(value) ? value : 16;
}

function getDamagedStructureOpacity(masterConfig) {
  const value = masterConfig
    && masterConfig.game
    && masterConfig.game.rendering
    && masterConfig.game.rendering.damaged_structures
    && masterConfig.game.rendering.damaged_structures.opacity_percent;
  const percent = Number.isFinite(value) ? value : 70;
  return clamp(percent / 100, 0, 1);
}

function resolveUrl(path) {
  return new URL(path, window.location.href).href;
}

function isAbortError(error) {
  return Boolean(error) && typeof error === "object" && error.name === "AbortError";
}

function kingdomKeyById(referenceData, id) {
  const kingdoms = toArray(referenceData && referenceData.kingdoms);
  const found = kingdoms.find(function (entry) {
    return Number(entry.id) === Number(id);
  });
  return found && found.key ? found.key : (Number(id) === 1 ? "black" : "white");
}

function kingdomIdByKey(referenceData, key) {
  const kingdoms = toArray(referenceData && referenceData.kingdoms);
  const found = kingdoms.find(function (entry) {
    return String(entry.key || "").toLowerCase() === String(key || "").toLowerCase();
  });
  return found ? Number(found.id) : (key === "black" ? 1 : 0);
}

function kingdomLabel(referenceData, id) {
  return localizedPerspectiveKingdomLabel(kingdomKeyById(referenceData, id));
}

function pieceTypeKey(referenceData, id) {
  const pieceTypes = toArray(referenceData && referenceData.pieceTypes);
  const found = pieceTypes.find(function (entry) {
    return Number(entry.id) === Number(id);
  });
  return found && found.key ? found.key : "pawn";
}

function resolveAutonomousUnitPieceTypeKey(referenceData, unit) {
  if (!unit || typeof unit !== "object") {
    return "pawn";
  }

  const infernal = unit.infernal && typeof unit.infernal === "object"
    ? unit.infernal
    : null;
  const explicitTypeKey = [
    unit.manifestedPieceTypeKey,
    unit.manifestedPieceKey,
    infernal && infernal.manifestedPieceTypeKey,
    infernal && infernal.manifestedPieceKey
  ].find(function (candidate) {
    return typeof candidate === "string" && candidate;
  });

  if (explicitTypeKey) {
    return explicitTypeKey;
  }

  const explicitTypeId = [
    unit.manifestedPieceType,
    unit.manifestedPieceTypeId,
    unit.pieceType,
    unit.pieceTypeId,
    unit.targetPieceType,
    infernal && infernal.manifestedPieceType,
    infernal && infernal.manifestedPieceTypeId,
    infernal && infernal.pieceType,
    infernal && infernal.pieceTypeId,
    infernal && infernal.targetPieceType
  ].find(function (candidate) {
    return Number.isFinite(candidate);
  });

  if (Number.isFinite(explicitTypeId)) {
    return pieceTypeKey(referenceData, explicitTypeId);
  }

  return "pawn";
}

function resolvePieceTypeId(piece) {
  if (typeof (piece && piece.type) === "number") {
    return piece.type;
  }

  if (typeof (piece && piece.pieceTypeId) === "number") {
    return piece.pieceTypeId;
  }

  if (typeof (piece && piece.pieceType) === "number") {
    return piece.pieceType;
  }

  return 0;
}

function resolveKingdomId(entity) {
  if (typeof (entity && entity.kingdom) === "number") {
    return entity.kingdom;
  }

  if (typeof (entity && entity.kingdomId) === "number") {
    return entity.kingdomId;
  }

  if (typeof (entity && entity.owner) === "number") {
    return entity.owner;
  }

  if (typeof (entity && entity.ownerKingdomId) === "number") {
    return entity.ownerKingdomId;
  }

  return null;
}

function resolveOwnerKingdomId(entity) {
  if (typeof (entity && entity.owner) === "number") {
    return entity.owner;
  }

  if (typeof (entity && entity.ownerKingdomId) === "number") {
    return entity.ownerKingdomId;
  }

  return null;
}

function resolveKingdomKey(referenceData, entity) {
  if (entity && typeof entity.kingdomKey === "string") {
    return entity.kingdomKey;
  }

  return kingdomKeyById(referenceData, resolveKingdomId(entity));
}

function readPerspectiveHiddenFlag(entity, kingdomKey) {
  if (!entity || typeof entity !== "object") {
    return null;
  }

  const hiddenProperty = kingdomKey === "black" ? "hiddenFromBlack" : "hiddenFromWhite";
  return typeof entity[hiddenProperty] === "boolean" ? entity[hiddenProperty] : null;
}

function normalizePerspectiveKingdomKey(value) {
  if (typeof value === "number") {
    return value === 1 ? "black" : "white";
  }

  const normalized = String(value || "").trim().toLowerCase();
  if (normalized === "white" || normalized === "black") {
    return normalized;
  }

  return null;
}

function resolveConfiguredPerspectiveKingdomKey(config) {
  if (config && config.perspectiveEnabled === false) {
    return null;
  }

  if (config && config.perspectiveEnabled === true) {
    return normalizePerspectiveKingdomKey(config.perspectiveKingdom)
      || normalizePerspectiveKingdomKey(config.initialPerspective)
      || "white";
  }

  return normalizePerspectiveKingdomKey(config && config.initialPerspective);
}

function localizedPerspectiveKingdomLabel(kingdomKey) {
  return KINGDOM_PERSPECTIVE_LABELS[kingdomKey] || KINGDOM_PERSPECTIVE_LABELS.white;
}

function normalizeTrackedTargetConfig(value) {
  if (!value || typeof value !== "object") {
    return null;
  }

  const kind = String(value.kind || value.type || "").trim().toLowerCase();
  if (!kind) {
    return null;
  }

  return {
    kind,
    id: typeof value.id === "number" ? value.id : null,
    index: typeof value.index === "number" ? value.index : null,
    x: typeof value.x === "number" ? value.x : null,
    y: typeof value.y === "number" ? value.y : null
  };
}

function resolveTrackedTargetGridPoint(frame, trackedTarget) {
  if (!frame || !trackedTarget) {
    return null;
  }

  if ((trackedTarget.kind === "terrain-cell" || trackedTarget.kind === "cell")
    && isGridCoordinateInside(frame, trackedTarget.x, trackedTarget.y)) {
    return resolveCellCenterPoint(trackedTarget.x, trackedTarget.y);
  }

  if (trackedTarget.kind === "building-cell" || trackedTarget.kind === "structure-cell") {
    if (isGridCoordinateInside(frame, trackedTarget.x, trackedTarget.y)) {
      return resolveCellCenterPoint(trackedTarget.x, trackedTarget.y);
    }

    if (typeof trackedTarget.id === "number") {
      const building = findBuildingById(frame, trackedTarget.id);
      if (building) {
        const origin = resolveBuildingOrigin(building);
        if (origin) {
          return resolveCellCenterPoint(origin.x, origin.y);
        }
      }
    }
  }

  if (trackedTarget.kind === "building" || trackedTarget.kind === "structure") {
    const building = findBuildingById(frame, trackedTarget.id);
    if (!building) {
      return null;
    }

    return resolveBuildingCenterPoint(building);
  }

  if (trackedTarget.kind === "piece") {
    const piece = findById(resolveDebugPieces(frame), trackedTarget.id);
    const piecePosition = piece ? resolvePosition(piece) : null;
    return piecePosition ? resolveCellCenterPoint(piecePosition.x, piecePosition.y) : null;
  }

  if (trackedTarget.kind === "map-object" || trackedTarget.kind === "object") {
    const object = findById(toArray(frame.mapObjects), trackedTarget.id);
    const objectPosition = object ? resolvePosition(object) : null;
    return objectPosition ? resolveCellCenterPoint(objectPosition.x, objectPosition.y) : null;
  }

  if (trackedTarget.kind === "autonomous-unit" || trackedTarget.kind === "unit") {
    const unit = findById(toArray(frame.autonomousUnits), trackedTarget.id);
    const unitPosition = unit ? resolvePosition(unit) : null;
    return unitPosition ? resolveCellCenterPoint(unitPosition.x, unitPosition.y) : null;
  }

  if (trackedTarget.kind === "active-infernal-unit" || trackedTarget.kind === "infernal-active-unit") {
    const unit = resolveActiveInfernalTrackedUnit(frame);
    const unitPosition = unit ? resolvePosition(unit) : null;
    return unitPosition ? resolveCellCenterPoint(unitPosition.x, unitPosition.y) : null;
  }

  if (trackedTarget.kind === "cloud" || trackedTarget.kind === "front" || trackedTarget.kind === "weather-front") {
    return resolveTrackedWeatherFrontPoint(frame, trackedTarget);
  }

  return null;
}

function resolveActiveInfernalTrackedUnit(frame) {
  const infernalAnalytics = frame && frame.analytics && frame.analytics.infernal && typeof frame.analytics.infernal === "object"
    ? frame.analytics.infernal
    : null;
  const activeInfernalUnitId = Number(infernalAnalytics && infernalAnalytics.activeInfernalUnitId);

  if (Number.isFinite(activeInfernalUnitId) && activeInfernalUnitId >= 0) {
    const activeUnit = findById(toArray(frame.autonomousUnits), Math.trunc(activeInfernalUnitId));
    if (activeUnit) {
      return activeUnit;
    }
  }

  return toArray(frame.autonomousUnits).find(isInfernalAutonomousUnit) || null;
}

function isInfernalAutonomousUnit(unit) {
  return Boolean(
    unit
    && (
      (unit.infernal && typeof unit.infernal === "object")
      || typeof unit.targetKingdomKey === "string"
      || Number.isFinite(unit.targetKingdom)
      || Number.isFinite(unit.targetPieceType)
      || Number.isFinite(unit.pieceType)
    )
  );
}

function resolveTrackedWeatherFrontPoint(frame, trackedTarget) {
  const fronts = getTrackedWeatherFronts(frame, trackedTarget);
  const centers = fronts
    .map(function (front) {
      return resolveWeatherFrontCenter(front);
    })
    .filter(Boolean);

  if (!centers.length) {
    return resolveBoardCenterGridPoint(frame);
  }

  const total = centers.reduce(function (accumulator, center) {
    return {
      x: accumulator.x + center.x,
      y: accumulator.y + center.y
    };
  }, { x: 0, y: 0 });

  return {
    x: total.x / centers.length,
    y: total.y / centers.length
  };
}

function resolveBoardCenterGridPoint(frame) {
  if (!frame || !Array.isArray(frame.grid) || !frame.grid.length) {
    return null;
  }

  const gridHeight = frame.grid.length;
  const gridWidth = Array.isArray(frame.grid[0]) ? frame.grid[0].length : 0;

  if (!gridWidth) {
    return null;
  }

  return {
    x: gridWidth / 2,
    y: gridHeight / 2
  };
}

function resolvePerspectivePresentation(frame) {
  if (!frame || !configuredPerspectiveKingdomKey) {
    return null;
  }

  return {
    kingdomKey: configuredPerspectiveKingdomKey,
    kingdomId: kingdomIdByKey(frame.referenceData, configuredPerspectiveKingdomKey),
    label: localizedPerspectiveKingdomLabel(configuredPerspectiveKingdomKey)
  };
}

function buildingTypeKey(referenceData, id) {
  const buildingTypes = toArray(referenceData && referenceData.buildingTypes);
  const found = buildingTypes.find(function (entry) {
    return Number(entry.id) === Number(id);
  });
  return found && found.key ? found.key : "barracks";
}

function getActiveWeatherFronts(frame) {
  const analyticsFronts = frame
    && frame.analytics
    && frame.analytics.weather
    && Array.isArray(frame.analytics.weather.fronts)
    ? frame.analytics.weather.fronts
    : null;

  if (analyticsFronts) {
    return analyticsFronts;
  }

  const snapshotFronts = frame
    && frame.snapshot
    && frame.snapshot.weatherState
    && Array.isArray(frame.snapshot.weatherState.activeFronts)
    ? frame.snapshot.weatherState.activeFronts
    : [];

  return snapshotFronts.map(function (front) {
    return {
      ...front,
      directionId: typeof front.directionId === "number" ? front.directionId : front.direction,
      directionKey: resolveWeatherFrontDirectionKey(front)
    };
  });
}

function resolveWeatherFrontDirectionKey(front) {
  if (front && typeof front.directionKey === "string") {
    return front.directionKey;
  }

  const directionId = typeof (front && front.directionId) === "number"
    ? front.directionId
    : typeof (front && front.direction) === "number"
      ? front.direction
      : null;

  return directionId === null ? null : (WEATHER_FRONT_DIRECTION_KEYS[directionId] || null);
}

function resolveWeatherFrontCenter(front) {
  if (!front || typeof front !== "object") {
    return null;
  }

  const currentTurnStep = Number(front.currentTurnStep) || 0;
  const centerStartX = Number(front.centerStartXTimes1000) || 0;
  const centerStartY = Number(front.centerStartYTimes1000) || 0;
  const stepX = Number(front.stepXTimes1000) || 0;
  const stepY = Number(front.stepYTimes1000) || 0;

  return {
    x: (centerStartX + (currentTurnStep * stepX)) / 1000,
    y: (centerStartY + (currentTurnStep * stepY)) / 1000
  };
}

function summarizeWeatherFront(front, index) {
  const center = resolveWeatherFrontCenter(front);
  return {
    index,
    directionKey: resolveWeatherFrontDirectionKey(front),
    currentTurnStep: Number(front && front.currentTurnStep) || 0,
    totalTurnSteps: Number(front && front.totalTurnSteps) || 0,
    centerX: center ? center.x : null,
    centerY: center ? center.y : null,
    radiusAlong: (Number(front && front.radiusAlongTimes1000) || 0) / 1000,
    radiusAcross: (Number(front && front.radiusAcrossTimes1000) || 0) / 1000
  };
}

function isGridCoordinateInside(frame, x, y) {
  return Number.isInteger(x)
    && Number.isInteger(y)
    && y >= 0
    && y < frame.grid.length
    && x >= 0
    && x < (frame.grid[y] ? frame.grid[y].length : 0);
}

function resolveDebugPieces(frame) {
  return Array.isArray(frame.piecesAnalytics) && frame.piecesAnalytics.length
    ? frame.piecesAnalytics
    : frame.pieces;
}

function resolveCellCenterPoint(x, y) {
  return {
    x: x + 0.5,
    y: y + 0.5
  };
}

function summarizeTerrainCell(cell) {
  if (!cell || typeof cell !== "object") {
    return null;
  }

  return {
    traversable: Boolean(cell.c),
    terrainTypeId: typeof cell.t === "number" ? cell.t : null,
    terrainTypeKey: CELL_TYPE_KEYS[cell.t] || "grass",
    brightness: typeof cell.b === "number" ? cell.b : null,
    flipMask: typeof cell.f === "number" ? cell.f : 0
  };
}

function summarizePiece(referenceData, piece) {
  const position = resolvePosition(piece);
  return {
    id: resolveEntityId(piece),
    pieceTypeKey: pieceTypeKey(referenceData, resolvePieceTypeId(piece)),
    kingdomKey: resolveKingdomKey(referenceData, piece),
    x: position ? position.x : null,
    y: position ? position.y : null,
    xp: typeof piece.xp === "number" ? piece.xp : null,
    hiddenFromWhite: readPerspectiveHiddenFlag(piece, "white"),
    hiddenFromBlack: readPerspectiveHiddenFlag(piece, "black")
  };
}

function summarizeBuilding(building) {
  const origin = resolveBuildingOrigin(building);
  return {
    id: resolveEntityId(building),
    buildingTypeKey: building.buildingTypeKey || null,
    ownerKingdomKey: building.ownerKingdomKey || null,
    isPublic: Boolean(building.isPublic),
    originX: origin ? origin.x : null,
    originY: origin ? origin.y : null,
    width: resolveBuildingWidth(building),
    height: resolveBuildingHeight(building),
    hiddenFromWhite: readPerspectiveHiddenFlag(building, "white"),
    hiddenFromBlack: readPerspectiveHiddenFlag(building, "black")
  };
}

function summarizeLegacyBuilding(referenceData, building) {
  return {
    id: resolveEntityId(building),
    buildingTypeKey: buildingTypeKey(referenceData, building.type),
    ownerKingdomKey: kingdomKeyById(referenceData, building.owner),
    isPublic: Boolean(building.isNeutral),
    originX: typeof building.ox === "number" ? building.ox : null,
    originY: typeof building.oy === "number" ? building.oy : null,
    width: typeof building.w === "number" ? building.w : 1,
    height: typeof building.h === "number" ? building.h : 1
  };
}

function summarizeMapObject(object) {
  const position = resolvePosition(object);
  return {
    id: resolveEntityId(object),
    typeKey: resolveMapObjectTypeKey(object),
    x: position ? position.x : null,
    y: position ? position.y : null,
    rewardType: typeof object.rewardType === "number" ? object.rewardType : null,
    rewardAmount: typeof object.rewardAmount === "number" ? object.rewardAmount : null,
    spawnTurn: typeof object.spawnTurn === "number" ? object.spawnTurn : null
  };
}

function summarizeAutonomousUnit(referenceData, unit) {
  const position = resolvePosition(unit);

  return {
    id: resolveEntityId(unit),
    pieceTypeKey: resolveAutonomousUnitPieceTypeKey(referenceData, unit),
    x: position ? position.x : null,
    y: position ? position.y : null,
    hiddenFromWhite: readPerspectiveHiddenFlag(unit, "white"),
    hiddenFromBlack: readPerspectiveHiddenFlag(unit, "black")
  };
}

function resolveEntityId(entity) {
  if (!entity || typeof entity !== "object") {
    return null;
  }

  if (typeof entity.id === "number") {
    return entity.id;
  }

  if (typeof entity.objectId === "number") {
    return entity.objectId;
  }

  if (typeof entity.unitId === "number") {
    return entity.unitId;
  }

  if (typeof entity.pieceId === "number") {
    return entity.pieceId;
  }

  if (typeof entity.buildingId === "number") {
    return entity.buildingId;
  }

  return null;
}

function findById(collection, id) {
  if (typeof id !== "number") {
    return null;
  }

  return toArray(collection).find(function (entry) {
    return resolveEntityId(entry) === id;
  }) || null;
}

function findBuildingById(frame, id) {
  const analyticsMatch = findById(toArray(frame.buildingsAnalytics), id);
  if (analyticsMatch) {
    return analyticsMatch;
  }

  return findById(toArray(frame.buildings), id);
}

function resolveBuildingOrigin(building) {
  if (!building || typeof building !== "object") {
    return null;
  }

  if (building.origin && typeof building.origin.x === "number" && typeof building.origin.y === "number") {
    return { x: building.origin.x, y: building.origin.y };
  }

  if (typeof building.ox === "number" && typeof building.oy === "number") {
    return { x: building.ox, y: building.oy };
  }

  return null;
}

function resolveBuildingWidth(building) {
  if (typeof (building && building.footprintWidth) === "number") {
    return building.footprintWidth;
  }

  if (typeof (building && building.w) === "number") {
    return getBuildingFootprintWidth(
      building.w,
      typeof building.h === "number" ? building.h : 1,
      typeof building.rotationQuarterTurns === "number"
        ? building.rotationQuarterTurns
        : building && typeof building.rot === "number"
          ? building.rot
          : 0
    );
  }

  return 1;
}

function resolveBuildingHeight(building) {
  if (typeof (building && building.footprintHeight) === "number") {
    return building.footprintHeight;
  }

  if (typeof (building && building.h) === "number") {
    return getBuildingFootprintHeight(
      typeof building.w === "number" ? building.w : 1,
      building.h,
      typeof building.rotationQuarterTurns === "number"
        ? building.rotationQuarterTurns
        : building && typeof building.rot === "number"
          ? building.rot
          : 0
    );
  }

  return 1;
}

function resolveBuildingCenterCell(building) {
  const origin = resolveBuildingOrigin(building);
  if (!origin) {
    return null;
  }

  return {
    x: origin.x + ((resolveBuildingWidth(building) - 1) / 2),
    y: origin.y + ((resolveBuildingHeight(building) - 1) / 2)
  };
}

function resolveBuildingCenterPoint(building) {
  const origin = resolveBuildingOrigin(building);
  if (!origin) {
    return null;
  }

  return {
    x: origin.x + (resolveBuildingWidth(building) / 2),
    y: origin.y + (resolveBuildingHeight(building) / 2)
  };
}

function legacyBuildingOccupiesCell(building, x, y) {
  const origin = resolveBuildingOrigin(building);
  if (!origin) {
    return false;
  }

  return x >= origin.x
    && x < origin.x + resolveBuildingWidth(building)
    && y >= origin.y
    && y < origin.y + resolveBuildingHeight(building);
}

function resolveMapObjectTypeKey(object) {
  if (object && typeof object.typeKey === "string") {
    return object.typeKey;
  }

  return Number(object && object.type) === 0 ? "chest" : "object";
}

function fallbackBuildingColor(buildingKey, opacity) {
  const palette = {
    church: "rgba(186, 180, 157, OPACITY)",
    mine: "rgba(122, 115, 106, OPACITY)",
    farm: "rgba(126, 152, 93, OPACITY)",
    barracks: "rgba(143, 94, 70, OPACITY)",
    wood_wall: "rgba(116, 82, 54, OPACITY)",
    stone_wall: "rgba(108, 108, 110, OPACITY)",
    bridge: "rgba(134, 103, 77, OPACITY)",
    arena: "rgba(137, 104, 82, OPACITY)"
  };
  return (palette[buildingKey] || palette.barracks).replace("OPACITY", String(clamp(opacity, 0, 1)));
}

function shadeColor(hexColor, brightness) {
  const normalized = hexColor.replace("#", "");
  const red = parseInt(normalized.slice(0, 2), 16);
  const green = parseInt(normalized.slice(2, 4), 16);
  const blue = parseInt(normalized.slice(4, 6), 16);
  return `rgb(${Math.round(red * brightness)}, ${Math.round(green * brightness)}, ${Math.round(blue * brightness)})`;
}

function clamp(value, min, max) {
  return Math.min(max, Math.max(min, value));
}

}