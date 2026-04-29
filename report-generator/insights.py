from __future__ import annotations

from collections import defaultdict
from typing import Any

from report_model import (
    REPORT_VERSION,
    build_table,
    confidence_label,
    format_number,
    format_percent,
    headline_metric,
    percent_metric,
    round_or_none,
    rows_to_count_map,
    safe_div,
    safe_max,
    safe_mean,
    utc_timestamp,
)


def build_report_insights(analysis: dict[str, Any], analysis_source_name: str) -> dict[str, Any]:
    sections = [
        build_overview_section(analysis),
        build_economy_section(analysis),
        build_weather_section(analysis),
        build_chest_section(analysis),
        build_infernal_section(analysis),
        build_behavior_section(analysis),
        build_cross_effects_section(analysis),
        build_limitations_section(analysis),
    ]
    executive_summary = []
    for section in sections:
        findings = section.get("findings", [])
        if findings:
            executive_summary.append(findings[0])

    return {
        "metadata": {
            "reportVersion": REPORT_VERSION,
            "generatedAtUtc": utc_timestamp(),
            "analysisVersion": analysis.get("metadata", {}).get("analysisVersion"),
            "analysisSourceName": analysis_source_name,
        },
        "executiveSummary": executive_summary,
        "sections": sections,
    }


def build_overview_section(analysis: dict[str, Any]) -> dict[str, Any]:
    metadata = as_dict(analysis.get("metadata"))
    match_summary = as_dict(analysis.get("matchSummary"))
    activity = as_dict(analysis.get("activity"))
    activity_summary = as_dict(activity.get("summary"))
    behavior_summary = as_dict(as_dict(analysis.get("behavior")).get("summary"))
    final_turn = to_int(metadata.get("finalTurn"), 0)
    total_moves = to_int(activity_summary.get("totalMoves"), 0)
    total_builds = to_int(activity_summary.get("totalBuilds"), 0)
    total_kills = to_int(activity_summary.get("totalKills"), 0)
    total_upgrades = to_int(activity_summary.get("totalUpgrades"), 0)
    turns_per_match = final_turn if final_turn > 0 else 1

    participants = as_list(path_get(analysis, "source", "sessionContext", "participants"))
    participant_names = [
        participant.get("participantName")
        for participant in participants
        if isinstance(participant, dict) and participant.get("participantName")
    ]
    summary = (
        f"Cette partie oppose {' contre '.join(participant_names) if participant_names else 'deux participants'} "
        f"sur {format_number(final_turn)} tours, avec un volume d'action eleve et une telemetrie exploitable."
    )
    findings = [
        make_finding(
            title="La partie presente un volume d'action eleve",
            confidence="forte",
            conclusion=(
                f"Le match cumule {total_moves} mouvements, {total_builds} constructions, "
                f"{total_kills} captures et {total_upgrades} upgrades sur {final_turn} tours."
            ),
            evidence=[
                f"Moyenne de mouvements par tour: {format_number(safe_div(total_moves, turns_per_match), 2)}.",
                f"Moyenne de constructions par tour: {format_number(safe_div(total_builds, turns_per_match), 2)}.",
                f"Moyenne d'audits de commandes par record: {format_number(behavior_summary.get('averageCommandAuditCount'))}.",
            ],
        ),
        make_finding(
            title="La telemetrie est suffisamment riche pour une lecture comportementale",
            confidence="forte",
            conclusion=(
                f"La partie dispose de {format_number(match_summary.get('telemetryEventCount'))} evenements telemetry "
                f"et d'une duree moyenne de record de {format_number(match_summary.get('averageTurnDurationMs'))} ms."
            ),
            evidence=[
                f"Behavioral telemetry active: {format_number(metadata.get('behavioralTelemetryEnabled'))}.",
                f"Data collection active: {format_number(metadata.get('dataCollectionEnabled'))}.",
            ],
        ),
    ]
    return {
        "key": "overview",
        "title": "Vue d'ensemble du match",
        "summary": summary,
        "headlineMetrics": [
            headline_metric("Tours", final_turn),
            headline_metric("Records", metadata.get("recordCount")),
            headline_metric("Mouvements", total_moves),
            headline_metric("Constructions", total_builds),
            headline_metric("Captures", total_kills),
            headline_metric("Upgrades", total_upgrades),
        ],
        "findings": findings,
        "tables": [],
    }


