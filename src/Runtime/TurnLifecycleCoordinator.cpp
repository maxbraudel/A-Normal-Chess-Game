#include "Runtime/TurnLifecycleCoordinator.hpp"

#include <iostream>

#include "Core/GameEngine.hpp"
#include "Input/InputHandler.hpp"
#include "Runtime/TurnCoordinator.hpp"
#include "UI/UIManager.hpp"

TurnLifecycleCoordinator::TurnLifecycleCoordinator(GameState& gameState,
                                                   bool& waitingForRemoteTurnResult,
                                                   GameEngine& engine,
                                                   TurnCoordinator& turnCoordinator,
                                                   InputHandler& input,
                                                   UIManager& uiManager)
    : m_gameState(gameState)
    , m_waitingForRemoteTurnResult(waitingForRemoteTurnResult)
    , m_engine(engine)
    , m_turnCoordinator(turnCoordinator)
    , m_input(input)
    , m_uiManager(uiManager) {}

CommitPlayerTurnDispatchPlan TurnLifecycleCoordinator::buildCommitPlayerTurnDispatchPlan(bool lanClient) {
    CommitPlayerTurnDispatchPlan plan;
    if (lanClient) {
        plan.submitClientTurn = true;
        return plan;
    }

    plan.commitAuthoritativeTurn = true;
    return plan;
}

ClientTurnSubmissionFailurePlan TurnLifecycleCoordinator::buildClientTurnSubmissionFailurePlan(
    const std::string& errorMessage,
    bool clientConnected) {
    ClientTurnSubmissionFailurePlan plan;
    if (!errorMessage.empty() && clientConnected) {
        plan.showAlert = true;
        plan.alertTitle = "Turn Not Sent";
        plan.alertMessage = errorMessage;
    }
    return plan;
}

TurnResetPlan TurnLifecycleCoordinator::buildResetPlan() {
    TurnResetPlan plan;
    plan.cancelLiveMove = true;
    plan.resetPendingCommands = true;
    plan.syncTurnDraftBeforeReconcile = true;
    plan.reconcileSelection = true;
    return plan;
}

void TurnLifecycleCoordinator::commitPlayerTurn(bool lanClient,
                                                bool lanHost,
                                                bool clientConnected,
                                                const TurnLifecycleCallbacks& callbacks) {
    const CommitPlayerTurnDispatchPlan dispatchPlan = buildCommitPlayerTurnDispatchPlan(lanClient);
    if (dispatchPlan.submitClientTurn) {
        std::string error;
        if (!submitClientTurn(lanClient, callbacks, &error)) {
            std::cerr << "Failed to submit multiplayer turn: " << error << std::endl;
            const ClientTurnSubmissionFailurePlan failurePlan =
                buildClientTurnSubmissionFailurePlan(error, clientConnected);
            if (failurePlan.showAlert) {
                m_uiManager.showMultiplayerAlert(failurePlan.alertTitle, failurePlan.alertMessage);
            }
        }
        return;
    }

    if (dispatchPlan.commitAuthoritativeTurn) {
        commitAuthoritativeTurn(lanHost, callbacks);
    }
}

