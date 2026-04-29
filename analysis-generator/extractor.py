from __future__ import annotations

from collections import Counter, defaultdict
from datetime import datetime, timezone
import json
from pathlib import Path
from statistics import mean
from typing import Any

from rich_metrics import (
    build_activity_metrics,
    build_feature_correlations,
    build_feature_distributions,
    build_record_feature_rows,
)


ANALYSIS_VERSION = "0.2.0"


def extract_analysis(input_path: Path) -> dict[str, Any]:
    data = load_companion(input_path)
    turn_records = build_turn_records(data)
    activity = build_activity_metrics(turn_records)
    behavior = build_behavior(turn_records)
    feature_rows = build_record_feature_rows(turn_records, activity, behavior)

    return {
        "metadata": build_metadata(data, input_path, turn_records),
        "source": build_source(data, input_path, turn_records),
        "config": build_config_snapshot(data),
        "reference": build_reference_snapshot(data),
        "matchSummary": build_match_summary(data, turn_records, activity, behavior),
        "timelines": build_timelines(turn_records, activity),
        "eventMarkers": build_event_markers(turn_records),
        "systems": build_systems(data, turn_records),
        "activity": activity,
        "behavior": behavior,
        "distributions": build_feature_distributions(feature_rows),
        "correlations": build_feature_correlations(feature_rows),
        "entityTrajectories": build_entity_trajectories(turn_records),
        "qualityChecks": build_quality_checks(data, turn_records),
        "limitations": build_limitations(data, turn_records),
    }


def load_companion(input_path: Path) -> dict[str, Any]:
    with input_path.open("r", encoding="utf-8") as handle:
        return json.load(handle)


def build_turn_records(data: dict[str, Any]) -> list[dict[str, Any]]:
    turn_history = as_list(data.get("turnHistory"))
    if not turn_history:
        fallback_turn = to_int(
            path_get(data, "currentMetrics", "turnNumber"),
            to_int(path_get(data, "currentAnalytics", "turnNumber"), 0),
        )
        return [
            {
                "recordSequenceIndex": 0,
                "turn": fallback_turn,
                "committedActiveKingdomKey": key_or_fallback(
                    path_get(data, "currentMetrics", "activeKingdomKey"), "unknown"
                ),
                "structuredEvents": [],
                "commandAuditTrail": [],
                "behavioralTelemetry": {},
                "xpAuditTrail": [],
                "turnDelta": {},
                "analytics": as_dict(data.get("currentAnalytics")),
                "snapshotMetrics": as_dict(data.get("currentMetrics")),
                "snapshot": as_dict(data.get("currentStateSummary")),
                "capturedAtUnix": None,
                "sourceKind": "current",
            }
        ]

    normalized_records: list[dict[str, Any]] = []
    for index, record in enumerate(turn_history):
        analytics = as_dict(path_get(record, "analytics"))
        snapshot_metrics = as_dict(path_get(record, "snapshotMetrics"))
        normalized_records.append(
            {
                "recordSequenceIndex": index,
                "turn": to_int(
                    record.get("committedTurnNumber"),
                    to_int(
                        snapshot_metrics.get("turnNumber"),
                        to_int(analytics.get("turnNumber"), index + 1),
                    ),
                ),
                "committedActiveKingdomKey": key_or_fallback(
                    record.get("committedActiveKingdomKey"),
                    key_or_fallback(snapshot_metrics.get("activeKingdomKey"), "unknown"),
                ),
                "structuredEvents": as_list(record.get("structuredEvents")),
                "commandAuditTrail": as_list(record.get("commandAuditTrail")),
                "behavioralTelemetry": as_dict(record.get("behavioralTelemetry")),
                "xpAuditTrail": as_list(record.get("xpAuditTrail")),
                "turnDelta": as_dict(record.get("turnDelta")),
                "analytics": analytics,
                "snapshotMetrics": snapshot_metrics,
                "snapshot": as_dict(record.get("snapshot")),
                "capturedAtUnix": to_int(record.get("capturedAtUnix"), None),
                "sourceKind": "history",
            }
        )

    return normalized_records


def build_metadata(
    data: dict[str, Any], input_path: Path, turn_records: list[dict[str, Any]]
) -> dict[str, Any]:
    provenance = as_dict(data.get("provenance"))
    current_state = as_dict(data.get("currentStateSummary"))
    current_metrics = as_dict(data.get("currentMetrics"))
    last_record = turn_records[-1] if turn_records else None

    return {
        "analysisVersion": ANALYSIS_VERSION,
        "generatedAtUtc": datetime.now(timezone.utc).isoformat(),
        "schemaVersion": to_int(data.get("schemaVersion"), None),
        "inputFileName": input_path.name,
        "inputFileSizeBytes": input_path.stat().st_size,
        "recordCount": len(turn_records),
        "finalTurn": last_record["turn"] if last_record else 0,
        "historyContinuityComplete": bool(data.get("historyContinuityComplete")),
        "loadedFromExistingCompanion": bool(data.get("loadedFromExistingCompanion")),
        "saveName": key_or_fallback(
            data.get("saveName"), key_or_fallback(current_state.get("gameName"), "unknown")
        ),
        "generator": key_or_fallback(provenance.get("generator"), "unknown"),
        "formatFamily": key_or_fallback(provenance.get("formatFamily"), "unknown"),
        "behavioralTelemetryEnabled": bool(
            current_state.get("behavioralTelemetryEnabled")
            or path_get(data, "sessionContext", "options", "behavioralTelemetryEnabled")
        ),
        "dataCollectionEnabled": bool(
            current_state.get("dataCollectionEnabled")
            or path_get(data, "sessionContext", "options", "dataCollectionEnabled")
        ),
        "currentActiveKingdomKey": key_or_fallback(
            current_metrics.get("activeKingdomKey"),
            last_record["committedActiveKingdomKey"] if last_record else "unknown",
        ),
    }


