import fs from "node:fs/promises";
import path from "node:path";

const PIECE_TYPE_KEYS = ["pawn", "knight", "bishop", "rook", "queen", "king"];
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

async function main() {
  const options = parseArgs(process.argv.slice(2));
  const inputPath = path.resolve(process.cwd(), options.input || "./data/KAZIMIRIUM 1.json");
  const outputPath = path.resolve(process.cwd(), options.output || deriveOutputPath(inputPath));

  const sourceText = await fs.readFile(inputPath, "utf8");
  const sourceReplay = JSON.parse(sourceText);
  const prunedReplay = pruneReplay(sourceReplay);
  const outputText = JSON.stringify(prunedReplay);

  await fs.writeFile(outputPath, outputText, "utf8");

  const inputBytes = Buffer.byteLength(sourceText, "utf8");
  const outputBytes = Buffer.byteLength(outputText, "utf8");
  const deltaBytes = inputBytes - outputBytes;
  const deltaRatio = inputBytes > 0 ? (deltaBytes / inputBytes) * 100 : 0;

  console.log(JSON.stringify({
    inputPath,
    outputPath,
    inputBytes,
    outputBytes,
    savedBytes: deltaBytes,
    savedPercent: Number(deltaRatio.toFixed(2)),
    turns: Array.isArray(prunedReplay.turnHistory) ? prunedReplay.turnHistory.length : 0
  }, null, 2));
}

function parseArgs(argv) {
  return argv.reduce((options, arg, index, allArgs) => {
    if (!arg.startsWith("--")) {
      return options;
    }

    const optionName = arg.slice(2);
    const nextValue = allArgs[index + 1];
    if (!nextValue || nextValue.startsWith("--")) {
      options[optionName] = true;
      return options;
    }

    options[optionName] = nextValue;
    return options;
  }, {});
}

function deriveOutputPath(inputPath) {
  const extension = path.extname(inputPath);
  const basename = extension ? inputPath.slice(0, -extension.length) : inputPath;
  return `${basename}.light${extension || ".json"}`;
}

function pruneReplay(sourceReplay) {
  const sharedGrid = resolveSourceSharedGrid(sourceReplay);
  const hasReplayableHistory = Array.isArray(sourceReplay?.turnHistory)
    && sourceReplay.turnHistory.some((record) => record && record.snapshot);
  const prunedReplay = {
    schemaVersion: toNumber(sourceReplay && sourceReplay.schemaVersion, 5),
    saveName: typeof sourceReplay?.saveName === "string"
      ? sourceReplay.saveName
      : typeof sourceReplay?.initialSnapshot?.gameName === "string"
        ? sourceReplay.initialSnapshot.gameName
        : "Replay",
    sharedGrid,
    referenceData: pruneReferenceData(sourceReplay?.referenceData),
    turnHistory: Array.isArray(sourceReplay?.turnHistory)
      ? sourceReplay.turnHistory.map((record) => pruneTurnRecord(record, { omitGrid: true })).filter(Boolean)
      : []
  };

  if (typeof sourceReplay?.historyContinuityComplete === "boolean") {
    prunedReplay.historyContinuityComplete = sourceReplay.historyContinuityComplete;
  }

  if (Number.isFinite(sourceReplay?.createdAtUnix)) {
    prunedReplay.createdAtUnix = Math.trunc(sourceReplay.createdAtUnix);
  }

  if (sourceReplay?.sessionContext && Number.isFinite(sourceReplay.sessionContext.worldSeed)) {
    prunedReplay.sessionContext = {
      worldSeed: Math.trunc(sourceReplay.sessionContext.worldSeed)
    };
  }

  if (sourceReplay?.configContext?.combat && Number.isFinite(sourceReplay.configContext.combat.globalMaxRange)) {
    prunedReplay.configContext = {
      combat: {
        globalMaxRange: Math.trunc(sourceReplay.configContext.combat.globalMaxRange)
      }
    };
  }

  if (sourceReplay?.initialSnapshot) {
    prunedReplay.initialSnapshot = pruneSnapshot(sourceReplay.initialSnapshot, {
      omitGrid: true,
      omitGameName: true,
      omitTurnNumber: true
    });
  }

  const initialAnalytics = pruneAnalytics(sourceReplay?.initialAnalytics, sourceReplay?.initialSnapshot);
  if (initialAnalytics) {
    prunedReplay.initialAnalytics = initialAnalytics;
  }

  const initialActiveValidation = pruneValidation(sourceReplay?.initialActiveValidation);
  if (initialActiveValidation) {
    prunedReplay.initialActiveValidation = initialActiveValidation;
  }

  const initialNextTurnValidation = pruneValidation(sourceReplay?.initialNextTurnValidation);
  if (initialNextTurnValidation) {
    prunedReplay.initialNextTurnValidation = initialNextTurnValidation;
  }

  if (!hasReplayableHistory && sourceReplay?.currentStateSummary) {
    prunedReplay.currentStateSummary = pruneSnapshot(sourceReplay.currentStateSummary);
  }

  if (!hasReplayableHistory) {
    const currentAnalytics = pruneAnalytics(sourceReplay?.currentAnalytics, sourceReplay?.currentStateSummary);
    if (currentAnalytics) {
      prunedReplay.currentAnalytics = currentAnalytics;
    }

    const currentActiveValidation = pruneValidation(sourceReplay?.currentActiveValidation);
    if (currentActiveValidation) {
      prunedReplay.currentActiveValidation = currentActiveValidation;
    }

    const currentNextTurnValidation = pruneValidation(sourceReplay?.currentNextTurnValidation);
    if (currentNextTurnValidation) {
      prunedReplay.currentNextTurnValidation = currentNextTurnValidation;
    }

    if (Number.isFinite(sourceReplay?.lastUpdatedAtUnix)) {
      prunedReplay.lastUpdatedAtUnix = Math.trunc(sourceReplay.lastUpdatedAtUnix);
    }
  }

  return prunedReplay;
}

