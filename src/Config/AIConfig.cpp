#include "Config/AIConfig.hpp"
#include <iostream>

namespace {

int clampMin(const char* label, int value, int minValue) {
    if (value >= minValue) {
        return value;
    }

    std::cerr << "AIConfig: Clamping " << label << " value "
              << value << " to " << minValue << ".\n";
    return minValue;
}

float clampRange(const char* label, float value, float minValue, float maxValue) {
    if (value < minValue) {
        std::cerr << "AIConfig: Clamping " << label << " value "
                  << value << " to " << minValue << ".\n";
        return minValue;
    }
    if (value > maxValue) {
        std::cerr << "AIConfig: Clamping " << label << " value "
                  << value << " to " << maxValue << ".\n";
        return maxValue;
    }
    return value;
}

void clampPhaseWeights(AIConfig::PhaseEvalWeights& weights,
                       const char* phaseLabel) {
    const std::string prefix = std::string("eval_weights.") + phaseLabel + ".";
    weights.material = clampRange((prefix + "material").c_str(), weights.material, 0.0f, 10.0f);
    weights.economy = clampRange((prefix + "economy").c_str(), weights.economy, 0.0f, 10.0f);
    weights.mapControl = clampRange((prefix + "map_control").c_str(), weights.mapControl, 0.0f, 10.0f);
    weights.kingSafety = clampRange((prefix + "king_safety").c_str(), weights.kingSafety, 0.0f, 10.0f);
    weights.development = clampRange((prefix + "development").c_str(), weights.development, 0.0f, 10.0f);
    weights.threat = clampRange((prefix + "threat").c_str(), weights.threat, 0.0f, 10.0f);
    weights.checkmate = clampRange((prefix + "checkmate").c_str(), weights.checkmate, 0.0f, 10.0f);
}

}

AIConfig::AIConfig()
    : farmPriority(0.8f), attackPriority(0.6f), defensePriority(0.7f),
      buildPriority(0.5f), upgradePriority(0.4f), marriagePriority(0.3f),
      minGoldBeforeAttack(100), minPiecesBeforeAttack(3), wallDefenseRadius(5),
      searchDepth(3), aggressionMaterialRatio(1.4f),
      maxTurnTimeMs(300), mctsBudgetFraction(0.6f), checkmateSolverBudgetFraction(0.2f),
      mateInOneMinBudgetMs(50), deepMateMinBudgetMs(100), deepMateMaxBudgetMs(60), deepMateDepth(4),
      earlyGameMaxTurn(8), buildUpMaxTurn(15), endgamePieceThreshold(3), enemyKingStaticTurnsThreshold(4),
      randomness(0.0f) {}

void AIConfig::loadPhaseWeights(const std::string& root,
                                const std::string& phaseKey,
                                PhaseEvalWeights& target) {
    const std::string evalSec = extractSection(root, "eval_weights");
    if (evalSec.empty()) {
        return;
    }

    const std::string phaseSec = extractSection(evalSec, phaseKey);
    if (phaseSec.empty()) {
        return;
    }

    target.material = extractFloat(phaseSec, "material", target.material);
    target.economy = extractFloat(phaseSec, "economy", target.economy);
    target.mapControl = extractFloat(phaseSec, "map_control", target.mapControl);
    target.kingSafety = extractFloat(phaseSec, "king_safety", target.kingSafety);
    target.development = extractFloat(phaseSec, "development", target.development);
    target.threat = extractFloat(phaseSec, "threat", target.threat);
    target.checkmate = extractFloat(phaseSec, "checkmate", target.checkmate);
}

std::string AIConfig::readFile(const std::string& path) {
    std::ifstream file(path);
    if (!file.is_open()) return "";
    std::stringstream ss;
    ss << file.rdbuf();
    return ss.str();
}

std::string AIConfig::extractSection(const std::string& json, const std::string& key) {
    std::string search = "\"" + key + "\"";
    auto pos = json.find(search);
    if (pos == std::string::npos) return "";
    pos = json.find('{', pos);
    if (pos == std::string::npos) return "";
    int depth = 1;
    size_t start = pos;
    ++pos;
    while (pos < json.size() && depth > 0) {
        if (json[pos] == '{') ++depth;
        if (json[pos] == '}') --depth;
        ++pos;
    }
    return json.substr(start, pos - start);
}