def build_source(
    data: dict[str, Any], input_path: Path, turn_records: list[dict[str, Any]]
) -> dict[str, Any]:
    session_context = as_dict(data.get("sessionContext"))
    multiplayer = as_dict(session_context.get("multiplayer"))
    participants = as_list(session_context.get("participants"))

    return {
        "path": str(input_path),
        "sessionContext": {
            "gameModeKey": key_or_fallback(session_context.get("gameModeKey"), "unknown"),
            "participants": participants,
            "multiplayer": multiplayer,
            "options": as_dict(session_context.get("options")),
        },
        "topLevelKeys": sorted(data.keys()),
        "turnRange": {
            "first": turn_records[0]["turn"] if turn_records else 0,
            "last": turn_records[-1]["turn"] if turn_records else 0,
        },
    }


def build_config_snapshot(data: dict[str, Any]) -> dict[str, Any]:
    config = as_dict(data.get("configContext"))
    return {
        "map": as_dict(config.get("map")),
        "economy": as_dict(config.get("economy")),
        "chest": as_dict(config.get("chest")),
        "infernal": as_dict(config.get("infernal")),
        "weather": as_dict(config.get("weather")),
        "xp": as_dict(config.get("xp")),
        "combat": as_dict(config.get("combat")),
    }


def build_reference_snapshot(data: dict[str, Any]) -> dict[str, Any]:
    reference = as_dict(data.get("referenceData"))
    summary: dict[str, Any] = {}
    for key, value in sorted(reference.items()):
        entries = as_list(value)
        summary[key] = {
            "count": len(entries),
            "keys": [item.get("key") for item in entries if isinstance(item, dict) and item.get("key")],
        }
    return {
        "catalogs": summary,
        "catalogCount": len(summary),
    }


def build_match_summary(
    data: dict[str, Any],
    turn_records: list[dict[str, Any]],
    activity: dict[str, Any],
    behavior: dict[str, Any],
) -> dict[str, Any]:
    current_metrics = as_dict(data.get("currentMetrics"))
    event_counter = Counter()
    command_counter = Counter()
    xp_total = 0
    xp_sources = Counter()
    total_behavior_events = 0

    for record in turn_records:
        for event in record["structuredEvents"]:
            if isinstance(event, dict):
                event_counter[key_or_fallback(event.get("typeKey"), "unknown")] += 1
        for entry in record["commandAuditTrail"]:
            if isinstance(entry, dict):
                command_counter[key_or_fallback(entry.get("actionKey"), "unknown")] += 1
        for grant in record["xpAuditTrail"]:
            if not isinstance(grant, dict):
                continue
            xp_total += to_int(grant.get("amount"), 0)
            xp_sources[key_or_fallback(grant.get("sourceKey"), "unknown")] += 1
        telemetry = record["behavioralTelemetry"]
        total_behavior_events += len(as_list(telemetry.get("interactionTimeline")))
        total_behavior_events += len(as_list(telemetry.get("orchestrationEvents")))

    turn_durations = extract_turn_durations(turn_records)
    return {
        "recordCount": len(turn_records),
        "firstTurn": turn_records[0]["turn"] if turn_records else 0,
        "finalTurn": turn_records[-1]["turn"] if turn_records else 0,
        "structuredEventCount": sum(event_counter.values()),
        "eventCountsByType": counter_rows(event_counter),
        "commandAuditCountsByAction": counter_rows(command_counter),
        "xp": {
            "totalAmount": xp_total,
            "grantCount": sum(xp_sources.values()),
            "grantCountsBySource": counter_rows(xp_sources),
        },
        "averageTurnDurationMs": safe_mean(turn_durations),
        "telemetryEventCount": total_behavior_events,
        "recordedEventCount": to_int(
            current_metrics.get("recordedEventCount"), sum(event_counter.values())
        ),
        "activity": activity["summary"],
        "behaviorSummary": behavior["summary"],
    }