function resolveSourceSharedGrid(sourceReplay) {
  if (Array.isArray(sourceReplay?.sharedGrid) && sourceReplay.sharedGrid.length) {
    return pruneGrid(sourceReplay.sharedGrid);
  }
  if (Array.isArray(sourceReplay?.initialSnapshot?.grid) && sourceReplay.initialSnapshot.grid.length) {
    return pruneGrid(sourceReplay.initialSnapshot.grid);
  }
  if (Array.isArray(sourceReplay?.currentStateSummary?.grid) && sourceReplay.currentStateSummary.grid.length) {
    return pruneGrid(sourceReplay.currentStateSummary.grid);
  }

  const turnHistory = Array.isArray(sourceReplay?.turnHistory) ? sourceReplay.turnHistory : [];
  const firstRecordWithGrid = turnHistory.find((record) => (
    record && record.snapshot && Array.isArray(record.snapshot.grid) && record.snapshot.grid.length
  ));

  return firstRecordWithGrid ? pruneGrid(firstRecordWithGrid.snapshot.grid) : [];
}

function pruneReferenceData(referenceData) {
  return {
    kingdoms: pruneReferenceEntries(referenceData?.kingdoms),
    pieceTypes: pruneReferenceEntries(referenceData?.pieceTypes),
    buildingTypes: pruneReferenceEntries(referenceData?.buildingTypes)
  };
}

function pruneReferenceEntries(entries) {
  return Array.isArray(entries)
    ? entries
        .map((entry) => {
          const id = toNumber(entry?.id, null);
          const key = typeof entry?.key === "string" ? entry.key : null;
          if (id === null || !key) {
            return null;
          }

          return { id, key };
        })
        .filter(Boolean)
    : [];
}

function pruneTurnRecord(record, snapshotOptions = {}) {
  if (!record || !record.snapshot) {
    return null;
  }

  const prunedRecord = {
    committedTurnNumber: toNumber(record.committedTurnNumber, 0),
    committedActiveKingdom: toNumber(record.committedActiveKingdom, -1),
    committedActiveKingdomKey: resolveCommittedKingdomKey(record),
    snapshot: pruneSnapshot(record.snapshot, {
      omitGameName: true,
      omitTurnNumber: true,
      ...snapshotOptions
    })
  };

  const analytics = pruneAnalytics(record.analytics, record.snapshot);
  if (analytics) {
    prunedRecord.analytics = analytics;
  }

  const activeValidation = pruneValidation(record.activeValidation);
  if (activeValidation) {
    prunedRecord.activeValidation = activeValidation;
  }

  const nextTurnValidation = pruneValidation(record.nextTurnValidation);
  if (nextTurnValidation) {
    prunedRecord.nextTurnValidation = nextTurnValidation;
  }

  const newEvents = pruneEventList(record.newEvents);
  if (newEvents.length) {
    prunedRecord.newEvents = newEvents;
  }

  const notifications = pruneNotifications(record.notifications);
  if (notifications.length) {
    prunedRecord.notifications = notifications;
  }

  const structuredEvents = pruneStructuredEvents(record.structuredEvents);
  if (structuredEvents.length) {
    prunedRecord.structuredEvents = structuredEvents;
  }

  if (record.gameOver === true) {
    prunedRecord.gameOver = true;
  }

  if (Number.isFinite(record.winner)) {
    prunedRecord.winner = Math.trunc(record.winner);
  }

  return prunedRecord;
}

