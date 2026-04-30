from __future__ import annotations

from collections import Counter, defaultdict
from statistics import mean
from typing import Any


def build_activity_metrics(turn_records: list[dict[str, Any]]) -> dict[str, Any]:
    move_totals: dict[str, int] = defaultdict(int)
    build_totals: dict[str, int] = defaultdict(int)
    kill_totals: dict[str, int] = defaultdict(int)
    upgrade_totals: dict[str, int] = defaultdict(int)

    move_points = []
    build_points = []
    kill_points = []
    upgrade_points = []
    move_reward_markers = []
    build_reward_markers = []
    per_record = []

    for record in turn_records:
        sequence_index = record["recordSequenceIndex"]
        turn = record["turn"]
        committed_active_kingdom_key = record["committedActiveKingdomKey"]

        move_increments: Counter[str] = Counter()
        build_increments: Counter[str] = Counter()
        kill_increments: Counter[str] = Counter()
        upgrade_increments: Counter[str] = Counter()
        chest_reward_type_keys = []
        chest_reward_amounts = []

        for event in as_list(record.get("structuredEvents")):
            if not isinstance(event, dict):
                continue

            event_key = key_or_fallback(event.get("typeKey"), "unknown")
            kingdom_key = key_or_fallback(event.get("kingdomKey"), committed_active_kingdom_key)

            if event_key == "piece_moved":
                move_increments[kingdom_key] += 1
            elif event_key == "building_placed":
                build_increments[kingdom_key] += 1
            elif event_key == "piece_removed" and key_or_fallback(event.get("cause"), "unknown") == "captured":
                kill_increments[committed_active_kingdom_key] += 1
            elif event_key == "piece_upgraded":
                upgrade_increments[kingdom_key] += 1
            elif event_key == "chest_opened":
                reward = as_dict(event.get("reward"))
                reward_type_key = key_or_fallback(reward.get("typeKey"), "unknown")
                reward_amount = to_int(reward.get("amount"), 0)
                chest_reward_type_keys.append(reward_type_key)
                chest_reward_amounts.append(reward_amount)
                if reward_type_key == "movement_points_max_bonus":
                    move_reward_markers.append(
                        {
                            "sequenceIndex": sequence_index,
                            "turn": turn,
                            "kingdomKey": kingdom_key,
                            "rewardTypeKey": reward_type_key,
                            "amount": reward_amount,
                        }
                    )
                elif reward_type_key == "build_points_max_bonus":
                    build_reward_markers.append(
                        {
                            "sequenceIndex": sequence_index,
                            "turn": turn,
                            "kingdomKey": kingdom_key,
                            "rewardTypeKey": reward_type_key,
                            "amount": reward_amount,
                        }
                    )

        for kingdom_key, increment in move_increments.items():
            move_totals[kingdom_key] += increment
        for kingdom_key, increment in build_increments.items():
            build_totals[kingdom_key] += increment
        for kingdom_key, increment in kill_increments.items():
            kill_totals[kingdom_key] += increment
        for kingdom_key, increment in upgrade_increments.items():
            upgrade_totals[kingdom_key] += increment

        move_points.append(
            {
                "sequenceIndex": sequence_index,
                "turn": turn,
                "moveCountThisRecord": sum(move_increments.values()),
                "whiteMoves": move_totals.get("white", 0),
                "blackMoves": move_totals.get("black", 0),
                "totalMoves": sum(move_totals.values()),
                "byKingdom": sorted_counter_dict(move_totals),
            }
        )
        build_points.append(
            {
                "sequenceIndex": sequence_index,
                "turn": turn,
                "buildCountThisRecord": sum(build_increments.values()),
                "whiteBuilds": build_totals.get("white", 0),
                "blackBuilds": build_totals.get("black", 0),
                "totalBuilds": sum(build_totals.values()),
                "byKingdom": sorted_counter_dict(build_totals),
            }
        )
        kill_points.append(
            {
                "sequenceIndex": sequence_index,
                "turn": turn,
                "killCountThisRecord": sum(kill_increments.values()),
                "whiteKills": kill_totals.get("white", 0),
                "blackKills": kill_totals.get("black", 0),
                "totalKills": sum(kill_totals.values()),
                "byKingdom": sorted_counter_dict(kill_totals),
            }
        )
        upgrade_points.append(
            {
                "sequenceIndex": sequence_index,
                "turn": turn,
                "upgradeCountThisRecord": sum(upgrade_increments.values()),
                "whiteUpgrades": upgrade_totals.get("white", 0),
                "blackUpgrades": upgrade_totals.get("black", 0),
                "totalUpgrades": sum(upgrade_totals.values()),
                "byKingdom": sorted_counter_dict(upgrade_totals),
            }
        )
        per_record.append(
            {
                "sequenceIndex": sequence_index,
                "turn": turn,
                "committedActiveKingdomKey": committed_active_kingdom_key,
                "moveCount": sum(move_increments.values()),
                "buildCount": sum(build_increments.values()),
                "killCount": sum(kill_increments.values()),
                "upgradeCount": sum(upgrade_increments.values()),
                "chestRewardTypeKeys": chest_reward_type_keys,
                "chestRewardAmounts": chest_reward_amounts,
            }
        )

    return {
        "perRecord": per_record,
        "moves": {
            "points": move_points,
            "rewardMarkers": move_reward_markers,
            "totalMoves": sum(move_totals.values()),
            "byKingdomTotals": sorted_counter_dict(move_totals),
        },
        "builds": {
            "points": build_points,
            "rewardMarkers": build_reward_markers,
            "totalBuilds": sum(build_totals.values()),
            "byKingdomTotals": sorted_counter_dict(build_totals),
        },
        "kills": {
            "points": kill_points,
            "totalKills": sum(kill_totals.values()),
            "byKingdomTotals": sorted_counter_dict(kill_totals),
        },
        "upgrades": {
            "points": upgrade_points,
            "totalUpgrades": sum(upgrade_totals.values()),
            "byKingdomTotals": sorted_counter_dict(upgrade_totals),
        },
        "summary": {
            "totalMoves": sum(move_totals.values()),
            "totalBuilds": sum(build_totals.values()),
            "totalKills": sum(kill_totals.values()),
            "totalUpgrades": sum(upgrade_totals.values()),
        },
    }