def build_timelines(
    turn_records: list[dict[str, Any]], activity: dict[str, Any]
) -> dict[str, Any]:
    sequence_rows = []
    kingdom_gold_series: dict[str, list[dict[str, Any]]] = defaultdict(list)
    kingdom_budget_series: dict[str, list[dict[str, Any]]] = defaultdict(list)

    for record in turn_records:
        analytics = record["analytics"]
        weather = as_dict(analytics.get("weather"))
        visibility = as_dict(analytics.get("visibility"))
        chest = as_dict(analytics.get("chest"))
        infernal = as_dict(analytics.get("infernal"))
        turn_delta = record["turnDelta"]
        budgets = as_dict(turn_delta.get("budgets"))
        economy = as_dict(turn_delta.get("economy"))
        economy_rows = as_list(economy.get("byKingdom"))

        sequence_rows.append(
            {
                "sequenceIndex": record["recordSequenceIndex"],
                "turn": record["turn"],
                "committedActiveKingdomKey": record["committedActiveKingdomKey"],
                "structuredEventCount": len(record["structuredEvents"]),
                "commandAuditCount": len(record["commandAuditTrail"]),
                "xpGrantCount": len(record["xpAuditTrail"]),
                "weather": {
                    "frontCount": to_int(weather.get("frontCount"), 0),
                    "nextSpawnTurnStep": to_int(weather.get("nextSpawnTurnStep"), 0),
                    "fogCellCount": to_int(visibility.get("fogCellCount"), 0),
                    "concealingFogCellCount": to_int(
                        visibility.get("concealingFogCellCount"), 0
                    ),
                },
                "chest": {
                    "activeChestObjectId": to_int(chest.get("activeChestObjectId"), -1),
                    "nextSpawnTurn": to_int(chest.get("nextSpawnTurn"), 0),
                    "rewardGeneration": to_int(
                        path_get(chest, "lootProgression", "currentRewardGeneration"), 0
                    ),
                },
                "infernal": {
                    "activeInfernalUnitId": to_int(infernal.get("activeInfernalUnitId"), -1),
                    "nextSpawnTurn": to_int(infernal.get("nextSpawnTurn"), 0),
                    "whiteBloodDebt": to_int(infernal.get("whiteBloodDebt"), 0),
                    "blackBloodDebt": to_int(infernal.get("blackBloodDebt"), 0),
                },
                "budgets": {
                    "movementPointsMax": to_int(budgets.get("movementPointsMax"), 0),
                    "movementPointsSpent": to_int(budgets.get("movementPointsSpent"), 0),
                    "buildPointsMax": to_int(budgets.get("buildPointsMax"), 0),
                    "buildPointsSpent": to_int(budgets.get("buildPointsSpent"), 0),
                },
            }
        )

        for economy_row in economy_rows:
            if not isinstance(economy_row, dict):
                continue
            kingdom_key = key_or_fallback(economy_row.get("kingdomKey"), "unknown")
            kingdom_gold_series[kingdom_key].append(
                {
                    "sequenceIndex": record["recordSequenceIndex"],
                    "turn": record["turn"],
                    "goldBefore": to_int(economy_row.get("goldBefore"), 0),
                    "goldAfter": to_int(economy_row.get("goldAfter"), 0),
                    "goldDelta": to_int(economy_row.get("goldDelta"), 0),
                }
            )
            kingdom_budget_series[kingdom_key].append(
                {
                    "sequenceIndex": record["recordSequenceIndex"],
                    "turn": record["turn"],
                    "movementPointsMaxBonusAfter": to_int(
                        economy_row.get("movementPointsMaxBonusAfter"), 0
                    ),
                    "buildPointsMaxBonusAfter": to_int(
                        economy_row.get("buildPointsMaxBonusAfter"), 0
                    ),
                }
            )

    return {
        "sequence": sequence_rows,
        "economyByKingdom": dict(kingdom_gold_series),
        "budgetBonusesByKingdom": dict(kingdom_budget_series),
        "activity": activity,
    }


def build_event_markers(turn_records: list[dict[str, Any]]) -> dict[str, Any]:
    markers: dict[str, list[dict[str, Any]]] = defaultdict(list)

    for record in turn_records:
        for event in record["structuredEvents"]:
            if not isinstance(event, dict):
                continue
            event_key = key_or_fallback(event.get("typeKey"), "unknown")
            marker = {
                "sequenceIndex": record["recordSequenceIndex"],
                "turn": record["turn"],
                "eventKey": event_key,
                "kingdomKey": key_or_fallback(
                    event.get("kingdomKey"), record["committedActiveKingdomKey"]
                ),
            }
            if event_key == "chest_opened" and isinstance(event.get("reward"), dict):
                reward = as_dict(event.get("reward"))
                marker["reward"] = {
                    "typeKey": key_or_fallback(reward.get("typeKey"), "unknown"),
                    "amount": to_int(reward.get("amount"), 0),
                }
            if event_key in {
                "weather_front_spawned",
                "weather_front_ended",
                "chest_spawned",
                "chest_opened",
                "infernal_spawned",
                "infernal_removed",
                "xp_granted",
                "piece_upgraded",
            }:
                markers[event_key].append(marker)

    return dict(markers)


def build_systems(data: dict[str, Any], turn_records: list[dict[str, Any]]) -> dict[str, Any]:
    return {
        "economy": build_economy_system(turn_records),
        "weather": build_weather_system(turn_records),
        "chest": build_chest_system(data, turn_records),
        "infernal": build_infernal_system(turn_records),
        "xp": build_xp_system(data, turn_records),
    }


