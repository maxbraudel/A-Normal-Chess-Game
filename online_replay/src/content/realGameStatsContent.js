import { REPLAY_CONFIG } from "../../config.js";
import { loadReplayData } from "../stores/replayDataStore.js";

const REPORT_COLORS = {
  ink: "#1f1f1f",
  graphite: "#5f5f5f",
  mist: "#d8d8d8",
  moss: "#5f6f58",
  sand: "#b7925b",
  brick: "#9b6958",
  slate: "#76869a"
};

const LEGACY_COLORS = {
  whiteKingdom: "rgba(154, 132, 94, 0.95)",
  blackKingdom: "rgba(62, 87, 125, 0.92)",
  water: "rgba(91, 178, 166, 0.92)",
  fog: "rgba(215, 164, 83, 0.85)",
  infernalTargetWhite: "rgba(233, 233, 223, 0.82)",
  infernalTargetBlack: "rgba(62, 87, 125, 0.86)",
  infernalSpan: "rgba(0, 0, 0, 0.22)",
  infernalSpanWhite: "rgba(150, 139, 122, 0.26)",
  infernalSpanBlack: "rgba(96, 108, 123, 0.28)"
};

const DEFAULT_GLOBAL_MAX_RANGE = 8;
const CELL_TYPE_VOID = 0;
const CELL_TYPE_WATER = 3;
const PIECE_TYPE_PAWN = 0;
const PIECE_TYPE_KNIGHT = 1;
const PIECE_TYPE_BISHOP = 2;
const PIECE_TYPE_ROOK = 3;
const PIECE_TYPE_QUEEN = 4;
const PIECE_TYPE_KING = 5;
const BUILDING_TYPE_WOOD_WALL = 4;
const BUILDING_TYPE_STONE_WALL = 5;

const EMPTY_REAL_GAME_STATS_REPORT = Object.freeze({
  observedDataLabel: "Données observées sur la partie réelle",
  processStatsByTitle: {}
});

let cachedRealGameStatsPromise = null;

export function loadRealGameStatsReport() {
  if (!cachedRealGameStatsPromise) {
    cachedRealGameStatsPromise = buildRealGameStatsReport().catch((error) => {
      cachedRealGameStatsPromise = null;
      throw error;
    });
  }

  return cachedRealGameStatsPromise;
}

async function buildRealGameStatsReport() {
  const replayData = await fetchReplayData();
  const saveName = keyOrFallback(replayData && replayData.saveName, "KAZIMIRIUM 1");
  const timeline = buildTimeline(replayData);

  if (!timeline.length) {
    return {
      ...EMPTY_REAL_GAME_STATS_REPORT,
      observedDataLabel: `Données observées sur la partie réelle (${saveName})`
    };
  }

  const infernal = normalizeInfernal(timeline);
  const weather = normalizeWeather(timeline);
  const waterDenied = normalizeWaterDenied(timeline, replayData);

  return {
    observedDataLabel: `Données observées sur la partie réelle (${saveName})`,
    processStatsByTitle: {
      "Royaume cible d'une pièce du diable": infernal.points.length
        ? [buildInfernalBlock(infernal)]
        : [],
      "Durée visible d'un brouillard": weather.points.length
        ? [buildWeatherBlock(weather)]
        : [],
      "Champ spatial de l'eau": waterDenied.points.length
        ? [buildWaterDeniedByKingdomBlock(waterDenied)]
        : []
    }
  };
}

async function fetchReplayData() {
  return loadReplayData(REPLAY_CONFIG.replayUrl);
}

function buildTimeline(replayData) {
  const turnHistory = Array.isArray(replayData && replayData.turnHistory)
    ? replayData.turnHistory
    : [];
  const sharedGrid = resolveSharedReplayGrid(replayData);

  return turnHistory
    .filter((record) => record && record.snapshot)
    .map((record, sequenceIndex) => ({
      sequenceIndex,
      xIndex: sequenceIndex,
      turn: toNumber(record.committedTurnNumber, sequenceIndex + 1),
      committedActiveKingdom: toNumber(record.committedActiveKingdom, -1),
      committedActiveKingdomKey: resolveCommittedKingdomKey(record),
      sharedGrid,
      snapshot: record.snapshot,
      snapshotMetrics: record.snapshotMetrics || null,
      analytics: record.analytics || null,
      turnDelta: record.turnDelta || null,
      structuredEvents: Array.isArray(record.structuredEvents) ? record.structuredEvents : []
    }));
}

function resolveSharedReplayGrid(replayData) {
  if (Array.isArray(replayData && replayData.sharedGrid) && replayData.sharedGrid.length) {
    return replayData.sharedGrid;
  }
  if (Array.isArray(replayData && replayData.initialSnapshot && replayData.initialSnapshot.grid) && replayData.initialSnapshot.grid.length) {
    return replayData.initialSnapshot.grid;
  }
  if (Array.isArray(replayData && replayData.currentStateSummary && replayData.currentStateSummary.grid) && replayData.currentStateSummary.grid.length) {
    return replayData.currentStateSummary.grid;
  }

  const turnHistory = Array.isArray(replayData && replayData.turnHistory) ? replayData.turnHistory : [];
  const firstRecordWithGrid = turnHistory.find((record) => (
    record && record.snapshot && Array.isArray(record.snapshot.grid) && record.snapshot.grid.length
  ));

  return firstRecordWithGrid ? firstRecordWithGrid.snapshot.grid : [];
}

function resolveCommittedKingdomKey(record) {
  return keyOrFallback(
    record && record.committedActiveKingdomKey,
    keyOrFallback(
      record && record.turnDelta && record.turnDelta.activeKingdomKey,
      keyOrFallback(
        record && record.behavioralTelemetry && record.behavioralTelemetry.activeKingdomKey,
        kingdomKeyFromId(record && record.committedActiveKingdom)
      )
    )
  );
}

function buildInfernalBlock(infernal) {
  const targetCounts = infernal.unitRows.reduce(
    (counts, row) => {
      if (row.targetKingdomKey === "white") {
        counts.white += 1;
      } else if (row.targetKingdomKey === "black") {
        counts.black += 1;
      } else {
        counts.unknown += 1;
      }
      return counts;
    },
    { white: 0, black: 0, unknown: 0 }
  );

  return {
    eyebrow: "Partie réelle",
    title: "Dette, apparitions et durée de vie",
    description:
      "Reprise du graphe gameplay du générateur historique: dettes de sang, marqueurs d'apparition/suppression et bandes de durée de vie des pièces du diable sur la partie chargée.",
    metrics: [
      { label: "Dette blanche moy.", value: formatStatNumber(infernal.averageWhiteDebt) },
      { label: "Dette noire moy.", value: formatStatNumber(infernal.averageBlackDebt) },
      { label: "Apparitions observées", value: formatInteger(infernal.spawnEvents.length) },
      {
        label: "Durée de vie moyenne",
        value: infernal.averageLifetime ? `${formatStatNumber(infernal.averageLifetime)} tours` : "n/d"
      }
    ],
    insights: [
      `${formatInteger(infernal.spawnEvents.length)} apparitions et ${formatInteger(infernal.unitRows.filter((row) => row.removedTurn !== null).length)} suppressions observées sur la partie réelle.`,
      targetCounts.white || targetCounts.black
        ? `Ciblages observés: ${formatInteger(targetCounts.white)} vers le blanc, ${formatInteger(targetCounts.black)} vers le noir${targetCounts.unknown ? `, ${formatInteger(targetCounts.unknown)} non qualifiés` : ""}.`
        : "Le companion ne qualifie pas toujours le royaume cible sur chaque apparition d'une pièce du diable; les dettes restent en revanche exactes."
    ],
    chartHeight: 360,
    chartLabel: "Dette de sang, apparitions et durée de vie sur la partie réelle",
    postChartInterpretation:
      "**Interprétation : on remarque en effet que, de manière générale, la dette de sang pendant la partie a été beaucoup plus élevée côté noir, de manière continue et soutenue. Cela fait qu'au total, même si le processus reste aléatoire, beaucoup plus de pièces du diable ont ciblé le joueur noir plutôt que le joueur blanc. On peut le voir dans l'exemple qui suit.**",
    exampleReplay: {
      sourceTag: "Partie réelle avec joueur",
      sourceKind: "real",
      label: "LE REPLAY MONTRE LES PIÈCES DU DIABLE CIBLER LE MÊME ROYAUME",
      description:
        "Du tour 74 au tour 136, la caméra suit automatiquement la pièce du diable active et se met à jour à chaque tour. Le déplacement manuel est verrouillé sur cet exemple pour conserver une lecture stable des apparitions, des suppressions et de la durée de vie des menaces infernales, mais le zoom reste disponible.",
      viewer: {
        replayUrl: REPLAY_CONFIG.replayUrl,
        minTurn: 74,
        maxTurn: 136,
        initialTurn: 74,
        autoplayOnMount: true,
        autoplayIntervalMs: 220,
        loopPlayback: true,
        initialZoom: 2.15,
        trackedTarget: {
          kind: "active-infernal-unit"
        },
        updateCameraOnEveryTick: true,
        lockCamera: true,
        showStatusOverlay: false
      }
    },
    chartOption: buildInfernalTimelineOption({
      xAxisName: "Tour",
      yAxes: [
        { name: "Dette de sang" }
      ],
      markers: infernal.spawnEvents.map((event) => ({
        xIndex: event.xIndex,
        color: REPORT_COLORS.ink,
        dashed: true
      })).concat(
        infernal.unitRows
          .filter((row) => row.removedTurn !== null)
          .map((row) => ({
            xIndex: row.removedXIndex,
            color: REPORT_COLORS.ink,
            dashed: true
          }))
      ),
      spans: infernal.unitRows.map((row) => ({
        xStartIndex: row.spawnXIndex,
        xEndIndex: row.removedTurn !== null ? row.removedXIndex : row.endXIndex,
        targetKingdomKey: row.targetKingdomKey,
        fillColor: infernalSpanFillColor(row.targetKingdomKey),
        pieceImageUrl: infernalPieceTextureUrl(row.manifestedPieceKey),
        pieceBadgeText: pieceBadgeLabel(row.manifestedPieceKey),
        pieceLabelFr: infernalPieceLabelFr(row.manifestedPieceKey)
      })),
      series: [
        buildTimelineSeriesSpec("Dette de sang blanche", infernal.points, "whiteDebt", LEGACY_COLORS.whiteKingdom, 0),
        buildTimelineSeriesSpec("Dette de sang noire", infernal.points, "blackDebt", LEGACY_COLORS.blackKingdom, 0)
      ]
    })
  };
}

