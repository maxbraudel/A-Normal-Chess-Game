from __future__ import annotations

from pathlib import Path
from typing import Any

import matplotlib

matplotlib.use("Agg")

import matplotlib.pyplot as plt


COLOR_WHITE = "#2C7FB8"
COLOR_BLACK = "#D95F0E"
COLOR_TERRAIN = "#2D6A4F"
COLOR_WEATHER = "#4C78A8"
COLOR_CHEST = "#F2A541"
COLOR_INFERNAL = "#C03A2B"
COLOR_UPGRADE = "#5AA469"


def render_report_charts(
    analysis: dict[str, Any], insights: dict[str, Any], charts_dir: Path
) -> list[dict[str, Any]]:
    charts_dir.mkdir(parents=True, exist_ok=True)
    for stale_chart in charts_dir.glob("*.png"):
        stale_chart.unlink(missing_ok=True)
    configure_matplotlib()
    artifacts = []
    for builder in (
        render_terrain_chart,
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

    section = section_by_key(insights, "weather")
    cover_table = first_table(section, "Comportement du royaume actif sous couverture")
    cover_rows = [
        row
        for row in as_list(cover_table.get("rows"))
        if isinstance(row, dict)
        and row.get("metricKey") in {"moves", "audits", "interactions", "commitSeconds"}
    ]
    x_values = [point["sequenceIndex"] for point in points]
    fog_values = [point["fogCellCount"] for point in points]
    concealing_values = [point["concealingFogCellCount"] for point in points]
    hidden_for_white = [point["whiteHiddenEnemyPieces"] for point in points]
    hidden_for_black = [point["blackHiddenEnemyPieces"] for point in points]

    fig, axes = plt.subplots(3, 1, figsize=(11, 10.2), sharex=False)
    axes[0].plot(x_values, fog_values, color=COLOR_WEATHER, linewidth=2.3, label="Brouillard total")
    axes[0].plot(x_values, concealing_values, color="#7AA6D1", linewidth=1.9, label="Brouillard dissimulant")
    add_vertical_markers(axes[0], as_list(path_get(analysis, "eventMarkers", "weather_front_spawned")), COLOR_WEATHER, "Spawn front")
    add_vertical_markers(axes[0], as_list(path_get(analysis, "eventMarkers", "weather_front_ended")), "#9BB8D3", "Fin front")
    axes[0].set_ylabel("Cellules")
    axes[0].set_title("Zone nuageuse et couverture dissimulante")
    deduplicate_legend(axes[0])

    axes[1].plot(x_values, hidden_for_white, color=COLOR_WHITE, linewidth=2.1, label="Pieces cachees aux blancs")
    axes[1].plot(x_values, hidden_for_black, color=COLOR_BLACK, linewidth=2.1, label="Pieces cachees aux noirs")
    add_vertical_markers(axes[1], as_list(path_get(analysis, "eventMarkers", "weather_front_spawned")), COLOR_WEATHER, "Spawn front")
    add_vertical_markers(axes[1], as_list(path_get(analysis, "eventMarkers", "weather_front_ended")), "#9BB8D3", "Fin front")
    axes[1].set_ylabel("Pieces ennemies masquees")
    axes[1].set_xlabel("Sequence de record")
    deduplicate_legend(axes[1])

    if cover_rows:
        render_delta_table_subplot(
            axes[2],
            cover_rows,
            "Effet comportemental quand le royaume actif est couvert",
            COLOR_WEATHER,
            xlabel="Variation % vs degage",
        )
    else:
        draw_placeholder(axes[2], "Aucune comparaison couvert/degage exploitable")

    return save_chart(
        fig,
        charts_dir / "weather_pressure.png",
        key="weather_pressure",
        title="Couverture meteo, visibilite masquee et effet comportemental",
        caption="En haut, la surface nuageuse totale et dissimulante; au milieu, les pieces cachees a chaque camp; en bas, l'ecart relatif de comportement quand le royaume actif perd reellement de la vision.",
        section_key="weather",
    )


def render_terrain_chart(
    analysis: dict[str, Any], insights: dict[str, Any], charts_dir: Path
) -> dict[str, Any] | None:
    terrain = as_dict(path_get(analysis, "systems", "terrain"))
    per_record_rows = as_list(terrain.get("perRecordActiveKingdom"))
    piece_rows = as_list(terrain.get("byPieceType"))
    if not per_record_rows and not piece_rows:
        return None

    fig, axes = plt.subplots(1, 2, figsize=(12, 5.8))
    if per_record_rows:
        ordered_rows = sorted(per_record_rows, key=lambda row: row.get("sequenceIndex", 0))
        x_values = [to_int(row.get("sequenceIndex"), 0) for row in ordered_rows]
        raw_values = [to_float(row.get("lostDestinationCount"), 0.0) or 0.0 for row in ordered_rows]
        smooth_values = rolling_average(raw_values, window=12)
        axes[0].plot(x_values, raw_values, color="#B7C3CC", linewidth=1.2, alpha=0.7, label="Brut")
        axes[0].plot(
            x_values,
            smooth_values,
            color=COLOR_TERRAIN,
            linewidth=2.5,
            label="Moyenne glissante (12 records)",
        )
        axes[0].set_title("Destinations perdues a cause de l'eau")
        axes[0].set_xlabel("Sequence de record")
        axes[0].set_ylabel("Destinations perdues")
        deduplicate_legend(axes[0])
    else:
        draw_placeholder(axes[0], "Aucune serie terrain exploitable")

    if piece_rows:
        sorted_rows = sorted(
            [row for row in piece_rows if isinstance(row, dict)],
            key=lambda row: to_float(row.get("lostMobilityShare"), 0.0) or 0.0,
            reverse=True,
        )
        labels = [str(row.get("pieceTypeLabel") or "unknown") for row in sorted_rows]
        percent_values = [100.0 * (to_float(row.get("lostMobilityShare"), 0.0) or 0.0) for row in sorted_rows]
        y_positions = list(range(len(labels)))
        axes[1].barh(y_positions, percent_values, color=COLOR_TERRAIN)
        axes[1].set_yticks(y_positions, labels)
        axes[1].invert_yaxis()
        axes[1].set_title("Sensibilite a l'eau par type de piece")
        axes[1].set_xlabel("% de mobilite seche perdue")
        max_value = max(percent_values, default=0.0)
        axes[1].set_xlim(0, max(1.0, max_value * 1.25))
        for index, (row, value) in enumerate(zip(sorted_rows, percent_values)):
            annotation = (
                f"{value:.1f}% | +{to_float(row.get('averageLostDestinationsPerActivePieceSample'), 0.0) or 0.0:.2f}"
            )
            axes[1].text(value + (max(0.15, max_value * 0.03)), index, annotation, va="center", ha="left", fontsize=8)
    else:
        draw_placeholder(axes[1], "Aucun profil piece exploitable")

    return save_chart(
        fig,
        charts_dir / "terrain_water_tax.png",
        key="terrain_water_tax",
        title="Taxe de mobilite due a l'eau",
        caption="Evolution de la mobilite seche perdue a cause de l'eau au fil du match, et sensibilite par type de piece.",
        section_key="terrain",
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
    spawns = as_list(path_get(analysis, "systems", "infernal", "spawnEvents"))
    section = section_by_key(insights, "infernal")
    spawn_table = first_table(section, "Dette observee au moment des spawns infernaux")
    spawn_rows = as_list(spawn_table.get("rows"))
    if not points:
        return None

    x_values = [point["sequenceIndex"] for point in points]
    white_debt = [point["whiteBloodDebt"] for point in points]
    black_debt = [point["blackBloodDebt"] for point in points]
    fig, axes = plt.subplots(2, 1, figsize=(11, 8), sharex=False)
    axes[0].plot(x_values, white_debt, color=COLOR_WHITE, linewidth=2.2, label="Dette blanche")
    axes[0].plot(x_values, black_debt, color=COLOR_BLACK, linewidth=2.2, label="Dette noire")
    annotate_infernal_spawns(axes[0], points, spawns)
    axes[0].set_title("Dette infernale par camp et spawns observes")
    axes[0].set_ylabel("Dette")
    axes[0].set_xlabel("Sequence de record")
    deduplicate_legend(axes[0])

    if spawn_rows:
        positions = list(range(len(spawn_rows)))
        width = 0.36
        white_spawn_debt = [to_float(row.get("whiteDebtAtSpawn"), 0.0) or 0.0 for row in spawn_rows]
        black_spawn_debt = [to_float(row.get("blackDebtAtSpawn"), 0.0) or 0.0 for row in spawn_rows]
        axes[1].bar([position - width / 2 for position in positions], white_spawn_debt, width=width, color=COLOR_WHITE, label="Dette blanche au spawn")
        axes[1].bar([position + width / 2 for position in positions], black_spawn_debt, width=width, color=COLOR_BLACK, label="Dette noire au spawn")
        axes[1].set_xticks(positions, [str(row.get("turn", "?")) for row in spawn_rows])
        axes[1].set_title("Dette observee au moment de chaque manifestation")
        axes[1].set_ylabel("Dette")
        axes[1].set_xlabel("Tour du spawn")
        annotate_spawn_groups(axes[1], positions, spawn_rows, white_spawn_debt, black_spawn_debt)
        deduplicate_legend(axes[1])
    else:
        draw_placeholder(axes[1], "Pas assez de spawns infernaux pour afficher le detail")

    return save_chart(
        fig,
        charts_dir / "infernal_pressure.png",
        key="infernal_pressure",
        title="Dette infernale par camp et contexte des spawns",
        caption="En haut, les courbes de dette blanche/noire avec les spawns annotes par piece et camp cible; en bas, le niveau de dette de chaque camp au moment exact des manifestations.",
        section_key="infernal",
    )


def render_behavior_regime_chart(
    analysis: dict[str, Any], insights: dict[str, Any], charts_dir: Path
) -> dict[str, Any] | None:
    weather_comparison = as_dict(path_get(analysis, "correlations", "weatherActiveVsInactive"))
    infernal_comparison = as_dict(path_get(analysis, "correlations", "infernalActiveVsInactive"))
    if not weather_comparison and not infernal_comparison:
        return None

    fig, axes = plt.subplots(1, 2, figsize=(12, 5.8), sharey=True)
    render_regime_delta_subplot(axes[0], weather_comparison, "Front actif vs sans front", COLOR_WEATHER)
    render_regime_delta_subplot(axes[1], infernal_comparison, "Infernal actif vs inactif", COLOR_INFERNAL)
    return save_chart(
        fig,
        charts_dir / "behavior_regimes.png",
        key="behavior_regimes",
        title="Ecarts relatifs de comportement par regime",
        caption="Variation relative des metriques de comportement entre regimes aleatoires actifs et inactifs, avec annotation du delta absolu par record.",
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
    fig, axes = plt.subplots(1, len(labels), figsize=(4.4 * len(labels), 5.5), sharey=False)
    if len(labels) == 1:
        axes = [axes]
    else:
        axes = list(axes)

    for ax, row in zip(axes, rows):
        render_window_delta_subplot(ax, row)

    return save_chart(
        fig,
        charts_dir / "event_windows.png",
        key="event_windows",
        title="Chocs comportementaux autour des evenements",
        caption="Chaque sous-graphe montre le delta moyen avant/apres evenement sur les metriques de comportement qui appuient directement le commentaire du rapport.",
        section_key="cross_effects",
    )


def render_regime_delta_subplot(ax: Any, comparison: dict[str, Any], title: str, accent_color: str) -> None:
    true_summary = as_dict(comparison.get("trueSummary"))
    false_summary = as_dict(comparison.get("falseSummary"))
    deltas = as_dict(comparison.get("trueMinusFalse"))
    if not true_summary or not false_summary:
        draw_placeholder(ax, "Aucune comparaison exploitable")
        return

    metric_specs = (
        ("Moves", "averageMoveCount", "deltaAverageMoveCount"),
        ("Builds", "averageBuildCount", "deltaAverageBuildCount"),
        ("Audits", "averageCommandAuditCount", "deltaAverageCommandAuditCount"),
        ("Interactions", "averageInteractionEventCount", "deltaAverageInteractionEventCount"),
    )
    labels = []
    percent_values = []
    annotations = []
    for label, average_key, delta_key in metric_specs:
        baseline = to_float(false_summary.get(average_key), 0.0)
        delta = to_float(deltas.get(delta_key), 0.0)
        percent_delta = safe_percent_delta(delta, baseline)
        labels.append(label)
        percent_values.append(percent_delta or 0.0)
        annotations.append(f"{format_signed_percent(percent_delta)} | {format_signed_number(delta)}")

    y_positions = list(range(len(labels)))
    colors = [accent_color if value >= 0 else "#B9C0CA" for value in percent_values]
    ax.barh(y_positions, percent_values, color=colors)
    ax.axvline(0.0, color="#333333", linewidth=1.0)
    ax.set_yticks(y_positions, labels)
    ax.invert_yaxis()
    ax.set_title(title)
    ax.set_xlabel("Variation % vs inactif")

    max_abs = max((abs(value) for value in percent_values), default=0.0)
    if max_abs == 0:
        max_abs = 1.0
    ax.set_xlim(-max_abs * 1.35, max_abs * 1.35)

    text_offset = max_abs * 0.04
    for index, (value, annotation) in enumerate(zip(percent_values, annotations)):
        x_position = value + text_offset if value >= 0 else value - text_offset
        ax.text(x_position, index, annotation, va="center", ha="left" if value >= 0 else "right", fontsize=8)

    ax.text(
        0.02,
        0.02,
        f"n actif/inactif: {to_int(true_summary.get('recordCount'), 0)}/{to_int(false_summary.get('recordCount'), 0)}",
        transform=ax.transAxes,
        fontsize=8,
        color="#555555",
    )


def render_delta_table_subplot(
    ax: Any,
    rows: list[dict[str, Any]],
    title: str,
    accent_color: str,
    xlabel: str,
) -> None:
    if not rows:
        draw_placeholder(ax, "Aucune comparaison exploitable")
        return

    labels = [str(row.get("metric", "Metrique")) for row in rows]
    percent_values = []
    annotations = []
    for row in rows:
        baseline = to_float(row.get("clearValue"), 0.0)
        delta = to_float(row.get("deltaValue"), 0.0)
        percent_delta = safe_percent_delta(delta, baseline)
        percent_values.append(percent_delta or 0.0)
        annotations.append(f"{format_signed_percent(percent_delta)} | {format_signed_number(delta)}")

    y_positions = list(range(len(labels)))
    colors = [accent_color if value >= 0 else "#B9C0CA" for value in percent_values]
    ax.barh(y_positions, percent_values, color=colors)
    ax.axvline(0.0, color="#333333", linewidth=1.0)
    ax.set_yticks(y_positions, labels)
    ax.invert_yaxis()
    ax.set_title(title)
    ax.set_xlabel(xlabel)

    max_abs = max((abs(value) for value in percent_values), default=0.0)
    if max_abs == 0:
        max_abs = 1.0
    ax.set_xlim(-max_abs * 1.35, max_abs * 1.35)

    text_offset = max_abs * 0.04
    for index, (value, annotation) in enumerate(zip(percent_values, annotations)):
        x_position = value + text_offset if value >= 0 else value - text_offset
        ax.text(x_position, index, annotation, va="center", ha="left" if value >= 0 else "right", fontsize=8)


def annotate_infernal_spawns(ax: Any, points: list[dict[str, Any]], spawns: list[dict[str, Any]]) -> None:
    points_by_sequence = {
        to_int(point.get("sequenceIndex"), -1): as_dict(point)
        for point in points
        if isinstance(point, dict)
    }
    seen_labels: set[str] = set()
    max_debt = max(
        [
            max(
                to_float(point.get("whiteBloodDebt"), 0.0) or 0.0,
                to_float(point.get("blackBloodDebt"), 0.0) or 0.0,
            )
            for point in points
            if isinstance(point, dict)
        ],
        default=0.0,
    )
    y_offset = max(0.4, max_debt * 0.04)

    for spawn in spawns:
        if not isinstance(spawn, dict):
            continue
        sequence_index = to_int(spawn.get("sequenceIndex"), None)
        if sequence_index is None:
            continue
        point = as_dict(points_by_sequence.get(sequence_index))
        target_kingdom_key = str(spawn.get("targetKingdomKey") or "unknown")
        manifested_piece_type_key = str(spawn.get("manifestedPieceTypeKey") or "unknown")
        marker_y = (
            to_float(point.get("whiteBloodDebt"), 0.0) or 0.0
            if target_kingdom_key == "white"
            else to_float(point.get("blackBloodDebt"), 0.0) or 0.0
        )
        color = COLOR_WHITE if target_kingdom_key == "white" else COLOR_BLACK if target_kingdom_key == "black" else COLOR_INFERNAL
        legend_label = f"Spawn cible {kingdom_chart_label(target_kingdom_key)}"
        ax.scatter(
            [sequence_index],
            [marker_y],
            color=color,
            s=44,
            zorder=5,
            label=legend_label if legend_label not in seen_labels else None,
        )
        seen_labels.add(legend_label)
        ax.text(
            sequence_index,
            marker_y + y_offset,
            infernal_event_label(manifested_piece_type_key, target_kingdom_key),
            fontsize=8,
            ha="center",
            va="bottom",
            color=color,
        )


def annotate_spawn_groups(
    ax: Any,
    positions: list[int],
    rows: list[dict[str, Any]],
    white_values: list[float],
    black_values: list[float],
) -> None:
    max_value = max(white_values + black_values, default=0.0)
    y_offset = max(0.25, max_value * 0.05)
    for position, row, white_value, black_value in zip(positions, rows, white_values, black_values):
        label = infernal_event_label(
            str(row.get("manifestedPieceTypeKey") or "unknown"),
            str(row.get("targetKingdomKey") or "unknown"),
        )
        color = COLOR_WHITE if row.get("targetKingdomKey") == "white" else COLOR_BLACK if row.get("targetKingdomKey") == "black" else COLOR_INFERNAL
        ax.text(
            position,
            max(white_value, black_value) + y_offset,
            label,
            ha="center",
            va="bottom",
            fontsize=8,
            color=color,
        )


def infernal_event_label(piece_type_key: str, target_kingdom_key: str) -> str:
    piece_codes = {
        "pawn": "P",
        "knight": "N",
        "bishop": "B",
        "rook": "R",
        "queen": "Q",
        "king": "K",
    }
    kingdom_codes = {
        "white": "W",
        "black": "B",
    }
    return f"{piece_codes.get(piece_type_key, '?')}>{kingdom_codes.get(target_kingdom_key, '?')}"


def kingdom_chart_label(kingdom_key: str) -> str:
    labels = {
        "white": "blanc",
        "black": "noir",
    }
    return labels.get(kingdom_key, kingdom_key)


def render_window_delta_subplot(ax: Any, row: dict[str, Any]) -> None:
    metric_specs = (
        ("Moves", "deltaAverageMoveCount"),
        ("Builds", "deltaAverageBuildCount"),
        ("Audits", "deltaAverageCommandAuditCount"),
        ("Interactions", "deltaAverageInteractionEventCount"),
    )
    labels = [label for label, _ in metric_specs]
    values = [to_float(row.get(key), 0.0) or 0.0 for _, key in metric_specs]
    color = color_for_window_label(str(row.get("windowLabel", "")))
    y_positions = list(range(len(labels)))

    ax.barh(y_positions, values, color=color)
    ax.axvline(0.0, color="#333333", linewidth=1.0)
    ax.set_yticks(y_positions, labels)
    ax.invert_yaxis()
    ax.set_title(str(row.get("windowLabel", "Evenement")))

    max_abs = max((abs(value) for value in values), default=0.0)
    if max_abs == 0:
        max_abs = 1.0
    ax.set_xlim(-max_abs * 1.35, max_abs * 1.35)

    text_offset = max_abs * 0.05
    for index, value in enumerate(values):
        x_position = value + text_offset if value >= 0 else value - text_offset
        ax.text(x_position, index, format_signed_number(value), va="center", ha="left" if value >= 0 else "right", fontsize=8)


def rolling_average(values: list[float], window: int) -> list[float]:
    if not values:
        return []
    window = max(1, window)
    averages = []
    for index in range(len(values)):
        start = max(0, index - window + 1)
        chunk = values[start : index + 1]
        averages.append(sum(chunk) / len(chunk))
    return averages


def safe_percent_delta(delta: float | None, baseline: float | None) -> float | None:
    if delta is None or baseline is None or baseline == 0:
        return None
    return 100.0 * delta / baseline


def format_signed_percent(value: float | None, digits: int = 0) -> str:
    if value is None:
        return "n/a"
    return f"{value:+.{digits}f}%"


def format_signed_number(value: float | None, digits: int = 2) -> str:
    if value is None:
        return "n/a"
    if value == 0:
        return "0"
    return f"{value:+.{digits}f}"


def color_for_window_label(label: str) -> str:
    lowered = label.lower()
    if "meteo" in lowered:
        return COLOR_WEATHER
    if "coffre" in lowered:
        return COLOR_CHEST
    if "infernal" in lowered:
        return COLOR_INFERNAL
    return "#777777"


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