def build_economy_system(turn_records: list[dict[str, Any]]) -> dict[str, Any]:
    rows = []
    public_resource_rows = []
    for record in turn_records:
        analytics = record["analytics"]
        economy = as_dict(analytics.get("economy"))
        for kingdom_row in as_list(economy.get("byKingdom")):
            if not isinstance(kingdom_row, dict):
                continue
            rows.append(
                {
                    "sequenceIndex": record["recordSequenceIndex"],
                    "turn": record["turn"],
                    "kingdomKey": key_or_fallback(kingdom_row.get("kingdomKey"), "unknown"),
                    "gold": to_int(kingdom_row.get("gold"), 0),
                    "grossIncome": to_int(kingdom_row.get("grossIncome"), 0),
                    "upkeepCost": to_int(kingdom_row.get("upkeepCost"), 0),
                    "netIncome": to_int(kingdom_row.get("netIncome"), 0),
                    "projectedEndingGold": to_int(kingdom_row.get("projectedEndingGold"), 0),
                    "wouldBeBankrupt": bool(kingdom_row.get("wouldBeBankrupt")),
                }
            )
        for building_row in as_list(economy.get("publicResourceBuildings")):
            if not isinstance(building_row, dict):
                continue
            public_resource_rows.append(
                {
                    "sequenceIndex": record["recordSequenceIndex"],
                    "turn": record["turn"],
                    "buildingId": to_int(building_row.get("buildingId"), -1),
                    "buildingTypeKey": key_or_fallback(
                        building_row.get("buildingTypeKey"), "unknown"
                    ),
                    "whiteIncome": to_int(building_row.get("whiteIncome"), 0),
                    "blackIncome": to_int(building_row.get("blackIncome"), 0),
                    "whiteOccupiedCells": to_int(building_row.get("whiteOccupiedCells"), 0),
                    "blackOccupiedCells": to_int(building_row.get("blackOccupiedCells"), 0),
                }
            )
    return {
        "byKingdom": rows,
        "publicResourceBuildings": public_resource_rows,
    }


def build_weather_system(turn_records: list[dict[str, Any]]) -> dict[str, Any]:
    points = []
    spawn_events = []
    end_events = []
    directions = Counter()

    for record in turn_records:
        analytics = record["analytics"]
        visibility = as_dict(analytics.get("visibility"))
        weather = as_dict(analytics.get("weather"))
        observers = index_by_key(as_list(visibility.get("byObserver")), "observerKingdomKey")
        white_observer = as_dict(observers.get("white"))
        black_observer = as_dict(observers.get("black"))

        points.append(
            {
                "sequenceIndex": record["recordSequenceIndex"],
                "turn": record["turn"],
                "fogCellCount": to_int(visibility.get("fogCellCount"), 0),
                "concealingFogCellCount": to_int(
                    visibility.get("concealingFogCellCount"), 0
                ),
                "frontCount": to_int(weather.get("frontCount"), 0),
                "hasActiveFront": bool(visibility.get("hasActiveFront")),
                "whiteHiddenEnemyPieces": len(as_list(white_observer.get("hiddenEnemyPieceIds"))),
                "blackHiddenEnemyPieces": len(as_list(black_observer.get("hiddenEnemyPieceIds"))),
                "weatherRngCounter": to_int(weather.get("rngCounter"), 0),
                "nextSpawnTurnStep": to_int(weather.get("nextSpawnTurnStep"), 0),
            }
        )

        for event in record["structuredEvents"]:
            if not isinstance(event, dict):
                continue
            if event.get("typeKey") not in {"weather_front_spawned", "weather_front_ended"}:
                continue
            front = as_dict(event.get("front"))
            direction_key = key_or_fallback(front.get("directionKey"), "unknown")
            directions[direction_key] += 1
            row = {
                "sequenceIndex": record["recordSequenceIndex"],
                "turn": record["turn"],
                "directionKey": direction_key,
                "shapeSeed": to_int(front.get("shapeSeed"), None),
                "densitySeed": to_int(front.get("densitySeed"), None),
            }
            if event.get("typeKey") == "weather_front_spawned":
                spawn_events.append(row)
            else:
                end_events.append(row)

    active_turns = [point for point in points if point["hasActiveFront"]]
    return {
        "points": points,
        "spawnEvents": spawn_events,
        "endEvents": end_events,
        "directionCounts": counter_rows(directions),
        "summary": {
            "averageFogCellCount": safe_mean([point["fogCellCount"] for point in points]),
            "averageConcealingFogCellCount": safe_mean(
                [point["concealingFogCellCount"] for point in points]
            ),
            "maxSimultaneousFronts": max_or_default(
                [point["frontCount"] for point in points], 0
            ),
            "activeFrontRecordCount": len(active_turns),
        },
    }