function buildWeatherBlock(weather) {
  const ambushPoint = weather.points.find((point) => point.turn === 113) || null;

  return {
    eyebrow: "Partie réelle",
    title: "Visibilité et brouillards",
    description:
      "Courbes de visibilité pendant la partie réelle: pièces ennemies masquées par royaume, avec marqueurs d'entrée et de fin des brouillards.",
    metrics: [
      { label: "Couverture moyenne", value: formatStatNumber(weather.averageCloudCoverage) },
      {
        label: "Intervalle moyen",
        value: weather.averageSpawnInterval ? `${formatStatNumber(weather.averageSpawnInterval)} tours` : "n/d"
      },
      { label: "Brouillards observés", value: formatInteger(weather.spawnEvents.length) },
      { label: "Pic de pièces masquées", value: formatInteger(weather.peakHiddenPieces) }
    ],
    insights: [
      `${formatInteger(weather.spawnEvents.length)} apparitions et ${formatInteger(weather.endEvents.length)} fins de brouillards observées sur la partie réelle.`
    ],
    chartHeight: 360,
    chartLabel: "Visibilité et brouillards sur la partie réelle",
    postChartInterpretation:
      "**Interprétation : on remarque que vers la fin de la partie, avant l'échec et mat, une partie des pièces ennemies, en pratique des pièces noires du point de vue blanc, sont masquées. Cela traduit une embuscade du joueur noir, qui va finalement lui permettre de gagner la partie, comme on peut le voir dans l'exemple qui suit.**",
    exampleReplay: {
      sourceTag: "Partie réelle avec joueur",
      sourceKind: "real",
      label: "Replay de l'embuscade de la partie réelle",
      description:
        "Entre les tours 120 et 133, un nuage imprévisible ouvre un renversement total au centre de la carte: les Noirs prennent les Blancs en embuscade et les deux reines sont éliminées.",
      viewer: {
        replayUrl: REPLAY_CONFIG.replayUrl,
        minTurn: 120,
        maxTurn: 133,
        initialTurn: 120,
        autoplayOnMount: true,
        loopPlayback: true,
        initialZoom: 2.4,
        enablePerspective: true,
        perspectiveKingdom: "white",
        showStatusOverlay: true
      }
    },
    chartOption: buildTimelineOption({
      xAxisName: "Tour",
      yAxes: [
        { name: "Pièces masquées" }
      ],
      markers: ambushPoint
        ? [
            {
              xIndex: ambushPoint.xIndex,
              color: REPORT_COLORS.ink,
              dashed: true,
              label: "Embuscade du royaume noir",
              labelPosition: "insideEndBottom",
              labelOffset: [0, 10]
            }
          ]
        : [],
      series: [
        buildTimelineSeriesSpec("Pièces ennemies masquées côté blanc", weather.points, "whiteHiddenPieces", LEGACY_COLORS.whiteKingdom, 0),
        buildTimelineSeriesSpec("Pièces ennemies masquées côté noir", weather.points, "blackHiddenPieces", LEGACY_COLORS.water, 0)
      ]
    })
  };
}

function buildWaterDeniedByKingdomBlock(waterDenied) {
  return {
    eyebrow: "Partie réelle",
    title: "Cellules refusées par l'eau par royaume",
    description:
      "Cette métrique recompte, à chaque enregistrement de la partie, **les cases de destination qui seraient atteignables si l'eau était retirée**, puis soustrait les pseudo-coups réellement autorisés sur le plateau courant. Le résultat est ensuite **sommé sur toutes les pièces d'un royaume**: une courbe haute signifie donc que l'eau supprime beaucoup d'options de déplacement à cet instant, pas qu'un royaume est bloqué partout sur la carte. Les murs et les autres obstacles restent inchangés pendant ce recalcul; **on isole uniquement l'effet de l'eau**.",
    metrics: [
      { label: "Eau blanche moy.", value: formatStatNumber(waterDenied.averageWhiteWaterDeniedCells) },
      { label: "Eau noire moy.", value: formatStatNumber(waterDenied.averageBlackWaterDeniedCells) },
      { label: "Pic total eau", value: formatInteger(waterDenied.peakWaterDeniedCells) }
    ],
    insights: [
      `Le calcul repart des mouvements pseudo-légaux, comme dans le site \`statistiques-generator\`, au lieu d'utiliser une simple approximation géométrique des lacs. L'écart moyen blanc/noir est ici de ${formatStatNumber(waterDenied.averageKingdomGap)} cellules refusées par enregistrement.`
    ],
    chartHeight: 330,
    chartLabel: "Cellules refusées par l'eau par royaume sur la partie réelle",
    postChartInterpretation:
      "**Interprétation : au début de la partie réelle, la courbe noire dépasse nettement la courbe blanche. L'eau a donc beaucoup plus empêché les premiers déplacements du royaume noir que ceux du royaume blanc. Cette contrainte initiale a freiné le développement noir et a contribué à l'avance prise par les Blancs au début de la partie.**",
    exampleReplay: {
      sourceTag: "Partie réelle avec joueur",
      sourceKind: "real",
      label: "Exemple tiré de la partie réelle",
      description:
        "Du tour 40 au tour 90, on voit les Noirs devoir contourner plus tôt et plus souvent l'eau centrale, pendant que les Blancs prennent plus vite le contrôle de l'église: une zone qui permet la promotion de reines.",
      viewer: {
        replayUrl: REPLAY_CONFIG.replayUrl,
        minTurn: 40,
        maxTurn: 90,
        initialTurn: 40,
        autoplayOnMount: true,
        autoplayIntervalMs: 240,
        loopPlayback: true,
        initialZoom: 2,
        enablePerspective: false,
        trackedTarget: {
          kind: "terrain-cell",
          x: 24,
          y: 17
        },
        showStatusOverlay: false
      }
    },
    chartOption: buildTimelineOption({
      xAxisName: "Tour",
      yAxes: [
        { name: "Cellules refusées par l'eau" }
      ],
      series: [
        buildTimelineSeriesSpec("Cellules refusées par l'eau du royaume blanc", waterDenied.points, "whiteWaterDeniedCells", LEGACY_COLORS.whiteKingdom, 0),
        buildTimelineSeriesSpec("Cellules refusées par l'eau du royaume noir", waterDenied.points, "blackWaterDeniedCells", LEGACY_COLORS.blackKingdom, 0)
      ]
    })
  };
}

function normalizeWeather(timeline) {
  const points = [];
  const spawnEvents = [];
  const endEvents = [];

  timeline.forEach((record) => {
    const visibility = (record.analytics && record.analytics.visibility) || {};
    const weather = (record.analytics && record.analytics.weather) || {};
    const whiteObserver = findObserverEntry(visibility, "white");
    const blackObserver = findObserverEntry(visibility, "black");

    points.push({
      xIndex: record.xIndex,
      turn: record.turn,
      committedActiveKingdomKey: record.committedActiveKingdomKey,
      whiteHiddenPieces: lengthOf(whiteObserver && whiteObserver.hiddenEnemyPieceIds),
      blackHiddenPieces: lengthOf(blackObserver && blackObserver.hiddenEnemyPieceIds),
      totalHiddenPieces: lengthOf(whiteObserver && whiteObserver.hiddenEnemyPieceIds)
        + lengthOf(blackObserver && blackObserver.hiddenEnemyPieceIds),
      fogCellCount: toNumber(visibility.fogCellCount, 0),
      concealingFogCellCount: toNumber(visibility.concealingFogCellCount, 0),
      frontCount: toNumber(weather.frontCount, 0),
      hasActiveFront: Boolean(visibility.hasActiveFront)
    });

    record.structuredEvents.forEach((event) => {
      const front = event && typeof event.front === "object" ? event.front : {};
      if (event && event.typeKey === "weather_front_spawned") {
        spawnEvents.push({
          turn: record.turn,
          xIndex: record.xIndex,
          x: record.turn,
          label: shortDirectionLabel(front),
          color: LEGACY_COLORS.fog,
          dashed: false
        });
      }

      if (event && event.typeKey === "weather_front_ended") {
        endEvents.push({
          turn: record.turn,
          xIndex: record.xIndex,
          x: record.turn,
          color: LEGACY_COLORS.water,
          dashed: true
        });
      }
    });
  });

  const fogCoverageTurns = points.filter((point) =>
    point.frontCount > 0 || point.concealingFogCellCount > 0 || point.fogCellCount > 0
  );

  return {
    points,
    spawnEvents,
    endEvents,
    averageCloudCoverage: mean(fogCoverageTurns.map((point) => point.concealingFogCellCount || point.fogCellCount)),
    averageSpawnInterval: mean(turnDiffs(spawnEvents.map((event) => event.x))),
    peakHiddenPieces: maxOf(points.map((point) => Math.max(point.whiteHiddenPieces, point.blackHiddenPieces)), 0)
  };
}