float AIConfig::extractFloat(const std::string& json, const std::string& key, float defaultVal) {
    std::string search = "\"" + key + "\"";
    auto pos = json.find(search);
    if (pos == std::string::npos) return defaultVal;
    pos = json.find(':', pos);
    if (pos == std::string::npos) return defaultVal;
    ++pos;
    while (pos < json.size() && (json[pos] == ' ' || json[pos] == '\t')) ++pos;
    try {
        size_t end;
        float val = std::stof(json.substr(pos), &end);
        return val;
    } catch (...) {
        return defaultVal;
    }
}

int AIConfig::extractInt(const std::string& json, const std::string& key, int defaultVal) {
    std::string search = "\"" + key + "\"";
    auto pos = json.find(search);
    if (pos == std::string::npos) return defaultVal;
    pos = json.find(':', pos);
    if (pos == std::string::npos) return defaultVal;
    ++pos;
    while (pos < json.size() && (json[pos] == ' ' || json[pos] == '\t')) ++pos;
    try {
        return std::stoi(json.substr(pos));
    } catch (...) {
        return defaultVal;
    }
}

bool AIConfig::extractBool(const std::string& json, const std::string& key, bool defaultVal) {
    std::string search = "\"" + key + "\"";
    auto pos = json.find(search);
    if (pos == std::string::npos) return defaultVal;
    pos = json.find(':', pos);
    if (pos == std::string::npos) return defaultVal;
    ++pos;
    while (pos < json.size() && (json[pos] == ' ' || json[pos] == '\t')) ++pos;
    if (pos < json.size() && json[pos] == 't') return true;
    if (pos < json.size() && json[pos] == 'f') return false;
    return defaultVal;
}