def build_chest_system(
    data: dict[str, Any], turn_records: list[dict[str, Any]]
) -> dict[str, Any]:
    config = as_dict(path_get(data, "configContext", "chest"))
    points = []
    spawns = []
    opens = []
    reward_types = Counter()

    for record in turn_records:
        chest = as_dict(path_get(record, "analytics", "chest"))
        loot_progression = as_dict(chest.get("lootProgression"))
        points.append(
            {
                "sequenceIndex": record["recordSequenceIndex"],
                "turn": record["turn"],
                "activeChestObjectId": to_int(chest.get("activeChestObjectId"), -1),
                "nextSpawnTurn": to_int(chest.get("nextSpawnTurn"), 0),
                "rngCounter": to_int(chest.get("rngCounter"), 0),
                "rewardRngCounter": to_int(chest.get("rewardRngCounter"), 0),
                "hasCurrentReward": bool(loot_progression.get("hasCurrentReward")),
                "currentRewardGeneration": to_int(
                    loot_progression.get("currentRewardGeneration"), 0
                ),
            }
        )

        for event in record["structuredEvents"]:
            if not isinstance(event, dict):
                continue
            event_key = key_or_fallback(event.get("typeKey"), "unknown")
            if event_key == "chest_spawned":
                spawns.append(
                    {
                        "sequenceIndex": record["recordSequenceIndex"],
                        "turn": record["turn"],
                        "position": as_dict(event.get("position")),
                    }
                )
            if event_key == "chest_opened":
                reward = as_dict(event.get("reward"))
                reward_type = key_or_fallback(reward.get("typeKey"), "unknown")
                reward_types[reward_type] += 1
                opens.append(
                    {
                        "sequenceIndex": record["recordSequenceIndex"],
                        "turn": record["turn"],
                        "kingdomKey": key_or_fallback(event.get("kingdomKey"), "unknown"),
                        "rewardTypeKey": reward_type,
                        "rewardAmount": to_int(reward.get("amount"), 0),
                    }
                )

    return {
        "config": config,
        "points": points,
        "spawnEvents": spawns,
        "openEvents": opens,
        "rewardTypeCounts": counter_rows(reward_types),
        "summary": {
            "spawnCount": len(spawns),
            "openCount": len(opens),
            "configuredCatchUpEnabled": bool(config.get("currentLootCatchUpEnabled")),
        },
    }


def build_infernal_system(turn_records: list[dict[str, Any]]) -> dict[str, Any]:
    points = []
    spawns = []
    removals = []
    target_kingdoms = Counter()
    manifested_piece_types = Counter()
    active_spans: dict[str, dict[str, Any]] = {}
    completed_spans: list[dict[str, Any]] = []

    for record in turn_records:
        infernal = as_dict(path_get(record, "analytics", "infernal"))
        autonomous_units = as_list(path_get(record, "analytics", "entities", "autonomousUnitIndex"))
        autonomous_by_id = index_by_int_id(autonomous_units)

        points.append(
            {
                "sequenceIndex": record["recordSequenceIndex"],
                "turn": record["turn"],
                "whiteBloodDebt": to_int(infernal.get("whiteBloodDebt"), 0),
                "blackBloodDebt": to_int(infernal.get("blackBloodDebt"), 0),
                "activeInfernalUnitId": to_int(infernal.get("activeInfernalUnitId"), -1),
                "nextSpawnTurn": to_int(infernal.get("nextSpawnTurn"), 0),
                "rngCounter": to_int(infernal.get("rngCounter"), 0),
                "activeInfernalCount": len(autonomous_units),
            }
        )

        for event in record["structuredEvents"]:
            if not isinstance(event, dict):
                continue
            event_key = key_or_fallback(event.get("typeKey"), "unknown")
            if event_key == "infernal_spawned":
                unit_id = to_int(event.get("unitId"), -1)
                unit = as_dict(autonomous_by_id.get(unit_id))
                infernal_state = as_dict(unit.get("infernal"))
                target_kingdom_key = key_or_fallback(
                    infernal_state.get("targetKingdomKey"), "unknown"
                )
                manifested_piece_key = key_or_fallback(
                    infernal_state.get("manifestedPieceTypeKey"), "unknown"
                )
                row = {
                    "sequenceIndex": record["recordSequenceIndex"],
                    "turn": record["turn"],
                    "unitId": unit_id,
                    "targetKingdomKey": target_kingdom_key,
                    "manifestedPieceTypeKey": manifested_piece_key,
                }
                target_kingdoms[target_kingdom_key] += 1
                manifested_piece_types[manifested_piece_key] += 1
                spawns.append(row)
                active_spans[str(unit_id)] = {
                    "unitId": unit_id,
                    "spawnTurn": record["turn"],
                    "targetKingdomKey": target_kingdom_key,
                    "manifestedPieceTypeKey": manifested_piece_key,
                    "removedTurn": None,
                }
            elif event_key == "infernal_removed":
                unit_id = str(to_int(event.get("unitId"), -1))
                removals.append(
                    {
                        "sequenceIndex": record["recordSequenceIndex"],
                        "turn": record["turn"],
                        "unitId": to_int(event.get("unitId"), -1),
                    }
                )
                if unit_id in active_spans:
                    active_spans[unit_id]["removedTurn"] = record["turn"]
                    completed_spans.append(active_spans[unit_id])
                    del active_spans[unit_id]

    last_turn = turn_records[-1]["turn"] if turn_records else 0
    lifespan_rows = []
    for span in completed_spans + list(active_spans.values()):
        removed_turn = span["removedTurn"] if span["removedTurn"] is not None else last_turn
        lifespan_rows.append(
            {
                **span,
                "observedLifetime": max(1, removed_turn - span["spawnTurn"] + 1),
            }
        )

    return {
        "points": points,
        "spawnEvents": spawns,
        "removeEvents": removals,
        "lifespans": lifespan_rows,
        "targetKingdomCounts": counter_rows(target_kingdoms),
        "manifestedPieceCounts": counter_rows(manifested_piece_types),
        "summary": {
            "spawnCount": len(spawns),
            "removalCount": len(removals),
            "averageObservedLifetime": safe_mean(
                [row["observedLifetime"] for row in lifespan_rows]
            ),
        },
    }


