#include "Runtime/SessionRuntimeCoordinator.hpp"

#include "Core/GameEngine.hpp"
#include "Core/GameState.hpp"
#include "Core/LocalPlayerContext.hpp"
#include "Multiplayer/MultiplayerRuntime.hpp"
#include "Runtime/MultiplayerJoinCoordinator.hpp"
#include "Runtime/SessionFlow.hpp"
#include "UI/MainMenuUI.hpp"

SessionRuntimeCoordinator::SessionRuntimeCoordinator(
    GameState& gameState,
    bool& waitingForRemoteTurnResult,
    LocalPlayerContext& localPlayerContext,
    GameEngine& engine,
    MultiplayerRuntime& multiplayer,
    SessionFlow& sessionFlow,
    MultiplayerJoinCoordinator& multiplayerJoinCoordinator)
    : m_gameState(gameState)
    , m_waitingForRemoteTurnResult(waitingForRemoteTurnResult)
    , m_localPlayerContext(localPlayerContext)
    , m_engine(engine)
    , m_multiplayer(multiplayer)
    , m_sessionFlow(sessionFlow)
    , m_multiplayerJoinCoordinator(multiplayerJoinCoordinator) {}

bool SessionRuntimeCoordinator::startNewGame(const GameSessionConfig& session,
                                             const SessionRuntimeCallbacks& callbacks,
                                             std::string* errorMessage) {
    applySessionResetPlan(SessionPresentationCoordinator::buildAuthoritativeSessionResetPlan(), callbacks);
    if (!m_sessionFlow.startNewSession(session, errorMessage)) {
        return false;
    }

    applySessionPresentationPlan(
        SessionPresentationCoordinator::buildSessionPresentationPlan(session, true),
        callbacks);
    return true;
}

bool SessionRuntimeCoordinator::loadGame(const std::string& saveName,
                                         const SessionRuntimeCallbacks& callbacks,
                                         std::string* errorMessage) {
    applySessionResetPlan(SessionPresentationCoordinator::buildAuthoritativeSessionResetPlan(), callbacks);
    if (!m_sessionFlow.loadSession(saveName, errorMessage)) {
        return false;
    }

    applySessionPresentationPlan(
        SessionPresentationCoordinator::buildSessionPresentationPlan(m_engine.sessionConfig(), false),
        callbacks);
    return true;
}

bool SessionRuntimeCoordinator::joinMultiplayer(const JoinMultiplayerRequest& request,
                                                const SessionRuntimeCallbacks& callbacks,
                                                std::string* errorMessage) {
    m_multiplayer.clearReconnectState();
    return joinMultiplayerInternal(request, false, callbacks, errorMessage);
}

bool SessionRuntimeCoordinator::reconnectToMultiplayerHost(const SessionRuntimeCallbacks& callbacks,
                                                           std::string* errorMessage) {
    JoinMultiplayerRequest joinRequest;
    if (!MultiplayerJoinCoordinator::buildReconnectJoinRequest(m_multiplayer,
                                                               joinRequest,
                                                               errorMessage)) {
        return false;
    }

    return joinMultiplayerInternal(joinRequest, true, callbacks, errorMessage);
}

void SessionRuntimeCoordinator::returnToMainMenu(const SessionRuntimeCallbacks& callbacks) {
    const MainMenuPresentationPlan plan = SessionPresentationCoordinator::buildReturnToMainMenuPlan();
    if (plan.stopMultiplayer && callbacks.stopMultiplayer) {
        callbacks.stopMultiplayer();
    }
    if (plan.clearMovePreview && callbacks.clearMovePreview) {
        callbacks.clearMovePreview();
    }
    if (plan.activateSelectTool && callbacks.activateSelectTool) {
        callbacks.activateSelectTool();
    }
    if (plan.resetPendingTurn && callbacks.resetPendingCommands) {
        callbacks.resetPendingCommands();
    }
    if (plan.invalidateTurnDraft && callbacks.invalidateTurnDraft) {
        callbacks.invalidateTurnDraft();
    }
    if (callbacks.clearHotseatFrontendState) {
        callbacks.clearHotseatFrontendState();
    }
    m_gameState = plan.nextGameState;
    if (plan.hideGameMenu && callbacks.hideGameMenu) {
        callbacks.hideGameMenu();
    }
    if (plan.showMainMenu && callbacks.showMainMenu) {
        callbacks.showMainMenu();
    }
}

