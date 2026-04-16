#pragma once

#include <optional>
#include <string>

#include "Core/GameState.hpp"

class GameConfig;
class GameEngine;
class InputHandler;
class MultiplayerRuntime;
class SaveManager;
struct JoinMultiplayerRequest;
struct LocalPlayerContext;

struct MultiplayerJoinPreparationPlan {
    bool invalidateTurnDraft = false;
    bool hideGameMenu = false;
    bool hideMultiplayerAlert = false;
    bool hideMultiplayerWaitingOverlay = false;
    bool clearMultiplayerStatus = false;
};

struct MultiplayerJoinPresentationPlan {
    bool joined = false;
    std::optional<GameState> nextGameState;
    bool refreshTurnPhase = false;
    bool reconcileSelection = false;
    bool centerCameraOnBlack = false;
    bool hideMultiplayerAlert = false;
    bool showHud = false;
    bool updateUI = false;
    std::string errorMessage;
};

class MultiplayerJoinCoordinator {
public:
    MultiplayerJoinCoordinator(GameEngine& engine,
                               MultiplayerRuntime& multiplayer,
                               SaveManager& saveManager,
                               InputHandler& input,
                               const GameConfig& config);

    static bool buildReconnectJoinRequest(const MultiplayerRuntime& multiplayer,
                                          JoinMultiplayerRequest& joinRequest,
                                          std::string* errorMessage = nullptr);

    MultiplayerJoinPreparationPlan prepareForClientConnectionAttempt(
        LocalPlayerContext& localPlayerContext,
        bool& waitingForRemoteTurnResult,
        bool preserveLanClientContext);

    MultiplayerJoinPresentationPlan joinMultiplayer(
        const JoinMultiplayerRequest& request,
        LocalPlayerContext& localPlayerContext,
        bool& waitingForRemoteTurnResult);

private:
    GameEngine& m_engine;
    MultiplayerRuntime& m_multiplayer;
    SaveManager& m_saveManager;
    InputHandler& m_input;
    const GameConfig& m_config;
};