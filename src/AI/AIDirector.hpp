#pragma once
#include <vector>
#include <string>
#include "AI/AIStrategy.hpp"
#include "AI/AIMCTS.hpp"
#include "AI/CheckmateSolver.hpp"
#include "AI/AIEconomyModule.hpp"
#include "AI/AIBuildModule.hpp"
#include "AI/AISpecialModule.hpp"
#include "AI/AIEvaluator.hpp"
#include "AI/AIBrain.hpp"
#include "AI/AITurnContext.hpp"
#include "AI/GameSnapshot.hpp"
#include "AI/ForwardModel.hpp"
#include "AI/TimeBudget.hpp"
#include "Config/AIConfig.hpp"
#include "Systems/TurnCommand.hpp"
#include "Kingdom/KingdomId.hpp"

class Board;
class Kingdom;
class Building;
class GameConfig;

/// Result of a full AI turn computation
struct AIDirectorPlan {
    std::vector<TurnCommand> commands;
    std::string objectiveName;
    // Metadata for tracking
    sf::Vector2i lastEnemyKingPos{-9999, -9999};
    int enemyKingStaticTurns = 0;
    int lastMovedPieceId = -1;
};

/// The AI Director — orchestrator replacing AIController.
/// Pipeline: checkmate-in-1 → strategy → MCTS → build → produce-all → marriage → fallback
class AIDirector {
public:
    AIDirector();
    bool loadConfig(const std::string& filepath);

    /// Main entry: compute the full turn plan.
    /// Called on a COPY of the game state (thread-safe).
    AIDirectorPlan computeTurn(Board& board, Kingdom& self, Kingdom& enemy,
                                const std::vector<Building>& publicBuildings,
                                int turnNumber, const GameConfig& config);

    /// Apply metadata from a completed plan (enemy king tracking)
    void applyPlanMetadata(const AIDirectorPlan& plan);

private:
    AIConfig m_config;
    AIStrategy m_strategy;
    AIMCTS m_mcts;
    CheckmateSolver m_checkmateSolver;
    AIEconomyModule m_economyModule;
    AIBuildModule m_buildModule;
    AISpecialModule m_specialModule;

    // Tracking
    sf::Vector2i m_lastEnemyKingPos{-9999, -9999};
    int m_enemyKingStaticTurns = 0;
    int m_lastMovedPieceId = -1;

    // Internal helpers
    void executeMove(AIDirectorPlan& plan, const GameSnapshot& snapshot,
                     const AITurnContext& ctx, KingdomId aiKingdom,
                     const TurnPlan& stratPlan, int globalMaxRange,
                          const EvalWeights& weights, int mctsBudgetMs,
                          const GameConfig& config,
                     bool forcePressure);
    void executeBuild(AIDirectorPlan& plan, const GameSnapshot& snapshot,
                      KingdomId aiKingdom, const TurnPlan& stratPlan,
                      int turnNumber, int incomePerTurn,
                      const GameConfig& config);
    void executeUpgrades(AIDirectorPlan& plan, const GameSnapshot& snapshot,
                         KingdomId aiKingdom, const GameConfig& config);
    void executeProductions(AIDirectorPlan& plan, const GameSnapshot& snapshot,
                             KingdomId aiKingdom, const TurnPlan& stratPlan,
                             int turnNumber, const GameConfig& config);
    void executeMarriage(AIDirectorPlan& plan, const GameSnapshot& snapshot,
                          KingdomId aiKingdom);

    // Heuristic fallback (when no time for MCTS)
    void heuristicMove(AIDirectorPlan& plan, const GameSnapshot& snapshot,
                        const AITurnContext& ctx, KingdomId aiKingdom,
                        StrategicObjective objective, int globalMaxRange);
    bool executePressureMove(AIDirectorPlan& plan, const GameSnapshot& snapshot,
                             KingdomId aiKingdom, int globalMaxRange);
};