function normalizeInfernal(timeline) {
  const points = [];
  const spawnEvents = [];
  const activeUnitSpans = Object.create(null);
  const completedUnitSpans = [];

  timeline.forEach((record) => {
    const infernal = (record.analytics && record.analytics.infernal) || {};
    const autonomousUnits = resolveInfernalAutonomousUnits(record);
    const autonomousById = buildIndexById(autonomousUnits);

    points.push({
      xIndex: record.xIndex,
      turn: record.turn,
      committedActiveKingdomKey: record.committedActiveKingdomKey,
      whiteDebt: toNumber(infernal.whiteBloodDebt, 0),
      blackDebt: toNumber(infernal.blackBloodDebt, 0),
      totalDebt: toNumber(infernal.whiteBloodDebt, 0) + toNumber(infernal.blackBloodDebt, 0),
      activeInfernalUnitId: toNumber(infernal.activeInfernalUnitId, -1),
      activeInfernalCount: autonomousUnits.length
    });

    record.structuredEvents.forEach((event) => {
      if (!event) {
        return;
      }

      if (event.typeKey === "infernal_spawned") {
        const unitId = toNumber(event.unitId, 0);
        const unitIdKey = String(unitId);
        const descriptor = extractInfernalDescriptor(record, autonomousById[unitId]);

        if (activeUnitSpans[unitIdKey]) {
          completedUnitSpans.push(activeUnitSpans[unitIdKey]);
        }

        spawnEvents.push({
          xIndex: record.xIndex,
          turn: record.turn,
          unitId,
          targetKingdomKey: descriptor.targetKingdomKey,
          manifestedPieceKey: descriptor.manifestedPieceKey,
          label: buildInfernalSpawnLabel(descriptor, unitId),
          color: infernalMarkerColor(descriptor.targetKingdomKey)
        });

        activeUnitSpans[unitIdKey] = {
          unitId,
          spawnXIndex: record.xIndex,
          spawnTurn: record.turn,
          targetKingdomKey: descriptor.targetKingdomKey,
          manifestedPieceKey: descriptor.manifestedPieceKey,
          removedXIndex: null,
          removedTurn: null
        };
        return;
      }

      if (event.typeKey === "infernal_removed") {
        const unitIdKey = String(toNumber(event.unitId, 0));
        if (activeUnitSpans[unitIdKey]) {
          activeUnitSpans[unitIdKey].removedXIndex = record.xIndex;
          activeUnitSpans[unitIdKey].removedTurn = record.turn;
          completedUnitSpans.push(activeUnitSpans[unitIdKey]);
          delete activeUnitSpans[unitIdKey];
        }
      }
    });
  });

  const lastTurn = timeline.length ? timeline[timeline.length - 1].turn : 0;
  const unitRows = completedUnitSpans
    .concat(Object.values(activeUnitSpans))
    .map((span) => {
      const endTurn = span.removedTurn !== null ? span.removedTurn : lastTurn;
      return {
        unitId: span.unitId,
        spawnXIndex: span.spawnXIndex,
        spawnTurn: span.spawnTurn,
        targetKingdomKey: span.targetKingdomKey,
        manifestedPieceKey: span.manifestedPieceKey,
        removedXIndex: span.removedXIndex,
        removedTurn: span.removedTurn,
        observedLifetime: Math.max(1, endTurn - span.spawnTurn + 1),
        endXIndex: span.removedXIndex !== null ? span.removedXIndex : timeline.length - 1,
        endTurn
      };
    })
    .sort((left, right) => left.spawnTurn - right.spawnTurn);

  return {
    points,
    spawnEvents,
    unitRows,
    averageWhiteDebt: mean(points.map((point) => point.whiteDebt)),
    averageBlackDebt: mean(points.map((point) => point.blackDebt)),
    averageSpawnInterval: mean(turnDiffs(spawnEvents.map((event) => event.turn))),
    averageLifetime: mean(unitRows.map((row) => row.observedLifetime))
  };
}

function extractInfernalDescriptor(record, unit) {
  const activeInfernal = record
    && record.analytics
    && record.analytics.infernal
    && typeof record.analytics.infernal.activeInfernal === "object"
      ? record.analytics.infernal.activeInfernal
      : null;
  const infernal = unit && unit.infernal && typeof unit.infernal === "object"
    ? unit.infernal
    : activeInfernal && activeInfernal.infernal && typeof activeInfernal.infernal === "object"
      ? activeInfernal.infernal
      : activeInfernal;

  return {
    targetKingdomKey: keyOrFallback(
      infernal && infernal.targetKingdomKey,
      keyOrFallback(unit && unit.targetKingdomKey,
      keyOrFallback(activeInfernal && activeInfernal.targetKingdomKey, "unknown")
      )
    ),
    manifestedPieceKey: keyOrFallback(
      infernal && (infernal.manifestedPieceTypeKey || infernal.manifestedPieceKey),
      keyOrFallback(
        unit && (unit.manifestedPieceTypeKey || unit.manifestedPieceKey),
        keyOrFallback(activeInfernal && (activeInfernal.manifestedPieceTypeKey || activeInfernal.manifestedPieceKey), "infernal")
      )
    )
  };
}

function resolveInfernalAutonomousUnits(record) {
  const analyticsEntities = record && record.analytics && record.analytics.entities && typeof record.analytics.entities === "object"
    ? record.analytics.entities
    : {};
  const analyticsUnits = Array.isArray(analyticsEntities.autonomousUnitIndex)
    ? analyticsEntities.autonomousUnitIndex
    : [];
  if (analyticsUnits.length) {
    return analyticsUnits;
  }

  const snapshotUnits = record && record.snapshot && Array.isArray(record.snapshot.autonomousUnits)
    ? record.snapshot.autonomousUnits
    : [];

  return snapshotUnits
    .map((unit) => normalizeSnapshotAutonomousUnit(unit))
    .filter(Boolean);
}

function normalizeSnapshotAutonomousUnit(unit) {
  const id = toNumber(unit && unit.id, null);
  if (id === null) {
    return null;
  }

  const infernal = unit && unit.infernal && typeof unit.infernal === "object"
    ? unit.infernal
    : null;

  return {
    id,
    x: toNumber(unit && unit.x, 0),
    y: toNumber(unit && unit.y, 0),
    infernal,
    targetKingdomKey: keyOrFallback(
      unit && unit.targetKingdomKey,
      keyOrFallback(
        infernal && infernal.targetKingdomKey,
        kingdomKeyFromId(unit && unit.targetKingdom)
      )
    ),
    manifestedPieceTypeKey: keyOrFallback(
      unit && unit.manifestedPieceTypeKey,
      keyOrFallback(
        unit && unit.manifestedPieceKey,
        keyOrFallback(
          infernal && (infernal.manifestedPieceTypeKey || infernal.manifestedPieceKey),
          pieceTypeKeyFromId(unit && unit.manifestedPieceType)
        )
      )
    )
  };
}

function pieceTypeKeyFromId(value) {
  const numericValue = toNumber(value, null);
  switch (numericValue) {
    case PIECE_TYPE_PAWN:
      return "pawn";
    case PIECE_TYPE_KNIGHT:
      return "knight";
    case PIECE_TYPE_BISHOP:
      return "bishop";
    case PIECE_TYPE_ROOK:
      return "rook";
    case PIECE_TYPE_QUEEN:
      return "queen";
    case PIECE_TYPE_KING:
      return "king";
    default:
      return "infernal";
  }
}

function infernalMarkerColor(targetKingdomKey) {
  if (targetKingdomKey === "white") {
    return LEGACY_COLORS.infernalTargetWhite;
  }
  if (targetKingdomKey === "black") {
    return LEGACY_COLORS.infernalTargetBlack;
  }
  return REPORT_COLORS.brick;
}

function infernalSpanFillColor(targetKingdomKey) {
  if (targetKingdomKey === "white") {
    return LEGACY_COLORS.infernalSpanWhite;
  }
  if (targetKingdomKey === "black") {
    return LEGACY_COLORS.infernalSpanBlack;
  }
  return LEGACY_COLORS.infernalSpan;
}

function buildInfernalSpawnLabel(descriptor, unitId) {
  const pieceLabel = descriptor.manifestedPieceKey
    && descriptor.manifestedPieceKey !== "infernal"
    ? pieceBadgeLabel(descriptor.manifestedPieceKey)
    : "INF";
  const kingdomLabel = descriptor.targetKingdomKey !== "unknown"
    ? ` > ${kingdomBadgeShort(descriptor.targetKingdomKey)}`
    : "";
  return `${pieceLabel}${kingdomLabel} #${unitId}`;
}