def build_record_feature_rows(
    turn_records: list[dict[str, Any]],
    activity: dict[str, Any],
    behavior: dict[str, Any],
) -> list[dict[str, Any]]:
    activity_rows = index_rows_by_sequence(as_list(activity.get("perRecord")))
    behavior_rows = index_rows_by_sequence(as_list(behavior.get("perTurn")))
    feature_rows = []

    for record in turn_records:
        sequence_index = record["recordSequenceIndex"]
        activity_row = as_dict(activity_rows.get(sequence_index))
        behavior_row = as_dict(behavior_rows.get(sequence_index))
        analytics = as_dict(record.get("analytics"))
        visibility = as_dict(analytics.get("visibility"))
        weather = as_dict(analytics.get("weather"))
        infernal = as_dict(analytics.get("infernal"))
        budgets = as_dict(as_dict(record.get("turnDelta")).get("budgets"))

        observers = index_by_key(as_list(visibility.get("byObserver")), "observerKingdomKey")
        white_observer = as_dict(observers.get("white"))
        black_observer = as_dict(observers.get("black"))

        event_counter: Counter[str] = Counter()
        for event in as_list(record.get("structuredEvents")):
            if not isinstance(event, dict):
                continue
            event_counter[key_or_fallback(event.get("typeKey"), "unknown")] += 1

        chest_reward_type_keys = as_list(activity_row.get("chestRewardTypeKeys"))
        chest_reward_amounts = as_list(activity_row.get("chestRewardAmounts"))
        active_infernal_unit_id = to_int(infernal.get("activeInfernalUnitId"), -1)
        feature_rows.append(
            {
                "sequenceIndex": sequence_index,
                "turn": record["turn"],
                "committedActiveKingdomKey": record["committedActiveKingdomKey"],
                "movementPointsSpent": to_int(budgets.get("movementPointsSpent"), 0),
                "buildPointsSpent": to_int(budgets.get("buildPointsSpent"), 0),
                "movementPointsMax": to_int(budgets.get("movementPointsMax"), 0),
                "buildPointsMax": to_int(budgets.get("buildPointsMax"), 0),
                "moveCount": to_int(activity_row.get("moveCount"), 0),
                "buildCount": to_int(activity_row.get("buildCount"), 0),
                "killCount": to_int(activity_row.get("killCount"), 0),
                "upgradeCount": to_int(activity_row.get("upgradeCount"), 0),
                "commandAuditCount": to_int(behavior_row.get("commandAuditCount"), 0),
                "interactionEventCount": to_int(behavior_row.get("interactionEventCount"), 0),
                "orchestrationEventCount": to_int(behavior_row.get("orchestrationEventCount"), 0),
                "replaceCount": to_int(behavior_row.get("replaceCount"), 0),
                "cancelCount": to_int(behavior_row.get("cancelCount"), 0),
                "queueEventCount": to_int(behavior_row.get("queueEventCount"), 0),
                "firstActionDelayMs": to_int(behavior_row.get("firstActionDelayMs"), None),
                "commitDelayMs": to_int(behavior_row.get("commitDelayMs"), None),
                "hasActiveFront": bool(visibility.get("hasActiveFront")),
                "frontCount": to_int(weather.get("frontCount"), 0),
                "fogCellCount": to_int(visibility.get("fogCellCount"), 0),
                "concealingFogCellCount": to_int(visibility.get("concealingFogCellCount"), 0),
                "whiteHiddenEnemyPieces": len(as_list(white_observer.get("hiddenEnemyPieceIds"))),
                "blackHiddenEnemyPieces": len(as_list(black_observer.get("hiddenEnemyPieceIds"))),
                "totalHiddenEnemyPieces": len(as_list(white_observer.get("hiddenEnemyPieceIds")))
                + len(as_list(black_observer.get("hiddenEnemyPieceIds"))),
                "weatherFrontSpawnedCount": event_counter.get("weather_front_spawned", 0),
                "weatherFrontEndedCount": event_counter.get("weather_front_ended", 0),
                "chestSpawnedCount": event_counter.get("chest_spawned", 0),
                "chestOpenedCount": event_counter.get("chest_opened", 0),
                "chestRewardTypeKeys": chest_reward_type_keys,
                "chestRewardAmounts": chest_reward_amounts,
                "infernalSpawnedCount": event_counter.get("infernal_spawned", 0),
                "infernalRemovedCount": event_counter.get("infernal_removed", 0),
                "infernalMovedCount": event_counter.get("infernal_moved", 0),
                "hasActiveInfernal": active_infernal_unit_id >= 0,
                "activeInfernalUnitId": active_infernal_unit_id,
                "whiteBloodDebt": to_int(infernal.get("whiteBloodDebt"), 0),
                "blackBloodDebt": to_int(infernal.get("blackBloodDebt"), 0),
                "totalBloodDebt": to_int(infernal.get("whiteBloodDebt"), 0)
                + to_int(infernal.get("blackBloodDebt"), 0),
                "structuredEventCount": len(as_list(record.get("structuredEvents"))),
            }
        )

    return feature_rows