def build_economy_section(analysis: dict[str, Any]) -> dict[str, Any]:
    economy_rows = as_list(path_get(analysis, "systems", "economy", "byKingdom"))
    by_kingdom: dict[str, list[dict[str, Any]]] = defaultdict(list)
    for row in economy_rows:
        if isinstance(row, dict):
            by_kingdom[key_or_fallback(row.get("kingdomKey"), "unknown")].append(row)

    table_rows = []
    average_gold_by_kingdom: dict[str, float] = {}
    bankruptcy_records = 0
    for kingdom_key, rows in sorted(by_kingdom.items()):
        avg_gold = safe_mean([to_int(row.get("gold"), 0) for row in rows])
        avg_net_income = safe_mean([to_int(row.get("netIncome"), 0) for row in rows])
        avg_upkeep = safe_mean([to_int(row.get("upkeepCost"), 0) for row in rows])
        bankrupt_count = sum(1 for row in rows if row.get("wouldBeBankrupt"))
        bankruptcy_records += bankrupt_count
        if avg_gold is not None:
            average_gold_by_kingdom[kingdom_key] = avg_gold
        table_rows.append(
            {
                "kingdomKey": kingdom_key,
                "averageGold": format_number(avg_gold),
                "averageNetIncome": format_number(avg_net_income),
                "averageUpkeep": format_number(avg_upkeep),
                "bankruptcyRiskRecords": bankrupt_count,
            }
        )

    dominant_kingdom = None
    if average_gold_by_kingdom:
        dominant_kingdom = max(average_gold_by_kingdom, key=average_gold_by_kingdom.get)

    findings = []
    if dominant_kingdom is not None and len(average_gold_by_kingdom) >= 2:
        sorted_gold = sorted(average_gold_by_kingdom.items(), key=lambda item: item[1], reverse=True)
        spread = sorted_gold[0][1] - sorted_gold[-1][1]
        findings.append(
            make_finding(
                title="L'avantage economique reste lisible sur la duree",
                confidence="forte",
                conclusion=(
                    f"Le royaume {dominant_kingdom} maintient la moyenne d'or la plus elevee, avec un ecart moyen d'environ {format_number(spread)} or."
                ),
                evidence=[
                    f"Moyennes d'or observees: {', '.join(f'{kingdom}={format_number(value)}' for kingdom, value in sorted_gold)}.",
                    f"Records a risque de faillite observes: {bankruptcy_records}.",
                ],
            )
        )
    if bankruptcy_records == 0:
        findings.append(
            make_finding(
                title="La pression economique ne mene pas a la faillite sur cet echantillon",
                confidence="forte",
                conclusion="Aucun record n'indique un etat de faillite projetee, meme lorsque les autres systemes aleatoires restent actifs.",
                evidence=[
                    "Le drapeau wouldBeBankrupt reste absent de toutes les lignes economie par royaume.",
                ],
            )
        )

    return {
        "key": "economy",
        "title": "Economie et pression de ressources",
        "summary": "L'economie sert de ligne de base pour voir si les evenements aleatoires creent une pression soutenable ou des a-coups visibles.",
        "headlineMetrics": [
            headline_metric("Records economie", len(economy_rows)),
            headline_metric("Records a risque de faillite", bankruptcy_records),
            headline_metric("Royaume moyen le plus riche", dominant_kingdom or "n/a"),
        ],
        "findings": findings,
        "tables": [
            build_table(
                "Synthese economie par royaume",
                [
                    ("kingdomKey", "Royaume"),
                    ("averageGold", "Or moyen"),
                    ("averageNetIncome", "Revenu net moyen"),
                    ("averageUpkeep", "Upkeep moyen"),
                    ("bankruptcyRiskRecords", "Records a risque"),
                ],
                table_rows,
            )
        ],
    }


