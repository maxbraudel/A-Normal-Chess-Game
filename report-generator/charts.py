from __future__ import annotations

from pathlib import Path
from typing import Any

import matplotlib

matplotlib.use("Agg")

import matplotlib.pyplot as plt


COLOR_WHITE = "#2C7FB8"
COLOR_BLACK = "#D95F0E"
COLOR_WEATHER = "#4C78A8"
COLOR_CHEST = "#F2A541"
COLOR_INFERNAL = "#C03A2B"
COLOR_UPGRADE = "#5AA469"


def render_report_charts(
    analysis: dict[str, Any], insights: dict[str, Any], charts_dir: Path
) -> list[dict[str, Any]]:
    charts_dir.mkdir(parents=True, exist_ok=True)
    configure_matplotlib()
    artifacts = []
    for builder in (
        render_match_flow_chart,
        render_economy_chart,
        render_weather_chart,
        render_chest_chart,
        render_infernal_chart,
        render_behavior_regime_chart,
        render_event_window_chart,
    ):
        artifact = builder(analysis, insights, charts_dir)
        if artifact is not None:
            artifacts.append(artifact)
    return artifacts


def configure_matplotlib() -> None:
    plt.style.use("seaborn-v0_8-whitegrid")
    plt.rcParams.update(
        {
            "figure.figsize": (11, 6),
            "axes.titlesize": 13,
            "axes.labelsize": 10,
            "legend.fontsize": 9,
            "font.size": 10,
        }
    )


def render_match_flow_chart(
    analysis: dict[str, Any], insights: dict[str, Any], charts_dir: Path
) -> dict[str, Any] | None:
    activity = as_dict(analysis.get("activity"))
    move_points = as_list(path_get(activity, "moves", "points"))
    build_points = as_list(path_get(activity, "builds", "points"))
    kill_points = as_list(path_get(activity, "kills", "points"))
    upgrade_points = as_list(path_get(activity, "upgrades", "points"))
    if not move_points:
        return None

    x_values = [point["sequenceIndex"] for point in move_points]
    fig, ax = plt.subplots()
    ax.plot(x_values, [point["totalMoves"] for point in move_points], label="Moves cumules", color=COLOR_WHITE, linewidth=2.4)
    ax.plot(x_values, [point["totalBuilds"] for point in build_points], label="Builds cumules", color=COLOR_CHEST, linewidth=2.0)
    ax.plot(x_values, [point["totalKills"] for point in kill_points], label="Captures cumulees", color=COLOR_INFERNAL, linewidth=2.0)
    ax.plot(x_values, [point["totalUpgrades"] for point in upgrade_points], label="Upgrades cumules", color=COLOR_UPGRADE, linewidth=2.0)

    add_vertical_markers(ax, as_list(path_get(analysis, "eventMarkers", "chest_opened")), COLOR_CHEST, "Ouverture coffre")
    add_vertical_markers(ax, as_list(path_get(analysis, "eventMarkers", "weather_front_spawned")), COLOR_WEATHER, "Spawn front meteo")
    add_vertical_markers(ax, as_list(path_get(analysis, "eventMarkers", "infernal_spawned")), COLOR_INFERNAL, "Spawn infernal")

    ax.set_title("Flow du match: activite cumulee et evenements aleatoires")
    ax.set_xlabel("Sequence de record")
    ax.set_ylabel("Volume cumule")
    deduplicate_legend(ax)
    return save_chart(
        fig,
        charts_dir / "match_flow.png",
        key="match_flow",
        title="Flow du match",
        caption="Evolution cumulee des actions avec marqueurs des evenements aleatoires majeurs.",
        section_key="overview",
    )


def render_economy_chart(
    analysis: dict[str, Any], insights: dict[str, Any], charts_dir: Path
) -> dict[str, Any] | None:
    economy_rows = as_list(path_get(analysis, "systems", "economy", "byKingdom"))
    if not economy_rows:
        return None

    grouped = group_by_key(economy_rows, "kingdomKey")
    fig, axes = plt.subplots(2, 1, figsize=(11, 8), sharex=True)
    for kingdom_key, color in (("white", COLOR_WHITE), ("black", COLOR_BLACK)):
        rows = grouped.get(kingdom_key, [])
        if not rows:
            continue
        rows = sorted(rows, key=lambda row: row.get("sequenceIndex", 0))
        x_values = [row["sequenceIndex"] for row in rows]
        axes[0].plot(x_values, [row["gold"] for row in rows], label=f"{kingdom_key} or", color=color, linewidth=2.2)
        axes[1].plot(x_values, [row["netIncome"] for row in rows], label=f"{kingdom_key} revenu net", color=color, linewidth=2.0)

    axes[0].set_title("Economie par royaume")
    axes[0].set_ylabel("Or courant")
    axes[1].set_ylabel("Revenu net")
    axes[1].set_xlabel("Sequence de record")
    deduplicate_legend(axes[0])
    deduplicate_legend(axes[1])
    return save_chart(
        fig,
        charts_dir / "economy_gold.png",
        key="economy_gold",
        title="Economie par royaume",
        caption="Trajectoires d'or et de revenu net pour les deux royaumes.",
        section_key="economy",
    )


