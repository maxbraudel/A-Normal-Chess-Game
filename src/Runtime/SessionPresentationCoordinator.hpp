#pragma once

#include "Core/GameSessionConfig.hpp"
#include "Core/GameState.hpp"
#include "Core/LocalPlayerContext.hpp"

struct SessionResetPlan {
    bool stopMultiplayer = false;
    bool clearMovePreview = false;
    bool activateSelectTool = false;
    bool resetPendingTurn = false;
    bool invalidateTurnDraft = false;
};

struct SessionPresentationPlan {
    LocalPlayerContext localPlayerContext{};
    bool clearWaitingForRemoteTurnResult = false;
    KingdomId cameraKingdom = KingdomId::White;
    GameState nextGameState = GameState::Playing;
    bool refreshTurnPhase = false;
    bool showHud = false;
    bool updateUI = false;
    bool saveAuthoritativeSession = false;
};

struct MainMenuPresentationPlan {
    bool stopMultiplayer = false;
    bool clearMovePreview = false;
    bool activateSelectTool = false;
    bool resetPendingTurn = false;
    bool invalidateTurnDraft = false;
    GameState nextGameState = GameState::MainMenu;
    bool hideGameMenu = false;
    bool showMainMenu = false;
};

class SessionPresentationCoordinator {
public:
    static SessionResetPlan buildAuthoritativeSessionResetPlan();
    static SessionPresentationPlan buildSessionPresentationPlan(const GameSessionConfig& session,
                                                               bool saveAuthoritativeSession);
    static MainMenuPresentationPlan buildReturnToMainMenuPlan();
};