def build_weather_section(analysis: dict[str, Any]) -> dict[str, Any]:
    weather = as_dict(path_get(analysis, "systems", "weather"))
    points = as_list(weather.get("points"))
    summary = as_dict(weather.get("summary"))
    quartile_rows = []
    for label, rows in split_into_quartiles(points):
        active_ratio = safe_div(sum(1 for row in rows if row.get("hasActiveFront")), len(rows))
        overlap_ratio = safe_div(sum(1 for row in rows if to_int(row.get("frontCount"), 0) > 1), len(rows))
        quartile_rows.append(
            {
                "quartile": label,
                "recordCount": len(rows),
                "activeRatio": format_percent(active_ratio),
                "averageFogCellCount": format_number(safe_mean([to_int(row.get("fogCellCount"), 0) for row in rows])),
                "overlapRatio": format_percent(overlap_ratio),
            }
        )

    first_quartile = as_dict(quartile_rows[0]) if quartile_rows else {}
    last_quartile = as_dict(quartile_rows[-1]) if quartile_rows else {}
    active_ratio_total = safe_div(to_int(summary.get("activeFrontRecordCount"), 0), len(points))
    overlap_count = sum(1 for row in points if to_int(row.get("frontCount"), 0) > 1)
    last_active_ratio_raw = parse_percent(last_quartile.get("activeRatio"))
    first_active_ratio_raw = parse_percent(first_quartile.get("activeRatio"))
    findings = []

    if last_active_ratio_raw is not None and first_active_ratio_raw is not None:
        findings.append(
            make_finding(
                title="La meteo devient structurelle en fin de partie",
                confidence=confidence_label(len(points)),
                conclusion=(
                    f"Le front actif passe d'environ {first_quartile.get('activeRatio', 'n/a')} dans le premier quartile "
                    f"a {last_quartile.get('activeRatio', 'n/a')} dans le dernier, ce qui signale une presence beaucoup plus continue en fin de match."
                ),
                evidence=[
                    f"Taux d'activite global des fronts: {format_percent(active_ratio_total)}.",
                    f"Fog cells moyennes: {format_number(summary.get('averageFogCellCount'))}.",
                ],
            )
        )
    if overlap_count > 0:
        findings.append(
            make_finding(
                title="Les fronts peuvent se chevaucher et densifier la pression visuelle",
                confidence=confidence_label(overlap_count),
                conclusion=(
                    f"{overlap_count} records montrent plus d'un front simultane, ce qui renforce les phases de brouillard dense plutot que de simples episodes ponctuels."
                ),
                evidence=[
                    f"Maximum de fronts simultanes observes: {format_number(summary.get('maxSimultaneousFronts'))}.",
                ],
            )
        )

    return {
        "key": "weather",
        "title": "Meteo et brouillard aleatoire",
        "summary": "La meteo est etudiee comme un systeme aleatoire de pression sur la visibilite, puis reliee au rythme d'action des joueurs.",
        "headlineMetrics": [
            headline_metric("Records meteo", len(points)),
            percent_metric("Records avec front actif", active_ratio_total),
            headline_metric("Max fronts simultanes", summary.get("maxSimultaneousFronts")),
            headline_metric("Records avec chevauchement", overlap_count),
        ],
        "findings": findings,
        "tables": [
            build_table(
                "Regimes meteo par quartile de match",
                [
                    ("quartile", "Quartile"),
                    ("recordCount", "Records"),
                    ("activeRatio", "Front actif"),
                    ("averageFogCellCount", "Fog moyen"),
                    ("overlapRatio", "Chevauchement"),
                ],
                quartile_rows,
            )
        ],
    }


def build_chest_section(analysis: dict[str, Any]) -> dict[str, Any]:
    chest = as_dict(path_get(analysis, "systems", "chest"))
    chest_config = as_dict(path_get(analysis, "config", "chest"))
    spawns = as_list(chest.get("spawnEvents"))
    opens = as_list(chest.get("openEvents"))
    reward_map = rows_to_count_map(as_list(chest.get("rewardTypeCounts")))
    pickup_rows = compute_chest_pickup_rows(spawns, opens)
    pickup_turns = [row["pickupLagTurns"] for row in pickup_rows]
    reward_rows = [
        {"rewardTypeKey": reward_type_key, "count": count}
        for reward_type_key, count in sorted(reward_map.items())
    ]
    phase_comparison_rows = build_chest_phase_comparison_rows(chest_config, opens)
    bonus_rewards = sum(
        count
        for reward_type_key, count in reward_map.items()
        if reward_type_key.endswith("_bonus")
    )
    open_rate = safe_div(len(opens), len(spawns))
    findings = []

    if reward_map:
        dominant_reward = max(reward_map, key=reward_map.get)
        findings.append(
            make_finding(
                title="Les coffres favorisent surtout des bonus de capacite sur cette partie",
                confidence=confidence_label(len(opens)),
                conclusion=(
                    f"Le type de recompense le plus observe est {dominant_reward}, avec {reward_map[dominant_reward]} occurrences."
                ),
                evidence=[
                    f"Part des recompenses de bonus (mouvement/build): {format_percent(safe_div(bonus_rewards, len(opens)))}.",
                    f"Taux d'ouverture par spawn: {format_percent(open_rate)}.",
                ],
            )
        )
    late_phase_row = next(
        (row for row in phase_comparison_rows if row.get("phase") == "late"),
        None,
    )
    if late_phase_row is not None and to_int(late_phase_row.get("openCount"), 0) > 0:
        findings.append(
            make_finding(
                title="La phase tardive des coffres colle tres bien aux poids configures",
                confidence=confidence_label(to_int(late_phase_row.get("openCount"), 0)),
                conclusion=(
                    "La distribution observee en phase tardive reproduit exactement la repartition attendue entre or, bonus de mouvement et bonus de build."
                ),
                evidence=[
                    f"Poids configures tardifs: {late_phase_row.get('expectedMixSummary', 'n/a')}.",
                    f"Mix observe tardif: {late_phase_row.get('observedMixSummary', 'n/a')}.",
                    f"Nombre d'ouvertures en phase tardive: {late_phase_row.get('openCount', 'n/a')}.",
                ],
            )
        )
    if pickup_rows:
        findings.append(
            make_finding(
                title="La collecte des coffres alterne entre opportunisme rapide et attente longue",
                confidence=confidence_label(len(pickup_rows)),
                conclusion=(
                    f"Le delai moyen avant collecte est de {format_number(safe_mean(pickup_turns))} tours intermediaires, avec un maximum observe de {format_number(safe_max(pickup_turns))} tours intermediaires."
                ),
                evidence=[
                    f"Nombre de coffres correctement apparies spawn->ouverture: {len(pickup_rows)}.",
                    "Le delai est mesure ici comme le nombre de tours complets entre le spawn et l'ouverture, hors tour du spawn et tour de collecte.",
                ],
            )
        )

    return {
        "key": "chest",
        "title": "Coffres et recompenses aleatoires",
        "summary": "Les coffres sont analyses comme un systeme de gratification aleatoire: quelle recompense sort, et a quel rythme les joueurs la convertissent en actions.",
        "headlineMetrics": [
            headline_metric("Spawns de coffres", len(spawns)),
            headline_metric("Ouvertures de coffres", len(opens)),
            percent_metric("Taux d'ouverture", open_rate),
            headline_metric("Delai max avant collecte (tours intermediaires)", safe_max(pickup_turns, 0)),
        ],
        "findings": findings,
        "tables": [
            build_table(
                "Distribution des recompenses de coffres",
                [("rewardTypeKey", "Type de recompense"), ("count", "Occurrences")],
                reward_rows,
            ),
            build_table(
                "Comparaison observe vs configure par phase de coffre",
                [
                    ("phase", "Phase"),
                    ("openCount", "Ouvertures"),
                    ("expectedMixSummary", "Mix configure"),
                    ("observedMixSummary", "Mix observe"),
                ],
                phase_comparison_rows,
            ),
            build_table(
                "Delais de collecte des coffres",
                [
                    ("spawnTurn", "Tour spawn"),
                    ("openTurn", "Tour ouverture"),
                    ("turnSpan", "Ecart brut (tours)"),
                    ("pickupLagTurns", "Tours intermediaires"),
                    ("pickupDelayRecords", "Delai (records)"),
                    ("rewardTypeKey", "Recompense"),
                ],
                pickup_rows,
            ),
        ],
    }