def render_weather_chart(
    analysis: dict[str, Any], insights: dict[str, Any], charts_dir: Path
) -> dict[str, Any] | None:
    points = as_list(path_get(analysis, "systems", "weather", "points"))
    if not points:
        return None

    x_values = [point["sequenceIndex"] for point in points]
    fog_values = [point["fogCellCount"] for point in points]
    concealing_values = [point["concealingFogCellCount"] for point in points]
    front_counts = [point["frontCount"] for point in points]
    hidden_values = [point["whiteHiddenEnemyPieces"] + point["blackHiddenEnemyPieces"] for point in points]

    fig, axes = plt.subplots(2, 1, figsize=(11, 8), sharex=True)
    axes[0].plot(x_values, fog_values, color=COLOR_WEATHER, linewidth=2.4, label="Fog cells")
    axes[0].plot(x_values, concealing_values, color="#7AA6D1", linewidth=1.8, label="Fog cells occultantes")
    axes[0].set_ylabel("Cellules")
    axes[0].set_title("Pression meteo et visibilite")
    deduplicate_legend(axes[0])

    axes[1].plot(x_values, front_counts, color=COLOR_WEATHER, linewidth=2.0, label="Nombre de fronts")
    axes[1].plot(x_values, hidden_values, color="#1D4E89", linewidth=1.6, label="Pieces ennemies masquees")
    add_vertical_markers(axes[1], as_list(path_get(analysis, "eventMarkers", "weather_front_spawned")), COLOR_WEATHER, "Spawn front")
    add_vertical_markers(axes[1], as_list(path_get(analysis, "eventMarkers", "weather_front_ended")), "#9BB8D3", "Fin front")
    axes[1].set_ylabel("Pression")
    axes[1].set_xlabel("Sequence de record")
    deduplicate_legend(axes[1])
    return save_chart(
        fig,
        charts_dir / "weather_pressure.png",
        key="weather_pressure",
        title="Pression meteo",
        caption="Fog, fronts actifs et pieces masquees au fil du match.",
        section_key="weather",
    )


def render_chest_chart(
    analysis: dict[str, Any], insights: dict[str, Any], charts_dir: Path
) -> dict[str, Any] | None:
    section = section_by_key(insights, "chest")
    reward_table = first_table(section, "Distribution des recompenses de coffres")
    pickup_table = first_table(section, "Delais de collecte des coffres")
    reward_rows = as_list(reward_table.get("rows"))
    pickup_rows = as_list(pickup_table.get("rows"))
    if not reward_rows and not pickup_rows:
        return None

    fig, axes = plt.subplots(1, 2, figsize=(12, 5.5))
    if reward_rows:
        labels = [row["rewardTypeKey"] for row in reward_rows]
        counts = [to_int(row.get("count"), 0) for row in reward_rows]
        axes[0].bar(labels, counts, color=[COLOR_CHEST, COLOR_WHITE, COLOR_UPGRADE, COLOR_BLACK][: len(labels)])
        axes[0].set_title("Recompenses de coffres")
        axes[0].set_ylabel("Occurrences")
        axes[0].tick_params(axis="x", rotation=25)
    else:
        draw_placeholder(axes[0], "Aucune recompense de coffre exploitable")

    pickup_delays = [to_int(row.get("pickupLagTurns"), 0) for row in pickup_rows]
    if pickup_delays:
        bins = build_integer_bins(pickup_delays)
        axes[1].hist(pickup_delays, bins=bins, color=COLOR_CHEST, edgecolor="white")
        axes[1].set_title("Delais de collecte")
        axes[1].set_xlabel("Tours intermediaires avant collecte")
        axes[1].set_ylabel("Frequence")
    else:
        draw_placeholder(axes[1], "Aucun delai de collecte apparié")

    return save_chart(
        fig,
        charts_dir / "chest_rewards.png",
        key="chest_rewards",
        title="Coffres et delais de collecte",
        caption="Distribution des recompenses et histogramme des delais de collecte des coffres.",
        section_key="chest",
    )


