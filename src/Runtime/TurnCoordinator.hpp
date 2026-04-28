#pragma once

#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "Core/GameplayNotification.hpp"
#include "Core/GameState.hpp"
#include "Kingdom/KingdomId.hpp"

class GameConfig;
class GameDataRecorder;
class GameEngine;
class GameStateDebugRecorder;
class MultiplayerRuntime;
class BehavioralTelemetryCollector;
struct MultiplayerTurnSubmission;

struct AuthoritativeTurnExecution {
    bool committed = false;
    bool gameOver = false;
    KingdomId winner = KingdomId::White;
    int committedTurnNumber = 0;
    KingdomId committedActiveKingdom = KingdomId::White;
    std::vector<GameplayNotification> notifications;
};

struct AuthoritativeCommitPlan {
    bool clearMovePreview = false;
    bool clearWaitingForRemoteTurnResult = false;
    bool refreshTurnPhase = false;
    bool syncTurnDraftBeforeReconcile = false;
    bool reconcileSelection = false;
    bool updateUI = false;
    bool persistLanHostSnapshot = false;
    std::optional<GameState> nextGameState;
    std::optional<std::pair<KingdomId, std::string>> eventLogEntry;
};

struct ClientTurnSubmissionResult {
    bool submitted = false;
    bool clearMovePreview = false;
    bool resetPendingCommands = false;
    bool syncTurnDraftBeforeReconcile = false;
    bool reconcileSelection = false;
    bool waitForRemoteTurnResult = false;
    std::string errorMessage;
};

struct RemoteTurnSubmissionResult {
    bool accepted = false;
    bool shouldCommitAuthoritativeTurn = false;
    bool shouldResetPendingCommands = false;
    std::string rejectionMessage;
};

class TurnCoordinator {
public:
    TurnCoordinator(GameEngine& engine,
                    MultiplayerRuntime& multiplayer,
                    GameStateDebugRecorder& debugRecorder,
                    GameDataRecorder& dataRecorder,
                    BehavioralTelemetryCollector& behavioralTelemetry,
                    const GameConfig& config);

    AuthoritativeTurnExecution executeAuthoritativeTurn();
    static AuthoritativeCommitPlan buildAuthoritativeCommitPlan(
        const AuthoritativeTurnExecution& execution,
        bool lanHost,
        const std::string& winnerName);

    ClientTurnSubmissionResult submitClientTurn(bool lanClient);
    RemoteTurnSubmissionResult applyRemoteTurnSubmission(bool lanHost,
                                                         const MultiplayerTurnSubmission& submission);

private:
    GameEngine& m_engine;
    MultiplayerRuntime& m_multiplayer;
    GameStateDebugRecorder& m_debugRecorder;
    GameDataRecorder& m_dataRecorder;
    BehavioralTelemetryCollector& m_behavioralTelemetry;
    const GameConfig& m_config;
};