def build_infernal_section(analysis: dict[str, Any]) -> dict[str, Any]:
    infernal = as_dict(path_get(analysis, "systems", "infernal"))
    points = as_list(infernal.get("points"))
    spawns = as_list(infernal.get("spawnEvents"))
    lifespans = as_list(infernal.get("lifespans"))
    spawn_rows = build_infernal_spawn_rows(points, spawns)
    positive_next_intervals = [
        row["nextSpawnIntervalTurns"]
        for row in spawn_rows
        if row.get("nextSpawnIntervalTurns") is not None and row.get("targetDebtAtSpawn", 0) > 0
    ]
    zero_next_intervals = [
        row["nextSpawnIntervalTurns"]
        for row in spawn_rows
        if row.get("nextSpawnIntervalTurns") is not None and row.get("targetDebtAtSpawn", 0) == 0
    ]
    target_counts = rows_to_count_map(as_list(infernal.get("targetKingdomCounts")))
    findings = []

    if positive_next_intervals:
        interval_text = format_number(safe_mean(positive_next_intervals))
        zero_text = format_number(safe_mean(zero_next_intervals)) if zero_next_intervals else "n/a"
        findings.append(
            make_finding(
                title="La dette infernale semble comprimer la cadence de spawn",
                confidence=confidence_label(len(positive_next_intervals)),
                conclusion=(
                    f"Quand la cible porte deja une dette au moment d'un spawn, l'intervalle moyen jusqu'au spawn suivant tombe a {interval_text} tours, contre {zero_text} lorsqu'il n'y a pas de dette cible."
                ),
                evidence=[
                    f"Nombre de spawns observes: {len(spawns)}.",
                    f"Lifetime moyen observe: {format_number(safe_mean([to_int(row.get('observedLifetime'), 0) for row in lifespans]))} tours.",
                    "La mesure porte ici sur la prochaine cadence observee apres le spawn, ce qui capture mieux l'effet de la dette cible sur la boucle infernale.",
                ],
            )
        )
    if target_counts:
        dominant_target = max(target_counts, key=target_counts.get)
        findings.append(
            make_finding(
                title="La pression infernale ne se repartit pas forcement de facon neutre",
                confidence=confidence_label(sum(target_counts.values())),
                conclusion=(
                    f"Le royaume {dominant_target} est la cible la plus frequente des infernaux sur cet echantillon."
                ),
                evidence=[
                    f"Repartition des cibles: {', '.join(f'{key}={value}' for key, value in sorted(target_counts.items()))}.",
                ],
            )
        )

    return {
        "key": "infernal",
        "title": "Systeme infernal et dette de sang",
        "summary": "Le systeme infernal est lu comme une boucle de pression: la dette accumulee doit pouvoir influencer la cadence et la cible des manifestations.",
        "headlineMetrics": [
            headline_metric("Spawns infernaux", len(spawns)),
            headline_metric("Removals infernaux", len(as_list(infernal.get("removeEvents")))),
            headline_metric("Lifetime moyen", safe_mean([to_int(row.get("observedLifetime"), 0) for row in lifespans])),
            headline_metric("Cible dominante", max(target_counts, key=target_counts.get) if target_counts else "n/a"),
        ],
        "findings": findings,
        "tables": [
            build_table(
                "Cadence et dette au moment des spawns infernaux",
                [
                    ("turn", "Tour"),
                    ("targetKingdomKey", "Cible"),
                    ("targetDebtAtSpawn", "Dette cible"),
                    ("otherDebtAtSpawn", "Dette autre"),
                    ("totalDebtAtSpawn", "Dette totale"),
                    ("spawnIntervalTurns", "Intervalle depuis spawn precedent"),
                    ("nextSpawnIntervalTurns", "Intervalle jusqu'au spawn suivant"),
                ],
                spawn_rows,
            )
        ],
    }