def build_xp_system(data: dict[str, Any], turn_records: list[dict[str, Any]]) -> dict[str, Any]:
    config_profiles = as_list(path_get(data, "configContext", "xp", "profiles"))
    grants = []
    grants_by_source = Counter()
    amounts_by_source: dict[str, list[int]] = defaultdict(list)

    for record in turn_records:
        for grant in record["xpAuditTrail"]:
            if not isinstance(grant, dict):
                continue
            source_key = key_or_fallback(grant.get("sourceKey"), "unknown")
            amount = to_int(grant.get("amount"), 0)
            grants_by_source[source_key] += 1
            amounts_by_source[source_key].append(amount)
            grants.append(
                {
                    "sequenceIndex": record["recordSequenceIndex"],
                    "turn": record["turn"],
                    "sourceKey": source_key,
                    "pieceId": to_int(grant.get("pieceId"), -1),
                    "amount": amount,
                }
            )

    source_rows = []
    for source_key, counts in sorted(grants_by_source.items()):
        source_rows.append(
            {
                "sourceKey": source_key,
                "grantCount": counts,
                "averageAmount": safe_mean(amounts_by_source[source_key]),
                "minimumAmount": min_or_default(amounts_by_source[source_key], 0),
                "maximumAmount": max_or_default(amounts_by_source[source_key], 0),
            }
        )

    return {
        "configProfiles": config_profiles,
        "grants": grants,
        "bySource": source_rows,
        "summary": {
            "grantCount": len(grants),
            "totalAmount": sum(grant["amount"] for grant in grants),
        },
    }


def build_behavior(turn_records: list[dict[str, Any]]) -> dict[str, Any]:
    command_actions = Counter()
    command_types = Counter()
    stage_counts = Counter()
    origin_counts = Counter()
    event_counts = Counter()
    turn_rows = []
    first_action_delays = []
    queue_delays = []
    commit_delays = []
    interaction_counts = []
    command_audit_counts = []
    total_replace_count = 0
    total_cancel_count = 0

    for record in turn_records:
        interaction_timeline = as_list(path_get(record, "behavioralTelemetry", "interactionTimeline"))
        orchestration_events = as_list(path_get(record, "behavioralTelemetry", "orchestrationEvents"))
        telemetry_events = [
            event for event in interaction_timeline + orchestration_events if isinstance(event, dict)
        ]

        replace_count = 0
        cancel_count = 0
        queue_count = 0
        turn_first_action = None
        turn_commit = None

        for entry in record["commandAuditTrail"]:
            if not isinstance(entry, dict):
                continue
            action_key = key_or_fallback(entry.get("actionKey"), "unknown")
            command_actions[action_key] += 1
            if action_key == "replace":
                replace_count += 1
            if action_key == "cancel":
                cancel_count += 1
            command = as_dict(entry.get("command"))
            command_types[key_or_fallback(command.get("typeKey"), "unknown")] += 1

        for event in telemetry_events:
            stage_key = key_or_fallback(event.get("stageKey"), "unknown")
            origin_key = key_or_fallback(event.get("originKey"), "unknown")
            event_key = key_or_fallback(event.get("eventKey"), "unknown")
            stage_counts[stage_key] += 1
            origin_counts[origin_key] += 1
            event_counts[event_key] += 1
            elapsed = to_int(event.get("turnElapsedMs"), None)
            if elapsed is not None and turn_first_action is None:
                turn_first_action = elapsed
            if elapsed is not None and event_key == "command_queued":
                queue_count += 1
                queue_delays.append(elapsed)
            if elapsed is not None and event_key == "commit_started":
                turn_commit = elapsed

        if turn_first_action is not None:
            first_action_delays.append(turn_first_action)
        if turn_commit is not None:
            commit_delays.append(turn_commit)
        interaction_counts.append(len(interaction_timeline))
        command_audit_counts.append(len(record["commandAuditTrail"]))
        total_replace_count += replace_count
        total_cancel_count += cancel_count

        turn_rows.append(
            {
                "sequenceIndex": record["recordSequenceIndex"],
                "turn": record["turn"],
                "committedActiveKingdomKey": record["committedActiveKingdomKey"],
                "commandAuditCount": len(record["commandAuditTrail"]),
                "interactionEventCount": len(interaction_timeline),
                "orchestrationEventCount": len(orchestration_events),
                "replaceCount": replace_count,
                "cancelCount": cancel_count,
                "queueEventCount": queue_count,
                "firstActionDelayMs": turn_first_action,
                "commitDelayMs": turn_commit,
            }
        )

    return {
        "perTurn": turn_rows,
        "commandActions": counter_rows(command_actions),
        "commandTypes": counter_rows(command_types),
        "telemetryStages": counter_rows(stage_counts),
        "telemetryOrigins": counter_rows(origin_counts),
        "telemetryEvents": counter_rows(event_counts),
        "summary": {
            "averageFirstActionDelayMs": safe_mean(first_action_delays),
            "averageQueueDelayMs": safe_mean(queue_delays),
            "averageCommitDelayMs": safe_mean(commit_delays),
            "averageInteractionEventCount": safe_mean(interaction_counts),
            "averageCommandAuditCount": safe_mean(command_audit_counts),
            "totalReplaceCount": total_replace_count,
            "totalCancelCount": total_cancel_count,
        },
    }


