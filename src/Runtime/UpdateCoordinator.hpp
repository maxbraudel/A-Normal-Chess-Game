#pragma once

#include "Core/GameState.hpp"

struct FrameUpdateState {
    GameState gameState = GameState::MainMenu;
    bool canMoveCamera = false;
    bool lanHost = false;
    bool lanClient = false;
};

struct FrameUpdatePlan {
    bool syncTurnDraft = true;
    bool updateCamera = false;
    bool updateMultiplayer = false;
    bool updateUI = false;
    bool updateUIManager = false;
};

class UpdateCoordinator {
public:
    static FrameUpdatePlan planFrameUpdate(const FrameUpdateState& state);
};