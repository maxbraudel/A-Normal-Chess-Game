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
        build_terrain_section(analysis),
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


def build_terrain_section(analysis: dict[str, Any]) -> dict[str, Any]:
    terrain = as_dict(path_get(analysis, "systems", "terrain"))
    summary = as_dict(terrain.get("summary"))
    phase_rows = [row for row in as_list(terrain.get("phaseRows")) if isinstance(row, dict)]
    kingdom_rows = [row for row in as_list(terrain.get("byKingdom")) if isinstance(row, dict)]
    piece_rows = [row for row in as_list(terrain.get("byPieceType")) if isinstance(row, dict)]
    top_pressure_rows = [row for row in as_list(terrain.get("topPressureRecords")) if isinstance(row, dict)]

    first_phase = as_dict(phase_rows[0]) if phase_rows else {}
    peak_phase = (
        max(
            phase_rows,
            key=lambda row: to_float(row.get("averageLostDestinationCount"), 0.0) or 0.0,
        )
        if phase_rows
        else {}
    )
    long_range_rows = [
        row for row in piece_rows if key_or_fallback(row.get("pieceTypeKey"), "unknown") in {"bishop", "rook", "queen"}
    ]
    total_lost_destinations = sum(to_int(row.get("lostDestinationCount"), 0) for row in piece_rows)
    long_range_lost_destinations = sum(
        to_int(row.get("lostDestinationCount"), 0) for row in long_range_rows
    )
    most_impacted_piece = (
        max(
            piece_rows,
            key=lambda row: to_float(row.get("lostMobilityShare"), 0.0) or 0.0,
        )
        if piece_rows
        else {}
    )
    findings = []

    if first_phase and peak_phase:
        findings.append(
            make_finding(
                title="L'eau devient une vraie taxe de mobilite apres l'ouverture",
                confidence=confidence_label(len(as_list(terrain.get("perRecordActiveKingdom")))),
                conclusion=(
                    f"La perte moyenne passe de {format_number(first_phase.get('averageLostDestinationCount'))} destination par record en {first_phase.get('quartile', 'Q1')} "
                    f"a {format_number(peak_phase.get('averageLostDestinationCount'))} en {peak_phase.get('quartile', 'Q?')}, soit {format_percent(to_float(peak_phase.get('averageLostMobilityShare'), None))} de la mobilite seche disponible."
                ),
                evidence=[
                    f"Couverture eau de la carte jouable: {format_percent(to_float(summary.get('waterCoverageOfInCircle'), None))}.",
                    f"Perte moyenne globale: {format_number(summary.get('averageActiveLostDestinationCount'))} destinations par record actif.",
                    f"Nombre moyen de pieces impactees: {format_number(summary.get('averageImpactedPieceCount'))}.",
                ],
                caveat=(
                    "Le calcul compare chaque snapshot a une version seche du meme plateau; quelques etats speciaux de breche de mur restent approximes."
                    if to_int(summary.get("wallBreachPieceSampleCount"), 0) > 0
                    else "Le calcul compare chaque snapshot a une version seche du meme plateau, a occupation identique."
                ),
            )
        )

    if piece_rows:
        findings.append(
            make_finding(
                title="La penalite touche surtout les pieces de longue portee",
                confidence=confidence_label(sum(to_int(row.get("activePieceSampleCount"), 0) for row in piece_rows)),
                conclusion=(
                    f"Les fous, tours et dames concentrent {format_percent(safe_div(long_range_lost_destinations, total_lost_destinations))} des destinations perdues a cause de l'eau, alors que les pions n'en perdent aucune sur cet echantillon."
                ),
                evidence=[
                    f"Piece la plus penalisee en part relative: {key_or_fallback(most_impacted_piece.get('pieceTypeLabel'), 'n/a')} ({format_percent(to_float(most_impacted_piece.get('lostMobilityShare'), None))}).",
                    f"Fous: {format_percent(to_float(next((row.get('lostMobilityShare') for row in piece_rows if row.get('pieceTypeKey') == 'bishop'), None), None))} de mobilite seche perdue.",
                    f"Tours: {format_percent(to_float(next((row.get('lostMobilityShare') for row in piece_rows if row.get('pieceTypeKey') == 'rook'), None), None))} de mobilite seche perdue.",
                    f"Dames: {format_percent(to_float(next((row.get('lostMobilityShare') for row in piece_rows if row.get('pieceTypeKey') == 'queen'), None), None))} de mobilite seche perdue.",
                ],
            )
        )

    return {
        "key": "terrain",
        "title": "Terrain aleatoire et taxe de mobilite",
        "summary": "Le terrain genere aleatoirement n'est pas un simple decor: ici, l'eau retire durablement des destinations de mouvement et penalise surtout les pieces qui vivent sur des lignes ouvertes.",
        "headlineMetrics": [
            headline_metric("Cellules eau", summary.get("waterCellCount")),
            percent_metric("Carte jouable en eau", to_float(summary.get("waterCoverageOfInCircle"), None)),
            headline_metric("Destinations perdues moyennes", summary.get("averageActiveLostDestinationCount")),
            percent_metric("Mobilite seche perdue", to_float(summary.get("averageActiveLostMobilityShare"), None)),
        ],
        "findings": findings,
        "tables": [
            build_table(
                "Taxe eau par quartile de match",
                [
                    ("quartile", "Quartile"),
                    ("recordCount", "Records"),
                    ("averageLostDestinationCount", "Destinations perdues moyennes"),
                    ("averageLostMobilityShare", "Part de mobilite seche perdue"),
                    ("averageImpactedPieceCount", "Pieces impactees moyennes"),
                ],
                [
                    {
                        "quartile": row.get("quartile"),
                        "recordCount": row.get("recordCount"),
                        "averageLostDestinationCount": format_number(row.get("averageLostDestinationCount")),
                        "averageLostMobilityShare": format_percent(to_float(row.get("averageLostMobilityShare"), None)),
                        "averageImpactedPieceCount": format_number(row.get("averageImpactedPieceCount")),
                    }
                    for row in phase_rows
                ],
            ),
            build_table(
                "Sensibilite a l'eau par type de piece",
                [
                    ("pieceTypeLabel", "Piece"),
                    ("activePieceSampleCount", "Echantillons actifs"),
                    ("lostDestinationCount", "Destinations perdues"),
                    ("lostMobilityShare", "Part de mobilite perdue"),
                    ("impactedPieceSampleRatio", "Taux d'impact"),
                ],
                [
                    {
                        "pieceTypeLabel": row.get("pieceTypeLabel"),
                        "activePieceSampleCount": row.get("activePieceSampleCount"),
                        "lostDestinationCount": row.get("lostDestinationCount"),
                        "lostMobilityShare": format_percent(to_float(row.get("lostMobilityShare"), None)),
                        "impactedPieceSampleRatio": format_percent(to_float(row.get("impactedPieceSampleRatio"), None)),
                    }
                    for row in sorted(
                        piece_rows,
                        key=lambda row: to_float(row.get("lostMobilityShare"), 0.0) or 0.0,
                        reverse=True,
                    )
                ],
            ),
            build_table(
                "Taxe eau par royaume actif",
                [
                    ("kingdomKey", "Royaume"),
                    ("recordCount", "Records"),
                    ("averageLostDestinationCount", "Destinations perdues moyennes"),
                    ("averageLostMobilityShare", "Part de mobilite perdue"),
                    ("averageImpactedPieceCount", "Pieces impactees moyennes"),
                ],
                [
                    {
                        "kingdomKey": row.get("kingdomKey"),
                        "recordCount": row.get("recordCount"),
                        "averageLostDestinationCount": format_number(row.get("averageLostDestinationCount")),
                        "averageLostMobilityShare": format_percent(to_float(row.get("averageLostMobilityShare"), None)),
                        "averageImpactedPieceCount": format_number(row.get("averageImpactedPieceCount")),
                    }
                    for row in kingdom_rows
                ],
            ),
            build_table(
                "Pics de pression eau",
                [
                    ("turn", "Tour"),
                    ("committedActiveKingdomKey", "Royaume actif"),
                    ("activePieceCount", "Pieces actives"),
                    ("lostDestinationCount", "Destinations perdues"),
                    ("lostMobilityShare", "Part de mobilite perdue"),
                ],
                [
                    {
                        "turn": row.get("turn"),
                        "committedActiveKingdomKey": row.get("committedActiveKingdomKey"),
                        "activePieceCount": row.get("activePieceCount"),
                        "lostDestinationCount": row.get("lostDestinationCount"),
                        "lostMobilityShare": format_percent(to_float(row.get("lostMobilityShare"), None)),
                    }
                    for row in top_pressure_rows[:5]
                ],
            ),
        ],
    }