void SessionRuntimeCoordinator::applySessionResetPlan(const SessionResetPlan& plan,
                                                      const SessionRuntimeCallbacks& callbacks) {
    if (plan.stopMultiplayer && callbacks.stopMultiplayer) {
        callbacks.stopMultiplayer();
    }
    if (plan.clearMovePreview && callbacks.clearMovePreview) {
        callbacks.clearMovePreview();
    }
    if (plan.activateSelectTool && callbacks.activateSelectTool) {
        callbacks.activateSelectTool();
    }
    if (plan.resetPendingTurn && callbacks.resetPendingTurn) {
        callbacks.resetPendingTurn();
    }
    if (plan.invalidateTurnDraft && callbacks.invalidateTurnDraft) {
        callbacks.invalidateTurnDraft();
    }
    if (callbacks.clearHotseatFrontendState) {
        callbacks.clearHotseatFrontendState();
    }
}

void SessionRuntimeCoordinator::applySessionPresentationPlan(const SessionPresentationPlan& plan,
                                                             const SessionRuntimeCallbacks& callbacks) {
    m_localPlayerContext = plan.localPlayerContext;
    if (plan.clearWaitingForRemoteTurnResult) {
        m_waitingForRemoteTurnResult = false;
    }
    if (callbacks.centerCameraOnKingdom) {
        callbacks.centerCameraOnKingdom(plan.cameraKingdom);
    }
    m_gameState = plan.nextGameState;
    if (plan.refreshTurnPhase && callbacks.refreshTurnPhase) {
        callbacks.refreshTurnPhase();
    }
    if (plan.showHud && callbacks.showHud) {
        callbacks.showHud();
    }
    if (plan.updateUI && callbacks.updateUI) {
        callbacks.updateUI();
    }
    if (plan.saveAuthoritativeSession && callbacks.saveGame) {
        callbacks.saveGame();
    }
}

bool SessionRuntimeCoordinator::joinMultiplayerInternal(const JoinMultiplayerRequest& request,
                                                        bool preserveLanClientContext,
                                                        const SessionRuntimeCallbacks& callbacks,
                                                        std::string* errorMessage) {
    const InputSelectionBookmark selectionBookmark = callbacks.captureSelectionBookmark
        ? callbacks.captureSelectionBookmark()
        : InputSelectionBookmark{};

    const MultiplayerJoinPreparationPlan preparationPlan =
        m_multiplayerJoinCoordinator.prepareForClientConnectionAttempt(
            m_localPlayerContext,
            m_waitingForRemoteTurnResult,
            preserveLanClientContext);
    if (preparationPlan.invalidateTurnDraft && callbacks.invalidateTurnDraft) {
        callbacks.invalidateTurnDraft();
    }
    if (preparationPlan.hideGameMenu && callbacks.hideGameMenu) {
        callbacks.hideGameMenu();
    }
    if (preparationPlan.hideMultiplayerAlert && callbacks.hideMultiplayerAlert) {
        callbacks.hideMultiplayerAlert();
    }
    if (preparationPlan.hideMultiplayerWaitingOverlay && callbacks.hideMultiplayerWaitingOverlay) {
        callbacks.hideMultiplayerWaitingOverlay();
    }
    if (preparationPlan.clearMultiplayerStatus && callbacks.clearMultiplayerStatus) {
        callbacks.clearMultiplayerStatus();
    }

    const MultiplayerJoinPresentationPlan joinPlan = m_multiplayerJoinCoordinator.joinMultiplayer(
        request,
        m_localPlayerContext,
        m_waitingForRemoteTurnResult);
    if (!joinPlan.joined) {
        if (errorMessage) {
            *errorMessage = joinPlan.errorMessage;
        }
        return false;
    }

    if (joinPlan.nextGameState.has_value()) {
        m_gameState = *joinPlan.nextGameState;
    }
    if (joinPlan.refreshTurnPhase && callbacks.refreshTurnPhase) {
        callbacks.refreshTurnPhase();
    }
    if (joinPlan.reconcileSelection && callbacks.reconcileSelectionBookmark) {
        callbacks.reconcileSelectionBookmark(selectionBookmark);
    }
    if (joinPlan.centerCameraOnBlack && callbacks.centerCameraOnKingdom) {
        callbacks.centerCameraOnKingdom(KingdomId::Black);
    }
    if (joinPlan.hideMultiplayerAlert && callbacks.hideMultiplayerAlert) {
        callbacks.hideMultiplayerAlert();
    }
    if (joinPlan.showHud && callbacks.showHud) {
        callbacks.showHud();
    }
    if (joinPlan.updateUI && callbacks.updateUI) {
        callbacks.updateUI();
    }
    return true;
}