def build_feature_distributions(feature_rows: list[dict[str, Any]]) -> dict[str, Any]:
    return {
        "movementSpentPerRecord": build_frequency_rows(
            [row["movementPointsSpent"] for row in feature_rows]
        ),
        "buildSpentPerRecord": build_frequency_rows(
            [row["buildPointsSpent"] for row in feature_rows]
        ),
        "moveCountPerRecord": build_frequency_rows([row["moveCount"] for row in feature_rows]),
        "buildCountPerRecord": build_frequency_rows([row["buildCount"] for row in feature_rows]),
        "killCountPerRecord": build_frequency_rows([row["killCount"] for row in feature_rows]),
        "upgradeCountPerRecord": build_frequency_rows(
            [row["upgradeCount"] for row in feature_rows]
        ),
        "fogCellCountPerRecord": build_frequency_rows(
            [row["fogCellCount"] for row in feature_rows]
        ),
        "totalBloodDebtPerRecord": build_frequency_rows(
            [row["totalBloodDebt"] for row in feature_rows]
        ),
        "commandAuditCountPerRecord": build_frequency_rows(
            [row["commandAuditCount"] for row in feature_rows]
        ),
    }


def build_feature_correlations(feature_rows: list[dict[str, Any]]) -> dict[str, Any]:
    return {
        "perRecordFeatures": feature_rows,
        "weatherActiveVsInactive": build_boolean_comparison(
            feature_rows,
            "hasActiveFront",
            true_label="active_front",
            false_label="no_front",
        ),
        "infernalActiveVsInactive": build_boolean_comparison(
            feature_rows,
            "hasActiveInfernal",
            true_label="infernal_active",
            false_label="infernal_inactive",
        ),
        "weatherFrontSpawnWindows": build_centered_windows(
            feature_rows,
            [row for row in feature_rows if row["weatherFrontSpawnedCount"] > 0],
            window_label_key="weather_front_spawn",
        ),
        "chestOpenWindows": build_centered_windows(
            feature_rows,
            [row for row in feature_rows if row["chestOpenedCount"] > 0],
            window_label_key="chest_opened",
        ),
        "infernalSpawnWindows": build_centered_windows(
            feature_rows,
            [row for row in feature_rows if row["infernalSpawnedCount"] > 0],
            window_label_key="infernal_spawned",
        ),
    }