function normalizeBuildsPerTurn(timeline) {
  const points = timeline.map((record) => {
    let whiteBuildsPerTurn = 0;
    let blackBuildsPerTurn = 0;
    let totalBuildsPerTurn = 0;

    record.structuredEvents.forEach((event) => {
      if (!event || event.typeKey !== "building_placed") {
        return;
      }

      totalBuildsPerTurn += 1;
      if (record.committedActiveKingdomKey === "white") {
        whiteBuildsPerTurn += 1;
      } else if (record.committedActiveKingdomKey === "black") {
        blackBuildsPerTurn += 1;
      }
    });

    return {
      turn: record.turn,
      whiteBuildsPerTurn,
      blackBuildsPerTurn,
      totalBuildsPerTurn
    };
  });

  return {
    points,
    averageTotalBuildsPerTurn: mean(points.map((point) => point.totalBuildsPerTurn)),
    totalBuildEvents: sum(points.map((point) => point.totalBuildsPerTurn))
  };
}

function normalizeWaterDenied(timeline, replayData) {
  const globalMaxRange = toNumber(
    replayData
      && replayData.configContext
      && replayData.configContext.combat
      && replayData.configContext.combat.globalMaxRange,
    DEFAULT_GLOBAL_MAX_RANGE
  );

  const points = timeline.map((record) => {
    const movementContext = createMovementContext(record);
    if (!movementContext || !movementContext.pieces.length) {
      return {
        xIndex: record.xIndex,
        turn: record.turn,
        committedActiveKingdomKey: record.committedActiveKingdomKey,
        whiteWaterDeniedCells: 0,
        blackWaterDeniedCells: 0,
        totalWaterDeniedCells: 0,
        analyzedPieceCount: 0
      };
    }

    const actualState = createMovementRulesState(movementContext, {
      ignoreWater: false,
      ignoreWalls: false
    });
    const noWaterState = createMovementRulesState(movementContext, {
      ignoreWater: true,
      ignoreWalls: false
    });

    let totalWaterDeniedCells = 0;
    let whiteWaterDeniedCells = 0;
    let blackWaterDeniedCells = 0;

    movementContext.pieces.forEach((piece) => {
      const actualMoves = getPseudoLegalMovesForMetrics(piece, actualState, globalMaxRange);
      const noWaterMoves = getPseudoLegalMovesForMetrics(piece, noWaterState, globalMaxRange);
      const actualMoveIndex = buildPositionIndex(actualMoves);
      const pieceWaterDeniedCells = countPositionDifference(actualMoveIndex, noWaterMoves);

      totalWaterDeniedCells += pieceWaterDeniedCells;
      if (piece.kingdomKey === "white") {
        whiteWaterDeniedCells += pieceWaterDeniedCells;
      } else if (piece.kingdomKey === "black") {
        blackWaterDeniedCells += pieceWaterDeniedCells;
      }
    });

    return {
      xIndex: record.xIndex,
      turn: record.turn,
      committedActiveKingdomKey: record.committedActiveKingdomKey,
      whiteWaterDeniedCells,
      blackWaterDeniedCells,
      totalWaterDeniedCells,
      analyzedPieceCount: movementContext.pieces.length
    };
  });

  return {
    points,
    averageWhiteWaterDeniedCells: mean(points.map((point) => point.whiteWaterDeniedCells)),
    averageBlackWaterDeniedCells: mean(points.map((point) => point.blackWaterDeniedCells)),
    averageWaterDeniedCells: mean(points.map((point) => point.totalWaterDeniedCells)),
    averageKingdomGap: mean(points.map((point) => Math.abs(point.whiteWaterDeniedCells - point.blackWaterDeniedCells))),
    peakKingdomGap: maxOf(points.map((point) => Math.abs(point.whiteWaterDeniedCells - point.blackWaterDeniedCells)), 0),
    peakWaterDeniedCells: maxOf(points.map((point) => point.totalWaterDeniedCells), 0)
  };
}

function createMovementContext(record) {
  const snapshot = record && record.snapshot;
  const grid = snapshot && Array.isArray(snapshot.grid)
    ? snapshot.grid
    : record && Array.isArray(record.sharedGrid)
      ? record.sharedGrid
      : [];
  const height = grid.length;
  const width = height && Array.isArray(grid[0]) ? grid[0].length : 0;
  if (!width || !height) {
    return null;
  }

  const pieces = collectMovementPieces(snapshot).filter((piece) => (
    piece
      && piece.x >= 0
      && piece.x < width
      && piece.y >= 0
      && piece.y < height
  ));
  const pieceIndexByPos = Object.create(null);
  const kingsByKingdom = Object.create(null);

  pieces.forEach((piece) => {
    pieceIndexByPos[positionKey(piece.x, piece.y)] = piece;
    if (piece.typeId === PIECE_TYPE_KING && !kingsByKingdom[piece.kingdomKey]) {
      kingsByKingdom[piece.kingdomKey] = piece;
    }
  });

  const autonomousIndexByPos = Object.create(null);
  const autonomousUnits = snapshot && Array.isArray(snapshot.autonomousUnits)
    ? snapshot.autonomousUnits
    : [];

  autonomousUnits.forEach((unit) => {
    const x = toNumber(unit && unit.x, null);
    const y = toNumber(unit && unit.y, null);
    if (x === null || y === null) {
      return;
    }
    autonomousIndexByPos[positionKey(x, y)] = {
      id: toNumber(unit && unit.id, 0),
      x,
      y
    };
  });

  return {
    grid,
    width,
    height,
    pieces,
    pieceIndexByPos,
    kingsByKingdom,
    autonomousIndexByPos,
    buildingCellByPos: buildMovementBuildingIndex(record)
  };
}

function collectMovementPieces(snapshot) {
  return collectMovementKingdomPieces(snapshot, "white", "whiteKingdom")
    .concat(collectMovementKingdomPieces(snapshot, "black", "blackKingdom"));
}

function collectMovementKingdomPieces(snapshot, kingdomKey, containerKey) {
  const kingdom = snapshot && snapshot[containerKey] && typeof snapshot[containerKey] === "object"
    ? snapshot[containerKey]
    : {};
  const pieces = Array.isArray(kingdom.pieces) ? kingdom.pieces : [];

  return pieces
    .map((piece) => normalizeMovementPiece(piece, kingdomKey))
    .filter(Boolean);
}

function normalizeMovementPiece(piece, kingdomKey) {
  const x = toNumber(piece && piece.x, null);
  const y = toNumber(piece && piece.y, null);
  if (x === null || y === null) {
    return null;
  }

  return {
    id: toNumber(piece && piece.id, 0),
    typeId: toNumber(piece && piece.type, PIECE_TYPE_PAWN),
    kingdomKey,
    x,
    y,
    hasWallBreachEntry: Boolean(piece && piece.hasWallBreachEntry),
    wallBreachEntryDx: toNumber(piece && piece.wallBreachEntryDx, 0),
    wallBreachEntryDy: toNumber(piece && piece.wallBreachEntryDy, 0),
    wallBreachCellX: toNumber(piece && piece.wallBreachCellX, -1),
    wallBreachCellY: toNumber(piece && piece.wallBreachCellY, -1)
  };
}

function buildMovementBuildingIndex(record) {
  const analyticsIndex = buildMovementBuildingIndexFromAnalytics(record);
  if (Object.keys(analyticsIndex).length) {
    return analyticsIndex;
  }

  return buildMovementBuildingIndexFromSnapshot(record && record.snapshot);
}

function buildMovementBuildingIndexFromAnalytics(record) {
  const entities = record && record.analytics && record.analytics.entities && typeof record.analytics.entities === "object"
    ? record.analytics.entities
    : {};
  const buildingIndex = Array.isArray(entities.buildingIndex) ? entities.buildingIndex : [];

  return buildingIndex.reduce((index, building) => {
    const buildingTypeId = toNumber(building && building.buildingTypeId, -1);
    const ownerKingdomKey = keyOrFallback(
      building && building.ownerKingdomKey,
      kingdomKeyFromId(building && building.ownerKingdomId)
    );
    const cells = Array.isArray(building && building.cells) ? building.cells : [];

    cells.forEach((cell) => {
      const worldCell = cell && cell.worldCell && typeof cell.worldCell === "object"
        ? cell.worldCell
        : null;
      const x = toNumber(worldCell && worldCell.x, null);
      const y = toNumber(worldCell && worldCell.y, null);
      if (x === null || y === null) {
        return;
      }

      index[positionKey(x, y)] = {
        id: toNumber(building && building.id, 0),
        buildingTypeId,
        buildingTypeKey: keyOrFallback(building && building.buildingTypeKey, "unknown"),
        isPublic: Boolean(building && building.isPublic),
        isNeutral: Boolean(building && building.isNeutral),
        ownerKingdomKey,
        destroyed: Boolean(cell && cell.destroyed),
        breached: Boolean(cell && cell.breached),
        hp: toNumber(cell && cell.hp, 0)
      };
    });

    return index;
  }, Object.create(null));
}

function buildMovementBuildingIndexFromSnapshot(snapshot) {
  return collectMovementBuildings(snapshot).reduce((index, building) => {
    expandMovementBuildingCells(building).forEach((cell) => {
      index[positionKey(cell.x, cell.y)] = cell;
    });

    return index;
  }, Object.create(null));
}

function collectMovementBuildings(snapshot) {
  return collectMovementKingdomBuildings(snapshot, "white", "whiteKingdom")
    .concat(collectMovementKingdomBuildings(snapshot, "black", "blackKingdom"))
    .concat(collectMovementPublicBuildings(snapshot));
}

function collectMovementKingdomBuildings(snapshot, defaultKingdomKey, containerKey) {
  const kingdom = snapshot && snapshot[containerKey] && typeof snapshot[containerKey] === "object"
    ? snapshot[containerKey]
    : {};
  const buildings = Array.isArray(kingdom.buildings) ? kingdom.buildings : [];

  return buildings
    .map((building) => normalizeMovementBuilding(building, defaultKingdomKey, false))
    .filter(Boolean);
}