def build_behavior_section(analysis: dict[str, Any]) -> dict[str, Any]:
    behavior = as_dict(analysis.get("behavior"))
    summary = as_dict(behavior.get("summary"))
    weather_comparison = as_dict(path_get(analysis, "correlations", "weatherActiveVsInactive"))
    infernal_comparison = as_dict(path_get(analysis, "correlations", "infernalActiveVsInactive"))
    weather_finding = build_regime_finding(
        weather_comparison,
        "Sous front actif, le comportement change de regime",
        "weather",
    )
    infernal_finding = build_regime_finding(
        infernal_comparison,
        "La presence infernale modifie le rythme de decision",
        "infernal",
    )
    findings = [finding for finding in (weather_finding, infernal_finding) if finding]
    findings.append(
        make_finding(
            title="Les joueurs reajustent regulierement leurs plans",
            confidence="forte",
            conclusion=(
                f"Le match contient {format_number(summary.get('totalReplaceCount'))} remplacements de commandes et {format_number(summary.get('totalCancelCount'))} annulations, signe d'un pilotage iteratif plutot que lineaire."
            ),
            evidence=[
                f"Delai moyen premiere action: {format_number(summary.get('averageFirstActionDelayMs'))} ms.",
                f"Delai moyen commit: {format_number(summary.get('averageCommitDelayMs'))} ms.",
            ],
        )
    )

    return {
        "key": "behavior",
        "title": "Comportement joueur et charge de decision",
        "summary": "La telemetrie de comportement permet de voir si les systemes aleatoires changent la densite d'actions et le temps de decision.",
        "headlineMetrics": [
            headline_metric("Audit moyen par record", summary.get("averageCommandAuditCount")),
            headline_metric("Interactions moyennes par record", summary.get("averageInteractionEventCount")),
            headline_metric("Delai moyen premiere action (ms)", summary.get("averageFirstActionDelayMs")),
            headline_metric("Delai moyen commit (ms)", summary.get("averageCommitDelayMs")),
        ],
        "findings": findings,
        "tables": [
            build_table(
                "Comparaison de regimes meteo",
                [
                    ("metric", "Metrique"),
                    ("activeFront", "Front actif"),
                    ("noFront", "Sans front"),
                    ("delta", "Delta actif - sans front"),
                ],
                build_regime_table_rows(weather_comparison),
            ),
            build_table(
                "Comparaison de regimes infernaux",
                [
                    ("metric", "Metrique"),
                    ("activeInfernal", "Infernal actif"),
                    ("noInfernal", "Sans infernal"),
                    ("delta", "Delta actif - sans infernal"),
                ],
                build_regime_table_rows(infernal_comparison),
            ),
        ],
    }


def build_cross_effects_section(analysis: dict[str, Any]) -> dict[str, Any]:
    correlations = as_dict(analysis.get("correlations"))
    weather_windows = as_list(correlations.get("weatherFrontSpawnWindows"))
    chest_windows = as_list(correlations.get("chestOpenWindows"))
    infernal_windows = as_list(correlations.get("infernalSpawnWindows"))

    weather_delta = summarize_window_deltas(weather_windows)
    chest_delta = summarize_window_deltas(chest_windows)
    infernal_delta = summarize_window_deltas(infernal_windows)

    findings = []
    for title, window_rows, delta_summary in (
        ("Apres les ouvertures de coffres", chest_windows, chest_delta),
        ("Apres les spawns infernaux", infernal_windows, infernal_delta),
        ("Apres les fronts meteo", weather_windows, weather_delta),
    ):
        if not delta_summary:
            continue
        findings.append(
            make_finding(
                title=title,
                confidence=confidence_label(len(window_rows)),
                conclusion=(
                    f"Le delta moyen le plus saillant porte sur {delta_summary['strongestMetricLabel']}, avec un ecart moyen de {delta_summary['strongestMetricValue']}."
                ),
                evidence=[
                    f"Fenetre moyenne delta mouvements: {delta_summary['deltaAverageMoveCount']}.",
                    f"Fenetre moyenne delta constructions: {delta_summary['deltaAverageBuildCount']}.",
                    f"Fenetre moyenne delta audits de commandes: {delta_summary['deltaAverageCommandAuditCount']}.",
                ],
                caveat="Les fenetres restent corrrelationnelles: elles montrent ce qui se passe juste avant/apres, pas une causalite prouvee.",
            )
        )

    return {
        "key": "cross_effects",
        "title": "Effets croises entre aleatoire et comportement",
        "summary": "Cette section croise les fenetres avant/apres evenement pour voir si un systeme aleatoire modifie le tempo observable juste autour de son apparition.",
        "headlineMetrics": [
            headline_metric("Fenetres meteo", len(weather_windows)),
            headline_metric("Fenetres coffres", len(chest_windows)),
            headline_metric("Fenetres infernales", len(infernal_windows)),
        ],
        "findings": findings,
        "tables": [
            build_table(
                "Deltas moyens apres evenement",
                [
                    ("windowLabel", "Evenement"),
                    ("deltaAverageMoveCount", "Delta moves"),
                    ("deltaAverageBuildCount", "Delta builds"),
                    ("deltaAverageCommandAuditCount", "Delta audits"),
                    ("deltaAverageFogCellCount", "Delta fog"),
                ],
                build_window_summary_rows(
                    {
                        "weather_front_spawn": weather_windows,
                        "chest_opened": chest_windows,
                        "infernal_spawned": infernal_windows,
                    }
                ),
            )
        ],
    }


