import { REPLAY_CONFIG } from "./config.js";

const TARGET_SCHEMA_VERSION = 5;
const MIN_CAMERA_ZOOM = 0.08;
const MAX_CAMERA_ZOOM = 32;
const WHEEL_ZOOM_FACTOR = 1.12;

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

const state = {
  replay: null,
  frameIndex: 0,
  masterConfig: DEFAULT_MASTER_CONFIG,
  textures: new Map(),
  autoPlayHandle: null,
  statusMessage: "Chargement du replay...",
  camera: {
    zoom: 1,
    centerWorldX: 0,
    centerWorldY: 0,
    initialized: false,
    boardKey: null,
    isDragging: false,
    pointerId: null,
    lastClientX: 0,
    lastClientY: 0
  }
};

const refs = resolveRefs();

bindEvents();
renderIdle();
bootstrap();

async function bootstrap() {
  if (window.location.protocol === "file:") {
    setError(
      "Le viewer est ouvert via file://. Servir le dossier en HTTP pour autoriser fetch sur le companion et les assets."
    );
    return;
  }

  setStatus("Chargement du replay...", REPLAY_CONFIG.replayUrl);

  try {
    const [masterConfig, rawReplay] = await Promise.all([
      loadMasterConfig(REPLAY_CONFIG.masterConfigUrl),
      loadReplay(REPLAY_CONFIG.replayUrl)
    ]);

    state.masterConfig = masterConfig;
    state.replay = buildReplayModel(rawReplay, masterConfig);
    state.statusMessage = "";
    await primeTextureCatalog(state.replay);
    resetCameraToFit(state.replay.frames[state.frameIndex]);
    syncTimelineBounds();
    renderCurrentFrame();
    setStatus(
      "Replay charge.",
      `${state.replay.meta.title} · ${state.replay.frames.length} frames disponibles`
    );
  } catch (error) {
    setError(error instanceof Error ? error.message : String(error));
  }
}

function resolveRefs() {
  return {
    replayCanvas: mustGet("replayCanvas"),
    activeTurnOverlay: mustGet("activeTurnOverlay"),
    activeKingdomShield: mustGet("activeKingdomShield"),
    activeKingdomValue: mustGet("activeKingdomValue"),
    statusOverlay: mustGet("statusOverlay"),
    statusText: mustGet("statusText"),
    firstTurnButton: mustGet("firstTurnButton"),
    prevTurnButton: mustGet("prevTurnButton"),
    playPauseButton: mustGet("playPauseButton"),
    nextTurnButton: mustGet("nextTurnButton"),
    lastTurnButton: mustGet("lastTurnButton"),
    turnSlider: mustGet("turnSlider")
  };
}

function mustGet(id) {
  const element = document.getElementById(id);
  if (!element) {
    throw new Error(`Missing required element #${id}`);
  }
  return element;
}