def summarize_feature_window(rows: list[dict[str, Any]]) -> dict[str, Any]:
    return {
        "recordCount": len(rows),
        "totalMoves": sum(row["moveCount"] for row in rows),
        "totalBuilds": sum(row["buildCount"] for row in rows),
        "totalKills": sum(row["killCount"] for row in rows),
        "totalUpgrades": sum(row["upgradeCount"] for row in rows),
        "averageMovementPointsSpent": safe_mean([row["movementPointsSpent"] for row in rows]),
        "averageBuildPointsSpent": safe_mean([row["buildPointsSpent"] for row in rows]),
        "averageMoveCount": safe_mean([row["moveCount"] for row in rows]),
        "averageBuildCount": safe_mean([row["buildCount"] for row in rows]),
        "averageKillCount": safe_mean([row["killCount"] for row in rows]),
        "averageUpgradeCount": safe_mean([row["upgradeCount"] for row in rows]),
        "averageCommandAuditCount": safe_mean([row["commandAuditCount"] for row in rows]),
        "averageInteractionEventCount": safe_mean(
            [row["interactionEventCount"] for row in rows]
        ),
        "averageReplaceCount": safe_mean([row["replaceCount"] for row in rows]),
        "averageCancelCount": safe_mean([row["cancelCount"] for row in rows]),
        "averageHiddenEnemyPieces": safe_mean(
            [row["totalHiddenEnemyPieces"] for row in rows]
        ),
        "averageFogCellCount": safe_mean([row["fogCellCount"] for row in rows]),
        "averageTotalBloodDebt": safe_mean([row["totalBloodDebt"] for row in rows]),
    }


def build_boolean_comparison(
    feature_rows: list[dict[str, Any]],
    flag_key: str,
    true_label: str,
    false_label: str,
) -> dict[str, Any]:
    true_rows = [row for row in feature_rows if row.get(flag_key)]
    false_rows = [row for row in feature_rows if not row.get(flag_key)]
    true_summary = summarize_feature_window(true_rows)
    false_summary = summarize_feature_window(false_rows)
    return {
        "flagKey": flag_key,
        "trueLabel": true_label,
        "falseLabel": false_label,
        "trueSummary": true_summary,
        "falseSummary": false_summary,
        "trueMinusFalse": diff_numeric_rows(true_summary, false_summary),
    }


def build_centered_windows(
    feature_rows: list[dict[str, Any]],
    centers: list[dict[str, Any]],
    window_label_key: str,
    before_radius: int = 3,
    after_radius: int = 3,
) -> list[dict[str, Any]]:
    rows_by_sequence = {row["sequenceIndex"]: row for row in feature_rows}
    windows = []

    for center in centers:
        sequence_index = center["sequenceIndex"]
        before_rows = [
            rows_by_sequence[index]
            for index in range(sequence_index - before_radius, sequence_index)
            if index in rows_by_sequence
        ]
        after_rows = [
            rows_by_sequence[index]
            for index in range(sequence_index + 1, sequence_index + after_radius + 1)
            if index in rows_by_sequence
        ]

        before_summary = summarize_feature_window(before_rows)
        after_summary = summarize_feature_window(after_rows)
        windows.append(
            {
                "windowLabelKey": window_label_key,
                "sequenceIndex": sequence_index,
                "turn": center["turn"],
                "committedActiveKingdomKey": center["committedActiveKingdomKey"],
                "before": before_summary,
                "after": after_summary,
                "afterMinusBefore": diff_numeric_rows(after_summary, before_summary),
            }
        )

    return windows


def diff_numeric_rows(left: dict[str, Any], right: dict[str, Any]) -> dict[str, Any]:
    deltas: dict[str, Any] = {}
    for key, left_value in left.items():
        right_value = right.get(key)
        if not isinstance(left_value, (int, float)) or not isinstance(right_value, (int, float)):
            continue
        deltas[f"delta{key[0].upper()}{key[1:]}"] = left_value - right_value
    return deltas


def safe_mean(values: list[int | float]) -> float | None:
    if not values:
        return None
    return float(mean(values))


def build_frequency_rows(values: list[int]) -> list[dict[str, int]]:
    counter = Counter(values)
    return [
        {"value": value, "count": count}
        for value, count in sorted(counter.items(), key=lambda item: item[0])
    ]


def index_rows_by_sequence(rows: list[Any]) -> dict[int, Any]:
    index: dict[int, Any] = {}
    for row in rows:
        if not isinstance(row, dict):
            continue
        sequence_index = to_int(row.get("sequenceIndex"), None)
        if sequence_index is not None:
            index[sequence_index] = row
    return index


def sorted_counter_dict(counter_like: dict[str, int]) -> dict[str, int]:
    return {key: counter_like[key] for key in sorted(counter_like)}


def index_by_key(rows: list[Any], key_name: str) -> dict[str, Any]:
    index: dict[str, Any] = {}
    for row in rows:
        if not isinstance(row, dict):
            continue
        key = row.get(key_name)
        if isinstance(key, str) and key:
            index[key] = row
    return index


def as_dict(value: Any) -> dict[str, Any]:
    return value if isinstance(value, dict) else {}


def as_list(value: Any) -> list[Any]:
    return value if isinstance(value, list) else []


def key_or_fallback(value: Any, fallback: str) -> str:
    if isinstance(value, str) and value:
        return value
    return fallback


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