void TurnLifecycleCoordinator::commitAuthoritativeTurn(bool lanHost,
                                                       const TurnLifecycleCallbacks& callbacks) {
    const InputSelectionBookmark selectionBookmark = callbacks.captureSelectionBookmark
        ? callbacks.captureSelectionBookmark()
        : InputSelectionBookmark{};
    const AuthoritativeTurnExecution execution = m_turnCoordinator.executeAuthoritativeTurn();
    const AuthoritativeCommitPlan plan = TurnCoordinator::buildAuthoritativeCommitPlan(
        execution,
        lanHost,
        m_engine.participantName(execution.winner));

    if (plan.nextGameState.has_value()) {
        m_gameState = *plan.nextGameState;
    }
    if (plan.eventLogEntry.has_value()) {
        m_engine.eventLog().log(m_engine.turnSystem().getTurnNumber(),
                                plan.eventLogEntry->first,
                                plan.eventLogEntry->second);
    }
    if (plan.clearMovePreview) {
        m_input.clearMovePreview();
    }
    if (plan.clearWaitingForRemoteTurnResult) {
        m_waitingForRemoteTurnResult = false;
    }
    if (plan.refreshTurnPhase && callbacks.refreshTurnPhase) {
        callbacks.refreshTurnPhase();
    }
    if (plan.syncTurnDraftBeforeReconcile && callbacks.ensureTurnDraftUpToDate) {
        callbacks.ensureTurnDraftUpToDate();
    }
    if (plan.reconcileSelection && callbacks.reconcileSelectionBookmark) {
        callbacks.reconcileSelectionBookmark(selectionBookmark);
    }
    if (plan.persistLanHostSnapshot && callbacks.persistLanHostSnapshot) {
        callbacks.persistLanHostSnapshot();
    }
    if (plan.updateUI && callbacks.updateUI) {
        callbacks.updateUI();
    }
    for (const GameplayNotification& notification : execution.notifications) {
        if (callbacks.showGameplayNotification) {
            callbacks.showGameplayNotification(notification);
        }
    }
}

void TurnLifecycleCoordinator::resetPlayerTurn(const TurnLifecycleCallbacks& callbacks) {
    const TurnResetPlan plan = buildResetPlan();
    const InputSelectionBookmark selectionBookmark = callbacks.captureSelectionBookmark
        ? callbacks.captureSelectionBookmark()
        : InputSelectionBookmark{};
    if (plan.cancelLiveMove) {
        m_input.cancelLiveMove(m_engine.activeKingdom(), m_engine.turnSystem());
    }
    if (plan.resetPendingCommands) {
        m_engine.turnSystem().resetPendingCommands();
    }
    if (plan.syncTurnDraftBeforeReconcile && callbacks.ensureTurnDraftUpToDate) {
        callbacks.ensureTurnDraftUpToDate();
    }
    if (plan.reconcileSelection && callbacks.reconcileSelectionBookmark) {
        callbacks.reconcileSelectionBookmark(selectionBookmark);
    }
}

bool TurnLifecycleCoordinator::applyRemoteTurnSubmission(bool lanHost,
                                                         const std::vector<TurnCommand>& commands,
                                                         const TurnLifecycleCallbacks& callbacks,
                                                         std::string* errorMessage) {
    const RemoteTurnSubmissionResult result = m_turnCoordinator.applyRemoteTurnSubmission(lanHost, commands);
    if (result.shouldCommitAuthoritativeTurn) {
        commitAuthoritativeTurn(lanHost, callbacks);
    } else if (result.shouldResetPendingCommands) {
        m_engine.turnSystem().resetPendingCommands();
    }

    if (!result.accepted) {
        if (errorMessage) {
            *errorMessage = result.rejectionMessage;
        }
        return false;
    }

    return true;
}

bool TurnLifecycleCoordinator::submitClientTurn(bool lanClient,
                                                const TurnLifecycleCallbacks& callbacks,
                                                std::string* errorMessage) {
    const ClientTurnSubmissionResult result = m_turnCoordinator.submitClientTurn(lanClient);
    if (!result.submitted) {
        if (errorMessage) {
            *errorMessage = result.errorMessage;
        }
        return false;
    }

    const InputSelectionBookmark selectionBookmark = callbacks.captureSelectionBookmark
        ? callbacks.captureSelectionBookmark()
        : InputSelectionBookmark{};
    if (result.waitForRemoteTurnResult) {
        m_waitingForRemoteTurnResult = true;
    }
    if (result.clearMovePreview) {
        m_input.clearMovePreview();
    }
    if (result.resetPendingCommands) {
        m_engine.turnSystem().resetPendingCommands();
    }
    if (result.syncTurnDraftBeforeReconcile && callbacks.ensureTurnDraftUpToDate) {
        callbacks.ensureTurnDraftUpToDate();
    }
    if (result.reconcileSelection && callbacks.reconcileSelectionBookmark) {
        callbacks.reconcileSelectionBookmark(selectionBookmark);
    }
    return true;
}