def render_infernal_chart(
    analysis: dict[str, Any], insights: dict[str, Any], charts_dir: Path
) -> dict[str, Any] | None:
    points = as_list(path_get(analysis, "systems", "infernal", "points"))
    section = section_by_key(insights, "infernal")
    cadence_table = first_table(section, "Cadence et dette au moment des spawns infernaux")
    cadence_rows = as_list(cadence_table.get("rows"))
    if not points:
        return None

    x_values = [point["sequenceIndex"] for point in points]
    total_debt = [point["whiteBloodDebt"] + point["blackBloodDebt"] for point in points]
    active_count = [point["activeInfernalCount"] for point in points]
    fig, axes = plt.subplots(2, 1, figsize=(11, 8), sharex=False)
    axes[0].plot(x_values, total_debt, color=COLOR_INFERNAL, linewidth=2.4, label="Dette totale")
    axes[0].plot(x_values, active_count, color="#F6A6A0", linewidth=1.8, label="Infernaux actifs")
    add_vertical_markers(axes[0], as_list(path_get(analysis, "eventMarkers", "infernal_spawned")), COLOR_INFERNAL, "Spawn infernal")
    axes[0].set_title("Dette infernale et presence active")
    axes[0].set_ylabel("Pression")
    deduplicate_legend(axes[0])

    intervals = [to_int(row.get("spawnIntervalTurns"), None) for row in cadence_rows if row.get("spawnIntervalTurns") is not None]
    colors = [COLOR_INFERNAL if to_int(row.get("totalDebtAtSpawn"), 0) > 0 else "#D8D8D8" for row in cadence_rows if row.get("spawnIntervalTurns") is not None]
    labels = [str(row.get("turn")) for row in cadence_rows if row.get("spawnIntervalTurns") is not None]
    if intervals:
        axes[1].bar(labels, intervals, color=colors)
        axes[1].set_title("Intervalle entre spawns infernaux")
        axes[1].set_ylabel("Tours")
        axes[1].set_xlabel("Tour du spawn")
    else:
        draw_placeholder(axes[1], "Pas assez de spawns pour afficher une cadence")

    return save_chart(
        fig,
        charts_dir / "infernal_pressure.png",
        key="infernal_pressure",
        title="Dette et cadence infernales",
        caption="Dette totale, activite infernale et intervalle entre manifestations.",
        section_key="infernal",
    )


def render_behavior_regime_chart(
    analysis: dict[str, Any], insights: dict[str, Any], charts_dir: Path
) -> dict[str, Any] | None:
    weather_comparison = as_dict(path_get(analysis, "correlations", "weatherActiveVsInactive"))
    infernal_comparison = as_dict(path_get(analysis, "correlations", "infernalActiveVsInactive"))
    if not weather_comparison and not infernal_comparison:
        return None

    fig, axes = plt.subplots(1, 2, figsize=(12, 5.5), sharey=True)
    render_regime_subplot(axes[0], weather_comparison, "Front actif vs sans front")
    render_regime_subplot(axes[1], infernal_comparison, "Infernal actif vs inactif")
    return save_chart(
        fig,
        charts_dir / "behavior_regimes.png",
        key="behavior_regimes",
        title="Changement de regime comportemental",
        caption="Comparaison de metriques comportementales sous regimes aleatoires actifs vs inactifs.",
        section_key="behavior",
    )


def render_event_window_chart(
    analysis: dict[str, Any], insights: dict[str, Any], charts_dir: Path
) -> dict[str, Any] | None:
    section = section_by_key(insights, "cross_effects")
    table = first_table(section, "Deltas moyens apres evenement")
    rows = as_list(table.get("rows"))
    if not rows:
        return None

    labels = [row["windowLabel"] for row in rows]
    move_values = [to_float(row.get("deltaAverageMoveCount"), 0.0) for row in rows]
    build_values = [to_float(row.get("deltaAverageBuildCount"), 0.0) for row in rows]
    audit_values = [to_float(row.get("deltaAverageCommandAuditCount"), 0.0) for row in rows]

    x_positions = range(len(labels))
    fig, ax = plt.subplots(figsize=(11, 5.5))
    bar_width = 0.25
    ax.bar([index - bar_width for index in x_positions], move_values, width=bar_width, label="Delta moves", color=COLOR_WHITE)
    ax.bar(x_positions, build_values, width=bar_width, label="Delta builds", color=COLOR_CHEST)
    ax.bar([index + bar_width for index in x_positions], audit_values, width=bar_width, label="Delta audits", color=COLOR_INFERNAL)
    ax.axhline(0.0, color="#333333", linewidth=1.0)
    ax.set_title("Effets moyens avant/apres evenement")
    ax.set_ylabel("Delta moyen")
    ax.set_xticks(list(x_positions), labels)
    deduplicate_legend(ax)
    return save_chart(
        fig,
        charts_dir / "event_windows.png",
        key="event_windows",
        title="Deltas moyens autour des evenements",
        caption="Comparaison des deltas moyens de comportement autour des spawns meteo, ouvertures de coffres et spawns infernaux.",
        section_key="cross_effects",
    )


