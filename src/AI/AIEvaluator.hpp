#pragma once
#include "AI/GameSnapshot.hpp"
#include "AI/ThreatMap.hpp"
#include "AI/ForwardModel.hpp"

class Board;
class Kingdom;
class GameConfig;
class AIConfig;

enum class AIPhase; // forward

/// Evaluation weights that change per phase
struct EvalWeights {
    float material   = 1.5f;
    float economy    = 1.0f;
    float mapControl = 1.0f;
    float kingSafety = 1.5f;
    float development = 1.0f;
    float threat     = 1.0f;
    float checkmate  = 2.0f;
};

class AIEvaluator {
public:
    // ---- NEW: evaluate a snapshot (main API for MCTS / forward model) ----
    static float evaluate(const GameSnapshot& s, KingdomId perspective,
                          int globalMaxRange, const EvalWeights& weights);

    // ---- NEW: get default weights for a phase ----
    static EvalWeights weightsForPhase(AIPhase phase);

    // ---- Piece value constants ----
    static float pieceValue(PieceType type);

    // ---- Legacy: evaluate from real game state (kept for backward compat) ----
    static float evaluate(const Board& board, const Kingdom& self, const Kingdom& enemy,
                           const GameConfig& config);

private:
    // Component scores on snapshot
    static float scoreMaterial(const GameSnapshot& s, KingdomId k);
    static float scoreEconomy(const GameSnapshot& s, KingdomId k,
                               int mineIncome = 10, int farmIncome = 5);
    static float scoreMapControl(const GameSnapshot& s, KingdomId k);
    static float scoreKingSafety(const GameSnapshot& s, KingdomId k, int globalMaxRange);
    static float scoreDevelopment(const GameSnapshot& s, KingdomId k);
    static float scoreThreat(const GameSnapshot& s, KingdomId k, int globalMaxRange);
    static float scoreCheckmateProximity(const GameSnapshot& s, KingdomId k, int globalMaxRange);

    // Legacy component scores
    static float scoreMaterial(const Kingdom& kingdom);
    static float scoreIncome(const Kingdom& kingdom, const Board& board, const GameConfig& config);
    static float scoreKingSafety(const Kingdom& kingdom, const Board& board, const GameConfig& config);
    static float scoreTerritorialControl(const Kingdom& kingdom, const Board& board);
    static float scoreBuildings(const Kingdom& kingdom);
    static float scoreThreats(const Kingdom& kingdom, const Kingdom& enemy,
                               const Board& board, const GameConfig& config);
};