function collectMovementPublicBuildings(snapshot) {
  const buildings = snapshot && Array.isArray(snapshot.publicBuildings)
    ? snapshot.publicBuildings
    : [];

  return buildings
    .map((building) => normalizeMovementBuilding(building, "white", true))
    .filter(Boolean);
}

function normalizeMovementBuilding(building, defaultKingdomKey, forcePublic) {
  const originX = toNumber(building && building.ox, null);
  const originY = toNumber(building && building.oy, null);
  const baseWidth = toNumber(building && building.w, null);
  const baseHeight = toNumber(building && building.h, null);
  if (originX === null || originY === null || baseWidth === null || baseHeight === null) {
    return null;
  }

  return {
    id: toNumber(building && building.id, 0),
    buildingTypeId: toNumber(building && building.type, -1),
    buildingTypeKey: "unknown",
    ownerKingdomKey: keyOrFallback(kingdomKeyFromId(building && building.owner), defaultKingdomKey),
    isPublic: Boolean(forcePublic || (building && building.isNeutral)),
    isNeutral: Boolean(building && building.isNeutral),
    originX,
    originY,
    baseWidth,
    baseHeight,
    rotationQuarterTurns: toNumber(building && building.rot, 0),
    flipMask: toNumber(building && building.fm, 0),
    hpByCell: Array.isArray(building && building.hp) ? building.hp : [],
    breachByCell: Array.isArray(building && building.breach) ? building.breach : []
  };
}

function expandMovementBuildingCells(building) {
  const normalizedRotation = normalizeRotationQuarterTurnsForMetrics(building.rotationQuarterTurns);
  const footprintWidth = getBuildingFootprintWidthForMetrics(building.baseWidth, building.baseHeight, normalizedRotation);
  const footprintHeight = getBuildingFootprintHeightForMetrics(building.baseWidth, building.baseHeight, normalizedRotation);
  const cells = [];

  for (let localY = 0; localY < footprintHeight; localY += 1) {
    for (let localX = 0; localX < footprintWidth; localX += 1) {
      const sourceLocal = mapFootprintToSourceLocalForMetrics(
        localX,
        localY,
        building.baseWidth,
        building.baseHeight,
        normalizedRotation,
        building.flipMask
      );
      const sourceIndex = (sourceLocal.y * building.baseWidth) + sourceLocal.x;
      const hp = toNumber(building.hpByCell[sourceIndex], 0);

      cells.push({
        id: building.id,
        buildingTypeId: building.buildingTypeId,
        buildingTypeKey: building.buildingTypeKey,
        isPublic: building.isPublic,
        isNeutral: building.isNeutral,
        ownerKingdomKey: building.ownerKingdomKey,
        destroyed: hp <= 0,
        breached: Boolean(toNumber(building.breachByCell[sourceIndex], 0)),
        hp,
        x: building.originX + localX,
        y: building.originY + localY
      });
    }
  }

  return cells;
}

function getBuildingFootprintWidthForMetrics(baseWidth, baseHeight, rotationQuarterTurns) {
  return rotationQuarterTurns % 2 === 0 ? baseWidth : baseHeight;
}

function getBuildingFootprintHeightForMetrics(baseWidth, baseHeight, rotationQuarterTurns) {
  return rotationQuarterTurns % 2 === 0 ? baseHeight : baseWidth;
}

function normalizeRotationQuarterTurnsForMetrics(rotationQuarterTurns) {
  if (!Number.isFinite(rotationQuarterTurns) || rotationQuarterTurns < 0) {
    return 0;
  }

  return Math.trunc(rotationQuarterTurns) % 4;
}

function normalizeFlipMaskForMetrics(flipMask) {
  if (!Number.isFinite(flipMask) || flipMask < 0) {
    return 0;
  }

  return Math.trunc(flipMask) & 3;
}

function mapFootprintToSourceLocalForMetrics(localX, localY, baseWidth, baseHeight, rotationQuarterTurns, flipMask) {
  const normalizedRotation = normalizeRotationQuarterTurnsForMetrics(rotationQuarterTurns);
  const footprintWidth = getBuildingFootprintWidthForMetrics(baseWidth, baseHeight, normalizedRotation);
  const footprintHeight = getBuildingFootprintHeightForMetrics(baseWidth, baseHeight, normalizedRotation);
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

  const normalizedFlipMask = normalizeFlipMaskForMetrics(flipMask);
  if ((normalizedFlipMask & 1) !== 0) {
    sourceX = baseWidth - 1 - sourceX;
  }
  if ((normalizedFlipMask & 2) !== 0) {
    sourceY = baseHeight - 1 - sourceY;
  }

  return { x: sourceX, y: sourceY };
}

function createMovementRulesState(context, options) {
  return {
    grid: context.grid,
    width: context.width,
    height: context.height,
    pieces: context.pieces,
    pieceIndexByPos: context.pieceIndexByPos,
    autonomousIndexByPos: context.autonomousIndexByPos,
    buildingCellByPos: context.buildingCellByPos,
    kingsByKingdom: context.kingsByKingdom,
    ignoreWater: Boolean(options && options.ignoreWater),
    ignoreWalls: Boolean(options && options.ignoreWalls)
  };
}

function getPseudoLegalMovesForMetrics(piece, state, globalMaxRange) {
  let moves = [];

  if (isRestrictedInsideEnemyStoneWallForMetrics(piece, state)) {
    moves = buildWallBreachHalfPlaneMovesForMetrics(piece, state, globalMaxRange);
  } else {
    switch (piece.typeId) {
      case PIECE_TYPE_PAWN:
        moves = getPawnMovesForMetrics(piece, state);
        break;
      case PIECE_TYPE_KNIGHT:
        moves = getKnightMovesForMetrics(piece, state);
        break;
      case PIECE_TYPE_BISHOP:
        [[-1, -1], [-1, 1], [1, -1], [1, 1]].forEach((direction) => {
          moves = moves.concat(getDirectionalMovesForMetrics(piece, state, direction[0], direction[1], globalMaxRange));
        });
        break;
      case PIECE_TYPE_ROOK:
        [[0, -1], [0, 1], [-1, 0], [1, 0]].forEach((direction) => {
          moves = moves.concat(getDirectionalMovesForMetrics(piece, state, direction[0], direction[1], globalMaxRange));
        });
        break;
      case PIECE_TYPE_QUEEN:
        for (let dy = -1; dy <= 1; dy += 1) {
          for (let dx = -1; dx <= 1; dx += 1) {
            if (dx === 0 && dy === 0) {
              continue;
            }
            moves = moves.concat(getDirectionalMovesForMetrics(piece, state, dx, dy, globalMaxRange));
          }
        }
        break;
      case PIECE_TYPE_KING:
        moves = getKingMovesForMetrics(piece, state);
        break;
      default:
        break;
    }
  }

  const enemyKing = getEnemyKingForMetrics(state, piece.kingdomKey);
  if (!enemyKing) {
    return moves;
  }

  const enemyKingCellKey = positionKey(enemyKing.x, enemyKing.y);
  return moves.filter((move) => positionKey(move.x, move.y) !== enemyKingCellKey);
}

function buildWallBreachHalfPlaneMovesForMetrics(piece, state, globalMaxRange) {
  let candidateMoves = [];

  switch (piece.typeId) {
    case PIECE_TYPE_PAWN:
      candidateMoves = getPawnMovesForMetrics(piece, state);
      break;
    case PIECE_TYPE_KNIGHT:
      candidateMoves = getKnightMovesForMetrics(piece, state);
      break;
    case PIECE_TYPE_BISHOP:
      [[-1, -1], [-1, 1], [1, -1], [1, 1]].forEach((direction) => {
        candidateMoves = candidateMoves.concat(
          getDirectionalMovesForMetrics(piece, state, direction[0], direction[1], globalMaxRange)
        );
      });
      break;
    case PIECE_TYPE_ROOK:
      [[0, -1], [0, 1], [-1, 0], [1, 0]].forEach((direction) => {
        candidateMoves = candidateMoves.concat(
          getDirectionalMovesForMetrics(piece, state, direction[0], direction[1], globalMaxRange)
        );
      });
      break;
    case PIECE_TYPE_QUEEN:
      for (let dy = -1; dy <= 1; dy += 1) {
        for (let dx = -1; dx <= 1; dx += 1) {
          if (dx === 0 && dy === 0) {
            continue;
          }
          candidateMoves = candidateMoves.concat(getDirectionalMovesForMetrics(piece, state, dx, dy, globalMaxRange));
        }
      }
      break;
    case PIECE_TYPE_KING:
      candidateMoves = getKingMovesForMetrics(piece, state);
      break;
    default:
      break;
  }

  return filterWallBreachSourceSideDestinationsForMetrics(piece, state, candidateMoves);
}