function pruneSnapshot(snapshot, options = {}) {
  if (!snapshot || typeof snapshot !== "object") {
    return null;
  }

  const prunedSnapshot = {
    activeKingdom: toNumber(snapshot.activeKingdom, 0),
    whiteKingdom: {
      pieces: prunePieces(snapshot.whiteKingdom?.pieces),
      buildings: pruneBuildings(snapshot.whiteKingdom?.buildings)
    },
    blackKingdom: {
      pieces: prunePieces(snapshot.blackKingdom?.pieces),
      buildings: pruneBuildings(snapshot.blackKingdom?.buildings)
    },
    publicBuildings: pruneBuildings(snapshot.publicBuildings),
    mapObjects: pruneMapObjects(snapshot.mapObjects),
    autonomousUnits: pruneAutonomousUnits(snapshot.autonomousUnits)
  };

  if (!options.omitGrid) {
    prunedSnapshot.grid = pruneGrid(snapshot.grid);
  }

  if (!options.omitGameName && typeof snapshot.gameName === "string") {
    prunedSnapshot.gameName = snapshot.gameName;
  }

  if (!options.omitTurnNumber && Number.isFinite(snapshot.turnNumber)) {
    prunedSnapshot.turnNumber = Math.trunc(snapshot.turnNumber);
  }

  const weatherState = pruneWeatherState(snapshot.weatherState);
  if (weatherState) {
    prunedSnapshot.weatherState = weatherState;
  }

  const events = pruneEventList(snapshot.events);
  if (events.length) {
    prunedSnapshot.events = events;
  }

  return prunedSnapshot;
}

function pruneGrid(grid) {
  return Array.isArray(grid)
    ? grid.map((row) => Array.isArray(row)
      ? row.map((cell) => pruneGridCell(cell))
      : [])
    : [];
}

function pruneGridCell(cell) {
  if (!cell || typeof cell !== "object") {
    return null;
  }

  return {
    t: toNumber(cell.t, 0),
    c: toNumber(cell.c, 0),
    f: toNumber(cell.f, 0),
    b: toNumber(cell.b, 255)
  };
}

function prunePieces(pieces) {
  return Array.isArray(pieces)
    ? pieces.map((piece) => prunePiece(piece)).filter(Boolean)
    : [];
}

function prunePiece(piece) {
  const id = toNumber(piece?.id, null);
  const type = toNumber(piece?.type, null);
  const kingdom = toNumber(piece?.kingdom, null);
  const x = toNumber(piece?.x, null);
  const y = toNumber(piece?.y, null);

  if (id === null || type === null || kingdom === null || x === null || y === null) {
    return null;
  }

  return {
    id,
    type,
    kingdom,
    x,
    y,
    xp: toNumber(piece?.xp, 0),
    hasWallBreachEntry: Boolean(piece?.hasWallBreachEntry),
    wallBreachEntryDx: toNumber(piece?.wallBreachEntryDx, 0),
    wallBreachEntryDy: toNumber(piece?.wallBreachEntryDy, 0),
    wallBreachCellX: toNumber(piece?.wallBreachCellX, -1),
    wallBreachCellY: toNumber(piece?.wallBreachCellY, -1)
  };
}

function pruneBuildings(buildings) {
  return Array.isArray(buildings)
    ? buildings.map((building) => pruneBuilding(building)).filter(Boolean)
    : [];
}

function pruneBuilding(building) {
  const id = toNumber(building?.id, null);
  const type = toNumber(building?.type, null);
  const owner = toNumber(building?.owner, null);
  const ox = toNumber(building?.ox, null);
  const oy = toNumber(building?.oy, null);
  const w = toNumber(building?.w, null);
  const h = toNumber(building?.h, null);

  if (id === null || type === null || owner === null || ox === null || oy === null || w === null || h === null) {
    return null;
  }

  return {
    id,
    type,
    owner,
    isNeutral: Boolean(building?.isNeutral),
    ox,
    oy,
    w,
    h,
    rot: toNumber(building?.rot, 0),
    fm: toNumber(building?.fm, 0),
    hp: Array.isArray(building?.hp) ? building.hp.map((value) => toNumber(value, 0)) : [],
    breach: Array.isArray(building?.breach) ? building.breach.map((value) => toNumber(value, 0)) : []
  };
}

