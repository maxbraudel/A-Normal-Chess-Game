#include "Runtime/TurnCoordinator.hpp"

#include "Config/GameConfig.hpp"
#include "Core/GameEngine.hpp"
#include "Data/GameDataRecorder.hpp"
#include "Debug/GameStateDebugRecorder.hpp"
#include "Multiplayer/MultiplayerRuntime.hpp"
#include "Telemetry/BehavioralTelemetryCollector.hpp"

namespace {

BehavioralTelemetryOrigin localAuthoritativeOrigin(const GameEngine& engine,
                                                   KingdomId activeKingdom) {
    if (engine.sessionConfig().multiplayer.enabled && activeKingdom == KingdomId::Black) {
        return BehavioralTelemetryOrigin::HostObserved;
    }

    return BehavioralTelemetryOrigin::LocalHost;
}

} // namespace

TurnCoordinator::TurnCoordinator(GameEngine& engine,
                                 MultiplayerRuntime& multiplayer,
                                 GameStateDebugRecorder& debugRecorder,
                                 GameDataRecorder& dataRecorder,
                                 BehavioralTelemetryCollector& behavioralTelemetry,
                                 const GameConfig& config)
    : m_engine(engine)
    , m_multiplayer(multiplayer)
    , m_debugRecorder(debugRecorder)
    , m_dataRecorder(dataRecorder)
    , m_behavioralTelemetry(behavioralTelemetry)
    , m_config(config) {}