function bindEvents() {
  refs.replayCanvas.title = "Molette: zoom. Glisser: camera. Double-clic: recadrer.";

  refs.firstTurnButton.addEventListener("click", function () {
    stopAutoplay();
    setFrameIndex(0);
  });

  refs.prevTurnButton.addEventListener("click", function () {
    stopAutoplay();
    setFrameIndex(state.frameIndex - 1);
  });

  refs.nextTurnButton.addEventListener("click", function () {
    stopAutoplay();
    setFrameIndex(state.frameIndex + 1);
  });

  refs.lastTurnButton.addEventListener("click", function () {
    stopAutoplay();
    if (!state.replay) {
      return;
    }
    setFrameIndex(state.replay.frames.length - 1);
  });

  refs.playPauseButton.addEventListener("click", function () {
    if (state.autoPlayHandle) {
      stopAutoplay();
    } else {
      startAutoplay();
    }
  });

  refs.turnSlider.addEventListener("input", function (event) {
    stopAutoplay();
    const nextIndex = Number(event.target.value);
    setFrameIndex(nextIndex);
  });

  refs.replayCanvas.addEventListener("pointerdown", onCanvasPointerDown);
  refs.replayCanvas.addEventListener("pointermove", onCanvasPointerMove);
  refs.replayCanvas.addEventListener("pointerup", onCanvasPointerUp);
  refs.replayCanvas.addEventListener("pointercancel", onCanvasPointerUp);
  refs.replayCanvas.addEventListener("wheel", onCanvasWheel, { passive: false });
  refs.replayCanvas.addEventListener("dblclick", function () {
    const frame = currentFrame();
    if (!frame) {
      return;
    }

    resetCameraToFit(frame);
    renderCurrentFrame();
  });

  window.addEventListener("resize", function () {
    if (state.replay) {
      renderCurrentFrame();
      return;
    }

    renderCanvasMessage(state.statusMessage);
  });

  window.addEventListener("keydown", function (event) {
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
      zoomCameraFromKeyboard(WHEEL_ZOOM_FACTOR);
    }

    if (event.key === "-" || event.key === "_") {
      event.preventDefault();
      zoomCameraFromKeyboard(1 / WHEEL_ZOOM_FACTOR);
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

function renderIdle() {
  syncTimelineBounds();
  syncControlsState();
  syncOverlays(null);
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
  try {
    const response = await fetch(resolveUrl(url), { cache: "no-store" });
    if (!response.ok) {
      throw new Error();
    }
    return await response.json();
  } catch (_error) {
    return DEFAULT_MASTER_CONFIG;
  }
}

async function loadReplay(url) {
  const response = await fetch(resolveUrl(url), { cache: "no-store" });
  if (!response.ok) {
    throw new Error(`Impossible de charger le replay: HTTP ${response.status} ${response.statusText}`);
  }

  const data = await response.json();
  validateReplay(data);
  return data;
}

function validateReplay(data) {
  if (!data || typeof data !== "object" || Array.isArray(data)) {
    throw new Error("Le companion charge n'est pas un objet JSON valide.");
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

  if (data.initialSnapshot) {
    frames.push(normalizeFrame({
      snapshot: data.initialSnapshot,
      analytics: data.initialAnalytics || null,
      committedTurnNumber: 0,
      capturedAtUnix: data.createdAtUnix || null,
      gameOver: false,
      winner: null,
      activeKingdom: data.initialSnapshot.activeKingdom,
      activeValidation: data.initialActiveValidation || null,
      nextTurnValidation: data.initialNextTurnValidation || null,
      label: "Etat initial",
      rawEvents: Array.isArray(data.initialSnapshot.events) ? data.initialSnapshot.events : []
    }, data.referenceData, masterConfig));
  }

  for (const record of turnHistory) {
    if (!record || !record.snapshot) {
      continue;
    }

    frames.push(normalizeFrame({
      snapshot: record.snapshot,
      analytics: record.analytics || null,
      committedTurnNumber: record.committedTurnNumber,
      capturedAtUnix: record.capturedAtUnix || null,
      gameOver: Boolean(record.gameOver),
      winner: typeof record.winner === "number" ? record.winner : null,
      activeKingdom: record.committedActiveKingdom,
      activeValidation: record.activeValidation || null,
      nextTurnValidation: record.nextTurnValidation || null,
      label: `Tour ${record.committedTurnNumber}`,
      rawEvents: Array.isArray(record.newEvents) ? record.newEvents : []
    }, data.referenceData, masterConfig));
  }

  if (!frames.length && data.currentStateSummary) {
    frames.push(normalizeFrame({
      snapshot: data.currentStateSummary,
      analytics: data.currentAnalytics || null,
      committedTurnNumber: data.currentStateSummary.turnNumber || 0,
      capturedAtUnix: data.lastUpdatedAtUnix || null,
      gameOver: false,
      winner: null,
      activeKingdom: data.currentStateSummary.activeKingdom,
      activeValidation: data.currentActiveValidation || data.currentStateSummary.activeValidation || null,
      nextTurnValidation: data.currentNextTurnValidation || data.currentStateSummary.nextTurnValidation || null,
      label: `Tour ${data.currentStateSummary.turnNumber || 0}`,
      rawEvents: Array.isArray(data.currentStateSummary.events) ? data.currentStateSummary.events : []
    }, data.referenceData, masterConfig));
  }

  if (!frames.length) {
    throw new Error("Aucune frame replayable n'a ete construite depuis le companion.");
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

function normalizeFrame(frameInput, referenceData, masterConfig) {
  const snapshot = frameInput.snapshot;
  const whitePieces = toArray(snapshot.whiteKingdom && snapshot.whiteKingdom.pieces);
  const blackPieces = toArray(snapshot.blackKingdom && snapshot.blackKingdom.pieces);
  const whiteBuildings = toArray(snapshot.whiteKingdom && snapshot.whiteKingdom.buildings);
  const blackBuildings = toArray(snapshot.blackKingdom && snapshot.blackKingdom.buildings);
  const publicBuildings = toArray(snapshot.publicBuildings);
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
    grid: toArray(snapshot.grid),
    pieces: whitePieces.concat(blackPieces),
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
    return "Aucun evenement marquant sur cette frame.";
  }

  return events
    .slice(-3)
    .map(function (entry) {
      return entry.message || entry.msg || entry.kindLabel || entry.kindKey || "Evenement";
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

  refs.turnSlider.min = "0";
  refs.turnSlider.max = String(Math.max(0, state.replay.frames.length - 1));
  refs.turnSlider.value = String(state.frameIndex);
}

function syncControlsState() {
  const frameCount = state.replay ? state.replay.frames.length : 0;
  const atStart = state.frameIndex <= 0;
  const atEnd = !state.replay || state.frameIndex >= (frameCount - 1);
  const canNavigate = frameCount > 1;
  const isAutoPlaying = Boolean(state.autoPlayHandle);

  refs.playPauseButton.textContent = isAutoPlaying ? "Pause" : "Lire";
  refs.playPauseButton.title = isAutoPlaying ? "Mettre en pause" : "Lancer la lecture";
  refs.firstTurnButton.disabled = !canNavigate || atStart;
  refs.prevTurnButton.disabled = !canNavigate || atStart;
  refs.nextTurnButton.disabled = !canNavigate || atEnd;
  refs.lastTurnButton.disabled = !canNavigate || atEnd;
  refs.playPauseButton.disabled = !canNavigate;
  refs.turnSlider.disabled = !canNavigate;
}

function setFrameIndex(nextIndex) {
  if (!state.replay) {
    return;
  }

  const bounded = clamp(nextIndex, 0, state.replay.frames.length - 1);
  state.frameIndex = bounded;
  renderCurrentFrame();
}

function startAutoplay() {
  if (!state.replay || state.autoPlayHandle || state.replay.frames.length < 2) {
    return;
  }

  if (state.frameIndex >= state.replay.frames.length - 1) {
    state.frameIndex = 0;
  }

  state.autoPlayHandle = window.setInterval(function () {
    if (!state.replay) {
      stopAutoplay();
      return;
    }

    const nextIndex = state.frameIndex + 1;
    if (nextIndex >= state.replay.frames.length) {
      stopAutoplay();
      return;
    }

    setFrameIndex(nextIndex);
  }, REPLAY_CONFIG.autoplayIntervalMs);

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

function renderCurrentFrame() {
  const frame = currentFrame();
  if (!frame) {
    renderCanvasMessage(state.statusMessage);
    return;
  }

  refs.turnSlider.value = String(state.frameIndex);
  refs.turnSlider.setAttribute("aria-valuetext", frame.label);
  refs.turnSlider.title = frame.label;
  syncControlsState();
  syncOverlays(frame);
  renderCanvasFrame(frame);
}

function syncOverlays(frame) {
  if (!frame) {
    refs.activeTurnOverlay.hidden = true;
    refs.statusOverlay.hidden = true;
    refs.statusText.textContent = "";
    refs.statusText.className = "status-chip";
    return;
  }

  refs.activeTurnOverlay.hidden = false;
  refs.activeKingdomValue.textContent = frame.sideToMoveLabel;
  refs.activeKingdomShield.src = resolveUrl(`${REPLAY_CONFIG.assetRoot}/${KINGDOM_SHIELD_PATHS[frame.sideToMoveKey] || KINGDOM_SHIELD_PATHS.white}`);
  refs.activeKingdomShield.alt = `Bouclier ${frame.sideToMoveLabel}`;

  const statusBadge = buildStatusBadge(frame);
  refs.statusText.className = "status-chip";
  if (!statusBadge) {
    refs.statusOverlay.hidden = true;
    refs.statusText.textContent = "";
    return;
  }

  refs.statusOverlay.hidden = false;
  refs.statusText.textContent = statusBadge.text;
  refs.statusText.classList.add(statusBadge.className);
}

function buildStatusBadge(frame) {
  const validation = frame.nextTurnValidation || frame.activeValidation;
  if (frame.gameOver) {
    return {
      text: "Echec et mat",
      className: "is-checkmate"
    };
  }

  if (validation && validation.activeKingInCheck) {
    return {
      text: "Echec",
      className: "is-check"
    };
  }

  return null;
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

  state.camera.isDragging = true;
  state.camera.pointerId = event.pointerId;
  state.camera.lastClientX = event.clientX;
  state.camera.lastClientY = event.clientY;
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

  state.camera.isDragging = false;
  state.camera.pointerId = null;
  refs.replayCanvas.classList.remove("is-dragging");

  if (refs.replayCanvas.hasPointerCapture(event.pointerId)) {
    refs.replayCanvas.releasePointerCapture(event.pointerId);
  }
}

function onCanvasWheel(event) {
  const frame = currentFrame();
  if (!frame) {
    return;
  }

  event.preventDefault();

  const canvasInfo = prepareCanvas(refs.replayCanvas);
  const nextZoom = clamp(
    state.camera.zoom * (event.deltaY < 0 ? WHEEL_ZOOM_FACTOR : 1 / WHEEL_ZOOM_FACTOR),
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

function drawTerrain(context, frame, transform) {
  for (let y = 0; y < frame.grid.length; y += 1) {
    const row = frame.grid[y];
    for (let x = 0; x < row.length; x += 1) {
      const cell = row[x];
      if (!cell || !cell.c) {
        continue;
      }

      const cellKey = CELL_TYPE_KEYS[cell.t] || "grass";
      const texturePath = `${REPLAY_CONFIG.assetRoot}/textures/cells/${cellKey}.png`;
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
    const width = Number(building.w) || 1;
    const height = Number(building.h) || 1;
    for (let dy = 0; dy < height; dy += 1) {
      for (let dx = 0; dx < width; dx += 1) {
        const screen = cellRect((Number(building.ox) || 0) + dx, (Number(building.oy) || 0) + dy, transform);
        context.fillStyle = fallbackBuildingColor(buildingTypeKey(frame.referenceData, building.type), 1);
        context.fillRect(screen.x, screen.y, screen.width, screen.height);
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
    const chestPath = `${REPLAY_CONFIG.assetRoot}/textures/objects/chest.png`;
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
  for (const piece of frame.pieces) {
    const position = resolvePosition(piece);
    if (!position) {
      continue;
    }

    const typeKey = pieceTypeKey(frame.referenceData, piece.type);
    const kingdomKey = kingdomKeyById(frame.referenceData, piece.kingdom);
    const texturePath = `${REPLAY_CONFIG.assetRoot}/textures/pieces/${kingdomKey}/${typeKey}.png`;
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
  for (const unit of frame.autonomousUnits) {
    const position = resolvePosition(unit);
    if (!position) {
      continue;
    }

    const pieceType = typeof unit.pieceType === "number"
      ? unit.pieceType
      : typeof unit.targetPieceType === "number"
        ? unit.targetPieceType
        : 4;
    const typeKey = pieceTypeKey(frame.referenceData, pieceType);
    const texturePath = `${REPLAY_CONFIG.assetRoot}/textures/pieces/evil/${typeKey}.png`;
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
  const weatherMask = frame.snapshot
    && frame.snapshot.weatherState
    && frame.snapshot.weatherState.mask
    ? frame.snapshot.weatherState.mask
    : null;

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
    texturePaths.add(`${REPLAY_CONFIG.assetRoot}/textures/cells/${cellKey}.png`);
  }

  texturePaths.add(`${REPLAY_CONFIG.assetRoot}/textures/objects/chest.png`);

  const pieceTypes = toArray(replay.referenceData && replay.referenceData.pieceTypes)
    .map(function (entry) {
      return entry.key;
    });

  for (const pieceKey of pieceTypes) {
    texturePaths.add(`${REPLAY_CONFIG.assetRoot}/textures/pieces/white/${pieceKey}.png`);
    texturePaths.add(`${REPLAY_CONFIG.assetRoot}/textures/pieces/black/${pieceKey}.png`);
    if (pieceKey !== "king") {
      texturePaths.add(`${REPLAY_CONFIG.assetRoot}/textures/pieces/evil/${pieceKey}.png`);
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
      texturePaths.add(`${REPLAY_CONFIG.assetRoot}/${basePath}`);
    }

    const chunkConfig = CHUNKED_BUILDINGS[buildingKey];
    if (chunkConfig) {
      for (let y = 0; y < chunkConfig.height; y += 1) {
        for (let x = 0; x < chunkConfig.width; x += 1) {
          texturePaths.add(
            `${REPLAY_CONFIG.assetRoot}/textures/cells/structures/${chunkConfig.id}/${chunkConfig.id}_${x + 1}_${y + 1}.png`
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
    return `${REPLAY_CONFIG.assetRoot}/textures/cells/structures/${chunkConfig.id}/${chunkConfig.id}_${sourceLocal.x + 1}_${sourceLocal.y + 1}.png`;
  }

  const directPath = BUILDING_TEXTURE_PATHS[buildingKey];
  return directPath ? `${REPLAY_CONFIG.assetRoot}/${directPath}` : null;
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

function kingdomKeyById(referenceData, id) {
  const kingdoms = toArray(referenceData && referenceData.kingdoms);
  const found = kingdoms.find(function (entry) {
    return Number(entry.id) === Number(id);
  });
  return found && found.key ? found.key : (Number(id) === 1 ? "black" : "white");
}

function kingdomLabel(referenceData, id) {
  const kingdoms = toArray(referenceData && referenceData.kingdoms);
  const found = kingdoms.find(function (entry) {
    return Number(entry.id) === Number(id);
  });
  return found && found.label ? found.label : (Number(id) === 1 ? "Black" : "White");
}

function pieceTypeKey(referenceData, id) {
  const pieceTypes = toArray(referenceData && referenceData.pieceTypes);
  const found = pieceTypes.find(function (entry) {
    return Number(entry.id) === Number(id);
  });
  return found && found.key ? found.key : "pawn";
}

function buildingTypeKey(referenceData, id) {
  const buildingTypes = toArray(referenceData && referenceData.buildingTypes);
  const found = buildingTypes.find(function (entry) {
    return Number(entry.id) === Number(id);
  });
  return found && found.key ? found.key : "barracks";
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