function pruneMapObjects(mapObjects) {
  return Array.isArray(mapObjects)
    ? mapObjects.map((mapObject) => pruneMapObject(mapObject)).filter(Boolean)
    : [];
}

function pruneMapObject(mapObject) {
  const id = toNumber(mapObject?.id, null);
  const type = toNumber(mapObject?.type, null);
  const x = toNumber(mapObject?.x, null);
  const y = toNumber(mapObject?.y, null);
  if (id === null || type === null || x === null || y === null) {
    return null;
  }

  return {
    id,
    type,
    x,
    y,
    rewardType: toNumber(mapObject?.rewardType, 0),
    rewardAmount: toNumber(mapObject?.rewardAmount, 0),
    spawnTurn: toNumber(mapObject?.spawnTurn, 0)
  };
}

function pruneAutonomousUnits(units) {
  return Array.isArray(units)
    ? units.map((unit) => pruneAutonomousUnit(unit)).filter(Boolean)
    : [];
}

function pruneAutonomousUnit(unit) {
  const id = toNumber(unit?.id, null);
  const type = toNumber(unit?.type, null);
  const x = toNumber(unit?.x, null);
  const y = toNumber(unit?.y, null);
  if (id === null || type === null || x === null || y === null) {
    return null;
  }

  return {
    id,
    type,
    x,
    y,
    targetKingdom: toNumber(unit?.targetKingdom, -1),
    targetPieceId: toNumber(unit?.targetPieceId, -1),
    manifestedPieceType: toNumber(unit?.manifestedPieceType, -1),
    preferredTargetType: toNumber(unit?.preferredTargetType, -1),
    phase: toNumber(unit?.phase, 0),
    returnX: toNumber(unit?.returnX, x),
    returnY: toNumber(unit?.returnY, y),
    spawnTurn: toNumber(unit?.spawnTurn, 0)
  };
}

function pruneWeatherState(weatherState) {
  if (!weatherState || typeof weatherState !== "object") {
    return null;
  }

  const prunedState = {};
  const mask = pruneWeatherMask(weatherState.mask);
  if (mask) {
    prunedState.mask = mask;
  }

  const activeFronts = Array.isArray(weatherState.activeFronts)
    ? weatherState.activeFronts.map((front) => pruneWeatherFront(front)).filter(Boolean)
    : [];
  if (activeFronts.length) {
    prunedState.activeFronts = activeFronts;
  }

  return Object.keys(prunedState).length ? prunedState : null;
}

function pruneWeatherMask(mask) {
  if (!mask || typeof mask !== "object") {
    return null;
  }

  return {
    diameter: toNumber(mask.diameter, 0),
    alphaByCell: Array.isArray(mask.alphaByCell) ? mask.alphaByCell.map((value) => toNumber(value, 0)) : [],
    shadeByCell: Array.isArray(mask.shadeByCell) ? mask.shadeByCell.map((value) => toNumber(value, 0)) : []
  };
}

function pruneWeatherFront(front) {
  if (!front || typeof front !== "object") {
    return null;
  }

  const directionId = Number.isFinite(front.directionId)
    ? Math.trunc(front.directionId)
    : Number.isFinite(front.direction)
      ? Math.trunc(front.direction)
      : null;

  return {
    directionKey: typeof front.directionKey === "string"
      ? front.directionKey
      : directionId === null
        ? null
        : WEATHER_FRONT_DIRECTION_KEYS[directionId] || null,
    currentTurnStep: toNumber(front.currentTurnStep, 0),
    totalTurnSteps: toNumber(front.totalTurnSteps, 0),
    centerStartXTimes1000: toNumber(front.centerStartXTimes1000, 0),
    centerStartYTimes1000: toNumber(front.centerStartYTimes1000, 0),
    stepXTimes1000: toNumber(front.stepXTimes1000, 0),
    stepYTimes1000: toNumber(front.stepYTimes1000, 0),
    radiusAlongTimes1000: toNumber(front.radiusAlongTimes1000, 0),
    radiusAcrossTimes1000: toNumber(front.radiusAcrossTimes1000, 0)
  };
}