def build_weather_section(analysis: dict[str, Any]) -> dict[str, Any]:
    weather = as_dict(path_get(analysis, "systems", "weather"))
    points = as_list(weather.get("points"))
    summary = as_dict(weather.get("summary"))
    visibility_rows = build_weather_visibility_rows(points)
    active_cover_comparison = build_active_cover_comparison(analysis)
    cover_true_summary = as_dict(active_cover_comparison.get("trueSummary"))
    cover_false_summary = as_dict(active_cover_comparison.get("falseSummary"))
    cover_deltas = as_dict(active_cover_comparison.get("trueMinusFalse"))
    cover_behavior_rows = build_cover_behavior_rows(active_cover_comparison)
    quartile_rows = []
    for label, rows in split_into_quartiles(points):
        active_ratio = safe_div(sum(1 for row in rows if row.get("hasActiveFront")), len(rows))
        quartile_rows.append(
            {
                "quartile": label,
                "recordCount": len(rows),
                "activeRatio": format_percent(active_ratio),
                "averageConcealingFogCellCount": format_number(
                    safe_mean([to_int(row.get("concealingFogCellCount"), 0) for row in rows])
                ),
                "averageHiddenEnemyPieces": format_number(
                    safe_mean(
                        [
                            to_int(row.get("whiteHiddenEnemyPieces"), 0)
                            + to_int(row.get("blackHiddenEnemyPieces"), 0)
                            for row in rows
                        ]
                    )
                ),
            }
        )

    first_quartile = as_dict(quartile_rows[0]) if quartile_rows else {}
    last_quartile = as_dict(quartile_rows[-1]) if quartile_rows else {}
    active_ratio_total = safe_div(to_int(summary.get("activeFrontRecordCount"), 0), len(points))
    last_active_ratio_raw = parse_percent(last_quartile.get("activeRatio"))
    first_active_ratio_raw = parse_percent(first_quartile.get("activeRatio"))
    covered_record_count = to_int(cover_true_summary.get("recordCount"), 0)
    peak_hidden_pieces = max(
        [to_int(row.get("peakHiddenEnemyPieces"), 0) for row in visibility_rows],
        default=0,
    )
    findings = []

    if cover_true_summary and cover_false_summary:
        findings.append(
            make_finding(
                title="Quand le royaume actif est couvert, le pilotage ralentit nettement",
                confidence=confidence_label(
                    min(
                        to_int(cover_true_summary.get("recordCount"), 0),
                        to_int(cover_false_summary.get("recordCount"), 0),
                    )
                ),
                conclusion=(
                    f"Quand au moins une piece ennemie est cachee au royaume actif, les interactions montent de {format_signed_number(cover_deltas.get('deltaAverageInteractionEventCount'))}, "
                    f"les audits de {format_signed_number(cover_deltas.get('deltaAverageCommandAuditCount'))}, et le commit moyen prend {format_signed_number(safe_div(to_float(cover_deltas.get('deltaAverageCommitDelayMs'), 0.0) or 0.0, 1000.0))} s de plus."
                ),
                evidence=[
                    f"Records couverts: {to_int(cover_true_summary.get('recordCount'), 0)}, records degages: {to_int(cover_false_summary.get('recordCount'), 0)}.",
                    f"Ennemis caches moyens: couvert={format_number(cover_true_summary.get('averageActiveHiddenEnemyPieces'))}, degage={format_number(cover_false_summary.get('averageActiveHiddenEnemyPieces'))}.",
                    f"Brouillard dissimulant moyen: couvert={format_number(cover_true_summary.get('averageConcealingFogCellCount'))}, degage={format_number(cover_false_summary.get('averageConcealingFogCellCount'))}.",
                ],
                caveat="Lecture comparative: la couverture agit souvent avec d'autres contraintes de milieu, elle ne prouve pas a elle seule la causalite.",
            )
        )

    if visibility_rows:
        white_row = as_dict(next((row for row in visibility_rows if row.get("kingdomKey") == "white"), {}))
        black_row = as_dict(next((row for row in visibility_rows if row.get("kingdomKey") == "black"), {}))
        findings.append(
            make_finding(
                title="Le brouillard ne masque pas les deux camps de la meme facon",
                confidence=confidence_label(len(points)),
                conclusion=(
                    f"Sur cet echantillon, le camp noir joue en moyenne avec {format_number(black_row.get('averageHiddenEnemyPieces'))} pieces ennemies masquees contre {format_number(white_row.get('averageHiddenEnemyPieces'))} pour le camp blanc."
                ),
                evidence=[
                    f"Pic de pieces ennemies masquees: blanc={format_number(white_row.get('peakHiddenEnemyPieces'))}, noir={format_number(black_row.get('peakHiddenEnemyPieces'))}.",
                    f"Records avec au moins une piece masquee: blanc={white_row.get('hiddenRecordRatio', 'n/a')}, noir={black_row.get('hiddenRecordRatio', 'n/a')}",
                ],
            )
        )

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
                    f"Brouillard dissimulant moyen: {format_number(summary.get('averageConcealingFogCellCount'))} cellules.",
                ],
            )
        )

    return {
        "key": "weather",
        "title": "Meteo et brouillard aleatoire",
        "summary": "La meteo est etudiee comme un systeme aleatoire de couverture: quelle surface est dissimulante, combien de pieces chaque camp perd reellement de vue, et comment ce brouillard change le pilotage du joueur actif.",
        "headlineMetrics": [
            headline_metric("Records meteo", len(points)),
            percent_metric("Records avec front actif", active_ratio_total),
            headline_metric("Brouillard dissimulant moyen", summary.get("averageConcealingFogCellCount")),
            headline_metric("Pic de pieces masquees", peak_hidden_pieces),
            headline_metric("Records couverts", covered_record_count),
        ],
        "findings": findings,
        "tables": [
            build_table(
                "Regimes meteo par quartile de match",
                [
                    ("quartile", "Quartile"),
                    ("recordCount", "Records"),
                    ("activeRatio", "Front actif"),
                    ("averageConcealingFogCellCount", "Brouillard dissimulant moyen"),
                    ("averageHiddenEnemyPieces", "Pieces ennemies masquees moyennes"),
                ],
                quartile_rows,
            ),
            build_table(
                "Visibilite masquee par camp",
                [
                    ("kingdomKey", "Camp"),
                    ("averageHiddenEnemyPieces", "Pieces ennemies masquees moyennes"),
                    ("peakHiddenEnemyPieces", "Pic masque"),
                    ("hiddenRecordRatio", "Part des records avec masque"),
                ],
                visibility_rows,
            ),
            build_table(
                "Comportement du royaume actif sous couverture",
                [
                    ("metric", "Metrique"),
                    ("covered", "Couvert"),
                    ("clear", "Degage"),
                    ("delta", "Delta couvert - degage"),
                ],
                cover_behavior_rows,
            ),
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
    early_phase_row = next(
        (row for row in phase_comparison_rows if row.get("phase") == "early"),
        None,
    )
    late_phase_row = next(
        (row for row in phase_comparison_rows if row.get("phase") == "late"),
        None,
    )
    if late_phase_row is not None and to_int(late_phase_row.get("openCount"), 0) > 0:
        early_open_count = to_int(early_phase_row.get("openCount"), 0) if early_phase_row else 0
        late_open_count = to_int(late_phase_row.get("openCount"), 0)
        findings.append(
            make_finding(
                title="Les coffres observes pesent presque uniquement en phase tardive",
                confidence=confidence_label(late_open_count),
                conclusion=(
                    f"Sur cette partie, {late_open_count} ouvertures sur {len(opens)} arrivent apres le seuil tardif: l'effet aleatoire mesurable provient donc surtout du mix de fin de partie."
                ),
                evidence=[
                    f"Ouvertures early: {early_open_count}.",
                    f"Ouvertures late: {late_open_count}.",
                    f"Poids configures tardifs: {late_phase_row.get('expectedMixSummary', 'n/a')}.",
                    f"Mix observe tardif: {late_phase_row.get('observedMixSummary', 'n/a')}.",
                ],
                caveat=(
                    "Aucune ouverture en phase early: cet echantillon ne permet pas de verifier la bascule early->late des coffres."
                    if early_open_count == 0
                    else None
                ),
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
    debt_rows = build_infernal_debt_rows(points)
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
    average_white_debt = safe_mean([to_int(row.get("whiteBloodDebt"), 0) for row in points])
    average_black_debt = safe_mean([to_int(row.get("blackBloodDebt"), 0) for row in points])
    max_white_debt = max_or_default([to_int(row.get("whiteBloodDebt"), 0) for row in points], 0)
    max_black_debt = max_or_default([to_int(row.get("blackBloodDebt"), 0) for row in points], 0)
    findings = []

    if target_counts and points:
        dominant_target = max(target_counts, key=target_counts.get)
        dominant_average_debt = average_white_debt if dominant_target == "white" else average_black_debt
        other_average_debt = average_black_debt if dominant_target == "white" else average_white_debt
        findings.append(
            make_finding(
                title="La boucle infernale s'ancre surtout sur une dette de camp, pas sur un total abstrait",
                confidence=confidence_label(len(points)),
                conclusion=(
                    f"La dette moyenne {kingdom_debt_label(dominant_target)} atteint {format_number(dominant_average_debt)} contre {format_number(other_average_debt)} pour l'autre camp, et {target_counts.get(dominant_target, 0)} spawns infernaux sur {len(spawns)} ciblent ce meme camp."
                ),
                evidence=[
                    f"Pics de dette: blanc={format_number(max_white_debt)}, noir={format_number(max_black_debt)}.",
                    f"Cibles de spawn: {', '.join(f'{key}={value}' for key, value in sorted(target_counts.items()))}.",
                    f"Tours de spawn observes: {', '.join(str(row.get('turn')) for row in spawn_rows)}.",
                ],
            )
        )

    if positive_next_intervals:
        interval_text = format_number(safe_mean(positive_next_intervals))
        if zero_next_intervals:
            conclusion = (
                f"Quand la cible porte deja une dette au moment d'un spawn, l'intervalle moyen jusqu'au spawn suivant tombe a {interval_text} tours, contre {format_number(safe_mean(zero_next_intervals))} lorsqu'il n'y a pas de dette cible."
            )
        else:
            conclusion = (
                f"Les spawns observes sur cible deja endettee sont suivis d'un prochain spawn apres {interval_text} tours en moyenne."
            )
        findings.append(
            make_finding(
                title="La dette infernale semble comprimer la cadence de spawn",
                confidence=confidence_label(len(positive_next_intervals)),
                conclusion=conclusion,
                evidence=[
                    f"Nombre de spawns observes: {len(spawns)}.",
                    f"Lifetime moyen observe: {format_number(safe_mean([to_int(row.get('observedLifetime'), 0) for row in lifespans]))} tours.",
                    "La mesure porte ici sur la prochaine cadence observee apres le spawn, ce qui capture mieux l'effet de la dette cible sur la boucle infernale.",
                ],
                caveat=(
                    "Lecture exploratoire: peu de transitions comparables pour isoler proprement l'effet de la dette."
                    if len(positive_next_intervals) < 3 or len(zero_next_intervals) < 2
                    else None
                ),
            )
        )

    return {
        "key": "infernal",
        "title": "Systeme infernal et dette de sang",
        "summary": "Le systeme infernal est lu comme une boucle de dette par camp: on suit separement la dette blanche et la dette noire, puis on relie les spawns a la cible, a la piece manifestee et au niveau de dette atteint au moment du declenchement.",
        "headlineMetrics": [
            headline_metric("Spawns infernaux", len(spawns)),
            headline_metric("Lifetime moyen", safe_mean([to_int(row.get("observedLifetime"), 0) for row in lifespans])),
            headline_metric("Dette blanche moyenne", average_white_debt),
            headline_metric("Dette noire moyenne", average_black_debt),
        ],
        "findings": findings,
        "tables": [
            build_table(
                "Dette infernale par royaume",
                [
                    ("kingdomKey", "Camp"),
                    ("averageDebt", "Dette moyenne"),
                    ("maxDebt", "Pic de dette"),
                ],
                debt_rows,
            ),
            build_table(
                "Dette observee au moment des spawns infernaux",
                [
                    ("turn", "Tour"),
                    ("targetKingdomKey", "Cible"),
                    ("manifestedPieceTypeKey", "Piece manifestee"),
                    ("whiteDebtAtSpawn", "Dette blanche"),
                    ("blackDebtAtSpawn", "Dette noire"),
                    ("nextSpawnIntervalTurns", "Intervalle jusqu'au spawn suivant"),
                ],
                spawn_rows,
            ),
        ],
    }


def build_behavior_section(analysis: dict[str, Any]) -> dict[str, Any]:
    behavior = as_dict(analysis.get("behavior"))
    summary = as_dict(behavior.get("summary"))
    weather_comparison = as_dict(path_get(analysis, "correlations", "weatherActiveVsInactive"))
    infernal_comparison = as_dict(path_get(analysis, "correlations", "infernalActiveVsInactive"))
    weather_finding = build_behavior_regime_finding(
        weather_comparison,
        "Les fronts meteo reconfigurent fortement le pilotage",
        "un front meteo est actif",
        "front actif",
        "sans front",
    )
    infernal_finding = build_behavior_regime_finding(
        infernal_comparison,
        "La pression infernale change surtout la charge de decision",
        "au moins un infernal est actif",
        "infernal actif",
        "sans infernal",
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
                build_regime_table_rows(weather_comparison, "activeFront", "noFront"),
            ),
            build_table(
                "Comparaison de regimes infernaux",
                [
                    ("metric", "Metrique"),
                    ("activeInfernal", "Infernal actif"),
                    ("noInfernal", "Sans infernal"),
                    ("delta", "Delta actif - sans infernal"),
                ],
                build_regime_table_rows(infernal_comparison, "activeInfernal", "noInfernal"),
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
    for title, event_subject, window_rows, delta_summary in (
        ("Les ouvertures de coffre se convertissent surtout en execution", "une ouverture de coffre", chest_windows, chest_delta),
        ("Les spawns infernaux provoquent surtout un surcroit de pilotage", "un spawn infernal", infernal_windows, infernal_delta),
        ("Les fronts meteo declenchent d'abord de la replanification", "un spawn de front meteo", weather_windows, weather_delta),
    ):
        finding = build_window_effect_finding(title, event_subject, window_rows, delta_summary)
        if finding is not None:
            findings.append(finding)

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
                    ("deltaAverageInteractionEventCount", "Delta interactions"),
                ],
                build_window_summary_rows(
                    {
                        "Spawn front meteo": weather_windows,
                        "Ouverture coffre": chest_windows,
                        "Spawn infernal": infernal_windows,
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


def build_behavior_regime_finding(
    comparison: dict[str, Any],
    title: str,
    regime_subject: str,
    active_label: str,
    inactive_label: str,
) -> dict[str, Any] | None:
    true_summary = as_dict(comparison.get("trueSummary"))
    false_summary = as_dict(comparison.get("falseSummary"))
    deltas = as_dict(comparison.get("trueMinusFalse"))
    if not true_summary or not false_summary:
        return None

    true_count = to_int(true_summary.get("recordCount"), 0)
    false_count = to_int(false_summary.get("recordCount"), 0)
    move_delta = to_float(deltas.get("deltaAverageMoveCount"), 0.0)
    build_delta = to_float(deltas.get("deltaAverageBuildCount"), 0.0)
    audit_delta = to_float(deltas.get("deltaAverageCommandAuditCount"), 0.0)
    interaction_delta = to_float(deltas.get("deltaAverageInteractionEventCount"), 0.0)
    return make_finding(
        title=title,
        confidence=confidence_label(min(true_count, false_count)),
        conclusion=(
            f"Quand {regime_subject}, la charge de decision change nettement: interactions {format_signed_number(interaction_delta)}, audits {format_signed_number(audit_delta)}, avec {format_signed_number(move_delta)} mouvements et {format_signed_number(build_delta)} constructions par record."
        ),
        evidence=[
            f"Records {active_label}: {true_count}, records {inactive_label}: {false_count}.",
            f"Interactions moyennes: {active_label}={format_number(true_summary.get('averageInteractionEventCount'))}, {inactive_label}={format_number(false_summary.get('averageInteractionEventCount'))}.",
            f"Audits moyens: {active_label}={format_number(true_summary.get('averageCommandAuditCount'))}, {inactive_label}={format_number(false_summary.get('averageCommandAuditCount'))}.",
            f"Moves/builds moyens: {active_label}={format_number(true_summary.get('averageMoveCount'))}/{format_number(true_summary.get('averageBuildCount'))}, {inactive_label}={format_number(false_summary.get('averageMoveCount'))}/{format_number(false_summary.get('averageBuildCount'))}.",
        ],
    )


def build_regime_table_rows(
    comparison: dict[str, Any],
    active_column_key: str,
    inactive_column_key: str,
) -> list[dict[str, Any]]:
    true_summary = as_dict(comparison.get("trueSummary"))
    false_summary = as_dict(comparison.get("falseSummary"))
    deltas = as_dict(comparison.get("trueMinusFalse"))
    return [
        {
            "metric": label,
            active_column_key: format_number(true_summary.get(true_key)),
            inactive_column_key: format_number(false_summary.get(true_key)),
            "delta": format_signed_number(deltas.get(delta_key)),
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
                "deltaAverageInteractionEventCount": summary.get("deltaAverageInteractionEventCount", "n/a"),
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
        "deltaAverageInteractionEventCount": [],
    }
    for window in windows:
        delta_row = as_dict(window.get("afterMinusBefore"))
        for key in metrics:
            value = to_float(delta_row.get(key), None)
            if value is not None:
                metrics[key].append(value)

    result = {key: format_signed_number(safe_mean(values)) for key, values in metrics.items()}
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
    result["strongestMetricValue"] = format_signed_number(strongest_value)
    return result


def build_window_effect_finding(
    title: str,
    event_subject: str,
    window_rows: list[dict[str, Any]],
    delta_summary: dict[str, Any],
) -> dict[str, Any] | None:
    if not delta_summary:
        return None

    move_delta = to_float(delta_summary.get("deltaAverageMoveCount"), 0.0)
    build_delta = to_float(delta_summary.get("deltaAverageBuildCount"), 0.0)
    audit_delta = to_float(delta_summary.get("deltaAverageCommandAuditCount"), 0.0)
    interaction_delta = to_float(delta_summary.get("deltaAverageInteractionEventCount"), 0.0)
    decision_shift = abs(audit_delta) + abs(interaction_delta)
    board_shift = abs(move_delta) + abs(build_delta)

    if decision_shift >= board_shift:
        conclusion = (
            f"Autour de {event_subject}, l'effet visible porte d'abord sur le pilotage: interactions {format_signed_number(interaction_delta)}, audits {format_signed_number(audit_delta)}, pour un effet de plateau de {format_signed_number(move_delta)} mouvements et {format_signed_number(build_delta)} constructions."
        )
    else:
        conclusion = (
            f"Autour de {event_subject}, l'aleatoire se convertit surtout en actions visibles: mouvements {format_signed_number(move_delta)}, constructions {format_signed_number(build_delta)}, avec aussi {format_signed_number(audit_delta)} audits et {format_signed_number(interaction_delta)} interactions."
        )

    return make_finding(
        title=title,
        confidence=confidence_label(len(window_rows)),
        conclusion=conclusion,
        evidence=[
            f"Fenetres observees: {len(window_rows)}.",
            f"Delta mouvements: {delta_summary.get('deltaAverageMoveCount', 'n/a')}.",
            f"Delta constructions: {delta_summary.get('deltaAverageBuildCount', 'n/a')}.",
            f"Delta audits/interactions: {delta_summary.get('deltaAverageCommandAuditCount', 'n/a')} / {delta_summary.get('deltaAverageInteractionEventCount', 'n/a')}.",
        ],
        caveat="Les fenetres restent correlationnelles: elles montrent ce qui se passe juste avant/apres, pas une causalite prouvee.",
    )


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
        manifested_piece_type_key = key_or_fallback(spawn.get("manifestedPieceTypeKey"), "unknown")
        white_debt = to_int(point.get("whiteBloodDebt"), 0)
        black_debt = to_int(point.get("blackBloodDebt"), 0)
        target_debt = white_debt if target_kingdom_key == "white" else black_debt if target_kingdom_key == "black" else 0
        other_debt = black_debt if target_kingdom_key == "white" else white_debt if target_kingdom_key == "black" else white_debt + black_debt - target_debt
        turn = to_int(spawn.get("turn"), 0)
        rows.append(
            {
                "turn": turn,
                "targetKingdomKey": target_kingdom_key,
                "manifestedPieceTypeKey": manifested_piece_type_key,
                "whiteDebtAtSpawn": white_debt,
                "blackDebtAtSpawn": black_debt,
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


def build_weather_visibility_rows(points: list[dict[str, Any]]) -> list[dict[str, Any]]:
    rows = []
    for kingdom_key, field_name in (("white", "whiteHiddenEnemyPieces"), ("black", "blackHiddenEnemyPieces")):
        values = [to_int(point.get(field_name), 0) for point in points if isinstance(point, dict)]
        rows.append(
            {
                "kingdomKey": kingdom_key,
                "averageHiddenEnemyPieces": format_number(safe_mean(values)),
                "peakHiddenEnemyPieces": format_number(max_or_default(values, 0)),
                "hiddenRecordRatio": format_percent(safe_div(sum(1 for value in values if value > 0), len(values))),
            }
        )
    return rows


def build_active_cover_comparison(analysis: dict[str, Any]) -> dict[str, Any]:
    feature_rows = [
        as_dict(row)
        for row in as_list(path_get(analysis, "correlations", "perRecordFeatures"))
        if isinstance(row, dict)
    ]
    if not feature_rows:
        return {}

    normalized_rows = []
    for row in feature_rows:
        active_kingdom_key = key_or_fallback(row.get("committedActiveKingdomKey"), "unknown")
        if active_kingdom_key == "white":
            active_hidden_enemy_pieces = to_int(row.get("whiteHiddenEnemyPieces"), 0)
        elif active_kingdom_key == "black":
            active_hidden_enemy_pieces = to_int(row.get("blackHiddenEnemyPieces"), 0)
        else:
            active_hidden_enemy_pieces = to_int(row.get("totalHiddenEnemyPieces"), 0)

        normalized_row = dict(row)
        normalized_row["activeHiddenEnemyPieces"] = active_hidden_enemy_pieces
        normalized_rows.append(normalized_row)

    covered_rows = [row for row in normalized_rows if to_int(row.get("activeHiddenEnemyPieces"), 0) > 0]
    clear_rows = [row for row in normalized_rows if to_int(row.get("activeHiddenEnemyPieces"), 0) <= 0]
    covered_summary = summarize_cover_rows(covered_rows)
    clear_summary = summarize_cover_rows(clear_rows)
    return {
        "trueSummary": covered_summary,
        "falseSummary": clear_summary,
        "trueMinusFalse": diff_numeric_rows(covered_summary, clear_summary),
    }


def summarize_cover_rows(rows: list[dict[str, Any]]) -> dict[str, Any]:
    return {
        "recordCount": len(rows),
        "averageMoveCount": safe_mean([to_float(row.get("moveCount"), 0.0) for row in rows]),
        "averageBuildCount": safe_mean([to_float(row.get("buildCount"), 0.0) for row in rows]),
        "averageCommandAuditCount": safe_mean([to_float(row.get("commandAuditCount"), 0.0) for row in rows]),
        "averageInteractionEventCount": safe_mean([to_float(row.get("interactionEventCount"), 0.0) for row in rows]),
        "averageCommitDelayMs": safe_mean(
            [
                to_float(row.get("commitDelayMs"), None)
                for row in rows
                if to_float(row.get("commitDelayMs"), None) is not None
            ]
        ),
        "averageConcealingFogCellCount": safe_mean(
            [to_float(row.get("concealingFogCellCount"), 0.0) for row in rows]
        ),
        "averageActiveHiddenEnemyPieces": safe_mean(
            [to_float(row.get("activeHiddenEnemyPieces"), 0.0) for row in rows]
        ),
    }


def build_cover_behavior_rows(comparison: dict[str, Any]) -> list[dict[str, Any]]:
    covered_summary = as_dict(comparison.get("trueSummary"))
    clear_summary = as_dict(comparison.get("falseSummary"))
    deltas = as_dict(comparison.get("trueMinusFalse"))
    rows = []
    metric_specs = (
        ("hiddenPieces", "Ennemis caches moyens", "averageActiveHiddenEnemyPieces", False),
        ("concealingFog", "Brouillard dissimulant moyen", "averageConcealingFogCellCount", False),
        ("moves", "Moves moyens", "averageMoveCount", False),
        ("builds", "Builds moyens", "averageBuildCount", False),
        ("audits", "Audits moyens", "averageCommandAuditCount", False),
        ("interactions", "Interactions moyennes", "averageInteractionEventCount", False),
        ("commitSeconds", "Commit moyen (s)", "averageCommitDelayMs", True),
    )
    for metric_key, label, summary_key, use_seconds in metric_specs:
        covered_value = to_float(covered_summary.get(summary_key), None)
        clear_value = to_float(clear_summary.get(summary_key), None)
        delta_key = f"delta{summary_key[0].upper()}{summary_key[1:]}"
        delta_value = to_float(deltas.get(delta_key), None)
        if use_seconds:
            covered_value = safe_div(covered_value, 1000.0)
            clear_value = safe_div(clear_value, 1000.0)
            delta_value = safe_div(delta_value, 1000.0)
        rows.append(
            {
                "metric": label,
                "metricKey": metric_key,
                "covered": format_number(covered_value),
                "clear": format_number(clear_value),
                "delta": format_signed_number(delta_value),
                "coveredValue": covered_value,
                "clearValue": clear_value,
                "deltaValue": delta_value,
            }
        )
    return rows


def build_infernal_debt_rows(points: list[dict[str, Any]]) -> list[dict[str, Any]]:
    rows = []
    for kingdom_key, field_name in (("white", "whiteBloodDebt"), ("black", "blackBloodDebt")):
        values = [to_int(point.get(field_name), 0) for point in points if isinstance(point, dict)]
        rows.append(
            {
                "kingdomKey": kingdom_key,
                "averageDebt": format_number(safe_mean(values)),
                "maxDebt": format_number(max_or_default(values, 0)),
                "averageDebtValue": safe_mean(values),
                "maxDebtValue": max_or_default(values, 0),
            }
        )
    return rows


def diff_numeric_rows(left: dict[str, Any], right: dict[str, Any]) -> dict[str, Any]:
    result = {}
    for key in left:
        left_value = left.get(key)
        right_value = right.get(key)
        if isinstance(left_value, (int, float)) and isinstance(right_value, (int, float)):
            result[f"delta{key[0].upper()}{key[1:]}"] = left_value - right_value
    return result


def max_or_default(values: list[int], default: int) -> int:
    return max(values) if values else default


def kingdom_debt_label(kingdom_key: str) -> str:
    mapping = {
        "white": "du camp blanc",
        "black": "du camp noir",
    }
    return mapping.get(kingdom_key, f"du camp {kingdom_key}")


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


def format_signed_number(value: Any, digits: int = 2) -> str:
    numeric_value = to_float(value, None)
    if numeric_value is None:
        return "n/a"
    if numeric_value == 0:
        return "0"
    formatted = format_number(abs(numeric_value), digits)
    return f"+{formatted}" if numeric_value > 0 else f"-{formatted}"


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