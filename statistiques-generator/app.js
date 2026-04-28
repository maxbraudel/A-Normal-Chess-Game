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
      configuredSourceLabel: mustGet("configuredSourceLabel"),
      loadStatus: mustGet("loadStatus"),
      schemaStatus: mustGet("schemaStatus"),
      summaryCards: mustGet("summaryCards"),
      topologyStrip: mustGet("topologyStrip"),
      rootKeyList: mustGet("rootKeyList"),
      alertStack: mustGet("alertStack"),
      historyBanner: mustGet("historyBanner"),
      emptyState: mustGet("emptyState"),
      sessionFactGrid: mustGet("sessionFactGrid"),
      signalFlow: mustGet("signalFlow"),
      weatherChart: mustGet("weatherChart"),
      weatherCards: mustGet("weatherCards"),
      weatherTableMeta: mustGet("weatherTableMeta"),
      weatherEventTableBody: mustGet("weatherEventTableBody"),
      infernalChart: mustGet("infernalChart"),
      infernalCards: mustGet("infernalCards"),
      infernalTableMeta: mustGet("infernalTableMeta"),
      infernalSpawnTableBody: mustGet("infernalSpawnTableBody"),
      xpTurnChart: mustGet("xpTurnChart"),
      xpSourceChart: mustGet("xpSourceChart"),
      xpCards: mustGet("xpCards"),
      xpSourceMeta: mustGet("xpSourceMeta"),
      xpSourceTableBody: mustGet("xpSourceTableBody"),
      xpGrantFilterInput: mustGet("xpGrantFilterInput"),
      xpGrantTableBody: mustGet("xpGrantTableBody")
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
    refs.xpGrantFilterInput.addEventListener("input", onGrantFilterInput);
  }

  function onGrantFilterInput() {
    state.grantFilter = (refs.xpGrantFilterInput.value || "").trim().toLowerCase();
    if (state.normalized) {
      renderXpGrantTable(state.normalized.xp);
    }
  }

  function bootstrapConfiguredSource() {
    const source = normalizeConfiguredSource(DATA_COMPANION_URL);
    refs.configuredSourceLabel.textContent = source.displayLabel;

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
    refs.xpGrantFilterInput.value = "";
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
    const weather = normalizeWeather(timeline);
    const infernal = normalizeInfernal(timeline);
    const xp = normalizeXp(timeline);
    const summary = buildSummary(data, timeline, weather, infernal, xp, sourceLabel);
    const alerts = buildAlerts(data, timeline, weather, infernal, xp);

    return {
      raw: data,
      sourceLabel: sourceLabel,
      timeline: timeline,
      summary: summary,
      alerts: alerts,
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
      xpAuditTrail: Array.isArray(record.xpAuditTrail) ? record.xpAuditTrail : [],
      capturedAtUnix: toNumber(record.capturedAtUnix, null),
      analytics: analytics,
      snapshotMetrics: snapshotMetrics,
      sourceKind: "history"
    };
  }

  function buildSummary(data, timeline, weather, infernal, xp, sourceLabel) {
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

  function buildAlerts(data, timeline, weather, infernal, xp) {
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

    return alerts;
  }

  function renderDashboard(normalized) {
    refs.emptyState.classList.add("is-hidden");
    renderSummaryCards(normalized.summary);
    renderSchema(normalized.schema);
    renderAlerts(normalized.alerts);
    renderHistoryBanner(normalized.summary);
    renderSessionPanel(normalized.summary, normalized.raw);
    renderWeather(normalized.weather);
    renderInfernal(normalized.infernal);
    renderXp(normalized.xp);
  }

  function renderEmpty() {
    destroyAllCharts();
    refs.summaryCards.innerHTML = "";
    refs.topologyStrip.innerHTML = "";
    refs.rootKeyList.innerHTML = "";
    refs.alertStack.innerHTML = "";
    refs.historyBanner.hidden = true;
    refs.sessionFactGrid.innerHTML = "";
    refs.signalFlow.innerHTML = "";
    refs.weatherCards.innerHTML = "";
    refs.weatherEventTableBody.innerHTML = tablePlaceholderRow(4, "Aucun evenement meteo charge.");
    refs.weatherTableMeta.textContent = "";
    refs.infernalCards.innerHTML = "";
    refs.infernalSpawnTableBody.innerHTML = tablePlaceholderRow(5, "Aucun spawn infernal charge.");
    refs.infernalTableMeta.textContent = "";
    refs.xpCards.innerHTML = "";
    refs.xpSourceTableBody.innerHTML = tablePlaceholderRow(4, "Aucune source XP chargee.");
    refs.xpSourceMeta.textContent = "";
    refs.xpGrantTableBody.innerHTML = tablePlaceholderRow(6, "Aucun grant XP charge.");
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
        value: "weather / infernal / xp",
        detail: "lecture depuis analytics.visibility, analytics.weather, analytics.infernal"
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
          buildLineDataset("White hidden enemy pieces", weather.points, "whiteHiddenPieces", "rgba(233, 233, 223, 0.9)", "y"),
          buildLineDataset("Black hidden enemy pieces", weather.points, "blackHiddenPieces", "rgba(91, 178, 166, 0.92)", "y"),
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
          buildLineDataset("White blood debt", infernal.points, "whiteDebt", "rgba(233, 233, 223, 0.92)", "y"),
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
            color: "#f6ebd7"
          }
        },
        tooltip: {
          backgroundColor: "rgba(8, 15, 15, 0.94)",
          titleColor: "#f6ebd7",
          bodyColor: "#d3c4a4",
          borderColor: "rgba(240, 204, 136, 0.22)",
          borderWidth: 1
        },
        eventMarkers: {
          markers: configuration.markers || []
        }
      },
      scales: {
        x: {
          type: "linear",
          ticks: {
            color: "#d3c4a4",
            precision: 0
          },
          title: {
            display: true,
            color: "#9fb7ad",
            text: configuration.xTitle
          },
          grid: {
            color: "rgba(255, 255, 255, 0.06)"
          },
          stacked: stacked
        },
        y: {
          ticks: {
            color: "#d3c4a4"
          },
          title: {
            display: true,
            color: "#9fb7ad",
            text: configuration.yTitle
          },
          grid: {
            color: "rgba(255, 255, 255, 0.06)"
          },
          stacked: stacked
        },
        y2: configuration.y2Title ? {
          position: "right",
          ticks: {
            color: "#9fb7ad"
          },
          title: {
            display: true,
            color: "#9fb7ad",
            text: configuration.y2Title
          },
          grid: {
            drawOnChartArea: false
          }
        } : undefined
      }
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
          backgroundColor: "rgba(8, 15, 15, 0.94)",
          titleColor: "#f6ebd7",
          bodyColor: "#d3c4a4"
        }
      },
      scales: {
        x: {
          ticks: {
            color: "#d3c4a4"
          },
          title: {
            display: true,
            color: "#9fb7ad",
            text: "Total XP"
          },
          grid: {
            color: "rgba(255, 255, 255, 0.06)"
          }
        },
        y: {
          ticks: {
            color: "#d3c4a4"
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

  function buildEventMarkersPlugin() {
    return {
      id: "eventMarkers",
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

          ctx.strokeStyle = marker.color || "rgba(240, 204, 136, 0.76)";
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

          ctx.fillStyle = "rgba(5, 12, 12, 0.9)";
          ctx.fillRect(labelX, labelY, labelWidth, 18);
          ctx.strokeStyle = marker.color || "rgba(240, 204, 136, 0.76)";
          ctx.strokeRect(labelX + 0.5, labelY + 0.5, labelWidth - 1, 17);
          ctx.fillStyle = marker.color || "rgba(240, 204, 136, 0.92)";
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
    setStatus("Chargement refuse.", message);
  }

  function setStatus(loadMessage, schemaMessage) {
    refs.loadStatus.textContent = loadMessage;
    refs.schemaStatus.textContent = schemaMessage;
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