function getPawnMovesForMetrics(piece, state) {
  const moves = [];
  const orthogonalDirs = [[0, -1], [0, 1], [-1, 0], [1, 0]];
  const diagonalDirs = [[-1, -1], [-1, 1], [1, -1], [1, 1]];

  orthogonalDirs.forEach((direction) => {
    const destination = {
      x: piece.x + direction[0],
      y: piece.y + direction[1]
    };
    if (!isTraversableForMetrics(state, destination.x, destination.y)) {
      return;
    }

    const destinationBuilding = getEffectiveBuildingCellForMetrics(state, destination.x, destination.y);
    if (isAlliedBlockingWallCellForMetrics(destinationBuilding, piece.kingdomKey)) {
      if (!pieceAtForMetrics(state, destination.x, destination.y)
        && !autonomousUnitAtForMetrics(state, destination.x, destination.y)) {
        moves.push(destination);
      }
      return;
    }

    if (pieceAtForMetrics(state, destination.x, destination.y)
      || autonomousUnitAtForMetrics(state, destination.x, destination.y)) {
      return;
    }
    if (isEnemyCapturableBuildingCellForMetrics(destinationBuilding, piece.kingdomKey)) {
      return;
    }
    if (isBlockingWallCellForMetrics(destinationBuilding)) {
      return;
    }

    moves.push(destination);
  });

  diagonalDirs.forEach((direction) => {
    const destination = {
      x: piece.x + direction[0],
      y: piece.y + direction[1]
    };
    if (!isTraversableForMetrics(state, destination.x, destination.y)) {
      return;
    }

    const occupant = pieceAtForMetrics(state, destination.x, destination.y);
    if (occupant && occupant.kingdomKey !== piece.kingdomKey) {
      moves.push(destination);
      return;
    }
    if (autonomousUnitAtForMetrics(state, destination.x, destination.y)) {
      moves.push(destination);
      return;
    }

    const destinationBuilding = getEffectiveBuildingCellForMetrics(state, destination.x, destination.y);
    if (isEnemyCapturableBuildingCellForMetrics(destinationBuilding, piece.kingdomKey)) {
      moves.push(destination);
    }
  });

  return moves;
}

function getKnightMovesForMetrics(piece, state) {
  const moves = [];
  const offsets = [
    [-2, -1], [-2, 1], [-1, -2], [-1, 2],
    [1, -2], [1, 2], [2, -1], [2, 1]
  ];

  offsets.forEach((offset) => {
    const destination = {
      x: piece.x + offset[0],
      y: piece.y + offset[1]
    };
    if (canLandOnForMetrics(state, destination, piece.kingdomKey)) {
      moves.push(destination);
    }
  });

  return moves;
}

function getDirectionalMovesForMetrics(piece, state, dx, dy, maxRange) {
  const moves = [];

  for (let index = 1; index <= maxRange; index += 1) {
    const destination = {
      x: piece.x + (dx * index),
      y: piece.y + (dy * index)
    };
    if (!isTraversableForMetrics(state, destination.x, destination.y)) {
      break;
    }

    const building = getEffectiveBuildingCellForMetrics(state, destination.x, destination.y);
    if (isAlliedBlockingWallCellForMetrics(building, piece.kingdomKey)) {
      if (canLandOnForMetrics(state, destination, piece.kingdomKey)) {
        moves.push(destination);
      }
      break;
    }
    if (isBlockingWallCellForMetrics(building)) {
      if (!building.isNeutral && building.ownerKingdomKey !== piece.kingdomKey) {
        moves.push(destination);
      }
      break;
    }

    const occupant = pieceAtForMetrics(state, destination.x, destination.y);
    if (occupant && occupant.kingdomKey === piece.kingdomKey) {
      break;
    }

    moves.push(destination);
    if (autonomousUnitAtForMetrics(state, destination.x, destination.y)) {
      break;
    }
    if (occupant && occupant.kingdomKey !== piece.kingdomKey) {
      break;
    }
  }

  return moves;
}

function getKingMovesForMetrics(piece, state) {
  const moves = [];
  const enemyKing = getEnemyKingForMetrics(state, piece.kingdomKey);

  for (let dy = -1; dy <= 1; dy += 1) {
    for (let dx = -1; dx <= 1; dx += 1) {
      if (dx === 0 && dy === 0) {
        continue;
      }

      const destination = {
        x: piece.x + dx,
        y: piece.y + dy
      };
      const building = getEffectiveBuildingCellForMetrics(state, destination.x, destination.y);

      if (isAlliedBlockingWallCellForMetrics(building, piece.kingdomKey)) {
        if (canLandOnForMetrics(state, destination, piece.kingdomKey)
          && !isKingAdjacentToCellForMetrics(enemyKing, destination)) {
          moves.push(destination);
        }
        continue;
      }

      if (!canLandOnForMetrics(state, destination, piece.kingdomKey)) {
        continue;
      }
      if (isKingAdjacentToCellForMetrics(enemyKing, destination)) {
        continue;
      }

      moves.push(destination);
    }
  }

  return moves;
}

function canLandOnForMetrics(state, position, moverKingdomKey) {
  if (!isTraversableForMetrics(state, position.x, position.y)) {
    return false;
  }

  const building = getEffectiveBuildingCellForMetrics(state, position.x, position.y);
  if (isBlockingWallCellForMetrics(building)) {
    if (building.isNeutral || building.ownerKingdomKey !== moverKingdomKey) {
      return !building.isNeutral && building.ownerKingdomKey !== moverKingdomKey;
    }
  }

  const occupant = pieceAtForMetrics(state, position.x, position.y);
  if (occupant && occupant.kingdomKey === moverKingdomKey) {
    return false;
  }

  return true;
}

function filterWallBreachSourceSideDestinationsForMetrics(piece, state, candidateMoves) {
  const entryDelta = resolveWallBreachEntryDeltaForMetrics(piece, state);
  if (!entryDelta) {
    return [];
  }

  return candidateMoves.filter((destination) => (
    isWallBreachSourceSideDestinationForMetrics(piece, state, entryDelta, destination)
  ));
}

function resolveWallBreachEntryDeltaForMetrics(piece, state) {
  if (!isRestrictedInsideEnemyStoneWallForMetrics(piece, state)
    || !piece.hasWallBreachEntry
    || piece.wallBreachCellX !== piece.x
    || piece.wallBreachCellY !== piece.y) {
    return null;
  }

  if (piece.wallBreachEntryDx === 0 && piece.wallBreachEntryDy === 0) {
    return null;
  }

  return {
    x: piece.wallBreachEntryDx,
    y: piece.wallBreachEntryDy
  };
}

function isRestrictedInsideEnemyStoneWallForMetrics(piece, state) {
  if (!isInBoundsForMetrics(state, piece.x, piece.y)) {
    return false;
  }

  const building = getEffectiveBuildingCellForMetrics(state, piece.x, piece.y);
  return isEnemyStoneWallCellForMetrics(building, piece.kingdomKey);
}

function isWallBreachSourceSideDestinationForMetrics(piece, state, entryDelta, destination) {
  const relativeX = destination.x - piece.x;
  const relativeY = destination.y - piece.y;
  const spanAxis = detectWallBreachSpanAxisForMetrics(piece, state);

  if (spanAxis === "horizontal" && entryDelta.y !== 0) {
    return respectsEntryComponentForMetrics(relativeY, entryDelta.y);
  }
  if (spanAxis === "vertical" && entryDelta.x !== 0) {
    return respectsEntryComponentForMetrics(relativeX, entryDelta.x);
  }
  if (spanAxis === "intersection") {
    return respectsEntryComponentForMetrics(relativeX, entryDelta.x)
      && respectsEntryComponentForMetrics(relativeY, entryDelta.y);
  }

  return ((relativeX * entryDelta.x) + (relativeY * entryDelta.y)) <= 0;
}

function detectWallBreachSpanAxisForMetrics(piece, state) {
  let horizontalNeighborCount = 0;
  let verticalNeighborCount = 0;

  [-1, 1].forEach((dx) => {
    const building = getEffectiveBuildingCellForMetrics(state, piece.x + dx, piece.y);
    if (isEnemyStoneWallCellForMetrics(building, piece.kingdomKey)) {
      horizontalNeighborCount += 1;
    }
  });

  [-1, 1].forEach((dy) => {
    const building = getEffectiveBuildingCellForMetrics(state, piece.x, piece.y + dy);
    if (isEnemyStoneWallCellForMetrics(building, piece.kingdomKey)) {
      verticalNeighborCount += 1;
    }
  });

  if (horizontalNeighborCount > 0 && verticalNeighborCount > 0) {
    if (horizontalNeighborCount === verticalNeighborCount) {
      return "intersection";
    }
    return horizontalNeighborCount > verticalNeighborCount ? "horizontal" : "vertical";
  }
  if (horizontalNeighborCount > 0) {
    return "horizontal";
  }
  if (verticalNeighborCount > 0) {
    return "vertical";
  }
  return "fallback";
}

function respectsEntryComponentForMetrics(relative, entryComponent) {
  return entryComponent === 0 || (relative * entryComponent) <= 0;
}

function isTraversableForMetrics(state, x, y) {
  if (!isInBoundsForMetrics(state, x, y)) {
    return false;
  }

  const row = state.grid[y];
  const cell = row && row[x] && typeof row[x] === "object" ? row[x] : null;
  if (!cell || toNumber(cell.c, 0) === 0) {
    return false;
  }

  const cellType = toNumber(cell.t, CELL_TYPE_VOID);
  if (cellType === CELL_TYPE_VOID) {
    return false;
  }
  if (!state.ignoreWater && cellType === CELL_TYPE_WATER) {
    return false;
  }

  return true;
}

function isInBoundsForMetrics(state, x, y) {
  return x >= 0 && x < state.width && y >= 0 && y < state.height;
}

function pieceAtForMetrics(state, x, y) {
  return state.pieceIndexByPos[positionKey(x, y)] || null;
}

function autonomousUnitAtForMetrics(state, x, y) {
  return state.autonomousIndexByPos[positionKey(x, y)] || null;
}