def build_limitations_section(analysis: dict[str, Any]) -> dict[str, Any]:
    limitations = as_list(analysis.get("limitations"))
    limitation_rows = [
        {
            "scope": key_or_fallback(limitation.get("scope"), "unknown"),
            "level": key_or_fallback(limitation.get("level"), "unknown"),
            "message": key_or_fallback(limitation.get("message"), "Aucune description."),
        }
        for limitation in limitations
        if isinstance(limitation, dict)
    ]
    if not limitation_rows:
        limitation_rows = [{"scope": "global", "level": "info", "message": "Aucune limitation declaree."}]
    return {
        "key": "limitations",
        "title": "Limites d'interpretation",
        "summary": "Le rapport doit rester credibile: les limites des donnees sont explicites et ne doivent pas etre masquees par les graphiques.",
        "headlineMetrics": [
            headline_metric("Nombre de limitations", len(limitation_rows)),
        ],
        "findings": [
            make_finding(
                title="Certaines conclusions doivent rester prudentes",
                confidence="forte",
                conclusion="Les comparaisons de fenetres et de regimes sont utiles pour produire des hypotheses solides, mais elles ne valent pas preuve causale a elles seules.",
                evidence=[
                    "Le JSON d'analyse declare explicitement des limites sur la causalite.",
                    f"Nombre de limitations source remontees: {len(limitation_rows)}.",
                ],
            )
        ],
        "tables": [
            build_table(
                "Limitations source", 
                [("scope", "Scope"), ("level", "Niveau"), ("message", "Message")],
                limitation_rows,
            )
        ],
    }


def build_regime_finding(comparison: dict[str, Any], title: str, regime_key: str) -> dict[str, Any] | None:
    true_summary = as_dict(comparison.get("trueSummary"))
    false_summary = as_dict(comparison.get("falseSummary"))
    deltas = as_dict(comparison.get("trueMinusFalse"))
    if not true_summary or not false_summary:
        return None

    strongest_key = None
    strongest_value = 0.0
    for candidate_key in (
        "deltaAverageMoveCount",
        "deltaAverageBuildCount",
        "deltaAverageCommandAuditCount",
        "deltaAverageInteractionEventCount",
        "deltaAverageFogCellCount",
    ):
        value = to_float(deltas.get(candidate_key), 0.0)
        if abs(value) > abs(strongest_value):
            strongest_key = candidate_key
            strongest_value = value

    if strongest_key is None:
        return None

    metric_label = metric_display_name(strongest_key)
    true_count = to_int(true_summary.get("recordCount"), 0)
    false_count = to_int(false_summary.get("recordCount"), 0)
    return make_finding(
        title=title,
        confidence=confidence_label(min(true_count, false_count)),
        conclusion=(
            f"Le regime {regime_key} a l'effet moyen le plus visible sur {metric_label}, avec un delta moyen de {format_number(strongest_value)} entre l'etat actif et l'etat inactif."
        ),
        evidence=[
            f"Records actifs: {true_count}, records inactifs: {false_count}.",
            f"Delta moves: {format_number(deltas.get('deltaAverageMoveCount'))}.",
            f"Delta builds: {format_number(deltas.get('deltaAverageBuildCount'))}.",
            f"Delta audits: {format_number(deltas.get('deltaAverageCommandAuditCount'))}.",
        ],
    )