def build_entity_trajectories(turn_records: list[dict[str, Any]]) -> dict[str, Any]:
    piece_trajectories: dict[str, dict[str, Any]] = {}
    building_trajectories: dict[str, dict[str, Any]] = {}
    map_object_trajectories: dict[str, dict[str, Any]] = {}
    autonomous_trajectories: dict[str, dict[str, Any]] = {}

    for record in turn_records:
        analytics = record["analytics"]
        entities = as_dict(analytics.get("entities"))
        ingest_entity_rows(
            piece_trajectories,
            as_list(entities.get("pieceIndex")),
            record,
            capture_piece_state,
        )
        ingest_entity_rows(
            building_trajectories,
            as_list(entities.get("buildingIndex")),
            record,
            capture_building_state,
        )
        ingest_entity_rows(
            map_object_trajectories,
            as_list(entities.get("mapObjectIndex")),
            record,
            capture_map_object_state,
        )
        ingest_entity_rows(
            autonomous_trajectories,
            as_list(entities.get("autonomousUnitIndex")),
            record,
            capture_autonomous_state,
        )

    return {
        "pieces": finalize_trajectories(piece_trajectories),
        "buildings": finalize_trajectories(building_trajectories),
        "mapObjects": finalize_trajectories(map_object_trajectories),
        "autonomousUnits": finalize_trajectories(autonomous_trajectories),
    }


def build_quality_checks(
    data: dict[str, Any], turn_records: list[dict[str, Any]]
) -> dict[str, Any]:
    expected_turn_history_count = len(as_list(data.get("turnHistory")))
    structured_event_count = sum(len(record["structuredEvents"]) for record in turn_records)
    telemetry_record_count = sum(
        1
        for record in turn_records
        if record["behavioralTelemetry"].get("interactionTimeline")
        or record["behavioralTelemetry"].get("orchestrationEvents")
    )
    return {
        "turnHistoryCountMatchesNormalizedRecords": expected_turn_history_count == len(turn_records),
        "normalizedRecordCount": len(turn_records),
        "sourceTurnHistoryCount": expected_turn_history_count,
        "structuredEventCount": structured_event_count,
        "telemetryRecordCount": telemetry_record_count,
        "hasCurrentAnalytics": bool(data.get("currentAnalytics")),
        "hasCurrentMetrics": bool(data.get("currentMetrics")),
    }


def build_limitations(
    data: dict[str, Any], turn_records: list[dict[str, Any]]
) -> list[dict[str, Any]]:
    limitations: list[dict[str, Any]] = []
    if not path_get(data, "sessionContext", "options", "behavioralTelemetryEnabled"):
        limitations.append(
            {
                "scope": "behavior",
                "level": "direct",
                "message": "Behavioral telemetry is disabled or absent, so behavior sections are incomplete.",
            }
        )
    if not any(record["xpAuditTrail"] for record in turn_records):
        limitations.append(
            {
                "scope": "xp",
                "level": "direct",
                "message": "No XP audit entries were found in the current companion.",
            }
        )
    limitations.append(
        {
            "scope": "causality",
            "level": "proxy",
            "message": "Cross-system comparisons are emitted as correlation-ready windows, not as proven causal effects.",
        }
    )
    return limitations


def extract_turn_durations(turn_records: list[dict[str, Any]]) -> list[int]:
    durations: list[int] = []
    for record in turn_records:
        telemetry = record["behavioralTelemetry"]
        telemetry_events = as_list(telemetry.get("interactionTimeline")) + as_list(
            telemetry.get("orchestrationEvents")
        )
        candidates = [
            to_int(event.get("turnElapsedMs"), None)
            for event in telemetry_events
            if isinstance(event, dict)
        ]
        candidates.extend(
            to_int(entry.get("turnElapsedMs"), None)
            for entry in record["commandAuditTrail"]
            if isinstance(entry, dict)
        )
        values = [candidate for candidate in candidates if candidate is not None]
        if values:
            durations.append(max(values))
    return durations


def ingest_entity_rows(
    target: dict[str, dict[str, Any]],
    rows: list[Any],
    record: dict[str, Any],
    state_builder,
) -> None:
    for row in rows:
        if not isinstance(row, dict):
            continue
        entity_id = to_int(row.get("id"), None)
        if entity_id is None:
            continue
        key = str(entity_id)
        entry = target.setdefault(
            key,
            {
                "id": entity_id,
                "firstSeenTurn": record["turn"],
                "lastSeenTurn": record["turn"],
                "states": [],
            },
        )
        entry["lastSeenTurn"] = record["turn"]
        if "typeKey" not in entry:
            if row.get("pieceTypeKey"):
                entry["typeKey"] = row.get("pieceTypeKey")
            elif row.get("buildingTypeKey"):
                entry["typeKey"] = row.get("buildingTypeKey")
            else:
                entry["typeKey"] = row.get("typeKey")
        entry["states"].append(state_builder(row, record))