function pruneAnalytics(analytics, snapshot) {
  if (!analytics || typeof analytics !== "object") {
    return null;
  }

  const prunedAnalytics = {};

  const visibility = pruneVisibilityAnalytics(analytics.visibility);
  if (visibility) {
    prunedAnalytics.visibility = visibility;
  }

  const weather = pruneWeatherAnalytics(analytics.weather, snapshot);
  if (weather) {
    prunedAnalytics.weather = weather;
  }

  const infernal = pruneInfernalAnalytics(analytics.infernal);
  if (infernal) {
    prunedAnalytics.infernal = infernal;
  }

  return Object.keys(prunedAnalytics).length ? prunedAnalytics : null;
}

function pruneVisibilityAnalytics(visibility) {
  if (!visibility || typeof visibility !== "object") {
    return null;
  }

  const byObserver = Array.isArray(visibility.byObserver)
    ? visibility.byObserver
        .map((row) => {
          const observerKingdomKey = typeof row?.observerKingdomKey === "string"
            ? row.observerKingdomKey
            : kingdomKeyFromId(row?.observerKingdom);
          if (!observerKingdomKey || observerKingdomKey === "unknown") {
            return null;
          }

          return {
            observerKingdomKey,
            hiddenEnemyPieceIds: Array.isArray(row?.hiddenEnemyPieceIds)
              ? row.hiddenEnemyPieceIds.map((value) => toNumber(value, 0)).filter((value) => value > 0)
              : []
          };
        })
        .filter(Boolean)
    : [];

  return {
    fogCellCount: toNumber(visibility.fogCellCount, 0),
    concealingFogCellCount: toNumber(visibility.concealingFogCellCount, 0),
    hasActiveFront: Boolean(visibility.hasActiveFront),
    byObserver
  };
}

function pruneWeatherAnalytics(weather, snapshot) {
  if ((!weather || typeof weather !== "object") && !snapshot?.weatherState?.activeFronts) {
    return null;
  }

  const frontCount = Number.isFinite(weather?.frontCount)
    ? Math.trunc(weather.frontCount)
    : Array.isArray(snapshot?.weatherState?.activeFronts)
      ? snapshot.weatherState.activeFronts.length
      : 0;

  return {
    frontCount
  };
}

function pruneInfernalAnalytics(infernal) {
  if (!infernal || typeof infernal !== "object") {
    return null;
  }

  const prunedInfernal = {
    whiteBloodDebt: toNumber(infernal.whiteBloodDebt, 0),
    blackBloodDebt: toNumber(infernal.blackBloodDebt, 0),
    activeInfernalUnitId: toNumber(infernal.activeInfernalUnitId, -1)
  };

  const activeInfernal = pruneInfernalDescriptorHolder(infernal.activeInfernal);
  if (activeInfernal) {
    prunedInfernal.activeInfernal = activeInfernal;
  }

  return prunedInfernal;
}

function pruneInfernalDescriptorHolder(value) {
  if (!value || typeof value !== "object") {
    return null;
  }

  const descriptor = {
    targetKingdomKey: normalizeKingdomKey(
      value.targetKingdomKey,
      value.targetKingdom,
      value.infernal?.targetKingdomKey,
      value.infernal?.targetKingdom
    ),
    manifestedPieceTypeKey: normalizePieceTypeKey(
      value.manifestedPieceTypeKey,
      value.manifestedPieceKey,
      value.manifestedPieceType,
      value.infernal?.manifestedPieceTypeKey,
      value.infernal?.manifestedPieceKey,
      value.infernal?.manifestedPieceType
    )
  };

  const pruned = {
    targetKingdomKey: descriptor.targetKingdomKey,
    manifestedPieceTypeKey: descriptor.manifestedPieceTypeKey
  };

  if (value.infernal && typeof value.infernal === "object") {
    pruned.infernal = {
      targetKingdomKey: descriptor.targetKingdomKey,
      manifestedPieceTypeKey: descriptor.manifestedPieceTypeKey
    };
  }

  return pruned;
}

function pruneValidation(validation) {
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
    projectedEndingGold: toNumber(validation.projectedEndingGold, 0),
    errorMessage: typeof validation.errorMessage === "string" ? validation.errorMessage : ""
  };
}

function pruneEventList(events) {
  return Array.isArray(events)
    ? events.map((event) => pruneEvent(event)).filter(Boolean)
    : [];
}

function pruneEvent(event) {
  if (typeof event === "string") {
    return event.trim() || null;
  }

  if (!event || typeof event !== "object") {
    return null;
  }

  return firstString(event.message, event.msg, event.kindLabel, event.kindKey);
}