def render_regime_subplot(ax: Any, comparison: dict[str, Any], title: str) -> None:
    true_summary = as_dict(comparison.get("trueSummary"))
    false_summary = as_dict(comparison.get("falseSummary"))
    if not true_summary or not false_summary:
        draw_placeholder(ax, "Aucune comparaison exploitable")
        return

    labels = ["Moves", "Builds", "Audits", "Interactions"]
    true_values = [
        to_float(true_summary.get("averageMoveCount"), 0.0),
        to_float(true_summary.get("averageBuildCount"), 0.0),
        to_float(true_summary.get("averageCommandAuditCount"), 0.0),
        to_float(true_summary.get("averageInteractionEventCount"), 0.0),
    ]
    false_values = [
        to_float(false_summary.get("averageMoveCount"), 0.0),
        to_float(false_summary.get("averageBuildCount"), 0.0),
        to_float(false_summary.get("averageCommandAuditCount"), 0.0),
        to_float(false_summary.get("averageInteractionEventCount"), 0.0),
    ]
    x_positions = range(len(labels))
    bar_width = 0.35
    ax.bar([index - bar_width / 2 for index in x_positions], true_values, width=bar_width, color=COLOR_WEATHER, label="Actif")
    ax.bar([index + bar_width / 2 for index in x_positions], false_values, width=bar_width, color="#C9D7EB", label="Inactif")
    ax.set_xticks(list(x_positions), labels)
    ax.set_title(title)
    deduplicate_legend(ax)


def save_chart(
    fig: Any,
    path: Path,
    key: str,
    title: str,
    caption: str,
    section_key: str,
) -> dict[str, Any]:
    path.parent.mkdir(parents=True, exist_ok=True)
    fig.tight_layout()
    fig.savefig(path, dpi=180, bbox_inches="tight")
    plt.close(fig)
    return {
        "key": key,
        "title": title,
        "caption": caption,
        "sectionKey": section_key,
        "relativePath": path.relative_to(path.parent.parent).as_posix(),
    }


def add_vertical_markers(ax: Any, rows: list[dict[str, Any]], color: str, label: str) -> None:
    first = True
    for row in rows:
        if not isinstance(row, dict):
            continue
        x_value = to_int(row.get("sequenceIndex"), None)
        if x_value is None:
            continue
        ax.axvline(x=x_value, color=color, linestyle="--", linewidth=1.0, alpha=0.5, label=label if first else None)
        first = False


def deduplicate_legend(ax: Any) -> None:
    handles, labels = ax.get_legend_handles_labels()
    if not handles:
        return
    unique = {}
    for handle, label in zip(handles, labels):
        unique[label] = handle
    ax.legend(unique.values(), unique.keys(), loc="best")


def build_integer_bins(values: list[int]) -> list[int]:
    max_value = max(values) if values else 1
    return list(range(0, max_value + 2))


def draw_placeholder(ax: Any, message: str) -> None:
    ax.axis("off")
    ax.text(0.5, 0.5, message, ha="center", va="center", fontsize=11)


def group_by_key(rows: list[dict[str, Any]], key_name: str) -> dict[str, list[dict[str, Any]]]:
    result: dict[str, list[dict[str, Any]]] = {}
    for row in rows:
        if not isinstance(row, dict):
            continue
        key = row.get(key_name)
        if not isinstance(key, str):
            continue
        result.setdefault(key, []).append(row)
    return result


def section_by_key(insights: dict[str, Any], section_key: str) -> dict[str, Any]:
    for section in as_list(insights.get("sections")):
        if isinstance(section, dict) and section.get("key") == section_key:
            return section
    return {}


def first_table(section: dict[str, Any], title: str) -> dict[str, Any]:
    for table in as_list(section.get("tables")):
        if isinstance(table, dict) and table.get("title") == title:
            return table
    return {}


def path_get(value: Any, *path: str) -> Any:
    current = value
    for key in path:
        if not isinstance(current, dict):
            return None
        current = current.get(key)
    return current


def as_dict(value: Any) -> dict[str, Any]:
    return value if isinstance(value, dict) else {}


def as_list(value: Any) -> list[Any]:
    return value if isinstance(value, list) else []


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


def to_float(value: Any, default: float | None) -> float | None:
    if value is None:
        return default
    if isinstance(value, bool):
        return float(int(value))
    if isinstance(value, (int, float)):
        return float(value)
    if isinstance(value, str):
        try:
            return float(value)
        except ValueError:
            return default
    return default