bool AIConfig::loadFromFile(const std::string& filepath) {
    std::string json = readFile(filepath);
    if (json.empty()) {
        std::cerr << "AIConfig: Could not load " << filepath << ", using defaults.\n";
        return false;
    }

    const std::string wrappedAiSection = extractSection(json, "ai");
    const std::string root = wrappedAiSection.empty() ? json : wrappedAiSection;

    // Keep defaults aligned with previously hardcoded evaluator behavior.
    earlyGameWeights = {1.0f, 3.0f, 2.0f, 1.0f, 2.0f, 0.5f, 0.3f};
    buildUpWeights = {1.5f, 2.0f, 1.5f, 1.0f, 1.5f, 1.0f, 0.5f};
    midGameWeights = {2.0f, 1.0f, 1.0f, 1.5f, 1.0f, 1.5f, 1.5f};
    aggressionWeights = {2.0f, 0.3f, 0.5f, 1.5f, 0.3f, 2.0f, 3.0f};
    endgameWeights = {1.5f, 0.2f, 0.3f, 2.0f, 0.2f, 2.0f, 4.0f};
    crisisWeights = {0.5f, 0.1f, 0.1f, 5.0f, 0.1f, 0.5f, 0.5f};

    std::string pieceValuesSec = extractSection(root, "piece_values");
    if (!pieceValuesSec.empty()) {
        pieceValues.pawn = extractFloat(pieceValuesSec, "pawn", pieceValues.pawn);
        pieceValues.knight = extractFloat(pieceValuesSec, "knight", pieceValues.knight);
        pieceValues.bishop = extractFloat(pieceValuesSec, "bishop", pieceValues.bishop);
        pieceValues.rook = extractFloat(pieceValuesSec, "rook", pieceValues.rook);
        pieceValues.queen = extractFloat(pieceValuesSec, "queen", pieceValues.queen);
        pieceValues.king = extractFloat(pieceValuesSec, "king", pieceValues.king);
    }

    loadPhaseWeights(root, "early_game", earlyGameWeights);
    loadPhaseWeights(root, "build_up", buildUpWeights);
    loadPhaseWeights(root, "mid_game", midGameWeights);
    loadPhaseWeights(root, "aggression", aggressionWeights);
    loadPhaseWeights(root, "endgame", endgameWeights);
    loadPhaseWeights(root, "crisis", crisisWeights);

    std::string pressureSec = extractSection(root, "pressure");
    if (!pressureSec.empty()) {
        pressure.assaultUncoveredEscapeWeight = extractFloat(
            pressureSec, "assault_uncovered_escape_weight", pressure.assaultUncoveredEscapeWeight);
        pressure.assaultEscapeWeight = extractFloat(
            pressureSec, "assault_escape_weight", pressure.assaultEscapeWeight);
        pressure.assaultNonEscapeWeight = extractFloat(
            pressureSec, "assault_non_escape_weight", pressure.assaultNonEscapeWeight);
        pressure.assaultExactEscapeBonus = extractFloat(
            pressureSec, "assault_exact_escape_bonus", pressure.assaultExactEscapeBonus);
        pressure.assaultExactNonEscapeBonus = extractFloat(
            pressureSec, "assault_exact_non_escape_bonus", pressure.assaultExactNonEscapeBonus);
        pressure.assaultSlotDistancePenalty = extractFloat(
            pressureSec, "assault_slot_distance_penalty", pressure.assaultSlotDistancePenalty);
        pressure.assaultSlotSectorLoadPenalty = extractFloat(
            pressureSec, "assault_slot_sector_load_penalty", pressure.assaultSlotSectorLoadPenalty);
        pressure.assaultMoveSectorLoadPenalty = extractFloat(
            pressureSec, "assault_move_sector_load_penalty", pressure.assaultMoveSectorLoadPenalty);

        pressure.nonKingNearDistance = extractInt(
            pressureSec, "non_king_near_distance", pressure.nonKingNearDistance);
        pressure.sectorLoadDistance = extractInt(
            pressureSec, "sector_load_distance", pressure.sectorLoadDistance);
        pressure.pieceInPositionDistance = extractInt(
            pressureSec, "piece_in_position_distance", pressure.pieceInPositionDistance);

        pressure.approachDistanceWeight = extractFloat(
            pressureSec, "approach_distance_weight", pressure.approachDistanceWeight);
        pressure.crowdReductionWeight = extractFloat(
            pressureSec, "crowd_reduction_weight", pressure.crowdReductionWeight);
        pressure.newCoverageWeight = extractFloat(
            pressureSec, "new_coverage_weight", pressure.newCoverageWeight);
        pressure.coverageDeltaWeight = extractFloat(
            pressureSec, "coverage_delta_weight", pressure.coverageDeltaWeight);
        pressure.safeEscapeReductionWeight = extractFloat(
            pressureSec, "safe_escape_reduction_weight", pressure.safeEscapeReductionWeight);
        pressure.safeEscapePenaltyWeight = extractFloat(
            pressureSec, "safe_escape_penalty_weight", pressure.safeEscapePenaltyWeight);
        pressure.assaultDeltaMultiplier = extractFloat(
            pressureSec, "assault_delta_multiplier", pressure.assaultDeltaMultiplier);

        pressure.givesCheckBonus = extractFloat(
            pressureSec, "gives_check_bonus", pressure.givesCheckBonus);
        pressure.mateBonus = extractFloat(
            pressureSec, "mate_bonus", pressure.mateBonus);
        pressure.kingMovePenalty = extractFloat(
            pressureSec, "king_move_penalty", pressure.kingMovePenalty);
        pressure.lastMovedPiecePenalty = extractFloat(
            pressureSec, "last_moved_piece_penalty", pressure.lastMovedPiecePenalty);
        pressure.captureValueMultiplier = extractFloat(
            pressureSec, "capture_value_multiplier", pressure.captureValueMultiplier);
        pressure.inPositionAssaultImproveThreshold = extractFloat(
            pressureSec, "in_position_assault_improve_threshold", pressure.inPositionAssaultImproveThreshold);
        pressure.noNetImprovePenalty = extractFloat(
            pressureSec, "no_net_improve_penalty", pressure.noNetImprovePenalty);
        pressure.netImproveBonus = extractFloat(
            pressureSec, "net_improve_bonus", pressure.netImproveBonus);
        pressure.closeDistanceBonus = extractFloat(
            pressureSec, "close_distance_bonus", pressure.closeDistanceBonus);
        pressure.driftPenalty = extractFloat(
            pressureSec, "drift_penalty", pressure.driftPenalty);
        pressure.pawnOvercrowdPenalty = extractFloat(
            pressureSec, "pawn_overcrowd_penalty", pressure.pawnOvercrowdPenalty);

        pressure.pieceTypeBonusRook = extractFloat(
            pressureSec, "piece_type_bonus_rook", pressure.pieceTypeBonusRook);
        pressure.pieceTypeBonusBishop = extractFloat(
            pressureSec, "piece_type_bonus_bishop", pressure.pieceTypeBonusBishop);
        pressure.pieceTypeBonusKnight = extractFloat(
            pressureSec, "piece_type_bonus_knight", pressure.pieceTypeBonusKnight);
        pressure.pieceTypeBonusPawn = extractFloat(
            pressureSec, "piece_type_bonus_pawn", pressure.pieceTypeBonusPawn);
        pressure.pieceTypeBonusQueen = extractFloat(
            pressureSec, "piece_type_bonus_queen", pressure.pieceTypeBonusQueen);
    }

    std::string weightsSec = extractSection(root, "weights");
    if (!weightsSec.empty()) {
        farmPriority = extractFloat(weightsSec, "farm_priority", farmPriority);
        attackPriority = extractFloat(weightsSec, "attack_priority", attackPriority);
        defensePriority = extractFloat(weightsSec, "defense_priority", defensePriority);
        buildPriority = extractFloat(weightsSec, "build_priority", buildPriority);
        upgradePriority = extractFloat(weightsSec, "upgrade_priority", upgradePriority);
        marriagePriority = extractFloat(weightsSec, "marriage_priority", marriagePriority);
    }

    std::string thresholdsSec = extractSection(root, "thresholds");
    if (!thresholdsSec.empty()) {
        minGoldBeforeAttack = extractInt(thresholdsSec, "min_gold_before_attack", minGoldBeforeAttack);
        minPiecesBeforeAttack = extractInt(thresholdsSec, "min_pieces_before_attack", minPiecesBeforeAttack);
        wallDefenseRadius = extractInt(thresholdsSec, "wall_defense_radius", wallDefenseRadius);
    }

    std::string tacticalSec = extractSection(root, "tactical");
    if (!tacticalSec.empty()) {
        searchDepth = extractInt(tacticalSec, "search_depth", searchDepth);
        aggressionMaterialRatio = extractFloat(tacticalSec, "aggression_material_ratio", aggressionMaterialRatio);
    }

    std::string timingSec = extractSection(root, "timing");
    if (!timingSec.empty()) {
        maxTurnTimeMs = extractInt(timingSec, "max_turn_time_ms", maxTurnTimeMs);
        mctsBudgetFraction = extractFloat(timingSec, "mcts_budget_fraction", mctsBudgetFraction);
        checkmateSolverBudgetFraction = extractFloat(
            timingSec, "checkmate_solver_budget_fraction", checkmateSolverBudgetFraction);
        mateInOneMinBudgetMs = extractInt(timingSec, "mate_in_one_min_budget_ms", mateInOneMinBudgetMs);
        deepMateMinBudgetMs = extractInt(timingSec, "deep_mate_min_budget_ms", deepMateMinBudgetMs);
        deepMateMaxBudgetMs = extractInt(timingSec, "deep_mate_max_budget_ms", deepMateMaxBudgetMs);
        deepMateDepth = extractInt(timingSec, "deep_mate_depth", deepMateDepth);
    }

    std::string phaseSec = extractSection(root, "phase_thresholds");
    if (!phaseSec.empty()) {
        earlyGameMaxTurn = extractInt(phaseSec, "early_game_max_turn", earlyGameMaxTurn);
        buildUpMaxTurn = extractInt(phaseSec, "build_up_max_turn", buildUpMaxTurn);
        endgamePieceThreshold = extractInt(phaseSec, "endgame_piece_threshold", endgamePieceThreshold);
        enemyKingStaticTurnsThreshold = extractInt(
            phaseSec, "enemy_king_static_turns_threshold", enemyKingStaticTurnsThreshold);
    }

    std::string heuristicSec = extractSection(root, "heuristic");
    if (!heuristicSec.empty()) {
        heuristic.checkmateApproachWeight  = extractFloat(heuristicSec, "checkmate_approach_weight",  heuristic.checkmateApproachWeight);
        heuristic.checkmateProximityBase   = extractFloat(heuristicSec, "checkmate_proximity_base",   heuristic.checkmateProximityBase);
        heuristic.checkmatePieceBonus      = extractFloat(heuristicSec, "checkmate_piece_bonus",      heuristic.checkmatePieceBonus);
        heuristic.buildArmyApproachWeight  = extractFloat(heuristicSec, "build_army_approach_weight", heuristic.buildArmyApproachWeight);
        heuristic.buildArmyPieceBonus      = extractFloat(heuristicSec, "build_army_piece_bonus",     heuristic.buildArmyPieceBonus);
        heuristic.economyResourceBonus     = extractFloat(heuristicSec, "economy_resource_bonus",     heuristic.economyResourceBonus);
        heuristic.economyResourceDistBase  = extractFloat(heuristicSec, "economy_resource_dist_base", heuristic.economyResourceDistBase);
        heuristic.economyResourceDistScale = extractFloat(heuristicSec, "economy_resource_dist_scale",heuristic.economyResourceDistScale);
        heuristic.defendKingApproachWeight = extractFloat(heuristicSec, "defend_king_approach_weight",heuristic.defendKingApproachWeight);
        heuristic.defendKingProximityBase  = extractFloat(heuristicSec, "defend_king_proximity_base", heuristic.defendKingProximityBase);
        heuristic.defendKingProximityScale = extractFloat(heuristicSec, "defend_king_proximity_scale",heuristic.defendKingProximityScale);
    }

    std::string evaluatorSec = extractSection(root, "evaluator");
    if (!evaluatorSec.empty()) {
        evaluator.goldFactor           = extractFloat(evaluatorSec, "gold_factor",            evaluator.goldFactor);
        evaluator.incomeFactor         = extractFloat(evaluatorSec, "income_factor",          evaluator.incomeFactor);
        evaluator.barracksFactor       = extractFloat(evaluatorSec, "barracks_factor",        evaluator.barracksFactor);
        evaluator.resourceCellBonus    = extractFloat(evaluatorSec, "resource_cell_bonus",    evaluator.resourceCellBonus);
        evaluator.contestedCellPenalty = extractFloat(evaluatorSec, "contested_cell_penalty", evaluator.contestedCellPenalty);
        evaluator.churchBonus          = extractFloat(evaluatorSec, "church_bonus",           evaluator.churchBonus);
        evaluator.arenaBonus           = extractFloat(evaluatorSec, "arena_bonus",            evaluator.arenaBonus);
        evaluator.inCheckPenalty       = extractFloat(evaluatorSec, "in_check_penalty",       evaluator.inCheckPenalty);
        evaluator.safeEscapeBonus      = extractFloat(evaluatorSec, "safe_escape_bonus",      evaluator.safeEscapeBonus);
        evaluator.defenderBonus        = extractFloat(evaluatorSec, "defender_bonus",         evaluator.defenderBonus);
        evaluator.enemyInCheckBonus    = extractFloat(evaluatorSec, "enemy_in_check_bonus",   evaluator.enemyInCheckBonus);
        evaluator.enemyEscapePenalty   = extractFloat(evaluatorSec, "enemy_escape_penalty",   evaluator.enemyEscapePenalty);
        evaluator.productionFactor     = extractFloat(evaluatorSec, "production_factor",      evaluator.productionFactor);
        evaluator.xpFactor             = extractFloat(evaluatorSec, "xp_factor",              evaluator.xpFactor);
        evaluator.queenBonus           = extractFloat(evaluatorSec, "queen_bonus",            evaluator.queenBonus);
        evaluator.threatGainFactor     = extractFloat(evaluatorSec, "threat_gain_factor",     evaluator.threatGainFactor);
        evaluator.threatLossFactor     = extractFloat(evaluatorSec, "threat_loss_factor",     evaluator.threatLossFactor);
        evaluator.checkmateProximityInCheckBonus = extractFloat(evaluatorSec, "checkmate_proximity_in_check_bonus", evaluator.checkmateProximityInCheckBonus);
        evaluator.checkmateProximityMateBonus    = extractFloat(evaluatorSec, "checkmate_proximity_mate_bonus",     evaluator.checkmateProximityMateBonus);
        evaluator.blockedEscapesBonus  = extractFloat(evaluatorSec, "blocked_escapes_bonus",  evaluator.blockedEscapesBonus);
        evaluator.avgDistBase          = extractFloat(evaluatorSec, "avg_dist_base",          evaluator.avgDistBase);
        evaluator.avgDistScale         = extractFloat(evaluatorSec, "avg_dist_scale",         evaluator.avgDistScale);
        evaluator.assaultPiecesBonus   = extractFloat(evaluatorSec, "assault_pieces_bonus",   evaluator.assaultPiecesBonus);
    }

    randomness = extractFloat(root, "randomness", randomness);
    useNewAI = extractBool(root, "use_new_ai", useNewAI);

    minGoldBeforeAttack = clampMin("thresholds.min_gold_before_attack", minGoldBeforeAttack, 0);
    minPiecesBeforeAttack = clampMin("thresholds.min_pieces_before_attack", minPiecesBeforeAttack, 1);
    wallDefenseRadius = clampMin("thresholds.wall_defense_radius", wallDefenseRadius, 0);

    searchDepth = clampMin("tactical.search_depth", searchDepth, 1);
    aggressionMaterialRatio = clampRange("tactical.aggression_material_ratio", aggressionMaterialRatio, 0.1f, 10.0f);

    maxTurnTimeMs = clampMin("timing.max_turn_time_ms", maxTurnTimeMs, 1);
    mctsBudgetFraction = clampRange("timing.mcts_budget_fraction", mctsBudgetFraction, 0.0f, 1.0f);
    checkmateSolverBudgetFraction = clampRange(
        "timing.checkmate_solver_budget_fraction", checkmateSolverBudgetFraction, 0.0f, 1.0f);
    mateInOneMinBudgetMs = clampMin("timing.mate_in_one_min_budget_ms", mateInOneMinBudgetMs, 1);
    deepMateMinBudgetMs = clampMin("timing.deep_mate_min_budget_ms", deepMateMinBudgetMs, 1);
    deepMateMaxBudgetMs = clampMin("timing.deep_mate_max_budget_ms", deepMateMaxBudgetMs, 1);
    deepMateDepth = clampMin("timing.deep_mate_depth", deepMateDepth, 1);

    earlyGameMaxTurn = clampMin("phase_thresholds.early_game_max_turn", earlyGameMaxTurn, 0);
    buildUpMaxTurn = clampMin("phase_thresholds.build_up_max_turn", buildUpMaxTurn, 0);
    endgamePieceThreshold = clampMin("phase_thresholds.endgame_piece_threshold", endgamePieceThreshold, 1);
    enemyKingStaticTurnsThreshold = clampMin(
        "phase_thresholds.enemy_king_static_turns_threshold", enemyKingStaticTurnsThreshold, 1);

    pieceValues.pawn = clampRange("piece_values.pawn", pieceValues.pawn, 1.0f, 100000.0f);
    pieceValues.knight = clampRange("piece_values.knight", pieceValues.knight, 1.0f, 100000.0f);
    pieceValues.bishop = clampRange("piece_values.bishop", pieceValues.bishop, 1.0f, 100000.0f);
    pieceValues.rook = clampRange("piece_values.rook", pieceValues.rook, 1.0f, 100000.0f);
    pieceValues.queen = clampRange("piece_values.queen", pieceValues.queen, 1.0f, 100000.0f);
    pieceValues.king = clampRange("piece_values.king", pieceValues.king, 1.0f, 100000.0f);

    clampPhaseWeights(earlyGameWeights, "early_game");
    clampPhaseWeights(buildUpWeights, "build_up");
    clampPhaseWeights(midGameWeights, "mid_game");
    clampPhaseWeights(aggressionWeights, "aggression");
    clampPhaseWeights(endgameWeights, "endgame");
    clampPhaseWeights(crisisWeights, "crisis");

    pressure.assaultUncoveredEscapeWeight = clampRange(
        "pressure.assault_uncovered_escape_weight", pressure.assaultUncoveredEscapeWeight, -100000.0f, 100000.0f);
    pressure.assaultEscapeWeight = clampRange(
        "pressure.assault_escape_weight", pressure.assaultEscapeWeight, -100000.0f, 100000.0f);
    pressure.assaultNonEscapeWeight = clampRange(
        "pressure.assault_non_escape_weight", pressure.assaultNonEscapeWeight, -100000.0f, 100000.0f);
    pressure.assaultExactEscapeBonus = clampRange(
        "pressure.assault_exact_escape_bonus", pressure.assaultExactEscapeBonus, -100000.0f, 100000.0f);
    pressure.assaultExactNonEscapeBonus = clampRange(
        "pressure.assault_exact_non_escape_bonus", pressure.assaultExactNonEscapeBonus, -100000.0f, 100000.0f);
    pressure.assaultSlotDistancePenalty = clampRange(
        "pressure.assault_slot_distance_penalty", pressure.assaultSlotDistancePenalty, -100000.0f, 100000.0f);
    pressure.assaultSlotSectorLoadPenalty = clampRange(
        "pressure.assault_slot_sector_load_penalty", pressure.assaultSlotSectorLoadPenalty, -100000.0f, 100000.0f);
    pressure.assaultMoveSectorLoadPenalty = clampRange(
        "pressure.assault_move_sector_load_penalty", pressure.assaultMoveSectorLoadPenalty, -100000.0f, 100000.0f);

    pressure.nonKingNearDistance = clampMin("pressure.non_king_near_distance", pressure.nonKingNearDistance, 0);
    pressure.sectorLoadDistance = clampMin("pressure.sector_load_distance", pressure.sectorLoadDistance, 0);
    pressure.pieceInPositionDistance = clampMin("pressure.piece_in_position_distance", pressure.pieceInPositionDistance, 0);

    pressure.approachDistanceWeight = clampRange(
        "pressure.approach_distance_weight", pressure.approachDistanceWeight, -100000.0f, 100000.0f);
    pressure.crowdReductionWeight = clampRange(
        "pressure.crowd_reduction_weight", pressure.crowdReductionWeight, -100000.0f, 100000.0f);
    pressure.newCoverageWeight = clampRange(
        "pressure.new_coverage_weight", pressure.newCoverageWeight, -100000.0f, 100000.0f);
    pressure.coverageDeltaWeight = clampRange(
        "pressure.coverage_delta_weight", pressure.coverageDeltaWeight, -100000.0f, 100000.0f);
    pressure.safeEscapeReductionWeight = clampRange(
        "pressure.safe_escape_reduction_weight", pressure.safeEscapeReductionWeight, -100000.0f, 100000.0f);
    pressure.safeEscapePenaltyWeight = clampRange(
        "pressure.safe_escape_penalty_weight", pressure.safeEscapePenaltyWeight, -100000.0f, 100000.0f);
    pressure.assaultDeltaMultiplier = clampRange(
        "pressure.assault_delta_multiplier", pressure.assaultDeltaMultiplier, -100000.0f, 100000.0f);

    pressure.givesCheckBonus = clampRange("pressure.gives_check_bonus", pressure.givesCheckBonus, -1000000.0f, 1000000.0f);
    pressure.mateBonus = clampRange("pressure.mate_bonus", pressure.mateBonus, -1000000000.0f, 1000000000.0f);
    pressure.kingMovePenalty = clampRange("pressure.king_move_penalty", pressure.kingMovePenalty, -100000.0f, 100000.0f);
    pressure.lastMovedPiecePenalty = clampRange("pressure.last_moved_piece_penalty", pressure.lastMovedPiecePenalty, -100000.0f, 100000.0f);
    pressure.captureValueMultiplier = clampRange("pressure.capture_value_multiplier", pressure.captureValueMultiplier, -1000.0f, 1000.0f);
    pressure.inPositionAssaultImproveThreshold = clampRange(
        "pressure.in_position_assault_improve_threshold", pressure.inPositionAssaultImproveThreshold, -100000.0f, 100000.0f);
    pressure.noNetImprovePenalty = clampRange("pressure.no_net_improve_penalty", pressure.noNetImprovePenalty, -100000.0f, 100000.0f);
    pressure.netImproveBonus = clampRange("pressure.net_improve_bonus", pressure.netImproveBonus, -100000.0f, 100000.0f);
    pressure.closeDistanceBonus = clampRange("pressure.close_distance_bonus", pressure.closeDistanceBonus, -100000.0f, 100000.0f);
    pressure.driftPenalty = clampRange("pressure.drift_penalty", pressure.driftPenalty, -100000.0f, 100000.0f);
    pressure.pawnOvercrowdPenalty = clampRange("pressure.pawn_overcrowd_penalty", pressure.pawnOvercrowdPenalty, -100000.0f, 100000.0f);

    pressure.pieceTypeBonusRook = clampRange("pressure.piece_type_bonus_rook", pressure.pieceTypeBonusRook, -100000.0f, 100000.0f);
    pressure.pieceTypeBonusBishop = clampRange("pressure.piece_type_bonus_bishop", pressure.pieceTypeBonusBishop, -100000.0f, 100000.0f);
    pressure.pieceTypeBonusKnight = clampRange("pressure.piece_type_bonus_knight", pressure.pieceTypeBonusKnight, -100000.0f, 100000.0f);
    pressure.pieceTypeBonusPawn = clampRange("pressure.piece_type_bonus_pawn", pressure.pieceTypeBonusPawn, -100000.0f, 100000.0f);
    pressure.pieceTypeBonusQueen = clampRange("pressure.piece_type_bonus_queen", pressure.pieceTypeBonusQueen, -100000.0f, 100000.0f);

    heuristic.checkmateApproachWeight  = clampRange("heuristic.checkmate_approach_weight",   heuristic.checkmateApproachWeight,  -100000.0f, 100000.0f);
    heuristic.checkmateProximityBase   = clampRange("heuristic.checkmate_proximity_base",    heuristic.checkmateProximityBase,   -100000.0f, 100000.0f);
    heuristic.checkmatePieceBonus      = clampRange("heuristic.checkmate_piece_bonus",       heuristic.checkmatePieceBonus,      -100000.0f, 100000.0f);
    heuristic.buildArmyApproachWeight  = clampRange("heuristic.build_army_approach_weight",  heuristic.buildArmyApproachWeight,  -100000.0f, 100000.0f);
    heuristic.buildArmyPieceBonus      = clampRange("heuristic.build_army_piece_bonus",      heuristic.buildArmyPieceBonus,      -100000.0f, 100000.0f);
    heuristic.economyResourceBonus     = clampRange("heuristic.economy_resource_bonus",      heuristic.economyResourceBonus,     -100000.0f, 100000.0f);
    heuristic.economyResourceDistBase  = clampRange("heuristic.economy_resource_dist_base",  heuristic.economyResourceDistBase,  -100000.0f, 100000.0f);
    heuristic.economyResourceDistScale = clampRange("heuristic.economy_resource_dist_scale", heuristic.economyResourceDistScale, -100000.0f, 100000.0f);
    heuristic.defendKingApproachWeight = clampRange("heuristic.defend_king_approach_weight", heuristic.defendKingApproachWeight, -100000.0f, 100000.0f);
    heuristic.defendKingProximityBase  = clampRange("heuristic.defend_king_proximity_base",  heuristic.defendKingProximityBase,  -100000.0f, 100000.0f);
    heuristic.defendKingProximityScale = clampRange("heuristic.defend_king_proximity_scale", heuristic.defendKingProximityScale, -100000.0f, 100000.0f);

    evaluator.goldFactor           = clampRange("evaluator.gold_factor",            evaluator.goldFactor,           -10000.0f, 10000.0f);
    evaluator.incomeFactor         = clampRange("evaluator.income_factor",          evaluator.incomeFactor,         -10000.0f, 10000.0f);
    evaluator.barracksFactor       = clampRange("evaluator.barracks_factor",        evaluator.barracksFactor,       -10000.0f, 10000.0f);
    evaluator.resourceCellBonus    = clampRange("evaluator.resource_cell_bonus",    evaluator.resourceCellBonus,    -10000.0f, 10000.0f);
    evaluator.contestedCellPenalty = clampRange("evaluator.contested_cell_penalty", evaluator.contestedCellPenalty, -10000.0f, 10000.0f);
    evaluator.churchBonus          = clampRange("evaluator.church_bonus",           evaluator.churchBonus,          -10000.0f, 10000.0f);
    evaluator.arenaBonus           = clampRange("evaluator.arena_bonus",            evaluator.arenaBonus,           -10000.0f, 10000.0f);
    evaluator.inCheckPenalty       = clampRange("evaluator.in_check_penalty",       evaluator.inCheckPenalty,       -1000000.0f, 1000000.0f);
    evaluator.safeEscapeBonus      = clampRange("evaluator.safe_escape_bonus",      evaluator.safeEscapeBonus,      -10000.0f, 10000.0f);
    evaluator.defenderBonus        = clampRange("evaluator.defender_bonus",         evaluator.defenderBonus,        -10000.0f, 10000.0f);
    evaluator.enemyInCheckBonus    = clampRange("evaluator.enemy_in_check_bonus",   evaluator.enemyInCheckBonus,    -1000000.0f, 1000000.0f);
    evaluator.enemyEscapePenalty   = clampRange("evaluator.enemy_escape_penalty",   evaluator.enemyEscapePenalty,   -10000.0f, 10000.0f);
    evaluator.productionFactor     = clampRange("evaluator.production_factor",      evaluator.productionFactor,     -1000.0f, 1000.0f);
    evaluator.xpFactor             = clampRange("evaluator.xp_factor",              evaluator.xpFactor,             -1000.0f, 1000.0f);
    evaluator.queenBonus           = clampRange("evaluator.queen_bonus",            evaluator.queenBonus,           -1000000.0f, 1000000.0f);
    evaluator.threatGainFactor     = clampRange("evaluator.threat_gain_factor",     evaluator.threatGainFactor,     -1000.0f, 1000.0f);
    evaluator.threatLossFactor     = clampRange("evaluator.threat_loss_factor",     evaluator.threatLossFactor,     -1000.0f, 1000.0f);
    evaluator.checkmateProximityInCheckBonus = clampRange("evaluator.checkmate_proximity_in_check_bonus", evaluator.checkmateProximityInCheckBonus, -1000000.0f, 1000000.0f);
    evaluator.checkmateProximityMateBonus    = clampRange("evaluator.checkmate_proximity_mate_bonus",     evaluator.checkmateProximityMateBonus,    -1000000000.0f, 1000000000.0f);
    evaluator.blockedEscapesBonus  = clampRange("evaluator.blocked_escapes_bonus",  evaluator.blockedEscapesBonus,  -10000.0f, 10000.0f);
    evaluator.avgDistBase          = clampRange("evaluator.avg_dist_base",          evaluator.avgDistBase,          -10000.0f, 10000.0f);
    evaluator.avgDistScale         = clampRange("evaluator.avg_dist_scale",         evaluator.avgDistScale,         -1000.0f, 1000.0f);
    evaluator.assaultPiecesBonus   = clampRange("evaluator.assault_pieces_bonus",   evaluator.assaultPiecesBonus,   -10000.0f, 10000.0f);

    randomness = clampRange("randomness", randomness, 0.0f, 1.0f);
    return true;
}