AuthoritativeTurnExecution TurnCoordinator::executeAuthoritativeTurn() {
    AuthoritativeTurnExecution execution;
    execution.committedActiveKingdom = m_engine.turnSystem().getActiveKingdom();
    execution.committedTurnNumber = m_engine.turnSystem().getTurnNumber();
    const BehavioralTelemetryOrigin telemetryOrigin = localAuthoritativeOrigin(
        m_engine,
        execution.committedActiveKingdom);
    m_behavioralTelemetry.beginPendingTurn(
        execution.committedTurnNumber,
        execution.committedActiveKingdom,
        telemetryOrigin);
    m_behavioralTelemetry.recordOrchestration(
        execution.committedTurnNumber,
        execution.committedActiveKingdom,
        telemetryOrigin,
        BehavioralTelemetryStage::Commit,
        "commit_started",
        "Commit Started");
    const std::vector<TurnCommand> queuedCommands = m_engine.turnSystem().getPendingCommands();
    const std::vector<TurnCommandAuditEntry> commandAuditTrail =
        m_engine.turnSystem().getCommandAuditTrail();
    const std::vector<XPRewardAuditEntry> xpAuditTrail = m_engine.xpRewardAuditTrail();

    const PendingTurnCommitResult commitResult = m_engine.commitPendingTurn(m_config);
    execution.committed = commitResult.committed;
    execution.gameOver = commitResult.gameOver;
    execution.winner = commitResult.winner;
    execution.notifications = commitResult.notifications;
    const BehavioralPendingTurnTelemetry committedTurnTelemetry =
        commitResult.committed ? m_behavioralTelemetry.snapshotPendingTurn()
                               : BehavioralPendingTurnTelemetry{};
    BehavioralTelemetryEventDetails commitDetails;
    commitDetails.hasAccepted = true;
    commitDetails.accepted = commitResult.committed;
    commitDetails.pendingStateRevision = m_engine.turnSystem().getPendingStateRevision();
    if (!commitResult.committed) {
        commitDetails.reason = "pending_turn_not_committed";
    }
    m_behavioralTelemetry.recordOrchestration(
        execution.committedTurnNumber,
        execution.committedActiveKingdom,
        telemetryOrigin,
        BehavioralTelemetryStage::Commit,
        commitResult.committed ? "commit_finished" : "commit_skipped",
        commitResult.committed ? "Commit Finished" : "Commit Skipped",
        commitDetails);

    if (commitResult.committed) {
        m_debugRecorder.logTurnState(execution.committedTurnNumber,
                                     m_engine.kingdoms(),
                                     "after player commit");
        m_debugRecorder.recordSnapshot(execution.committedTurnNumber,
                                       execution.committedActiveKingdom,
                                       m_engine.kingdoms(),
                                       "after_player_commit");

        if (m_dataRecorder.isEnabled()) {
            m_dataRecorder.recordCommittedTurn(
                queuedCommands,
                commandAuditTrail,
                xpAuditTrail,
                execution.committedTurnNumber,
                execution.committedActiveKingdom,
                commitResult.activeValidation,
                commitResult.nextTurnValidation,
                commitResult.gameOver,
                commitResult.winner,
                commitResult.notifications,
                committedTurnTelemetry,
                m_engine.createSaveData());
        }

            m_behavioralTelemetry.finalizeCommittedTurn();
        m_engine.turnSystem().clearCommandAuditTrail();
        m_engine.clearXPRewardAuditTrail();
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

    const int turnNumber = m_engine.turnSystem().getTurnNumber();
    const KingdomId activeKingdom = m_engine.turnSystem().getActiveKingdom();
    m_behavioralTelemetry.beginPendingTurn(
        turnNumber,
        activeKingdom,
        BehavioralTelemetryOrigin::LocalClient);
    m_behavioralTelemetry.recordOrchestration(
        turnNumber,
        activeKingdom,
        BehavioralTelemetryOrigin::LocalClient,
        BehavioralTelemetryStage::Submission,
        "submit_requested",
        "Submit Requested");

    if (!m_multiplayer.clientIsAuthenticated()) {
        result.errorMessage = "The multiplayer host connection is not authenticated.";
        BehavioralTelemetryEventDetails details;
        details.reason = result.errorMessage;
        m_behavioralTelemetry.recordOrchestration(
            turnNumber,
            activeKingdom,
            BehavioralTelemetryOrigin::LocalClient,
            BehavioralTelemetryStage::Submission,
            "submit_authentication_failed",
            "Submit Authentication Failed",
            details);
        return result;
    }

    const CheckTurnValidation validation = m_engine.validatePendingTurn(m_config);
    if (!validation.valid) {
        result.errorMessage = validation.errorMessage;
        BehavioralTelemetryEventDetails details;
        details.reason = validation.errorMessage;
        m_behavioralTelemetry.recordOrchestration(
            turnNumber,
            activeKingdom,
            BehavioralTelemetryOrigin::LocalClient,
            BehavioralTelemetryStage::Validation,
            "submit_validation_failed",
            "Submit Validation Failed",
            details);
        return result;
    }

    MultiplayerTurnSubmission submission;
    submission.commands = m_engine.turnSystem().getPendingCommands();
    if (m_engine.sessionConfig().dataCollectionEnabled
        && m_engine.sessionConfig().behavioralTelemetryEnabled) {
        submission.behavioralTelemetry = m_behavioralTelemetry.snapshotPendingTurn();
    }

    if (!m_multiplayer.submitTurnSubmission(submission, &result.errorMessage)) {
        BehavioralTelemetryEventDetails details;
        details.reason = result.errorMessage;
        m_behavioralTelemetry.recordOrchestration(
            turnNumber,
            activeKingdom,
            BehavioralTelemetryOrigin::LocalClient,
            BehavioralTelemetryStage::Submission,
            "submission_send_failed",
            "Submission Send Failed",
            details);
        return result;
    }

    BehavioralTelemetryEventDetails details;
    details.hasAccepted = true;
    details.accepted = true;
    details.pendingStateRevision = m_engine.turnSystem().getPendingStateRevision();
    m_behavioralTelemetry.recordOrchestration(
        turnNumber,
        activeKingdom,
        BehavioralTelemetryOrigin::LocalClient,
        BehavioralTelemetryStage::Submission,
        "submission_sent",
        "Submission Sent",
        details);

    result.submitted = true;
    result.clearMovePreview = true;
    result.waitForRemoteTurnResult = true;
    return result;
}

RemoteTurnSubmissionResult TurnCoordinator::applyRemoteTurnSubmission(
    bool lanHost,
    const MultiplayerTurnSubmission& submission) {
    RemoteTurnSubmissionResult result;
    const int turnNumber = m_engine.turnSystem().getTurnNumber();
    const KingdomId activeKingdom = m_engine.turnSystem().getActiveKingdom();
    m_behavioralTelemetry.beginPendingTurn(
        turnNumber,
        activeKingdom,
        BehavioralTelemetryOrigin::HostObserved);
    m_engine.turnSystem().setBehavioralTelemetry(
        &m_behavioralTelemetry,
        BehavioralTelemetryOrigin::HostObserved);
    BehavioralTelemetryEventDetails receivedDetails;
    receivedDetails.hasAccepted = true;
    receivedDetails.accepted = true;
    receivedDetails.pendingStateRevision = m_engine.turnSystem().getPendingStateRevision();
    m_behavioralTelemetry.recordOrchestration(
        turnNumber,
        activeKingdom,
        BehavioralTelemetryOrigin::HostObserved,
        BehavioralTelemetryStage::Submission,
        "remote_submission_received",
        "Remote Submission Received",
        receivedDetails);
    if (m_engine.sessionConfig().dataCollectionEnabled
        && m_engine.sessionConfig().behavioralTelemetryEnabled
        && submission.behavioralTelemetry.telemetryRevision > 0) {
        m_behavioralTelemetry.replaceRemoteClientReportedPendingTurn(
            submission.behavioralTelemetry,
            0);
        m_dataRecorder.setPendingTurnTelemetry(m_behavioralTelemetry.snapshotPendingTurn());
    }

    if (!lanHost) {
        result.rejectionMessage = "Cannot apply a remote turn submission outside LAN host mode.";
        BehavioralTelemetryEventDetails details;
        details.reason = result.rejectionMessage;
        m_behavioralTelemetry.recordOrchestration(
            turnNumber,
            activeKingdom,
            BehavioralTelemetryOrigin::HostObserved,
            BehavioralTelemetryStage::Submission,
            "remote_submission_rejected",
            "Remote Submission Rejected",
            details);
        return result;
    }

    if (m_engine.turnSystem().getActiveKingdom() != KingdomId::Black) {
        result.rejectionMessage = "Remote turns are only accepted when Black is the active kingdom.";
        BehavioralTelemetryEventDetails details;
        details.reason = result.rejectionMessage;
        m_behavioralTelemetry.recordOrchestration(
            turnNumber,
            activeKingdom,
            BehavioralTelemetryOrigin::HostObserved,
            BehavioralTelemetryStage::Submission,
            "remote_submission_rejected",
            "Remote Submission Rejected",
            details);
        return result;
    }

    if (!m_engine.replacePendingCommands(submission.commands,
                                         m_config,
                                         true,
                                         &result.rejectionMessage)) {
        BehavioralTelemetryEventDetails details;
        details.reason = result.rejectionMessage;
        m_behavioralTelemetry.recordOrchestration(
            turnNumber,
            activeKingdom,
            BehavioralTelemetryOrigin::HostObserved,
            BehavioralTelemetryStage::Validation,
            "remote_submission_replace_failed",
            "Remote Submission Replace Failed",
            details);
        return result;
    }

    const CheckTurnValidation validation = m_engine.validatePendingTurn(m_config);
    if (!validation.valid) {
        result.rejectionMessage = validation.errorMessage;
        result.shouldCommitAuthoritativeTurn =
            validation.activeKingInCheck && !validation.hasAnyLegalResponse;
        result.shouldResetPendingCommands = !result.shouldCommitAuthoritativeTurn;
        BehavioralTelemetryEventDetails details;
        details.reason = validation.errorMessage;
        details.hasAccepted = true;
        details.accepted = false;
        details.pendingStateRevision = m_engine.turnSystem().getPendingStateRevision();
        m_behavioralTelemetry.recordOrchestration(
            turnNumber,
            activeKingdom,
            BehavioralTelemetryOrigin::HostObserved,
            BehavioralTelemetryStage::Validation,
            "remote_submission_validation_failed",
            "Remote Submission Validation Failed",
            details);
        return result;
    }

    result.accepted = true;
    result.shouldCommitAuthoritativeTurn = true;
    BehavioralTelemetryEventDetails details;
    details.hasAccepted = true;
    details.accepted = true;
    details.pendingStateRevision = m_engine.turnSystem().getPendingStateRevision();
    m_behavioralTelemetry.recordOrchestration(
        turnNumber,
        activeKingdom,
        BehavioralTelemetryOrigin::HostObserved,
        BehavioralTelemetryStage::Validation,
        "remote_submission_accepted",
        "Remote Submission Accepted",
        details);
    return result;
}