def finalize_trajectories(trajectories: dict[str, dict[str, Any]]) -> list[dict[str, Any]]:
    rows = []
    for entry in trajectories.values():
        rows.append(
            {
                "id": entry["id"],
                "typeKey": key_or_fallback(entry.get("typeKey"), "unknown"),
                "firstSeenTurn": entry["firstSeenTurn"],
                "lastSeenTurn": entry["lastSeenTurn"],
                "stateCount": len(entry["states"]),
                "states": entry["states"],
            }
        )
    rows.sort(key=lambda row: (row["typeKey"], row["id"]))
    return rows


def capture_piece_state(row: dict[str, Any], record: dict[str, Any]) -> dict[str, Any]:
    return {
        "sequenceIndex": record["recordSequenceIndex"],
        "turn": record["turn"],
        "kingdomKey": key_or_fallback(row.get("kingdomKey"), "unknown"),
        "position": as_dict(row.get("position")),
        "xp": to_int(row.get("xp"), 0),
        "formationId": to_int(row.get("formationId"), -1),
        "hiddenFromWhite": bool(row.get("hiddenFromWhite")),
        "hiddenFromBlack": bool(row.get("hiddenFromBlack")),
    }


def capture_building_state(row: dict[str, Any], record: dict[str, Any]) -> dict[str, Any]:
    return {
        "sequenceIndex": record["recordSequenceIndex"],
        "turn": record["turn"],
        "ownerKingdomKey": key_or_fallback(row.get("ownerKingdomKey"), "unknown"),
        "isPublic": bool(row.get("isPublic")),
        "isNeutral": bool(row.get("isNeutral")),
        "destroyed": bool(row.get("destroyed")),
        "destroyedCellCount": to_int(row.get("destroyedCellCount"), 0),
        "isProducing": bool(row.get("isProducing")),
        "turnsRemaining": to_int(row.get("turnsRemaining"), 0),
        "origin": as_dict(row.get("origin")),
    }


def capture_map_object_state(row: dict[str, Any], record: dict[str, Any]) -> dict[str, Any]:
    return {
        "sequenceIndex": record["recordSequenceIndex"],
        "turn": record["turn"],
        "position": as_dict(row.get("position")),
        "isActiveChest": bool(row.get("isActiveChest")),
        "chest": as_dict(row.get("chest")),
    }


def capture_autonomous_state(row: dict[str, Any], record: dict[str, Any]) -> dict[str, Any]:
    return {
        "sequenceIndex": record["recordSequenceIndex"],
        "turn": record["turn"],
        "position": as_dict(row.get("position")),
        "infernal": as_dict(row.get("infernal")),
        "hiddenFromWhite": bool(row.get("hiddenFromWhite")),
        "hiddenFromBlack": bool(row.get("hiddenFromBlack")),
    }


def as_dict(value: Any) -> dict[str, Any]:
    return value if isinstance(value, dict) else {}


def as_list(value: Any) -> list[Any]:
    return value if isinstance(value, list) else []


def key_or_fallback(value: Any, fallback: str) -> str:
    if isinstance(value, str) and value:
        return value
    return fallback


def path_get(value: Any, *path: str) -> Any:
    current = value
    for key in path:
        if not isinstance(current, dict):
            return None
        current = current.get(key)
    return current


def to_int(value: Any, default: int | None) -> int | None:
    if value is None:
        return default
    if isinstance(value, bool):
        return int(value)
    if isinstance(value, (int, float)):
        return int(value)
    if isinstance(value, str):
        try:
            return int(float(value))
        except ValueError:
            return default
    return default


def safe_mean(values: list[int | float]) -> float | None:
    if not values:
        return None
    return float(mean(values))


def counter_rows(counter: Counter[str]) -> list[dict[str, Any]]:
    return [
        {"key": key, "count": count}
        for key, count in sorted(counter.items(), key=lambda item: (-item[1], item[0]))
    ]


def build_frequency_rows(values: list[int]) -> list[dict[str, int]]:
    counter = Counter(values)
    return [
        {"value": value, "count": count}
        for value, count in sorted(counter.items(), key=lambda item: item[0])
    ]


def index_by_key(rows: list[Any], key_name: str) -> dict[str, Any]:
    index: dict[str, Any] = {}
    for row in rows:
        if not isinstance(row, dict):
            continue
        key = row.get(key_name)
        if isinstance(key, str) and key:
            index[key] = row
    return index


def index_by_int_id(rows: list[Any]) -> dict[int, Any]:
    index: dict[int, Any] = {}
    for row in rows:
        if not isinstance(row, dict):
            continue
        row_id = to_int(row.get("id"), None)
        if row_id is not None:
            index[row_id] = row
    return index


def max_or_default(values: list[int], default: int) -> int:
    return max(values) if values else default


def min_or_default(values: list[int], default: int) -> int:
    return min(values) if values else default