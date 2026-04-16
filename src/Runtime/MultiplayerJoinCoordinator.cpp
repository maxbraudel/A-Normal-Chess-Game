#include "Runtime/MultiplayerJoinCoordinator.hpp"

#include "Config/GameConfig.hpp"
#include "Core/GameEngine.hpp"
#include "Core/LocalPlayerContext.hpp"
#include "Core/ToolState.hpp"
#include "Input/InputHandler.hpp"
#include "Multiplayer/MultiplayerRuntime.hpp"
#include "Runtime/MultiplayerEventCoordinator.hpp"
#include "Save/SaveData.hpp"
#include "Save/SaveManager.hpp"
#include "UI/MainMenuUI.hpp"

namespace {

void writeJoinError(std::string* errorMessage, const std::string& message) {
    if (errorMessage) {
        *errorMessage = message;
    }
}

}

MultiplayerJoinCoordinator::MultiplayerJoinCoordinator(GameEngine& engine,
                                                       MultiplayerRuntime& multiplayer,
                                                       SaveManager& saveManager,
                                                       InputHandler& input,
                                                       const GameConfig& config)
    : m_engine(engine)
    , m_multiplayer(multiplayer)
    , m_saveManager(saveManager)
    , m_input(input)
    , m_config(config) {}

bool MultiplayerJoinCoordinator::buildReconnectJoinRequest(const MultiplayerRuntime& multiplayer,
                                                           JoinMultiplayerRequest& joinRequest,
                                                           std::string* errorMessage) {
    if (!multiplayer.hasReconnectRequest()) {
        writeJoinError(errorMessage, "No previous multiplayer host is available for reconnect.");
        return false;
    }

    const MultiplayerJoinCredentials& request = multiplayer.reconnectRequest();
    joinRequest.host = request.host;
    joinRequest.port = request.port;
    joinRequest.password = request.password;
    return true;
}

MultiplayerJoinPreparationPlan MultiplayerJoinCoordinator::prepareForClientConnectionAttempt(
    LocalPlayerContext& localPlayerContext,
    bool& waitingForRemoteTurnResult,
    bool preserveLanClientContext) {
    MultiplayerJoinPreparationPlan plan;
    plan.invalidateTurnDraft = true;
    plan.hideGameMenu = true;
    plan.hideMultiplayerAlert = true;
    plan.hideMultiplayerWaitingOverlay = true;
    plan.clearMultiplayerStatus = true;

    m_multiplayer.resetConnections();
    waitingForRemoteTurnResult = false;
    if (!preserveLanClientContext) {
        localPlayerContext = LocalPlayerContext{};
    }

    m_input.clearMovePreview();
    m_input.clearSelection();
    m_input.setTool(ToolState::Select);
    m_engine.resetPendingTurn();
    return plan;
}

MultiplayerJoinPresentationPlan MultiplayerJoinCoordinator::joinMultiplayer(
    const JoinMultiplayerRequest& request,
    LocalPlayerContext& localPlayerContext,
    bool& waitingForRemoteTurnResult) {
    MultiplayerJoinPresentationPlan plan;

    SaveData snapshotData;
    if (!m_multiplayer.join(MultiplayerJoinCredentials{request.host, request.port, request.password},
                            m_saveManager,
                            snapshotData,
                            &plan.errorMessage)) {
        return plan;
    }

    std::string restoreError;
    if (!m_engine.restoreFromSave(snapshotData, m_config, &restoreError)) {
        plan.errorMessage = restoreError.empty() ? "Unable to restore the multiplayer snapshot." : restoreError;
        m_multiplayer.resetConnections();
        return plan;
    }

    MultiplayerEventCoordinator::applyClientSnapshotState(
        m_multiplayer,
        m_input,
        localPlayerContext,
        waitingForRemoteTurnResult);
    plan.joined = true;
    plan.nextGameState = GameState::Playing;
    plan.refreshTurnPhase = true;
    plan.reconcileSelection = true;
    plan.centerCameraOnBlack = true;
    plan.hideMultiplayerAlert = true;
    plan.showHud = true;
    plan.updateUI = true;
    return plan;
}