function pruneNotifications(notifications) {
  return Array.isArray(notifications)
    ? notifications.map((notification) => pruneNotification(notification)).filter(Boolean)
    : [];
}

function pruneNotification(notification) {
  if (!notification || typeof notification !== "object") {
    return null;
  }

  if (notification.kindKey !== "chest_reward") {
    return null;
  }

  const kingdomKey = normalizeKingdomKey(notification.kingdomKey, notification.kingdom);
  const chestRewardTypeKey = normalizeRewardTypeKey(
    notification.chestRewardTypeKey,
    notification.chestReward?.typeKey,
    notification.chestRewardType,
    notification.chestReward?.type
  );
  const chestRewardAmount = Number.isFinite(notification.chestRewardAmount)
    ? Math.trunc(notification.chestRewardAmount)
    : Number.isFinite(notification.chestReward?.amount)
      ? Math.trunc(notification.chestReward.amount)
      : 0;

  return {
    kindKey: "chest_reward",
    kingdomKey,
    chestReward: {
      typeKey: chestRewardTypeKey,
      amount: chestRewardAmount
    }
  };
}

function pruneStructuredEvents(events) {
  return Array.isArray(events)
    ? events.map((event) => pruneStructuredEvent(event)).filter(Boolean)
    : [];
}

function pruneStructuredEvent(event) {
  if (!event || typeof event !== "object") {
    return null;
  }

  switch (event.typeKey) {
    case "weather_front_spawned":
      return {
        typeKey: event.typeKey,
        front: pruneStructuredWeatherFront(event.front)
      };
    case "weather_front_ended":
      return {
        typeKey: event.typeKey
      };
    case "infernal_spawned":
    case "infernal_removed":
      return {
        typeKey: event.typeKey,
        unitId: toNumber(event.unitId, 0)
      };
    default:
      return null;
  }
}

function pruneStructuredWeatherFront(front) {
  if (!front || typeof front !== "object") {
    return null;
  }

  const directionKey = normalizeWeatherDirectionKey(front.directionKey, front.directionId, front.direction);
  return directionKey ? { directionKey } : null;
}

function resolveCommittedKingdomKey(record) {
  return firstString(
    record?.committedActiveKingdomKey,
    record?.turnDelta?.activeKingdomKey,
    record?.behavioralTelemetry?.activeKingdomKey,
    kingdomKeyFromId(record?.committedActiveKingdom)
  ) || "white";
}

function normalizeKingdomKey(...values) {
  for (const value of values) {
    if (typeof value === "string") {
      const normalized = value.trim().toLowerCase();
      if (normalized === "white" || normalized === "black") {
        return normalized;
      }
    }

    const numericValue = toNumber(value, null);
    if (numericValue === 0) {
      return "white";
    }
    if (numericValue === 1) {
      return "black";
    }
  }

  return "unknown";
}

function kingdomKeyFromId(value) {
  return normalizeKingdomKey(value);
}

function normalizePieceTypeKey(...values) {
  for (const value of values) {
    if (typeof value === "string") {
      const normalized = value.trim().toLowerCase();
      if (normalized) {
        return normalized;
      }
    }

    const numericValue = toNumber(value, null);
    if (numericValue !== null && numericValue >= 0 && numericValue < PIECE_TYPE_KEYS.length) {
      return PIECE_TYPE_KEYS[numericValue];
    }
  }

  return "infernal";
}

function normalizeRewardTypeKey(...values) {
  for (const value of values) {
    if (typeof value === "string" && value.trim()) {
      return value.trim();
    }

    const numericValue = toNumber(value, null);
    if (numericValue === 0) {
      return "gold";
    }
    if (numericValue === 1) {
      return "xp";
    }
  }

  return "gold";
}

function normalizeWeatherDirectionKey(...values) {
  for (const value of values) {
    if (typeof value === "string" && value.trim()) {
      return value.trim();
    }

    const numericValue = toNumber(value, null);
    if (numericValue !== null && numericValue >= 0 && numericValue < WEATHER_FRONT_DIRECTION_KEYS.length) {
      return WEATHER_FRONT_DIRECTION_KEYS[numericValue];
    }
  }

  return null;
}

function firstString(...values) {
  for (const value of values) {
    if (typeof value === "string" && value.trim()) {
      return value.trim();
    }
  }

  return null;
}

function toNumber(value, fallback) {
  const numericValue = Number(value);
  return Number.isFinite(numericValue) ? Math.trunc(numericValue) : fallback;
}

main().catch((error) => {
  console.error(error);
  process.exitCode = 1;
});