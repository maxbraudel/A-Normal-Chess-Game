const DATA_COMPANION_URL = "../build/Data/KAZIMIRIUM%201.json";

(function () {
  "use strict";

  const TARGET_SCHEMA_VERSION = 5;
  const UTC_FORMATTER = new Intl.DateTimeFormat("fr-FR", {
    dateStyle: "medium",
    timeStyle: "short",
    timeZone: "UTC"
  });

  const state = {
    charts: Object.create(null),
    normalized: null,
    grantFilter: ""
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
  const WHITE_KINGDOM_CURVE_COLOR = "rgba(154, 132, 94, 0.95)";

  const refs = resolveRefs();

  if (typeof window.Chart !== "undefined") {
    window.Chart.register(buildEventMarkersPlugin());
  }

  bindEvents();
  renderEmpty();
  bootstrapConfiguredSource();

  function resolveRefs() {
    return {
      body: document.body,
      emptyState: mustGet("emptyState"),
      tempoChart: mustGet("tempoChart"),
      tempoCards: mustGet("tempoCards"),
      decisionChart: mustGet("decisionChart"),
      decisionHesitationChart: mustGet("decisionHesitationChart"),
      decisionCards: mustGet("decisionCards"),
      economyChart: mustGet("economyChart"),
      economyCards: mustGet("economyCards"),
      blockersWaterChart: mustGet("blockersWaterChart"),
      blockersWallChart: mustGet("blockersWallChart"),
      blockersCards: mustGet("blockersCards"),
      weatherChart: mustGet("weatherChart"),
      weatherCards: mustGet("weatherCards"),
      weatherTableMeta: mustGet("weatherTableMeta"),
      weatherEventTableBody: mustGet("weatherEventTableBody"),
      infernalChart: mustGet("infernalChart"),
      infernalCards: mustGet("infernalCards"),
      infernalTableMeta: mustGet("infernalTableMeta"),
      infernalSpawnTableBody: mustGet("infernalSpawnTableBody")
    };
  }

  function mustGet(id) {
    const element = document.getElementById(id);
    if (!element) {
      throw new Error("Missing required element #" + id);
    }
    return element;
  }

  function bindEvents() {
    return;
  }

  function onGrantFilterInput() {
    state.grantFilter = (refs.xpGrantFilterInput.value || "").trim().toLowerCase();
    if (state.normalized) {
      renderXpGrantTable(state.normalized.xp);
    }
  }

  function bootstrapConfiguredSource() {
    const source = normalizeConfiguredSource(DATA_COMPANION_URL);

    if (!source.ok) {
      showLoadError(source.message);
      return;
    }

    if (window.location.protocol === "file:") {
      showLoadError(
        "La page est ouverte en file://. Les navigateurs bloquent fetch sur les fichiers locaux dans ce mode. Lance `python statistiques-generator/serve.py`, ouvre http://127.0.0.1:8765/statistiques-generator/index.html puis garde DATA_COMPANION_URL sur une URL relative servie, par exemple ../build/Data/KAZIMIRIUM%201.json."
      );
      return;
    }

    loadConfiguredSource(source);
  }

  async function loadConfiguredSource(source) {
    setStatus("Chargement automatique...", source.fetchUrl);

    try {
      const response = await fetch(source.fetchUrl, { cache: "no-store" });
      if (!response.ok) {
        throw new Error("HTTP " + response.status + " " + response.statusText);
      }
      const text = await response.text();
      processLoadedText(text, source.displayLabel);
    } catch (error) {
      showLoadError(buildLoadFailureMessage(source, error));
    }
  }

  function normalizeConfiguredSource(rawSource) {
    const trimmedSource = String(rawSource || "").trim();
    if (!trimmedSource) {
      return {
        ok: false,
        displayLabel: "DATA_COMPANION_URL non configure.",
        message: "Aucune source configuree. Definir DATA_COMPANION_URL en haut de app.js."
      };
    }

    if (isWindowsAbsolutePath(trimmedSource)) {
      return {
        ok: false,
        displayLabel: trimmedSource,
        message:
          "DATA_COMPANION_URL pointe vers un chemin Windows brut. Dans un navigateur, il faut une URL servie, pas un chemin disque. Utiliser plutot une URL relative comme ../build/Data/KAZIMIRIUM%201.json puis ouvrir la page via http://127.0.0.1:8765/statistiques-generator/index.html."
      };
    }

    if (/^file:\/\//i.test(trimmedSource)) {
      return {
        ok: false,
        displayLabel: trimmedSource,
        message:
          "DATA_COMPANION_URL utilise file://. Les navigateurs bloquent ce chargement pour des raisons de securite. Servir le projet en HTTP et utiliser une URL relative comme ../build/Data/KAZIMIRIUM%201.json."
      };
    }

    const fetchUrl = new URL(trimmedSource, window.location.href).href;
    return {
      ok: true,
      displayLabel: trimmedSource,
      fetchUrl: fetchUrl
    };
  }

  function buildLoadFailureMessage(source, error) {
    if (/^https?:/i.test(source.fetchUrl)) {
      return "Impossible de charger DATA_COMPANION_URL: " + error.message;
    }

    return "Impossible de charger DATA_COMPANION_URL: "
      + error.message
      + ". Verifier que la page est ouverte via http://127.0.0.1:8765/statistiques-generator/index.html et que DATA_COMPANION_URL vise une URL servie, par exemple ../build/Data/KAZIMIRIUM%201.json.";
  }

  function processLoadedText(text, sourceLabel) {
    let data;
    try {
      data = JSON.parse(text);
    } catch (error) {
      showLoadError("JSON invalide: " + error.message);
      return;
    }

    const validation = validateCompanion(data);
    if (!validation.ok) {
      showLoadError(validation.message);
      return;
    }

    state.normalized = normalizeCompanion(data, sourceLabel);
    state.grantFilter = "";
    renderDashboard(state.normalized);
    setStatus("Companion charge: " + sourceLabel, "Schema V5 valide et rendu actualise.");
  }

  function validateCompanion(data) {
    if (!data || typeof data !== "object" || Array.isArray(data)) {
      return { ok: false, message: "Le fichier charge n'est pas un objet JSON racine valide." };
    }

    if (toNumber(data.schemaVersion, 0) !== TARGET_SCHEMA_VERSION) {
      return {
        ok: false,
        message: "Schema incompatible: ce viewer attend schemaVersion 5 et refuse les companions legacy."
      };
    }

    if (!data.currentMetrics || typeof data.currentMetrics !== "object") {
      return { ok: false, message: "Le fichier ne contient pas currentMetrics." };
    }

    if (!data.currentAnalytics || typeof data.currentAnalytics !== "object") {
      return { ok: false, message: "Le fichier ne contient pas currentAnalytics." };
    }

    if (!Array.isArray(data.turnHistory) && !data.currentStateSummary && !data.initialSnapshot) {
      return {
        ok: false,
        message: "Le fichier n'expose ni turnHistory exploitable, ni snapshot courant/initial de secours."
      };
    }

    return { ok: true };
  }

  function normalizeCompanion(data, sourceLabel) {
    const timeline = buildTimeline(data);
    const tempo = normalizeTempo(timeline);
    const decision = normalizeDecision(timeline);
    const economy = normalizeEconomy(timeline);
    const blockers = normalizeMovementBlockers(timeline, data);
    const weather = normalizeWeather(timeline);
    const infernal = normalizeInfernal(timeline);
    const xp = normalizeXp(timeline);
    const summary = buildSummary(data, timeline, tempo, weather, infernal, xp, sourceLabel);
    const alerts = buildAlerts(data, timeline, tempo, decision, weather, infernal, xp);

    return {
      raw: data,
      sourceLabel: sourceLabel,
      timeline: timeline,
      summary: summary,
      alerts: alerts,
      tempo: tempo,
      decision: decision,
      economy: economy,
      blockers: blockers,
      weather: weather,
      infernal: infernal,
      xp: xp,
      schema: buildSchemaSnapshot(data, timeline, xp)
    };
  }

  function buildTimeline(data) {
    const turnHistory = Array.isArray(data.turnHistory) ? data.turnHistory : [];
    if (turnHistory.length) {
      return turnHistory.map(function (record, index) {
        return normalizeTurnRecord(record, index);
      });
    }

    const fallbackTurn = toNumber(
      data.currentMetrics && data.currentMetrics.turnNumber,
      toNumber(data.currentAnalytics && data.currentAnalytics.turnNumber, 0)
    );

    return [
      {
        turn: fallbackTurn,
        committedActiveKingdomKey: keyOrFallback(data.currentMetrics && data.currentMetrics.activeKingdomKey, "unknown"),
        structuredEvents: [],
        xpAuditTrail: [],
        capturedAtUnix: null,
        analytics: data.currentAnalytics || {},
        snapshotMetrics: data.currentMetrics || {},
        sourceKind: "current"
      }
    ];
  }

  function normalizeTurnRecord(record, index) {
    const analytics = record.analytics || {};
    const snapshotMetrics = record.snapshotMetrics || {};
    const turn = toNumber(
      record.committedTurnNumber,
      toNumber(snapshotMetrics.turnNumber, toNumber(analytics.turnNumber, index + 1))
    );

    return {
      turn: turn,
      committedActiveKingdomKey: keyOrFallback(record.committedActiveKingdomKey, keyOrFallback(snapshotMetrics.activeKingdomKey, "unknown")),
      structuredEvents: Array.isArray(record.structuredEvents) ? record.structuredEvents : [],
      commandAuditTrail: Array.isArray(record.commandAuditTrail) ? record.commandAuditTrail : [],
      behavioralTelemetry: record.behavioralTelemetry && typeof record.behavioralTelemetry === "object"
        ? record.behavioralTelemetry
        : {},
      xpAuditTrail: Array.isArray(record.xpAuditTrail) ? record.xpAuditTrail : [],
      capturedAtUnix: toNumber(record.capturedAtUnix, null),
      analytics: analytics,
      snapshotMetrics: snapshotMetrics,
      snapshot: record.snapshot && typeof record.snapshot === "object" ? record.snapshot : null,
      sourceKind: "history"
    };
  }

  function buildSummary(data, timeline, tempo, weather, infernal, xp, sourceLabel) {
    const currentMetrics = data.currentMetrics || {};
    const currentState = data.currentStateSummary || {};
    const turnCount = timeline.length;
    const finalTurn = timeline.length ? timeline[timeline.length - 1].turn : 0;
    const totalStructuredEvents = timeline.reduce(function (sum, record) {
      return sum + record.structuredEvents.length;
    }, 0);

    return {
      sourceLabel: sourceLabel,
      schemaVersion: TARGET_SCHEMA_VERSION,
      saveName: keyOrFallback(data.saveName, keyOrFallback(currentState.gameName, "unknown")),
      turnCount: turnCount,
      finalTurn: finalTurn,
      activeKingdomKey: keyOrFallback(currentMetrics.activeKingdomKey, keyOrFallback(currentState.activeKingdomKey, "unknown")),
      historyContinuityComplete: Boolean(data.historyContinuityComplete),
      loadedFromExistingCompanion: Boolean(data.loadedFromExistingCompanion),
      dataCollectionEnabled: Boolean(currentState.dataCollectionEnabled),
      behavioralTelemetryEnabled: Boolean(currentState.behavioralTelemetryEnabled),
      createdAtUnix: toNumber(data.createdAtUnix, null),
      lastUpdatedAtUnix: toNumber(data.lastUpdatedAtUnix, null),
      totalStructuredEvents: totalStructuredEvents,
      recordedEventCount: toNumber(currentMetrics.recordedEventCount, totalStructuredEvents),
      totalXp: xp.totalAmount,
      averageTurnDurationMs: tempo.averageTurnDurationMs,
      infernalSpawnCount: infernal.spawnEvents.length,
      weatherSpawnCount: weather.spawnEvents.length,
      provenanceGenerator: keyOrFallback(data.provenance && data.provenance.generator, "unknown"),
      formatFamily: keyOrFallback(data.provenance && data.provenance.formatFamily, "unknown")
    };
  }

  function buildSchemaSnapshot(data, timeline, xp) {
    const topLevelKeys = Object.keys(data).sort();
    const rootRows = topLevelKeys.map(function (key) {
      const value = data[key];
      return {
        key: key,
        descriptor: describeValue(value)
      };
    });

    return {
      topologyNodes: [
        {
          label: "Root",
          value: topLevelKeys.length,
          detail: "champs top-level"
        },
        {
          label: "Turns",
          value: timeline.length,
          detail: "enregistrements temporels"
        },
        {
          label: "XP grants",
          value: xp.grants.length,
          detail: "audit brut"
        },
        {
          label: "Current",
          value: data.currentStateSummary ? 1 : 0,
          detail: data.currentStateSummary ? "snapshot present" : "no snapshot"
        }
      ],
      rootRows: rootRows
    };
  }

  function normalizeWeather(timeline) {
    const points = [];
    const spawnEvents = [];
    const endEvents = [];
    const timelineEvents = [];

    timeline.forEach(function (record) {
      const visibility = (record.analytics && record.analytics.visibility) || {};
      const weather = (record.analytics && record.analytics.weather) || {};
      const whiteObserver = findObserverEntry(visibility, "white");
      const blackObserver = findObserverEntry(visibility, "black");

      points.push({
        turn: record.turn,
        whiteHiddenPieces: lengthOf(whiteObserver && whiteObserver.hiddenEnemyPieceIds),
        blackHiddenPieces: lengthOf(blackObserver && blackObserver.hiddenEnemyPieceIds),
        totalHiddenPieces: lengthOf(whiteObserver && whiteObserver.hiddenEnemyPieceIds)
          + lengthOf(blackObserver && blackObserver.hiddenEnemyPieceIds),
        fogCellCount: toNumber(visibility.fogCellCount, 0),
        concealingFogCellCount: toNumber(visibility.concealingFogCellCount, 0),
        frontCount: toNumber(weather.frontCount, 0),
        hasActiveFront: Boolean(visibility.hasActiveFront)
      });

      record.structuredEvents.forEach(function (event) {
        const front = event.front || {};
        const row = {
          turn: record.turn,
          typeKey: keyOrFallback(event.typeKey, "unknown"),
          typeLabel: keyOrFallback(event.typeLabel, keyOrFallback(event.typeKey, "unknown")),
          frontLabel: keyOrFallback(front.directionLabel, keyOrFallback(front.directionKey, "n/a")),
          coverageLabel: buildFrontCoverageLabel(front)
        };

        if (event.typeKey === "weather_front_spawned") {
          spawnEvents.push({
            turn: record.turn,
            label: shortDirectionLabel(front),
            color: "rgba(240, 204, 136, 0.8)",
            lineDash: [2, 0]
          });
          timelineEvents.push(row);
          return;
        }

        if (event.typeKey === "weather_front_ended") {
          endEvents.push({
            turn: record.turn,
            label: "END",
            color: "rgba(91, 178, 166, 0.7)",
            lineDash: [5, 4]
          });
          timelineEvents.push(row);
        }
      });
    });

    const fogCoverageTurns = points.filter(function (point) {
      return point.frontCount > 0 || point.concealingFogCellCount > 0 || point.fogCellCount > 0;
    });

    return {
      points: points,
      spawnEvents: spawnEvents,
      endEvents: endEvents,
      timelineEvents: timelineEvents.slice(-12).reverse(),
      averageCloudCoverage: mean(fogCoverageTurns.map(function (point) {
        return point.concealingFogCellCount || point.fogCellCount;
      })),
      averageSpawnInterval: mean(turnDiffs(spawnEvents.map(function (event) { return event.turn; }))),
      maxSimultaneousFronts: maxOf(points.map(function (point) { return point.frontCount; }), 0),
      minSimultaneousFronts: minOf(points.map(function (point) { return point.frontCount; }), 0),
      peakHiddenPieces: maxOf(points.map(function (point) {
        return Math.max(point.whiteHiddenPieces, point.blackHiddenPieces);
      }), 0),
      averageFogCells: mean(points.map(function (point) { return point.fogCellCount; }))
    };
  }

  function normalizeTempo(timeline) {
    const points = timeline.map(function (record) {
      const telemetry = record.behavioralTelemetry || {};
      const interactionTimeline = Array.isArray(telemetry.interactionTimeline) ? telemetry.interactionTimeline : [];
      const orchestrationEvents = Array.isArray(telemetry.orchestrationEvents) ? telemetry.orchestrationEvents : [];
      const telemetryEvents = interactionTimeline.concat(orchestrationEvents);
      const telemetryElapsed = telemetryEvents
        .map(function (event) {
          return toNumber(event && event.turnElapsedMs, null);
        })
        .filter(Number.isFinite);
      const auditElapsed = record.commandAuditTrail
        .map(function (entry) {
          return toNumber(entry && entry.turnElapsedMs, null);
        })
        .filter(Number.isFinite);
      const durationMs = maxOf(telemetryElapsed.concat(auditElapsed), 0);
      const submissionEvent = orchestrationEvents.find(function (event) {
        return event
          && (event.eventKey === "submit_requested" || event.stageKey === "submission")
          && Number.isFinite(toNumber(event.turnElapsedMs, null));
      }) || null;
      const validationEvent = orchestrationEvents.find(function (event) {
        return event
          && event.stageKey === "validation"
          && Number.isFinite(toNumber(event.turnElapsedMs, null));
      }) || null;
      const submissionMs = submissionEvent ? toNumber(submissionEvent.turnElapsedMs, null) : null;
      const validationMs = validationEvent ? toNumber(validationEvent.turnElapsedMs, null) : null;
      const validationLatencyMs = submissionMs !== null
        && validationMs !== null
        && validationMs >= submissionMs
        ? validationMs - submissionMs
        : null;

      return {
        turn: record.turn,
        activeKingdomKey: record.committedActiveKingdomKey,
        turnDurationMs: durationMs,
        validationLatencyMs: validationLatencyMs,
        hasValidationTelemetry: validationLatencyMs !== null
      };
    });

    const validationRows = points.filter(function (point) {
      return point.validationLatencyMs !== null;
    });

    return {
      points: points,
      averageTurnDurationMs: mean(points.map(function (point) { return point.turnDurationMs; })),
      peakTurnDurationMs: maxOf(points.map(function (point) { return point.turnDurationMs; }), 0),
      averageValidationLatencyMs: mean(validationRows.map(function (point) { return point.validationLatencyMs; })),
      peakValidationLatencyMs: maxOf(validationRows.map(function (point) { return point.validationLatencyMs; }), 0),
      validatedTurnCount: validationRows.length
    };
  }

  function normalizeDecision(timeline) {
    const points = timeline.map(function (record) {
      const backtracks = record.commandAuditTrail.filter(function (entry) {
        const actionKey = keyOrFallback(entry && entry.actionKey, "unknown");
        return actionKey === "replace" || actionKey === "cancel" || actionKey === "reset";
      }).length;
      const moveDraftCount = record.commandAuditTrail.filter(function (entry) {
        const actionKey = keyOrFallback(entry && entry.actionKey, "unknown");
        const command = entry && entry.command;
        const commandTypeKey = keyOrFallback(command && command.typeKey, "unknown");
        if (commandTypeKey !== "move") {
          return false;
        }
        return actionKey === "queue" || actionKey === "replace" || actionKey === "cancel" || actionKey === "reset";
      }).length;
      const committedMoveCount = record.structuredEvents.filter(function (event) {
        return event
          && event.typeKey === "command_committed"
          && (event.commandTypeKey === "move" || keyOrFallback(event.command && event.command.typeKey, "unknown") === "move");
      }).length;

      return {
        turn: record.turn,
        backtracks: backtracks,
        moveDraftCount: moveDraftCount,
        committedMoveCount: committedMoveCount,
        hesitationRatio: committedMoveCount ? moveDraftCount / committedMoveCount : 0
      };
    });

    return {
      points: points,
      averageBacktracks: mean(points.map(function (point) { return point.backtracks; })),
      peakBacktracks: maxOf(points.map(function (point) { return point.backtracks; }), 0),
      averageHesitationRatio: mean(points.map(function (point) { return point.hesitationRatio; })),
      peakHesitationRatio: maxOf(points.map(function (point) { return point.hesitationRatio; }), 0)
    };
  }

  function normalizeEconomy(timeline) {
    const points = timeline.map(function (record) {
      const whiteNetIncome = toNumber(record.snapshotMetrics.whiteNetIncome, 0);
      const blackNetIncome = toNumber(record.snapshotMetrics.blackNetIncome, 0);

      return {
        turn: record.turn,
        whiteNetIncome: whiteNetIncome,
        blackNetIncome: blackNetIncome,
        totalNetIncome: whiteNetIncome + blackNetIncome
      };
    });

    return {
      points: points,
      averageTotalNetIncome: mean(points.map(function (point) { return point.totalNetIncome; })),
      peakTotalNetIncome: maxOf(points.map(function (point) { return point.totalNetIncome; }), 0),
      peakWhiteNetIncome: maxOf(points.map(function (point) { return point.whiteNetIncome; }), 0),
      peakBlackNetIncome: maxOf(points.map(function (point) { return point.blackNetIncome; }), 0)
    };
  }

  function normalizeMovementBlockers(timeline, data) {
    const globalMaxRange = toNumber(
      data
      && data.configContext
      && data.configContext.combat
      && data.configContext.combat.globalMaxRange,
      DEFAULT_GLOBAL_MAX_RANGE
    );

    const points = timeline.map(function (record) {
      const movementContext = createMovementContext(record);
      if (!movementContext || !movementContext.pieces.length) {
        return {
          turn: record.turn,
          whiteWaterDeniedCells: 0,
          blackWaterDeniedCells: 0,
          totalWaterDeniedCells: 0,
          whiteWallDeniedCells: 0,
          blackWallDeniedCells: 0,
          totalWallDeniedCells: 0,
          waterDeniedCells: 0,
          wallDeniedCells: 0,
          totalDeniedCells: 0,
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
      const noWallState = createMovementRulesState(movementContext, {
        ignoreWater: false,
        ignoreWalls: true
      });

      let waterDeniedCells = 0;
      let wallDeniedCells = 0;
      let whiteWaterDeniedCells = 0;
      let blackWaterDeniedCells = 0;
      let whiteWallDeniedCells = 0;
      let blackWallDeniedCells = 0;

      movementContext.pieces.forEach(function (piece) {
        const actualMoves = getPseudoLegalMovesForMetrics(piece, actualState, globalMaxRange);
        const noWaterMoves = getPseudoLegalMovesForMetrics(piece, noWaterState, globalMaxRange);
        const noWallMoves = getPseudoLegalMovesForMetrics(piece, noWallState, globalMaxRange);
        const actualMoveIndex = buildPositionIndex(actualMoves);
        const pieceWaterDeniedCells = countPositionDifference(actualMoveIndex, noWaterMoves);
        const pieceWallDeniedCells = countPositionDifference(actualMoveIndex, noWallMoves);

        waterDeniedCells += pieceWaterDeniedCells;
        wallDeniedCells += pieceWallDeniedCells;

        if (piece.kingdomKey === "white") {
          whiteWaterDeniedCells += pieceWaterDeniedCells;
          whiteWallDeniedCells += pieceWallDeniedCells;
        } else if (piece.kingdomKey === "black") {
          blackWaterDeniedCells += pieceWaterDeniedCells;
          blackWallDeniedCells += pieceWallDeniedCells;
        }
      });

      return {
        turn: record.turn,
        whiteWaterDeniedCells: whiteWaterDeniedCells,
        blackWaterDeniedCells: blackWaterDeniedCells,
        totalWaterDeniedCells: waterDeniedCells,
        whiteWallDeniedCells: whiteWallDeniedCells,
        blackWallDeniedCells: blackWallDeniedCells,
        totalWallDeniedCells: wallDeniedCells,
        waterDeniedCells: waterDeniedCells,
        wallDeniedCells: wallDeniedCells,
        totalDeniedCells: waterDeniedCells + wallDeniedCells,
        analyzedPieceCount: movementContext.pieces.length
      };
    });

    return {
      points: points,
      analyzedTurnCount: points.filter(function (point) {
        return point.analyzedPieceCount > 0;
      }).length,
      averageAnalyzedPieceCount: mean(points.map(function (point) { return point.analyzedPieceCount; })),
      averageWaterDeniedCells: mean(points.map(function (point) { return point.waterDeniedCells; })),
      averageWallDeniedCells: mean(points.map(function (point) { return point.wallDeniedCells; })),
      averageTotalDeniedCells: mean(points.map(function (point) { return point.totalDeniedCells; })),
      peakWaterDeniedCells: maxOf(points.map(function (point) { return point.waterDeniedCells; }), 0),
      peakWallDeniedCells: maxOf(points.map(function (point) { return point.wallDeniedCells; }), 0),
      peakTotalDeniedCells: maxOf(points.map(function (point) { return point.totalDeniedCells; }), 0)
    };
  }

  function createMovementContext(record) {
    const snapshot = record && record.snapshot;
    const grid = snapshot && Array.isArray(snapshot.grid) ? snapshot.grid : [];
    const height = grid.length;
    const width = height && Array.isArray(grid[0]) ? grid[0].length : 0;
    if (!width || !height) {
      return null;
    }

    const pieces = collectMovementPieces(snapshot).filter(function (piece) {
      return piece
        && piece.x >= 0
        && piece.x < width
        && piece.y >= 0
        && piece.y < height;
    });
    const pieceIndexByPos = Object.create(null);
    const kingsByKingdom = Object.create(null);

    pieces.forEach(function (piece) {
      pieceIndexByPos[positionKey(piece.x, piece.y)] = piece;
      if (piece.typeId === PIECE_TYPE_KING && !kingsByKingdom[piece.kingdomKey]) {
        kingsByKingdom[piece.kingdomKey] = piece;
      }
    });

    const autonomousIndexByPos = Object.create(null);
    const autonomousUnits = snapshot && Array.isArray(snapshot.autonomousUnits)
      ? snapshot.autonomousUnits
      : [];

    autonomousUnits.forEach(function (unit) {
      const x = toNumber(unit && unit.x, null);
      const y = toNumber(unit && unit.y, null);
      if (x === null || y === null) {
        return;
      }
      autonomousIndexByPos[positionKey(x, y)] = {
        id: toNumber(unit && unit.id, 0),
        x: x,
        y: y
      };
    });

    return {
      grid: grid,
      width: width,
      height: height,
      pieces: pieces,
      pieceIndexByPos: pieceIndexByPos,
      kingsByKingdom: kingsByKingdom,
      autonomousIndexByPos: autonomousIndexByPos,
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
      .map(function (piece) {
        return normalizeMovementPiece(piece, kingdomKey);
      })
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
      kingdomKey: kingdomKey,
      x: x,
      y: y,
      hasWallBreachEntry: Boolean(piece && piece.hasWallBreachEntry),
      wallBreachEntryDx: toNumber(piece && piece.wallBreachEntryDx, 0),
      wallBreachEntryDy: toNumber(piece && piece.wallBreachEntryDy, 0),
      wallBreachCellX: toNumber(piece && piece.wallBreachCellX, -1),
      wallBreachCellY: toNumber(piece && piece.wallBreachCellY, -1)
    };
  }

  function buildMovementBuildingIndex(record) {
    const entities = record && record.analytics && record.analytics.entities && typeof record.analytics.entities === "object"
      ? record.analytics.entities
      : {};
    const buildingIndex = Array.isArray(entities.buildingIndex) ? entities.buildingIndex : [];

    return buildingIndex.reduce(function (index, building) {
      const buildingTypeId = toNumber(building && building.buildingTypeId, -1);
      const ownerKingdomKey = keyOrFallback(
        building && building.ownerKingdomKey,
        kingdomKeyFromId(building && building.ownerKingdomId)
      );
      const cells = Array.isArray(building && building.cells) ? building.cells : [];

      cells.forEach(function (cell) {
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
          buildingTypeId: buildingTypeId,
          buildingTypeKey: keyOrFallback(building && building.buildingTypeKey, "unknown"),
          isPublic: Boolean(building && building.isPublic),
          isNeutral: Boolean(building && building.isNeutral),
          ownerKingdomKey: ownerKingdomKey,
          destroyed: Boolean(cell && cell.destroyed),
          breached: Boolean(cell && cell.breached),
          hp: toNumber(cell && cell.hp, 0)
        };
      });

      return index;
    }, Object.create(null));
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
          [[-1, -1], [-1, 1], [1, -1], [1, 1]].forEach(function (direction) {
            moves = moves.concat(getDirectionalMovesForMetrics(piece, state, direction[0], direction[1], globalMaxRange));
          });
          break;
        case PIECE_TYPE_ROOK:
          [[0, -1], [0, 1], [-1, 0], [1, 0]].forEach(function (direction) {
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
    return moves.filter(function (move) {
      return positionKey(move.x, move.y) !== enemyKingCellKey;
    });
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
        [[-1, -1], [-1, 1], [1, -1], [1, 1]].forEach(function (direction) {
          candidateMoves = candidateMoves.concat(
            getDirectionalMovesForMetrics(piece, state, direction[0], direction[1], globalMaxRange)
          );
        });
        break;
      case PIECE_TYPE_ROOK:
        [[0, -1], [0, 1], [-1, 0], [1, 0]].forEach(function (direction) {
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

    orthogonalDirs.forEach(function (direction) {
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

    diagonalDirs.forEach(function (direction) {
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

    offsets.forEach(function (offset) {
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

    return candidateMoves.filter(function (destination) {
      return isWallBreachSourceSideDestinationForMetrics(piece, state, entryDelta, destination);
    });
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

    [-1, 1].forEach(function (dx) {
      const building = getEffectiveBuildingCellForMetrics(state, piece.x + dx, piece.y);
      if (isEnemyStoneWallCellForMetrics(building, piece.kingdomKey)) {
        horizontalNeighborCount += 1;
      }
    });

    [-1, 1].forEach(function (dy) {
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
    return positions.reduce(function (index, position) {
      index[positionKey(position.x, position.y)] = true;
      return index;
    }, Object.create(null));
  }

  function countPositionDifference(referenceIndex, candidatePositions) {
    const candidateIndex = buildPositionIndex(candidatePositions);
    return Object.keys(candidateIndex).reduce(function (count, key) {
      return count + (referenceIndex[key] ? 0 : 1);
    }, 0);
  }

  function positionKey(x, y) {
    return x + "," + y;
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

  function normalizeInfernal(timeline) {
    const points = [];
    const spawnEvents = [];
    const unitSpans = Object.create(null);

    timeline.forEach(function (record) {
      const infernal = (record.analytics && record.analytics.infernal) || {};
      const autonomousUnits = ((record.analytics && record.analytics.entities) || {}).autonomousUnitIndex || [];
      const autonomousById = buildIndexById(autonomousUnits);

      points.push({
        turn: record.turn,
        whiteDebt: toNumber(infernal.whiteBloodDebt, 0),
        blackDebt: toNumber(infernal.blackBloodDebt, 0),
        totalDebt: toNumber(infernal.whiteBloodDebt, 0) + toNumber(infernal.blackBloodDebt, 0),
        activeInfernalUnitId: toNumber(infernal.activeInfernalUnitId, 0),
        activeInfernalCount: autonomousUnits.length
      });

      record.structuredEvents.forEach(function (event) {
        if (event.typeKey === "infernal_spawned") {
          const unitId = toNumber(event.unitId, 0);
          const unit = autonomousById[unitId] || null;
          const infernalData = (unit && unit.infernal) || {};
          const targetKingdomKey = keyOrFallback(infernalData.targetKingdomKey, "unknown");
          const manifestedPieceKey = keyOrFallback(infernalData.manifestedPieceTypeKey, "unknown");
          const spawnEvent = {
            turn: record.turn,
            unitId: unitId,
            targetKingdomKey: targetKingdomKey,
            manifestedPieceKey: manifestedPieceKey,
            label: pieceBadgeLabel(manifestedPieceKey) + " > " + kingdomBadgeShort(targetKingdomKey),
            color: targetKingdomKey === "white" ? "rgba(233, 233, 223, 0.82)" : "rgba(62, 87, 125, 0.86)"
          };
          spawnEvents.push(spawnEvent);
          unitSpans[String(unitId)] = {
            unitId: unitId,
            turn: record.turn,
            targetKingdomKey: targetKingdomKey,
            manifestedPieceKey: manifestedPieceKey,
            removedTurn: null
          };
          return;
        }

        if (event.typeKey === "infernal_removed") {
          const unitId = String(toNumber(event.unitId, 0));
          if (unitSpans[unitId]) {
            unitSpans[unitId].removedTurn = record.turn;
          }
        }
      });
    });

    const lastTurn = timeline.length ? timeline[timeline.length - 1].turn : 0;
    const unitRows = Object.keys(unitSpans)
      .map(function (key) {
        const span = unitSpans[key];
        const endTurn = span.removedTurn !== null ? span.removedTurn : lastTurn;
        const observedLifetime = Math.max(1, endTurn - span.turn + 1);
        return {
          unitId: span.unitId,
          spawnTurn: span.turn,
          targetKingdomKey: span.targetKingdomKey,
          manifestedPieceKey: span.manifestedPieceKey,
          removedTurn: span.removedTurn,
          observedLifetime: observedLifetime
        };
      })
      .sort(function (left, right) {
        return left.spawnTurn - right.spawnTurn;
      });

    return {
      points: points,
      spawnEvents: spawnEvents,
      unitRows: unitRows,
      averageWhiteDebt: mean(points.map(function (point) { return point.whiteDebt; })),
      averageBlackDebt: mean(points.map(function (point) { return point.blackDebt; })),
      averageSpawnInterval: mean(turnDiffs(spawnEvents.map(function (event) { return event.turn; }))),
      averageLifetime: mean(unitRows.map(function (row) { return row.observedLifetime; })),
      maxWhiteDebt: maxOf(points.map(function (point) { return point.whiteDebt; }), 0),
      maxBlackDebt: maxOf(points.map(function (point) { return point.blackDebt; }), 0)
    };
  }

  function normalizeXp(timeline) {
    const grants = [];
    const sourceMap = Object.create(null);
    const recipientMap = Object.create(null);
    const turnMap = Object.create(null);

    timeline.forEach(function (record) {
      record.xpAuditTrail.forEach(function (entry) {
        const amount = toNumber(entry.amount, 0);
        const sourceKey = keyOrFallback(entry.sourceKey, keyOrFallback(entry.sourceLabel, "unknown"));
        const sourceLabel = keyOrFallback(entry.sourceLabel, sourceKey);
        const kingdomKey = keyOrFallback(entry.recipientKingdomKey, "unknown");
        const grant = {
          turn: record.turn,
          sourceKey: sourceKey,
          sourceLabel: sourceLabel,
          amount: amount,
          recipientPieceId: toNumber(entry.recipientPieceId, 0),
          recipientKingdomKey: kingdomKey,
          recipientXPBefore: toNumber(entry.recipientXPBefore, 0),
          recipientXPAfter: toNumber(entry.recipientXPAfter, 0),
          recipientPosition: formatPosition(entry.recipientPosition),
          searchable: [
            sourceKey,
            sourceLabel,
            kingdomKey,
            String(entry.recipientPieceId || ""),
            formatPosition(entry.recipientPosition)
          ].join(" ").toLowerCase()
        };

        grants.push(grant);

        if (!sourceMap[sourceKey]) {
          sourceMap[sourceKey] = {
            sourceKey: sourceKey,
            sourceLabel: sourceLabel,
            totalAmount: 0,
            grantCount: 0
          };
        }
        sourceMap[sourceKey].totalAmount += amount;
        sourceMap[sourceKey].grantCount += 1;

        const recipientKey = kingdomKey + "#" + grant.recipientPieceId;
        if (!recipientMap[recipientKey]) {
          recipientMap[recipientKey] = {
            recipientPieceId: grant.recipientPieceId,
            recipientKingdomKey: kingdomKey,
            totalAmount: 0,
            grantCount: 0
          };
        }
        recipientMap[recipientKey].totalAmount += amount;
        recipientMap[recipientKey].grantCount += 1;

        if (!turnMap[String(record.turn)]) {
          turnMap[String(record.turn)] = {
            turn: record.turn,
            whiteAmount: 0,
            blackAmount: 0,
            totalAmount: 0
          };
        }
        turnMap[String(record.turn)].totalAmount += amount;
        if (kingdomKey === "white") {
          turnMap[String(record.turn)].whiteAmount += amount;
        } else if (kingdomKey === "black") {
          turnMap[String(record.turn)].blackAmount += amount;
        }
      });
    });

    const amounts = grants.map(function (grant) { return grant.amount; });
    const sourceRows = Object.keys(sourceMap)
      .map(function (key) {
        const row = sourceMap[key];
        row.averageAmount = row.grantCount ? row.totalAmount / row.grantCount : 0;
        return row;
      })
      .sort(function (left, right) {
        return right.totalAmount - left.totalAmount;
      });

    const topRecipients = Object.keys(recipientMap)
      .map(function (key) {
        return recipientMap[key];
      })
      .sort(function (left, right) {
        return right.totalAmount - left.totalAmount;
      })
      .slice(0, 8);

    const turnRows = Object.keys(turnMap)
      .map(function (key) { return turnMap[key]; })
      .sort(function (left, right) { return left.turn - right.turn; });

    return {
      grants: grants,
      turnRows: turnRows,
      sourceRows: sourceRows,
      topRecipients: topRecipients,
      totalAmount: sum(amounts),
      averageAmount: mean(amounts),
      medianAmount: median(amounts),
      trimmedMeanAmount: trimmedMean(amounts, 0.1),
      maxAmount: maxOf(amounts, 0),
      minAmount: minOf(amounts, 0),
      grantCount: grants.length
    };
  }

  function buildAlerts(data, timeline, tempo, decision, weather, infernal, xp) {
    const alerts = [];

    if (!data.historyContinuityComplete) {
      alerts.push({
        level: "warn",
        title: "Historique partiel",
        message: "Le companion a ete reconstruit depuis un snapshot de reprise. Les courbes temporelles n'incluent peut-etre pas le vrai debut de partie."
      });
    }

    if (!data.loadedFromExistingCompanion) {
      alerts.push({
        level: "info",
        title: "Archive bootstrap",
        message: "Le recorder n'a pas relu un ancien companion complet. Considere les premiers points comme un point de reprise possible."
      });
    }

    if (timeline.length <= 1) {
      alerts.push({
        level: "warn",
        title: "Serie temporelle courte",
        message: "Le viewer n'a qu'un seul point de timeline exploitable. Les graphiques restent valides, mais les moyennes d'intervalle sont peu informatives."
      });
    }

    if (!weather.spawnEvents.length) {
      alerts.push({
        level: "info",
        title: "Aucun front observe",
        message: "Aucun evenement weather_front_spawned n'a ete trouve dans l'historique fourni."
      });
    }

    if (!infernal.spawnEvents.length) {
      alerts.push({
        level: "info",
        title: "Aucun spawn infernal observe",
        message: "La section infernale reste active mais ne detecte pas encore de spawns dans ce fichier."
      });
    }

    if (!xp.grantCount) {
      alerts.push({
        level: "info",
        title: "Aucun grant XP",
        message: "xpAuditTrail est vide sur la plage chargee; les tableaux XP restent donc principalement descriptifs."
      });
    }

    if (!tempo.validatedTurnCount) {
      alerts.push({
        level: "info",
        title: "Validation distante absente",
        message: "Aucun couple submission -> validation n'a ete observe dans la plage chargee; la courbe de validation restera vide ou quasi nulle."
      });
    }

    if (!decision.peakBacktracks) {
      alerts.push({
        level: "info",
        title: "Aucun backtrack observe",
        message: "Aucun replace/cancel/reset n'a ete detecte dans la plage chargee; la courbe de retours en arriere restera plate."
      });
    }

    return alerts;
  }

  function renderDashboard(normalized) {
    refs.emptyState.classList.add("is-hidden");
    renderTempo(normalized.tempo);
    renderDecision(normalized.decision);
    renderEconomy(normalized.economy);
    renderMovementBlockers(normalized.blockers);
    renderWeather(normalized.weather);
    renderInfernal(normalized.infernal);
  }

  function renderEmpty() {
    destroyAllCharts();
    refs.tempoCards.innerHTML = "";
    refs.decisionCards.innerHTML = "";
    refs.economyCards.innerHTML = "";
    refs.blockersCards.innerHTML = "";
    refs.weatherCards.innerHTML = "";
    refs.weatherEventTableBody.innerHTML = tablePlaceholderRow(4, "Aucun evenement meteo charge.");
    refs.weatherTableMeta.textContent = "";
    refs.infernalCards.innerHTML = "";
    refs.infernalSpawnTableBody.innerHTML = tablePlaceholderRow(5, "Aucun spawn infernal charge.");
    refs.infernalTableMeta.textContent = "";
    refs.emptyState.classList.remove("is-hidden");
  }

  function renderSummaryCards(summary) {
    const cards = [
      {
        label: "Save",
        value: summary.saveName,
        detail: summary.sourceLabel
      },
      {
        label: "Turns",
        value: String(summary.turnCount),
        detail: "dernier turn " + summary.finalTurn
      },
      {
        label: "Events",
        value: String(summary.recordedEventCount),
        detail: summary.totalStructuredEvents + " structured events lus"
      },
      {
        label: "XP total",
        value: formatNumber(summary.totalXp),
        detail: "audit brut cumule"
      },
      {
        label: "Meteo",
        value: String(summary.weatherSpawnCount),
        detail: "spawns de fronts"
      },
      {
        label: "Infernal",
        value: String(summary.infernalSpawnCount),
        detail: "spawns observes"
      }
    ];

    refs.summaryCards.innerHTML = cards.map(renderMetricCard).join("");
  }

  function renderSchema(schema) {
    refs.topologyStrip.innerHTML = schema.topologyNodes.map(function (node) {
      return "<article class=\"topology-node\">"
        + "<span>" + escapeHtml(node.label) + "</span>"
        + "<strong>" + escapeHtml(String(node.value)) + "</strong>"
        + "<p>" + escapeHtml(node.detail) + "</p>"
        + "</article>";
    }).join("");

    refs.rootKeyList.innerHTML = schema.rootRows.map(function (row) {
      return "<div class=\"schema-row\">"
        + "<div><strong>" + escapeHtml(row.key) + "</strong></div>"
        + "<div class=\"schema-key\">" + escapeHtml(row.descriptor) + "</div>"
        + "</div>";
    }).join("");
  }

  function renderAlerts(alerts) {
    refs.alertStack.innerHTML = alerts.map(function (alert) {
      const className = alert.level === "danger"
        ? "alert-card alert-danger"
        : alert.level === "info"
          ? "alert-card alert-info"
          : "alert-card";
      return "<article class=\"" + className + "\">"
        + "<span>" + escapeHtml(alert.title) + "</span>"
        + "<p>" + escapeHtml(alert.message) + "</p>"
        + "</article>";
    }).join("");
  }

  function renderHistoryBanner(summary) {
    if (summary.historyContinuityComplete) {
      refs.historyBanner.hidden = true;
      refs.historyBanner.textContent = "";
      return;
    }

    refs.historyBanner.hidden = false;
    refs.historyBanner.textContent = "Historique partiel: les moyennes temporelles et la densite d'evenements commencent sur un point de reprise, pas forcement au vrai debut de partie.";
  }

  function renderSessionPanel(summary, rawData) {
    const facts = [
      { label: "Schema", value: "V" + summary.schemaVersion, detail: summary.formatFamily },
      { label: "Generator", value: summary.provenanceGenerator, detail: "format family data" },
      { label: "Actif", value: summary.activeKingdomKey, detail: "royaume courant" },
      { label: "Continuite", value: summary.historyContinuityComplete ? "complete" : "partielle", detail: summary.loadedFromExistingCompanion ? "ancien companion relu" : "bootstrap local" },
      { label: "Data", value: summary.dataCollectionEnabled ? "on" : "off", detail: summary.behavioralTelemetryEnabled ? "telemetry on" : "telemetry off" },
      { label: "Maj UTC", value: formatUtc(summary.lastUpdatedAtUnix), detail: "creation " + formatUtc(summary.createdAtUnix) }
    ];

    refs.sessionFactGrid.innerHTML = facts.map(function (fact) {
      return "<article class=\"fact-card\">"
        + "<span>" + escapeHtml(fact.label) + "</span>"
        + "<strong>" + escapeHtml(fact.value) + "</strong>"
        + "<p>" + escapeHtml(fact.detail) + "</p>"
        + "</article>";
    }).join("");

    const signalSteps = [
      {
        label: "Root",
        value: summary.saveName,
        detail: "schemaVersion " + summary.schemaVersion + ", saveName, provenance, current state"
      },
      {
        label: "Turns",
        value: String(summary.turnCount),
        detail: "turnHistory avec structuredEvents et xpAuditTrail"
      },
      {
        label: "Analytics",
        value: "analytics + snapshot",
        detail: "meteo/xp via analytics, blockers via snapshot.grid + buildingIndex"
      },
      {
        label: "Viewer",
        value: "cards + charts + tables",
        detail: "agregats derives localement dans le navigateur"
      }
    ];

    refs.signalFlow.innerHTML = signalSteps.map(function (step) {
      return "<article class=\"signal-step\">"
        + "<div><span>" + escapeHtml(step.label) + "</span><strong>" + escapeHtml(step.value) + "</strong></div>"
        + "<p>" + escapeHtml(step.detail) + "</p>"
        + "</article>";
    }).join("");

    if (!rawData) {
      refs.emptyState.classList.remove("is-hidden");
    }
  }

  function renderTempo(tempo) {
    refs.tempoCards.innerHTML = [
      {
        label: "Tour moyen",
        value: formatDurationMs(tempo.averageTurnDurationMs),
        detail: "max turnElapsedMs par tour"
      },
      {
        label: "Pic tour",
        value: formatDurationMs(tempo.peakTurnDurationMs),
        detail: "turn le plus long observe"
      },
      {
        label: "Validation moyenne",
        value: tempo.validatedTurnCount ? formatDurationMs(tempo.averageValidationLatencyMs) : "n/a",
        detail: tempo.validatedTurnCount + " turns avec submission -> validation"
      },
      {
        label: "Pic validation",
        value: tempo.validatedTurnCount ? formatDurationMs(tempo.peakValidationLatencyMs) : "n/a",
        detail: "latence distante observee"
      }
    ].map(renderMetricCard).join("");

    drawChart("tempoChart", refs.tempoChart, {
      type: "line",
      data: {
        datasets: [
          buildLineDataset("Turn duration", tempo.points, "turnDurationMs", "rgba(224, 114, 99, 0.92)", "y")
        ]
      },
      options: buildTimelineChartOptions({
        xTitle: "Turn",
        yTitle: "Milliseconds",
        markers: []
      })
    });
  }

  function renderDecision(decision) {
    refs.decisionCards.innerHTML = [
      {
        label: "Backtracks moyens",
        value: formatNumber(decision.averageBacktracks),
        detail: "replace/cancel/reset par tour"
      },
      {
        label: "Pic backtracks",
        value: formatNumber(decision.peakBacktracks),
        detail: "turn le plus instable"
      },
      {
        label: "Hesitation moy.",
        value: formatNumber(decision.averageHesitationRatio),
        detail: "draft moves / moves commits"
      },
      {
        label: "Pic hesitation",
        value: formatNumber(decision.peakHesitationRatio),
        detail: "sur la plage chargee"
      }
    ].map(renderMetricCard).join("");

    drawChart("decisionChart", refs.decisionChart, {
      type: "line",
      data: {
        datasets: [
          buildLineDataset("Backtracks", decision.points, "backtracks", "rgba(215, 164, 83, 0.9)", "y")
        ]
      },
      options: buildTimelineChartOptions({
        xTitle: "Turn",
        yTitle: "Backtracks",
        markers: []
      })
    });

    drawChart("decisionHesitationChart", refs.decisionHesitationChart, {
      type: "line",
      data: {
        datasets: [
          buildLineDataset("Hesitation ratio", decision.points, "hesitationRatio", "rgba(91, 178, 166, 0.92)", "y")
        ]
      },
      options: buildTimelineChartOptions({
        xTitle: "Turn",
        yTitle: "Ratio",
        markers: []
      })
    });
  }

  function renderEconomy(economy) {
    refs.economyCards.innerHTML = [
      {
        label: "Income total moyen",
        value: formatNumber(economy.averageTotalNetIncome),
        detail: "white + black"
      },
      {
        label: "Pic total",
        value: formatNumber(economy.peakTotalNetIncome),
        detail: "sur la plage chargee"
      },
      {
        label: "Pic blanc",
        value: formatNumber(economy.peakWhiteNetIncome),
        detail: "income net max"
      },
      {
        label: "Pic noir",
        value: formatNumber(economy.peakBlackNetIncome),
        detail: "income net max"
      }
    ].map(renderMetricCard).join("");

    drawChart("economyChart", refs.economyChart, {
      type: "line",
      data: {
        datasets: [
          buildLineDataset("White net income", economy.points, "whiteNetIncome", WHITE_KINGDOM_CURVE_COLOR, "y"),
          buildLineDataset("Black net income", economy.points, "blackNetIncome", "rgba(62, 87, 125, 0.92)", "y"),
          buildLineDataset("Total net income", economy.points, "totalNetIncome", "rgba(140, 188, 120, 0.9)", "y")
        ]
      },
      options: buildTimelineChartOptions({
        xTitle: "Turn",
        yTitle: "Net income",
        markers: []
      })
    });
  }

  function renderMovementBlockers(blockers) {
    refs.blockersCards.innerHTML = [
      {
        label: "Denied moyen",
        value: formatNumber(blockers.averageTotalDeniedCells),
        detail: blockers.analyzedTurnCount + " turns analyses"
      },
      {
        label: "Pic total",
        value: formatNumber(blockers.peakTotalDeniedCells),
        detail: "eau + murs cumules"
      },
      {
        label: "Eau moyenne",
        value: formatNumber(blockers.averageWaterDeniedCells),
        detail: "cells refusees par tour"
      },
      {
        label: "Murs moyens",
        value: formatNumber(blockers.averageWallDeniedCells),
        detail: "inclut la demi-plan stone wall"
      }
    ].map(renderMetricCard).join("");

    drawChart("blockersWaterChart", refs.blockersWaterChart, {
      type: "line",
      data: {
        datasets: [
          buildLineDataset("White water denied cells", blockers.points, "whiteWaterDeniedCells", WHITE_KINGDOM_CURVE_COLOR, "y"),
          buildLineDataset("Black water denied cells", blockers.points, "blackWaterDeniedCells", "rgba(62, 87, 125, 0.92)", "y"),
          buildLineDataset("Total water denied cells", blockers.points, "totalWaterDeniedCells", "rgba(91, 178, 166, 0.92)", "y")
        ]
      },
      options: buildTimelineChartOptions({
        xTitle: "Turn",
        yTitle: "Denied water cells",
        markers: []
      })
    });

    drawChart("blockersWallChart", refs.blockersWallChart, {
      type: "line",
      data: {
        datasets: [
          buildLineDataset("White wall denied cells", blockers.points, "whiteWallDeniedCells", WHITE_KINGDOM_CURVE_COLOR, "y"),
          buildLineDataset("Black wall denied cells", blockers.points, "blackWallDeniedCells", "rgba(62, 87, 125, 0.92)", "y"),
          buildLineDataset("Total wall denied cells", blockers.points, "totalWallDeniedCells", "rgba(224, 114, 99, 0.9)", "y")
        ]
      },
      options: buildTimelineChartOptions({
        xTitle: "Turn",
        yTitle: "Denied wall cells",
        markers: []
      })
    });
  }

  function renderWeather(weather) {
    refs.weatherCards.innerHTML = [
      {
        label: "Couverture moyenne",
        value: formatNumber(weather.averageCloudCoverage),
        detail: "approximation via cellules de brouillard exposees"
      },
      {
        label: "Intervalle moyen",
        value: weather.averageSpawnInterval ? formatNumber(weather.averageSpawnInterval) + " turns" : "n/a",
        detail: "entre spawns de fronts"
      },
      {
        label: "Fronts simultanes",
        value: weather.maxSimultaneousFronts + " / " + weather.minSimultaneousFronts,
        detail: "max / min observes"
      },
      {
        label: "Pieces masquees",
        value: formatNumber(weather.peakHiddenPieces),
        detail: "pic blanc/noir sur un meme turn"
      }
    ].map(renderMetricCard).join("");

    refs.weatherTableMeta.textContent = weather.timelineEvents.length + " evenements affiches";
    refs.weatherEventTableBody.innerHTML = weather.timelineEvents.length
      ? weather.timelineEvents.map(function (event) {
          return "<tr>"
            + "<td>" + escapeHtml(String(event.turn)) + "</td>"
            + "<td>" + escapeHtml(event.typeLabel) + "</td>"
            + "<td>" + escapeHtml(event.frontLabel) + "</td>"
            + "<td>" + escapeHtml(event.coverageLabel) + "</td>"
            + "</tr>";
        }).join("")
      : tablePlaceholderRow(4, "Aucun evenement meteo dans la plage chargee.");

    drawChart("weatherChart", refs.weatherChart, {
      type: "line",
      data: {
        datasets: [
          buildLineDataset("White hidden enemy pieces", weather.points, "whiteHiddenPieces", WHITE_KINGDOM_CURVE_COLOR, "y"),
          buildLineDataset("Black hidden enemy pieces", weather.points, "blackHiddenPieces", "rgba(91, 178, 166, 0.92)", "y"),
          buildLineDataset("Total hidden enemy pieces", weather.points, "totalHiddenPieces", "rgba(140, 188, 120, 0.9)", "y"),
          buildLineDataset("Concealing fog cells", weather.points, "concealingFogCellCount", "rgba(215, 164, 83, 0.85)", "y2")
        ]
      },
      options: buildTimelineChartOptions({
        xTitle: "Turn",
        yTitle: "Hidden pieces",
        y2Title: "Fog cells",
        markers: weather.spawnEvents.concat(weather.endEvents)
      })
    });
  }

  function renderInfernal(infernal) {
    refs.infernalCards.innerHTML = [
      {
        label: "Dette blanche moy.",
        value: formatNumber(infernal.averageWhiteDebt),
        detail: "moyenne sur la plage chargee"
      },
      {
        label: "Dette noire moy.",
        value: formatNumber(infernal.averageBlackDebt),
        detail: "moyenne sur la plage chargee"
      },
      {
        label: "Delai moyen",
        value: infernal.averageSpawnInterval ? formatNumber(infernal.averageSpawnInterval) + " turns" : "n/a",
        detail: "entre deux spawns infernaux"
      },
      {
        label: "Lifetime moyen",
        value: infernal.averageLifetime ? formatNumber(infernal.averageLifetime) + " turns" : "n/a",
        detail: "duree de vie observee"
      }
    ].map(renderMetricCard).join("");

    refs.infernalTableMeta.textContent = infernal.unitRows.length + " unites suivies";
    refs.infernalSpawnTableBody.innerHTML = infernal.unitRows.length
      ? infernal.unitRows.map(function (row) {
          return "<tr>"
            + "<td>" + escapeHtml(String(row.spawnTurn)) + "</td>"
            + "<td>#" + escapeHtml(String(row.unitId)) + "</td>"
            + "<td>" + kingdomBadge(row.targetKingdomKey) + "</td>"
            + "<td><span class=\"pill-inline badge-neutral\">" + escapeHtml(row.manifestedPieceKey) + "</span></td>"
            + "<td>" + escapeHtml(String(row.observedLifetime)) + "</td>"
            + "</tr>";
        }).join("")
      : tablePlaceholderRow(5, "Aucun spawn infernal sur la plage chargee.");

    drawChart("infernalChart", refs.infernalChart, {
      type: "line",
      data: {
        datasets: [
          buildLineDataset("White blood debt", infernal.points, "whiteDebt", WHITE_KINGDOM_CURVE_COLOR, "y"),
          buildLineDataset("Black blood debt", infernal.points, "blackDebt", "rgba(62, 87, 125, 0.92)", "y")
        ]
      },
      options: buildTimelineChartOptions({
        xTitle: "Turn",
        yTitle: "Blood debt",
        markers: infernal.spawnEvents.map(function (event) {
          return {
            x: event.turn,
            label: event.label,
            color: event.color,
            lineDash: [3, 2],
            minLabelGap: 34
          };
        }).concat(infernal.unitRows.filter(function (row) {
          return row.removedTurn !== null;
        }).map(function (row) {
          return {
            x: row.removedTurn,
            lineDash: []
          };
        })),
        spans: infernal.unitRows.filter(function (row) {
          return row.removedTurn !== null;
        }).map(function (row) {
          return {
            xStart: row.spawnTurn,
            xEnd: row.removedTurn,
            fillColor: "rgba(0, 0, 0, 0.3)"
          };
        })
      })
    });
  }

  function renderXp(xp) {
    refs.xpCards.innerHTML = [
      {
        label: "Total XP",
        value: formatNumber(xp.totalAmount),
        detail: xp.grantCount + " grants"
      },
      {
        label: "Moyenne",
        value: formatNumber(xp.averageAmount),
        detail: "par grant"
      },
      {
        label: "Mediane",
        value: formatNumber(xp.medianAmount),
        detail: "distribution brute"
      },
      {
        label: "Moyenne tronquee",
        value: formatNumber(xp.trimmedMeanAmount),
        detail: "trim 10% sur chaque bord"
      }
    ].map(renderMetricCard).join("");

    refs.xpSourceMeta.textContent = xp.sourceRows.length + " sources distinctes";
    refs.xpSourceTableBody.innerHTML = xp.sourceRows.length
      ? xp.sourceRows.map(function (row) {
          return "<tr>"
            + "<td>" + escapeHtml(row.sourceLabel) + "</td>"
            + "<td>" + escapeHtml(String(row.grantCount)) + "</td>"
            + "<td>" + escapeHtml(formatNumber(row.totalAmount)) + "</td>"
            + "<td>" + escapeHtml(formatNumber(row.averageAmount)) + "</td>"
            + "</tr>";
        }).join("")
      : tablePlaceholderRow(4, "Aucune source XP dans la plage chargee.");

    renderXpGrantTable(xp);

    drawChart("xpTurnChart", refs.xpTurnChart, {
      type: "bar",
      data: {
        datasets: [
          buildBarDataset("White XP", xp.turnRows, "whiteAmount", "rgba(233, 233, 223, 0.88)"),
          buildBarDataset("Black XP", xp.turnRows, "blackAmount", "rgba(62, 87, 125, 0.92)"),
          buildBarDataset("Total XP", xp.turnRows, "totalAmount", "rgba(215, 164, 83, 0.55)")
        ]
      },
      options: buildTimelineChartOptions({
        xTitle: "Turn",
        yTitle: "XP",
        stacked: true,
        markers: []
      })
    });

    drawChart("xpSourceChart", refs.xpSourceChart, {
      type: "bar",
      data: {
        labels: xp.sourceRows.map(function (row) { return row.sourceLabel; }),
        datasets: [
          {
            label: "Total XP by source",
            data: xp.sourceRows.map(function (row) { return row.totalAmount; }),
            backgroundColor: [
              "rgba(215, 164, 83, 0.9)",
              "rgba(91, 178, 166, 0.82)",
              "rgba(224, 114, 99, 0.82)",
              "rgba(233, 233, 223, 0.8)",
              "rgba(62, 87, 125, 0.82)",
              "rgba(140, 188, 120, 0.8)",
              "rgba(182, 145, 230, 0.76)"
            ]
          }
        ]
      },
      options: buildSourceChartOptions()
    });
  }

  function renderXpGrantTable(xp) {
    const filteredGrants = xp.grants.filter(function (grant) {
      return !state.grantFilter || grant.searchable.indexOf(state.grantFilter) >= 0;
    });

    refs.xpGrantTableBody.innerHTML = filteredGrants.length
      ? filteredGrants.slice(0, 400).map(function (grant) {
          return "<tr>"
            + "<td>" + escapeHtml(String(grant.turn)) + "</td>"
            + "<td>" + escapeHtml(grant.sourceLabel) + "</td>"
            + "<td>" + escapeHtml(formatNumber(grant.amount)) + "</td>"
            + "<td>#" + escapeHtml(String(grant.recipientPieceId))
            + "<div class=\"muted-line\">" + escapeHtml(grant.recipientPosition) + "</div></td>"
            + "<td>" + kingdomBadge(grant.recipientKingdomKey) + "</td>"
            + "<td>" + escapeHtml(String(grant.recipientXPBefore)) + " -> " + escapeHtml(String(grant.recipientXPAfter)) + "</td>"
            + "</tr>";
        }).join("")
      : tablePlaceholderRow(6, "Aucun grant XP ne correspond au filtre courant.");
  }

  function drawChart(key, canvas, config) {
    if (typeof window.Chart === "undefined") {
      setStatus(refs.loadStatus.textContent, "Chart.js indisponible. Le CDN n'a pas charge.");
      return;
    }

    if (state.charts[key]) {
      state.charts[key].destroy();
    }
    state.charts[key] = new window.Chart(canvas.getContext("2d"), config);
  }

  function destroyAllCharts() {
    Object.keys(state.charts).forEach(function (key) {
      state.charts[key].destroy();
    });
    state.charts = Object.create(null);
  }

  function buildTimelineChartOptions(configuration) {
    const stacked = Boolean(configuration.stacked);
    const scales = {
      x: {
        type: "linear",
        ticks: {
          color: "#000000",
          precision: 0
        },
        title: {
          display: true,
          color: "#000000",
          text: configuration.xTitle
        },
        grid: {
          color: "rgba(0, 0, 0, 0.12)"
        },
        stacked: stacked
      },
      y: {
        ticks: {
          color: "#000000"
        },
        title: {
          display: true,
          color: "#000000",
          text: configuration.yTitle
        },
        grid: {
          color: "rgba(0, 0, 0, 0.12)"
        },
        stacked: stacked
      }
    };

    if (configuration.y2Title) {
      scales.y2 = {
        position: "right",
        ticks: {
          color: "#000000"
        },
        title: {
          display: true,
          color: "#000000",
          text: configuration.y2Title
        },
        grid: {
          drawOnChartArea: false
        }
      };
    }

    return {
      responsive: true,
      maintainAspectRatio: false,
      interaction: {
        mode: "index",
        intersect: false
      },
      plugins: {
        legend: {
          labels: {
            color: "#000000"
          }
        },
        tooltip: {
          backgroundColor: "rgba(255, 255, 255, 0.96)",
          titleColor: "#000000",
          bodyColor: "#000000",
          borderColor: "rgba(0, 0, 0, 0.18)",
          borderWidth: 1
        },
        eventMarkers: {
          markers: configuration.markers || [],
          spans: configuration.spans || []
        }
      },
      scales: scales
    };
  }

  function buildSourceChartOptions() {
    return {
      indexAxis: "y",
      responsive: true,
      maintainAspectRatio: false,
      plugins: {
        legend: {
          display: false
        },
        tooltip: {
          backgroundColor: "rgba(255, 255, 255, 0.96)",
          titleColor: "#000000",
          bodyColor: "#000000",
          borderColor: "rgba(0, 0, 0, 0.18)",
          borderWidth: 1
        }
      },
      scales: {
        x: {
          ticks: {
            color: "#000000"
          },
          title: {
            display: true,
            color: "#000000",
            text: "Total XP"
          },
          grid: {
            color: "rgba(0, 0, 0, 0.12)"
          }
        },
        y: {
          ticks: {
            color: "#000000"
          },
          grid: {
            display: false
          }
        }
      }
    };
  }

  function buildLineDataset(label, rows, key, color, axisId) {
    return {
      label: label,
      data: rows.map(function (row) {
        return { x: row.turn, y: toNumber(row[key], 0) };
      }),
      borderColor: color,
      backgroundColor: color,
      pointRadius: 2,
      pointHoverRadius: 4,
      tension: 0.25,
      borderWidth: 2,
      yAxisID: axisId
    };
  }

  function buildBarDataset(label, rows, key, color) {
    return {
      label: label,
      data: rows.map(function (row) {
        return { x: row.turn, y: toNumber(row[key], 0) };
      }),
      borderColor: color,
      backgroundColor: color,
      borderWidth: 1
    };
  }

  function buildNullableLineDataset(label, rows, key, color, axisId) {
    return {
      label: label,
      data: rows.map(function (row) {
        return {
          x: row.turn,
          y: row[key] === null || row[key] === undefined ? null : toNumber(row[key], null)
        };
      }),
      borderColor: color,
      backgroundColor: color,
      pointRadius: 2,
      pointHoverRadius: 4,
      tension: 0.25,
      borderWidth: 2,
      spanGaps: true,
      yAxisID: axisId
    };
  }

  function buildEventMarkersPlugin() {
    return {
      id: "eventMarkers",
      beforeDatasetsDraw: function (chart, _args, options) {
        const spans = (options && options.spans) || [];
        if (!spans.length || !chart.scales.x) {
          return;
        }

        const ctx = chart.ctx;
        const xScale = chart.scales.x;
        const chartArea = chart.chartArea;

        ctx.save();
        spans.forEach(function (span) {
          const xStart = xScale.getPixelForValue(span.xStart);
          const xEnd = xScale.getPixelForValue(span.xEnd);
          if (!Number.isFinite(xStart) || !Number.isFinite(xEnd)) {
            return;
          }

          const left = clamp(Math.min(xStart, xEnd), chartArea.left, chartArea.right);
          const right = clamp(Math.max(xStart, xEnd), chartArea.left, chartArea.right);
          const width = Math.max(right - left, 1);

          ctx.fillStyle = span.fillColor || "rgba(0, 0, 0, 0.3)";
          ctx.fillRect(left, chartArea.top + 4, width, Math.max(chartArea.bottom - chartArea.top - 8, 1));
        });
        ctx.restore();
      },
      afterDatasetsDraw: function (chart, _args, options) {
        const markers = (options && options.markers) || [];
        if (!markers.length || !chart.scales.x) {
          return;
        }

        const ctx = chart.ctx;
        const xScale = chart.scales.x;
        const chartArea = chart.chartArea;
        let lastLabelX = -Infinity;

        ctx.save();
        markers.forEach(function (marker) {
          const x = xScale.getPixelForValue(marker.x);
          if (!Number.isFinite(x) || x < chartArea.left || x > chartArea.right) {
            return;
          }

          ctx.strokeStyle = "#000000";
          ctx.lineWidth = 1.5;
          ctx.setLineDash(marker.lineDash || [3, 3]);
          ctx.beginPath();
          ctx.moveTo(x, chartArea.top + 4);
          ctx.lineTo(x, chartArea.bottom - 4);
          ctx.stroke();

          if (!marker.label) {
            return;
          }

          const minLabelGap = marker.minLabelGap || 28;
          if (x - lastLabelX < minLabelGap) {
            return;
          }
          lastLabelX = x;

          const labelText = String(marker.label);
          ctx.setLineDash([]);
          ctx.font = "10px Segoe UI";
          const textWidth = ctx.measureText(labelText).width;
          const paddingX = 6;
          const labelWidth = textWidth + (paddingX * 2);
          const labelX = clamp(x - (labelWidth / 2), chartArea.left, chartArea.right - labelWidth);
          const labelY = chartArea.bottom + 8;

          ctx.fillStyle = "#000000";
          ctx.fillRect(labelX, labelY, labelWidth, 18);
          ctx.strokeStyle = "#000000";
          ctx.strokeRect(labelX + 0.5, labelY + 0.5, labelWidth - 1, 17);
          ctx.fillStyle = "#ffffff";
          ctx.fillText(labelText, labelX + paddingX, labelY + 12);
        });
        ctx.restore();
      }
    };
  }

  function renderMetricCard(card) {
    return "<article class=\"metric-card\">"
      + "<span>" + escapeHtml(card.label) + "</span>"
      + "<strong>" + escapeHtml(String(card.value)) + "</strong>"
      + "<p>" + escapeHtml(card.detail) + "</p>"
      + "</article>";
  }

  function showLoadError(message) {
    destroyAllCharts();
    state.normalized = null;
    renderEmpty();
    refs.emptyState.innerHTML = "<p class=\"eyebrow\">Erreur</p>"
      + "<h2>Impossible de charger le companion</h2>"
      + "<p>" + escapeHtml(message) + "</p>";
    refs.emptyState.classList.remove("is-hidden");
  }

  function setStatus(loadMessage, schemaMessage) {
    void loadMessage;
    void schemaMessage;
  }

  function tablePlaceholderRow(columnCount, message) {
    return "<tr><td colspan=\"" + columnCount + "\">" + escapeHtml(message) + "</td></tr>";
  }

  function findObserverEntry(visibility, observerKey) {
    const rows = visibility && Array.isArray(visibility.byObserver) ? visibility.byObserver : [];
    return rows.find(function (row) {
      return row && row.observerKingdomKey === observerKey;
    }) || null;
  }

  function buildIndexById(rows) {
    return (Array.isArray(rows) ? rows : []).reduce(function (index, row) {
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

  function buildFrontCoverageLabel(front) {
    const along = toNumber(front.radiusAlongTimes1000, 0) / 1000;
    const across = toNumber(front.radiusAcrossTimes1000, 0) / 1000;
    if (!along && !across) {
      return "n/a";
    }
    return formatNumber(along) + " x " + formatNumber(across);
  }

  function shortDirectionLabel(front) {
    const label = keyOrFallback(front && front.directionKey, "front");
    return label.slice(0, 3).toUpperCase();
  }

  function pieceBadgeLabel(pieceKey) {
    const normalized = keyOrFallback(pieceKey, "?").toUpperCase();
    return normalized.length <= 3 ? normalized : normalized.slice(0, 3);
  }

  function kingdomBadgeShort(kingdomKey) {
    return kingdomKey === "white" ? "W" : kingdomKey === "black" ? "B" : "?";
  }

  function kingdomBadge(kingdomKey) {
    const className = kingdomKey === "white"
      ? "badge badge-white"
      : kingdomKey === "black"
        ? "badge badge-black"
        : "badge badge-neutral";
    return "<span class=\"" + className + "\">" + escapeHtml(kingdomKey) + "</span>";
  }

  function formatPosition(position) {
    if (!position || typeof position !== "object") {
      return "n/a";
    }
    const x = toNumber(position.x, null);
    const y = toNumber(position.y, null);
    if (x === null || y === null) {
      return "n/a";
    }
    return "(" + x + ", " + y + ")";
  }

  function formatNumber(value) {
    if (!Number.isFinite(value)) {
      return "n/a";
    }
    const rounded = Math.abs(value) >= 100 ? value.toFixed(0) : value.toFixed(1);
    return rounded.replace(/\.0$/, "");
  }

  function formatDurationMs(value) {
    if (!Number.isFinite(value)) {
      return "n/a";
    }
    if (Math.abs(value) >= 1000) {
      return formatNumber(value / 1000) + " s";
    }
    return formatNumber(value) + " ms";
  }

  function formatUtc(unixSeconds) {
    if (!Number.isFinite(unixSeconds) || unixSeconds <= 0) {
      return "n/a";
    }
    return UTC_FORMATTER.format(new Date(unixSeconds * 1000)) + " UTC";
  }

  function describeValue(value) {
    if (Array.isArray(value)) {
      return "array(" + value.length + ")";
    }
    if (value === null) {
      return "null";
    }
    if (typeof value === "object") {
      return "object(" + Object.keys(value).length + ")";
    }
    return typeof value;
  }

  function toNumber(value, fallback) {
    const parsed = Number(value);
    return Number.isFinite(parsed) ? parsed : fallback;
  }

  function isWindowsAbsolutePath(value) {
    return /^[a-zA-Z]:[\\/]/.test(value);
  }

  function keyOrFallback(value, fallback) {
    return typeof value === "string" && value ? value : fallback;
  }

  function mean(values) {
    if (!values.length) {
      return 0;
    }
    return sum(values) / values.length;
  }

  function sum(values) {
    return values.reduce(function (accumulator, value) {
      return accumulator + toNumber(value, 0);
    }, 0);
  }

  function median(values) {
    if (!values.length) {
      return 0;
    }
    const sorted = values.slice().sort(function (left, right) {
      return left - right;
    });
    const middle = Math.floor(sorted.length / 2);
    return sorted.length % 2
      ? sorted[middle]
      : (sorted[middle - 1] + sorted[middle]) / 2;
  }

  function trimmedMean(values, ratio) {
    if (!values.length) {
      return 0;
    }
    const sorted = values.slice().sort(function (left, right) {
      return left - right;
    });
    const trim = Math.floor(sorted.length * ratio);
    const trimmed = trim > 0 && (trim * 2) < sorted.length
      ? sorted.slice(trim, sorted.length - trim)
      : sorted;
    return mean(trimmed);
  }

  function turnDiffs(turns) {
    const sortedTurns = turns.slice().sort(function (left, right) {
      return left - right;
    });
    const diffs = [];
    for (let index = 1; index < sortedTurns.length; index += 1) {
      diffs.push(sortedTurns[index] - sortedTurns[index - 1]);
    }
    return diffs;
  }

  function maxOf(values, fallback) {
    return values.length ? Math.max.apply(Math, values) : fallback;
  }

  function minOf(values, fallback) {
    return values.length ? Math.min.apply(Math, values) : fallback;
  }

  function clamp(value, minValue, maxValue) {
    return Math.max(minValue, Math.min(maxValue, value));
  }

  function escapeHtml(value) {
    return String(value)
      .replace(/&/g, "&amp;")
      .replace(/</g, "&lt;")
      .replace(/>/g, "&gt;")
      .replace(/\"/g, "&quot;")
      .replace(/'/g, "&#39;");
  }
})();