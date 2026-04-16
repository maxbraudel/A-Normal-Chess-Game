#include "Runtime/UpdateCoordinator.hpp"

FrameUpdatePlan UpdateCoordinator::planFrameUpdate(const FrameUpdateState& state) {
    FrameUpdatePlan plan;
    plan.updateCamera = state.canMoveCamera;
    plan.updateMultiplayer = state.gameState == GameState::Playing
        || state.gameState == GameState::Paused
        || state.lanClient
        || state.lanHost;

    switch (state.gameState) {
        case GameState::Playing:
            plan.runAITurn = state.activeAI;
            plan.updateUI = true;
            plan.updateUIManager = true;
            break;

        case GameState::Paused:
        case GameState::GameOver:
            plan.updateUI = true;
            plan.updateUIManager = true;
            break;

        default:
            break;
    }

    return plan;
}