function getEffectiveBuildingCellForMetrics(state, x, y) {
  if (!isInBoundsForMetrics(state, x, y)) {
    return null;
  }

  const building = state.buildingCellByPos[positionKey(x, y)] || null;
  if (!building) {
    return null;
  }
  if (state.ignoreWalls && isWallTypeId(building.buildingTypeId)) {
    return null;
  }

  return building;
}

function isEnemyCapturableBuildingCellForMetrics(building, moverKingdomKey) {
  return Boolean(building)
    && !building.isNeutral
    && building.ownerKingdomKey !== moverKingdomKey;
}

function isBlockingWallCellForMetrics(building) {
  return Boolean(building)
    && isWallTypeId(building.buildingTypeId)
    && !building.destroyed;
}

function isEnemyStoneWallCellForMetrics(building, moverKingdomKey) {
  return Boolean(building)
    && building.buildingTypeId === BUILDING_TYPE_STONE_WALL
    && !building.isNeutral
    && building.ownerKingdomKey !== moverKingdomKey
    && !building.destroyed;
}

function isAlliedBlockingWallCellForMetrics(building, moverKingdomKey) {
  return isBlockingWallCellForMetrics(building)
    && !building.isNeutral
    && building.ownerKingdomKey === moverKingdomKey;
}

function isWallTypeId(buildingTypeId) {
  return buildingTypeId === BUILDING_TYPE_WOOD_WALL || buildingTypeId === BUILDING_TYPE_STONE_WALL;
}

function getEnemyKingForMetrics(state, kingdomKey) {
  return state.kingsByKingdom[otherKingdomKey(kingdomKey)] || null;
}

function otherKingdomKey(kingdomKey) {
  return kingdomKey === "white" ? "black" : kingdomKey === "black" ? "white" : "unknown";
}

function isKingAdjacentToCellForMetrics(enemyKing, cell) {
  return Boolean(enemyKing)
    && Math.abs(cell.x - enemyKing.x) <= 1
    && Math.abs(cell.y - enemyKing.y) <= 1;
}

function buildPositionIndex(positions) {
  return positions.reduce((index, position) => {
    index[positionKey(position.x, position.y)] = true;
    return index;
  }, Object.create(null));
}

function countPositionDifference(referenceIndex, candidatePositions) {
  const candidateIndex = buildPositionIndex(candidatePositions);
  return Object.keys(candidateIndex).reduce((count, key) => count + (referenceIndex[key] ? 0 : 1), 0);
}

function positionKey(x, y) {
  return `${x},${y}`;
}

function kingdomKeyFromId(value) {
  const numericValue = toNumber(value, null);
  if (numericValue === 0) {
    return "white";
  }
  if (numericValue === 1) {
    return "black";
  }
  return "unknown";
}

function buildTimelineOption({ series, xAxisName, yAxes, markers = [], spans = [] }) {
  const option = baseGridOption();
  const xRows = series.length ? series[0].rows : [];

  option.tooltip = {
    ...option.tooltip,
    trigger: "axis",
    formatter: (params) => formatTimelineTooltip(params, xRows),
    axisPointer: {
      type: "cross"
    }
  };
  option.grid = {
    ...option.grid,
    right: yAxes.length > 1 ? 72 : 18,
    bottom: 68
  };
  option.xAxis = {
    ...option.xAxis,
    type: "category",
    boundaryGap: false,
    data: xRows.map((row) => formatTurnAxisLabel(row)),
    name: xAxisName,
    nameLocation: "middle",
    nameGap: 42,
    nameTextStyle: axisNameTextStyle(),
    axisLabel: {
      ...option.xAxis.axisLabel
    }
  };
  option.yAxis = yAxes.map((axis, index) => ({
    ...baseValueAxis(axis.name),
    position: axis.position || (index === 0 ? "left" : "right"),
    splitLine: index === 0
      ? { lineStyle: { color: "#ececec" } }
      : { show: false }
  }));
  option.series = series.map((seriesSpec, index) => buildTimelineSeries(
    seriesSpec,
    index === 0 ? markers : [],
    index === 0 ? spans : []
  ));

  return option;
}

function buildInfernalTimelineOption(configuration) {
  const infernalSpans = Array.isArray(configuration.spans) ? configuration.spans : [];
  const option = buildTimelineOption({
    ...configuration,
    spans: []
  });
  option.grid = {
    ...option.grid,
    bottom: 72
  };
  if (infernalSpans.length) {
    const whiteTargetSpans = infernalSpans.filter((span) => span.targetKingdomKey === "white");
    const blackTargetSpans = infernalSpans.filter((span) => span.targetKingdomKey === "black");
    const otherSpans = infernalSpans.filter((span) => span.targetKingdomKey !== "white" && span.targetKingdomKey !== "black");

    if (whiteTargetSpans.length) {
      option.series.push(buildInfernalSpanOverlaySeries({
        name: "Pièces du diable ciblant le royaume blanc",
        legendColor: LEGACY_COLORS.infernalSpanWhite,
        spans: whiteTargetSpans
      }));
    }

    if (blackTargetSpans.length) {
      option.series.push(buildInfernalSpanOverlaySeries({
        name: "Pièces du diable ciblant le royaume noir",
        legendColor: LEGACY_COLORS.infernalSpanBlack,
        spans: blackTargetSpans
      }));
    }

    if (otherSpans.length) {
      option.series.push(buildInfernalSpanOverlaySeries({
        name: "__infernal_span_overlay_unknown",
        legendColor: LEGACY_COLORS.infernalSpan,
        spans: otherSpans
      }));
    }
  }

  option.legend = {
    ...option.legend,
    data: option.series
      .map((series) => series && series.name)
      .filter((name) => typeof name === "string" && !name.startsWith("__"))
  };

  return option;
}

function buildTimelineSeriesSpec(name, rows, key, color, yAxisIndex) {
  return {
    name,
    rows,
    key,
    color,
    yAxisIndex
  };
}

function buildTimelineSeries(spec, markers, spans) {
  const series = {
    name: spec.name,
    type: "line",
    smooth: 0.25,
    showSymbol: true,
    symbol: "circle",
    symbolSize: 5,
    connectNulls: true,
    yAxisIndex: spec.yAxisIndex || 0,
    lineStyle: {
      width: 2,
      color: spec.color
    },
    itemStyle: {
      color: spec.color
    },
    emphasis: {
      focus: "none"
    },
    data: spec.rows.map((row) => (row[spec.key] === null || row[spec.key] === undefined ? null : toNumber(row[spec.key], 0)))
  };

  if (markers.length) {
    series.markLine = {
      symbol: ["none", "none"],
      silent: true,
      animation: false,
      data: markers.map((marker) => ({
        xAxis: marker.xIndex,
        lineStyle: {
          color: marker.color || REPORT_COLORS.sand,
          type: marker.dashed ? "dashed" : "solid",
          width: 1.5
        },
        label: marker.label
          ? {
              show: true,
              formatter: marker.label,
              position: marker.labelPosition || "insideEndTop",
              offset: Array.isArray(marker.labelOffset) ? marker.labelOffset : [0, 0],
              color: marker.color || REPORT_COLORS.ink,
              fontFamily: 'Georgia, "Times New Roman", serif',
              fontSize: 11,
              fontWeight: 700
            }
          : { show: false }
      }))
    };
  }

  if (spans.length) {
    const hasSpanLabels = spans.some((span) => Boolean(span.labelText));
    series.markArea = {
      silent: true,
      animation: false,
      label: hasSpanLabels
        ? infernalSpanLabelStyle()
        : { show: false },
      data: spans.map((span) => ([
        {
          name: span.labelText || "",
          xAxis: span.xStartIndex,
          itemStyle: {
            color: span.fillColor || LEGACY_COLORS.infernalSpan
          },
          label: span.labelText
            ? infernalSpanLabelStyle()
            : { show: false }
        },
        {
          xAxis: span.xEndIndex
        }
      ]))
    };
  }

  return series;
}

