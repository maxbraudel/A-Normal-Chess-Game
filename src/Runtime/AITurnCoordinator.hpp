#pragma once

#include <optional>
#include <string>
#include <vector>

#include "Core/AITurnRunner.hpp"
#include "Core/GameState.hpp"

class AIDirector;
class GameConfig;
class GameEngine;

struct AITurnRuntimeState {
    GameState gameState = GameState::MainMenu;
    bool activeAI = false;
    bool runnerBusy = false;
    KingdomId activeKingdom = KingdomId::White;
    int turnNumber = 0;
};

struct AITurnStartPlan {
    bool shouldStart = false;
    KingdomId activeKingdom = KingdomId::White;
    int turnNumber = 0;
};

struct AITurnCompletionPlan {
    bool shouldIgnore = false;
    bool applyPlanMetadata = false;
    bool printDebugSummary = false;
    bool stageTurn = false;
    bool logObjective = false;
    bool logPlanningComplete = false;
    bool commitAuthoritativeTurn = false;
    KingdomId activeKingdom = KingdomId::White;
    int turnNumber = 0;
    std::string objectiveName;
    std::vector<std::string> debugLines;
};

class AITurnCoordinator {
public:
    AITurnCoordinator(GameEngine& engine,
                      AITurnRunner& turnRunner,
                      AIDirector& director,
                      const GameConfig& config);

    static AITurnStartPlan buildStartPlan(const AITurnRuntimeState& state);
    static AITurnCompletionPlan buildCompletionPlan(
        const AITurnRuntimeState& state,
        const std::optional<AITurnRunner::CompletedTurn>& completedTurn);

    void cancelPendingTurn();
    void startTurnIfNeeded(GameState gameState);
    bool pollCompletedTurn(GameState gameState);

private:
    AITurnRuntimeState makeRuntimeState(GameState gameState) const;
    static std::vector<std::string> buildDebugLines(const AIDirectorPlan& plan);

    GameEngine& m_engine;
    AITurnRunner& m_turnRunner;
    AIDirector& m_director;
    const GameConfig& m_config;
};