def build_regime_table_rows(comparison: dict[str, Any]) -> list[dict[str, Any]]:
    true_summary = as_dict(comparison.get("trueSummary"))
    false_summary = as_dict(comparison.get("falseSummary"))
    deltas = as_dict(comparison.get("trueMinusFalse"))
    return [
        {
            "metric": label,
            "activeFront": format_number(true_summary.get(true_key)),
            "noFront": format_number(false_summary.get(true_key)),
            "delta": format_number(deltas.get(delta_key)),
            "activeInfernal": format_number(true_summary.get(true_key)),
            "noInfernal": format_number(false_summary.get(true_key)),
        }
        for label, true_key, delta_key in (
            ("Moves moyens", "averageMoveCount", "deltaAverageMoveCount"),
            ("Builds moyens", "averageBuildCount", "deltaAverageBuildCount"),
            ("Audits moyens", "averageCommandAuditCount", "deltaAverageCommandAuditCount"),
            ("Interactions moyennes", "averageInteractionEventCount", "deltaAverageInteractionEventCount"),
        )
    ]


def build_window_summary_rows(window_groups: dict[str, list[dict[str, Any]]]) -> list[dict[str, Any]]:
    rows = []
    for label, windows in window_groups.items():
        summary = summarize_window_deltas(windows)
        rows.append(
            {
                "windowLabel": label,
                "deltaAverageMoveCount": summary.get("deltaAverageMoveCount", "n/a"),
                "deltaAverageBuildCount": summary.get("deltaAverageBuildCount", "n/a"),
                "deltaAverageCommandAuditCount": summary.get("deltaAverageCommandAuditCount", "n/a"),
                "deltaAverageFogCellCount": summary.get("deltaAverageFogCellCount", "n/a"),
            }
        )
    return rows


def summarize_window_deltas(windows: list[dict[str, Any]]) -> dict[str, Any]:
    if not windows:
        return {}
    metrics = {
        "deltaAverageMoveCount": [],
        "deltaAverageBuildCount": [],
        "deltaAverageCommandAuditCount": [],
        "deltaAverageFogCellCount": [],
    }
    for window in windows:
        delta_row = as_dict(window.get("afterMinusBefore"))
        for key in metrics:
            value = to_float(delta_row.get(key), None)
            if value is not None:
                metrics[key].append(value)

    result = {key: format_number(safe_mean(values)) for key, values in metrics.items()}
    strongest_key = None
    strongest_value = 0.0
    for key, values in metrics.items():
        mean_value = safe_mean(values)
        if mean_value is None:
            continue
        if abs(mean_value) > abs(strongest_value):
            strongest_key = key
            strongest_value = mean_value

    result["strongestMetricLabel"] = metric_display_name(strongest_key) if strongest_key else "n/a"
    result["strongestMetricValue"] = format_number(strongest_value)
    return result


def build_infernal_spawn_rows(
    points: list[dict[str, Any]], spawns: list[dict[str, Any]]
) -> list[dict[str, Any]]:
    points_by_sequence = {
        to_int(point.get("sequenceIndex"), -1): point
        for point in points
        if isinstance(point, dict)
    }
    rows = []
    previous_turn = None
    for spawn in spawns:
        if not isinstance(spawn, dict):
            continue
        sequence_index = to_int(spawn.get("sequenceIndex"), -1)
        point = as_dict(points_by_sequence.get(sequence_index))
        target_kingdom_key = key_or_fallback(spawn.get("targetKingdomKey"), "unknown")
        white_debt = to_int(point.get("whiteBloodDebt"), 0)
        black_debt = to_int(point.get("blackBloodDebt"), 0)
        target_debt = white_debt if target_kingdom_key == "white" else black_debt if target_kingdom_key == "black" else 0
        other_debt = black_debt if target_kingdom_key == "white" else white_debt if target_kingdom_key == "black" else white_debt + black_debt - target_debt
        turn = to_int(spawn.get("turn"), 0)
        rows.append(
            {
                "turn": turn,
                "targetKingdomKey": target_kingdom_key,
                "targetDebtAtSpawn": target_debt,
                "otherDebtAtSpawn": other_debt,
                "totalDebtAtSpawn": white_debt + black_debt,
                "spawnIntervalTurns": None if previous_turn is None else turn - previous_turn,
                "nextSpawnIntervalTurns": None,
            }
        )
        previous_turn = turn

    for index in range(len(rows) - 1):
        rows[index]["nextSpawnIntervalTurns"] = rows[index + 1]["turn"] - rows[index]["turn"]
    return rows


