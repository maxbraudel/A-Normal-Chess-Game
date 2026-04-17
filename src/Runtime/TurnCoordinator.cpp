#include "Runtime/TurnCoordinator.hpp"

#include "Config/GameConfig.hpp"
#include "Core/GameEngine.hpp"
#include "Debug/GameStateDebugRecorder.hpp"
#include "Multiplayer/MultiplayerRuntime.hpp"

TurnCoordinator::TurnCoordinator(GameEngine& engine,
                                 MultiplayerRuntime& multiplayer,
                                 GameStateDebugRecorder& debugRecorder,
                                 const GameConfig& config)
    : m_engine(engine)
    , m_multiplayer(multiplayer)
    , m_debugRecorder(debugRecorder)
    , m_config(config) {}

AuthoritativeTurnExecution TurnCoordinator::executeAuthoritativeTurn() {
    AuthoritativeTurnExecution execution;
    execution.committedActiveKingdom = m_engine.turnSystem().getActiveKingdom();
    execution.committedTurnNumber = m_engine.turnSystem().getTurnNumber();

    const PendingTurnCommitResult commitResult = m_engine.commitPendingTurn(m_config);
    execution.committed = commitResult.committed;
    execution.gameOver = commitResult.gameOver;
    execution.winner = commitResult.winner;
    execution.notifications = commitResult.notifications;

    if (commitResult.committed) {
        m_debugRecorder.logTurnState(execution.committedTurnNumber,
                                     m_engine.kingdoms(),
                                     "after player commit");
        m_debugRecorder.recordSnapshot(execution.committedTurnNumber,
                                       execution.committedActiveKingdom,
                                       m_engine.kingdoms(),
                                       "after_player_commit");
    }

    return execution;
}

AuthoritativeCommitPlan TurnCoordinator::buildAuthoritativeCommitPlan(
    const AuthoritativeTurnExecution& execution,
    bool lanHost,
    const std::string& winnerName) {
    AuthoritativeCommitPlan plan;

    if (!execution.committed) {
        if (execution.gameOver) {
            plan.nextGameState = GameState::GameOver;
            plan.eventLogEntry = std::make_pair(
                execution.winner,
                "Checkmate! " + winnerName + " wins!");
            plan.clearMovePreview = true;
            plan.reconcileSelection = true;
            plan.updateUI = true;
            plan.persistLanHostSnapshot = lanHost;
        }
        return plan;
    }

    plan.clearMovePreview = true;
    plan.clearWaitingForRemoteTurnResult = true;
    plan.refreshTurnPhase = true;
    plan.syncTurnDraftBeforeReconcile = true;
    plan.reconcileSelection = true;

    if (execution.gameOver) {
        plan.nextGameState = GameState::GameOver;
        plan.eventLogEntry = std::make_pair(
            execution.winner,
            "Checkmate! " + winnerName + " wins!");
        plan.updateUI = true;
        plan.persistLanHostSnapshot = lanHost;
        return plan;
    }

    plan.persistLanHostSnapshot = lanHost;
    return plan;
}

ClientTurnSubmissionResult TurnCoordinator::submitClientTurn(bool lanClient) {
    ClientTurnSubmissionResult result;
    if (!lanClient) {
        return result;
    }

    if (!m_multiplayer.clientIsAuthenticated()) {
        result.errorMessage = "The multiplayer host connection is not authenticated.";
        return result;
    }

    const CheckTurnValidation validation = m_engine.validatePendingTurn(m_config);
    if (!validation.valid) {
        result.errorMessage = validation.errorMessage;
        return result;
    }

    if (!m_multiplayer.submitTurnSubmission(m_engine.turnSystem().getPendingCommands(),
                                            &result.errorMessage)) {
        return result;
    }

    result.submitted = true;
    result.clearMovePreview = true;
    result.waitForRemoteTurnResult = true;
    return result;
}

RemoteTurnSubmissionResult TurnCoordinator::applyRemoteTurnSubmission(
    bool lanHost,
    const std::vector<TurnCommand>& commands) {
    RemoteTurnSubmissionResult result;
    if (!lanHost) {
        result.rejectionMessage = "Cannot apply a remote turn submission outside LAN host mode.";
        return result;
    }

    if (m_engine.turnSystem().getActiveKingdom() != KingdomId::Black) {
        result.rejectionMessage = "Remote turns are only accepted when Black is the active kingdom.";
        return result;
    }

    if (!m_engine.replacePendingCommands(commands,
                                         m_config,
                                         true,
                                         &result.rejectionMessage)) {
        return result;
    }

    const CheckTurnValidation validation = m_engine.validatePendingTurn(m_config);
    if (!validation.valid) {
        result.rejectionMessage = validation.errorMessage;
        result.shouldCommitAuthoritativeTurn =
            validation.activeKingInCheck && !validation.hasAnyLegalResponse;
        result.shouldResetPendingCommands = !result.shouldCommitAuthoritativeTurn;
        return result;
    }

    result.accepted = true;
    result.shouldCommitAuthoritativeTurn = true;
    return result;
}