function buildInfernalSpanOverlaySeries({ name, legendColor, spans }) {
  return {
    name,
    type: "custom",
    coordinateSystem: "cartesian2d",
    silent: true,
    animation: false,
      z: 20,
    zlevel: 0,
    yAxisIndex: 0,
    itemStyle: {
      color: legendColor
    },
    encode: {
      x: [1, 2],
      y: 0,
      tooltip: []
    },
    tooltip: {
      show: false
    },
    data: spans.map((span) => ([
      0,
      span.xStartIndex,
      span.xEndIndex,
      span.fillColor || LEGACY_COLORS.infernalSpan,
      span.pieceImageUrl || "",
      span.pieceLabelFr || "Pièce du diable",
      span.pieceBadgeText || "INF"
    ])),
    renderItem(params, api) {
      const coordSys = params.coordSys;
      const startX = api.coord([api.value(1), 0])[0];
      const endX = api.coord([api.value(2), 0])[0];
      const left = Math.min(startX, endX);
      const right = Math.max(startX, endX);
      const width = Math.max(3, right - left);
      const top = coordSys.y;
      const height = coordSys.height;
      const centerX = left + width / 2;
      const centerY = top + height / 2;
      const iconSize = Math.max(18, Math.min(width * 0.32, height * 0.26, 42));
      const textOffsetY = iconSize * 0.6 + 14;
      const textFontSize = Math.max(11, Math.min(width * 0.09, 16));
      const snappedIconSize = Math.round(iconSize);
      const snappedIconX = Math.round(centerX - snappedIconSize / 2);
      const snappedIconY = Math.round(centerY - snappedIconSize / 2 - 10);
      const snappedTextY = Math.round(centerY + textOffsetY);
      const pieceImageUrl = api.value(4);
      const pieceLabelFr = api.value(5);
      const pieceBadgeText = api.value(6);

      const children = [
        {
          type: "rect",
          z2: 0,
          shape: {
            x: left,
            y: top,
            width,
            height
          },
          style: {
            fill: api.value(3)
          }
        }
      ];

      if (pieceImageUrl) {
        children.push({
          type: "image",
          z2: 10,
          style: {
            image: pieceImageUrl,
            x: snappedIconX,
            y: snappedIconY,
            width: snappedIconSize,
            height: snappedIconSize,
            opacity: 0.94
          }
        });
      } else {
        children.push({
          type: "text",
          z2: 10,
          style: {
            x: Math.round(centerX),
            y: Math.round(centerY - 10),
            text: pieceBadgeText,
            textAlign: "center",
            textVerticalAlign: "middle",
            font: `700 ${Math.max(12, iconSize * 0.42)}px Georgia, \"Times New Roman\", serif`,
            fill: REPORT_COLORS.ink
          }
        });
      }

      children.push({
        type: "text",
        z2: 10,
        style: {
          x: Math.round(centerX),
          y: snappedTextY,
          text: pieceLabelFr,
          textAlign: "center",
          textVerticalAlign: "middle",
          font: `600 ${textFontSize}px Georgia, \"Times New Roman\", serif`,
          fill: "#111111"
        }
      });

      return {
        type: "group",
        clipPath: {
          type: "rect",
          shape: {
            x: left,
            y: top,
            width,
            height
          }
        },
        children
      };
    }
  };
}

function baseGridOption() {
  return {
    animation: false,
    textStyle: {
      fontFamily: 'Georgia, "Times New Roman", serif',
      color: REPORT_COLORS.graphite
    },
    tooltip: {
      backgroundColor: "rgba(255, 255, 255, 0.96)",
      borderColor: REPORT_COLORS.mist,
      borderWidth: 1,
      textStyle: {
        color: REPORT_COLORS.ink,
        fontFamily: 'Georgia, "Times New Roman", serif'
      }
    },
    legend: {
      top: 0,
      textStyle: {
        color: REPORT_COLORS.graphite,
        fontFamily: 'Georgia, "Times New Roman", serif'
      }
    },
    grid: {
      top: 46,
      left: 74,
      right: 18,
      bottom: 76,
      containLabel: true
    },
    xAxis: {
      axisLine: {
        lineStyle: {
          color: "#cfcfcf"
        }
      },
      axisTick: {
        show: false
      },
      axisLabel: {
        color: REPORT_COLORS.graphite
      }
    },
    yAxis: {
      splitLine: {
        lineStyle: {
          color: "#ececec"
        }
      },
      axisLine: {
        show: false
      },
      axisTick: {
        show: false
      },
      axisLabel: {
        color: REPORT_COLORS.graphite
      }
    }
  };
}

function baseValueAxis(name) {
  return {
    type: "value",
    name,
    nameLocation: "middle",
    nameGap: 58,
    nameRotate: 90,
    nameTextStyle: axisNameTextStyle(),
    axisLine: {
      show: false
    },
    axisTick: {
      show: false
    },
    axisLabel: {
      color: REPORT_COLORS.graphite
    }
  };
}

function axisNameTextStyle() {
  return {
    color: REPORT_COLORS.ink,
    fontFamily: 'Georgia, "Times New Roman", serif',
    fontSize: 13,
    fontWeight: 600
  };
}

function infernalSpanLabelStyle() {
  return {
    show: true,
    position: "bottom",
    distance: 18,
    color: "#ffffff",
    backgroundColor: "#000000",
    borderRadius: 4,
    padding: [4, 8],
    fontFamily: 'Georgia, "Times New Roman", serif',
    fontSize: 11,
    fontWeight: 700,
    formatter: (params) => params.name || ""
  };
}

function formatTurnAxisLabel(row) {
  return `${toNumber(row && row.turn, 0)}`;
}

function formatTimelineTooltip(params, xRows) {
  const rows = Array.isArray(params) ? params : [params];
  if (!rows.length) {
    return "";
  }

  const deduplicatedParams = Array.from(
    rows.reduce((map, param) => {
      if (param && typeof param.seriesName === "string" && !map.has(param.seriesName)) {
        map.set(param.seriesName, param);
      }
      return map;
    }, new Map()).values()
  ).filter((param) => !(param && typeof param.seriesName === "string" && param.seriesName.startsWith("__")));
  if (!deduplicatedParams.length) {
    return "";
  }
  const dataIndex = toNumber(deduplicatedParams[0] && deduplicatedParams[0].dataIndex, -1);
  const row = dataIndex >= 0 ? xRows[dataIndex] : null;
  const header = buildTimelineTooltipHeader(row, deduplicatedParams[0]);
  const lines = deduplicatedParams.map((param) => {
    const value = Array.isArray(param.value) ? param.value[1] : param.value;
    return `${param.marker || ""}${escapeHtml(param.seriesName)}: <strong>${escapeHtml(formatTooltipNumber(value))}</strong>`;
  });

  return [header].concat(lines).join("<br/>");
}

function buildTimelineTooltipHeader(row, param) {
  if (!row) {
    return escapeHtml(`${param.axisValueLabel || "Tour"}`);
  }

  const activeKingdomLabel = row.committedActiveKingdomKey === "white"
    ? "blanc"
    : row.committedActiveKingdomKey === "black"
      ? "noir"
      : null;
  const sequenceLabel = Number.isFinite(row.xIndex) ? `, séquence ${row.xIndex + 1}` : "";

  return activeKingdomLabel
    ? `Tour ${escapeHtml(row.turn)} (${activeKingdomLabel}${sequenceLabel})`
    : `Tour ${escapeHtml(row.turn)}${sequenceLabel}`;
}

function formatTooltipNumber(value) {
  if (!Number.isFinite(Number(value))) {
    return "n/d";
  }

  return new Intl.NumberFormat("fr-FR", {
    minimumFractionDigits: 0,
    maximumFractionDigits: Math.abs(Number(value)) >= 100 ? 0 : 2
  }).format(Number(value));
}

function escapeHtml(value) {
  return String(value)
    .replace(/&/g, "&amp;")
    .replace(/</g, "&lt;")
    .replace(/>/g, "&gt;")
    .replace(/\"/g, "&quot;");
}

function findObserverEntry(visibility, observerKey) {
  const rows = visibility && Array.isArray(visibility.byObserver) ? visibility.byObserver : [];
  return rows.find((row) => row && row.observerKingdomKey === observerKey) || null;
}

function buildIndexById(rows) {
  return (Array.isArray(rows) ? rows : []).reduce((index, row) => {
    const id = toNumber(row && row.id, 0);
    if (id) {
      index[id] = row;
    }
    return index;
  }, Object.create(null));
}

function lengthOf(value) {
  return Array.isArray(value) ? value.length : 0;
}

function shortDirectionLabel(front) {
  const label = keyOrFallback(front && front.directionKey, "brouillard");
  return label.slice(0, 3).toUpperCase();
}

function pieceBadgeLabel(pieceKey) {
  const normalized = keyOrFallback(pieceKey, "?").toUpperCase();
  return normalized.length <= 3 ? normalized : normalized.slice(0, 3);
}

function infernalPieceLabelFr(pieceKey) {
  switch (keyOrFallback(pieceKey, "infernal")) {
    case "pawn":
      return "Pion";
    case "knight":
      return "Cavalier";
    case "bishop":
      return "Fou";
    case "rook":
      return "Tour";
    case "queen":
      return "Reine";
    default:
      return "Pièce du diable";
  }
}

function infernalPieceTextureUrl(pieceKey) {
  const normalized = keyOrFallback(pieceKey, "");
  if (!["pawn", "knight", "bishop", "rook", "queen"].includes(normalized)) {
    return "";
  }

  return new URL(`${REPLAY_CONFIG.assetRoot}/textures/pieces/evil/${normalized}.png`, window.location.href).toString();
}

function kingdomBadgeShort(kingdomKey) {
  return kingdomKey === "white" ? "W" : kingdomKey === "black" ? "B" : "?";
}

function formatInteger(value) {
  if (!Number.isFinite(value)) {
    return "n/d";
  }
  return new Intl.NumberFormat("fr-FR", {
    maximumFractionDigits: 0
  }).format(Math.round(value));
}

function formatStatNumber(value, digits = 1) {
  if (!Number.isFinite(value)) {
    return "n/d";
  }
  return new Intl.NumberFormat("fr-FR", {
    minimumFractionDigits: digits,
    maximumFractionDigits: digits
  }).format(value);
}

function toNumber(value, fallback) {
  const parsed = Number(value);
  return Number.isFinite(parsed) ? parsed : fallback;
}

function keyOrFallback(value, fallback) {
  return typeof value === "string" && value ? value : fallback;
}

function mean(values) {
  return values.length ? sum(values) / values.length : 0;
}

function sum(values) {
  return values.reduce((accumulator, value) => accumulator + toNumber(value, 0), 0);
}

function turnDiffs(turns) {
  const sortedTurns = turns.slice().sort((left, right) => left - right);
  const diffs = [];
  for (let index = 1; index < sortedTurns.length; index += 1) {
    diffs.push(sortedTurns[index] - sortedTurns[index - 1]);
  }
  return diffs;
}

function maxOf(values, fallback) {
  return values.length ? Math.max(...values) : fallback;
}