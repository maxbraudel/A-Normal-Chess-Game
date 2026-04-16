#pragma once
#include <string>
#include <fstream>
#include <sstream>

class AIConfig {
public:
    struct PhaseEvalWeights {
        float material = 1.0f;
        float economy = 1.0f;
        float mapControl = 1.0f;
        float kingSafety = 1.0f;
        float development = 1.0f;
        float threat = 1.0f;
        float checkmate = 1.0f;
    };

    struct PieceValues {
        float pawn = 100.0f;
        float knight = 320.0f;
        float bishop = 330.0f;
        float rook = 500.0f;
        float queen = 900.0f;
        float king = 10000.0f;
    };

    struct HeuristicTuning {
        // CHECKMATE_PRESS / RUSH_ATTACK objective
        float checkmateApproachWeight   = 35.0f;
        float checkmateProximityBase    = 30.0f;
        float checkmatePieceBonus       = 10.0f;

        // BUILD_ARMY objective
        float buildArmyApproachWeight   = 18.0f;
        float buildArmyPieceBonus       = 12.0f;

        // ECONOMY_EXPAND objective
        float economyResourceBonus      = 200.0f;
        float economyResourceDistBase   = 120.0f;
        float economyResourceDistScale  = 8.0f;

        // DEFEND_KING objective
        float defendKingApproachWeight  = 18.0f;
        float defendKingProximityBase   = 30.0f;
        float defendKingProximityScale  = 2.0f;
    };

    struct EvaluatorTuning {
        // Economy scoring
        float goldFactor      = 0.5f;
        float incomeFactor    = 5.0f;
        float barracksFactor  = 30.0f;

        // Map control scoring
        float resourceCellBonus    = 15.0f;
        float contestedCellPenalty = 5.0f;
        float churchBonus          = 50.0f;
        float arenaBonus           = 20.0f;

        // King safety scoring
        float inCheckPenalty      = 500.0f;
        float safeEscapeBonus     = 30.0f;
        float defenderBonus       = 40.0f;
        float enemyInCheckBonus   = 400.0f;
        float enemyEscapePenalty  = 25.0f;

        // Development scoring
        float productionFactor = 0.3f;
        float xpFactor         = 0.1f;
        float queenBonus       = 200.0f;

        // Threat scoring
        float threatGainFactor = 0.3f;
        float threatLossFactor = 0.4f;

        // Checkmate proximity scoring
        float checkmateProximityInCheckBonus    = 300.0f;
        float checkmateProximityMateBonus       = 100000.0f;
        float blockedEscapesBonus               = 40.0f;
        float avgDistBase                       = 100.0f;
        float avgDistScale                      = 3.0f;
        float assaultPiecesBonus                = 50.0f;
    };

    struct PressureTuning {
        float assaultUncoveredEscapeWeight = 80.0f;
        float assaultEscapeWeight = 26.0f;
        float assaultNonEscapeWeight = 14.0f;
        float assaultExactEscapeBonus = 24.0f;
        float assaultExactNonEscapeBonus = 10.0f;
        float assaultSlotDistancePenalty = 8.0f;
        float assaultSlotSectorLoadPenalty = 20.0f;
        float assaultMoveSectorLoadPenalty = 12.0f;

        int nonKingNearDistance = 4;
        int sectorLoadDistance = 6;
        int pieceInPositionDistance = 4;

        float approachDistanceWeight = 20.0f;
        float crowdReductionWeight = 14.0f;
        float newCoverageWeight = 90.0f;
        float coverageDeltaWeight = 50.0f;
        float safeEscapeReductionWeight = 140.0f;
        float safeEscapePenaltyWeight = 35.0f;
        float assaultDeltaMultiplier = 1.5f;

        float givesCheckBonus = 180.0f;
        float mateBonus = 100000.0f;
        float kingMovePenalty = 140.0f;
        float lastMovedPiecePenalty = 40.0f;
        float captureValueMultiplier = 0.5f;
        float inPositionAssaultImproveThreshold = 18.0f;
        float noNetImprovePenalty = 180.0f;
        float netImproveBonus = 30.0f;
        float closeDistanceBonus = 20.0f;
        float driftPenalty = 120.0f;
        float pawnOvercrowdPenalty = 40.0f;

        float pieceTypeBonusRook = 40.0f;
        float pieceTypeBonusBishop = 28.0f;
        float pieceTypeBonusKnight = 22.0f;
        float pieceTypeBonusPawn = 12.0f;
        float pieceTypeBonusQueen = 50.0f;
    };

    AIConfig();
    bool loadFromFile(const std::string& filepath);

    // Weights
    float farmPriority;
    float attackPriority;
    float defensePriority;
    float buildPriority;
    float upgradePriority;
    float marriagePriority;

    // Thresholds
    int minGoldBeforeAttack;
    int minPiecesBeforeAttack;
    int wallDefenseRadius;

    // Tactical engine
    int searchDepth;          // Minimax search depth (default 3)
    float aggressionMaterialRatio; // Material ratio to trigger aggression (default 1.4)

    // Timing budget
    int maxTurnTimeMs;
    float mctsBudgetFraction;
    float checkmateSolverBudgetFraction;
    int mateInOneMinBudgetMs;
    int deepMateMinBudgetMs;
    int deepMateMaxBudgetMs;
    int deepMateDepth;

    // Phase thresholds
    int earlyGameMaxTurn;
    int buildUpMaxTurn;
    int endgamePieceThreshold;
    int enemyKingStaticTurnsThreshold;

    // Evaluation and piece values
    PieceValues pieceValues;
    PressureTuning pressure;
    HeuristicTuning heuristic;
    EvaluatorTuning evaluator;
    PhaseEvalWeights earlyGameWeights;
    PhaseEvalWeights buildUpWeights;
    PhaseEvalWeights midGameWeights;
    PhaseEvalWeights aggressionWeights;
    PhaseEvalWeights endgameWeights;
    PhaseEvalWeights crisisWeights;

    float randomness;

    // New AI toggle
    bool useNewAI = true;

private:
    static void loadPhaseWeights(const std::string& root,
                                 const std::string& phaseKey,
                                 PhaseEvalWeights& target);
    static std::string readFile(const std::string& path);
    static float extractFloat(const std::string& json, const std::string& key, float defaultVal);
    static int extractInt(const std::string& json, const std::string& key, int defaultVal);
    static bool extractBool(const std::string& json, const std::string& key, bool defaultVal);
    static std::string extractSection(const std::string& json, const std::string& key);
};