def compute_chest_pickup_rows(
    spawns: list[dict[str, Any]], opens: list[dict[str, Any]]
) -> list[dict[str, Any]]:
    pickup_rows = []
    used_open_indexes: set[int] = set()
    for spawn_index, spawn in enumerate(spawns):
        if not isinstance(spawn, dict):
            continue
        next_spawn_sequence = None
        if spawn_index + 1 < len(spawns) and isinstance(spawns[spawn_index + 1], dict):
            next_spawn_sequence = to_int(spawns[spawn_index + 1].get("sequenceIndex"), None)

        matched_index = None
        matched_open = None
        spawn_sequence = to_int(spawn.get("sequenceIndex"), -1)
        for open_index, open_row in enumerate(opens):
            if open_index in used_open_indexes or not isinstance(open_row, dict):
                continue
            open_sequence = to_int(open_row.get("sequenceIndex"), -1)
            if open_sequence <= spawn_sequence:
                continue
            if next_spawn_sequence is not None and open_sequence >= next_spawn_sequence:
                continue
            matched_index = open_index
            matched_open = open_row
            break

        if matched_index is None or matched_open is None:
            continue

        used_open_indexes.add(matched_index)
        spawn_turn = to_int(spawn.get("turn"), 0)
        open_turn = to_int(matched_open.get("turn"), 0)
        pickup_rows.append(
            {
                "spawnTurn": spawn_turn,
                "openTurn": open_turn,
                "turnSpan": max(0, open_turn - spawn_turn),
                "pickupLagTurns": max(0, open_turn - spawn_turn - 1),
                "pickupDelayRecords": max(
                    0,
                    to_int(matched_open.get("sequenceIndex"), 0)
                    - to_int(spawn.get("sequenceIndex"), 0),
                ),
                "rewardTypeKey": key_or_fallback(matched_open.get("rewardTypeKey"), "unknown"),
            }
        )

    return pickup_rows


def build_chest_phase_comparison_rows(
    chest_config: dict[str, Any],
    opens: list[dict[str, Any]],
) -> list[dict[str, Any]]:
    late_game_turn = to_int(chest_config.get("lateGameTurn"), 0)
    phase_rows = []
    for phase, weights in (
        ("early", as_dict(chest_config.get("earlyWeights"))),
        ("late", as_dict(chest_config.get("lateWeights"))),
    ):
        phase_opens = [
            open_row
            for open_row in opens
            if isinstance(open_row, dict)
            and (
                to_int(open_row.get("turn"), 0) < late_game_turn
                if phase == "early"
                else to_int(open_row.get("turn"), 0) >= late_game_turn
            )
        ]
        observed_counts = {"gold": 0, "movementBonus": 0, "buildBonus": 0}
        for open_row in phase_opens:
            family = reward_type_to_family(key_or_fallback(open_row.get("rewardTypeKey"), "unknown"))
            if family in observed_counts:
                observed_counts[family] += 1

        phase_rows.append(
            {
                "phase": phase,
                "openCount": len(phase_opens),
                "expectedMixSummary": summarize_weight_mix(weights),
                "observedMixSummary": summarize_observed_mix(observed_counts, len(phase_opens)),
            }
        )
    return phase_rows


def reward_type_to_family(reward_type_key: str) -> str:
    mapping = {
        "gold": "gold",
        "movement_points_max_bonus": "movementBonus",
        "build_points_max_bonus": "buildBonus",
    }
    return mapping.get(reward_type_key, reward_type_key)


def summarize_weight_mix(weights: dict[str, Any]) -> str:
    total = sum(to_int(value, 0) for value in weights.values())
    if total <= 0:
        return "n/a"
    return ", ".join(
        f"{key}={format_percent(safe_div(to_int(value, 0), total))}"
        for key, value in sorted(weights.items())
    )


def summarize_observed_mix(observed_counts: dict[str, int], total: int) -> str:
    if total <= 0:
        return "n/a"
    return ", ".join(
        f"{key}={format_percent(safe_div(value, total))}"
        for key, value in sorted(observed_counts.items())
    )


def split_into_quartiles(rows: list[dict[str, Any]]) -> list[tuple[str, list[dict[str, Any]]]]:
    result = []
    if not rows:
        return result
    total = len(rows)
    for quartile_index in range(4):
        start = int(quartile_index * total / 4)
        end = int((quartile_index + 1) * total / 4)
        label = f"Q{quartile_index + 1}"
        result.append((label, rows[start:end]))
    return result


def metric_display_name(key: str | None) -> str:
    labels = {
        "deltaAverageMoveCount": "le nombre moyen de mouvements",
        "deltaAverageBuildCount": "le nombre moyen de constructions",
        "deltaAverageCommandAuditCount": "le nombre moyen d'audits de commandes",
        "deltaAverageInteractionEventCount": "le nombre moyen d'interactions",
        "deltaAverageFogCellCount": "le volume moyen de brouillard",
    }
    if key is None:
        return "la metrique dominante"
    return labels.get(key, key)


def make_finding(
    title: str,
    confidence: str,
    conclusion: str,
    evidence: list[str],
    caveat: str | None = None,
) -> dict[str, Any]:
    return {
        "title": title,
        "confidence": confidence,
        "conclusion": conclusion,
        "evidence": evidence,
        "caveat": caveat,
    }


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


def parse_percent(value: Any) -> float | None:
    if value is None:
        return None
    text = str(value).strip()
    if not text.endswith("%"):
        return to_float(text, None)
    try:
        return float(text[:-1]) / 100.0
    except ValueError:
        return None