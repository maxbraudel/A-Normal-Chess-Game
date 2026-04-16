#include "Runtime/MultiplayerRuntimeCoordinator.hpp"

#include <iostream>

#include "Config/GameConfig.hpp"
#include "Core/GameEngine.hpp"
#include "Core/LocalPlayerContext.hpp"
#include "Input/InputHandler.hpp"
#include "Multiplayer/MultiplayerRuntime.hpp"
#include "Save/SaveManager.hpp"

namespace {

MultiplayerAlertActionBindings makeAlertActionBindings(MultiplayerRuntime& runtime,
                                                       UIManager& uiManager,
                                                       const MultiplayerRuntimeCallbacks& callbacks);

void showMultiplayerAlertPlan(MultiplayerRuntime& runtime,
                              UIManager& uiManager,
                              const MultiplayerAlertPlan& alert,
                              const MultiplayerRuntimeCallbacks& callbacks);

void showLanClientDisconnectAlert(MultiplayerRuntime& runtime,
                                  UIManager& uiManager,
                                  const std::string& title,
                                  const std::string& message,
                                  const MultiplayerRuntimeCallbacks& callbacks);

MultiplayerAlertActionBindings makeAlertActionBindings(MultiplayerRuntime& runtime,
                                                       UIManager& uiManager,
                                                       const MultiplayerRuntimeCallbacks& callbacks) {
    MultiplayerAlertActionBindings bindings;
    bindings.onReturnToMainMenu = callbacks.returnToMainMenu;
    bindings.onReconnectToMultiplayerHost = callbacks.reconnectToMultiplayerHost;
    bindings.onShowReconnectFailure = [&runtime, &uiManager, callbacks](const std::string& title,
                                                                        const std::string& message) {
        showLanClientDisconnectAlert(runtime, uiManager, title, message, callbacks);
    };
    bindings.reconnectAttemptInProgress = [&runtime]() {
        return runtime.reconnectAttemptInProgress();
    };
    bindings.setReconnectAttemptInProgress = [&runtime](bool inProgress) {
        runtime.setReconnectAttemptInProgress(inProgress);
    };
    return bindings;
}

void showMultiplayerAlertPlan(MultiplayerRuntime& runtime,
                              UIManager& uiManager,
                              const MultiplayerAlertPlan& alert,
                              const MultiplayerRuntimeCallbacks& callbacks) {
    const MultiplayerAlertActionBindings bindings = makeAlertActionBindings(runtime, uiManager, callbacks);
    const MultiplayerDialogAction primaryAction =
        MultiplayerRuntimeCoordinator::buildDialogAction(alert.primaryAction, bindings);
    if (alert.secondaryAction.has_value()) {
        uiManager.showMultiplayerAlert(
            alert.title,
            alert.message,
            primaryAction,
            MultiplayerRuntimeCoordinator::buildDialogAction(*alert.secondaryAction, bindings));
        return;
    }

    uiManager.showMultiplayerAlert(alert.title,
                                   alert.message,
                                   primaryAction,
                                   std::nullopt);
}

void showLanClientDisconnectAlert(MultiplayerRuntime& runtime,
                                  UIManager& uiManager,
                                  const std::string& title,
                                  const std::string& message,
                                  const MultiplayerRuntimeCallbacks& callbacks) {
    runtime.noteReconnectAwaiting(message);
    uiManager.hideGameMenu();
    showMultiplayerAlertPlan(
        runtime,
        uiManager,
        MultiplayerEventCoordinator::buildClientDisconnectAlert(
            MultiplayerReconnectDialogState{runtime.hasReconnectRequest()},
            title,
            message),
        callbacks);
}

} // namespace

MultiplayerRuntimeCoordinator::MultiplayerRuntimeCoordinator(MultiplayerRuntime& runtime,
                                                             SaveManager& saveManager,
                                                             GameEngine& engine,
                                                             InputHandler& input,
                                                             UIManager& uiManager,
                                                             LocalPlayerContext& localPlayerContext,
                                                             bool& waitingForRemoteTurnResult,
                                                             const GameConfig& config)
    : m_runtime(runtime)
    , m_saveManager(saveManager)
    , m_engine(engine)
    , m_input(input)
    , m_uiManager(uiManager)
    , m_localPlayerContext(localPlayerContext)
    , m_waitingForRemoteTurnResult(waitingForRemoteTurnResult)
    , m_config(config) {}

void MultiplayerRuntimeCoordinator::update(const MultiplayerRuntimeCallbacks& callbacks) {
    if (m_localPlayerContext.mode == LocalSessionMode::LanHost) {
        m_runtime.update();
        while (m_runtime.hasPendingServerEvent()) {
            processServerEvent(m_runtime.popNextServerEvent(), callbacks);
        }
    }

    if (m_localPlayerContext.mode == LocalSessionMode::LanClient) {
        m_runtime.update();
        while (m_runtime.hasPendingClientEvent()) {
            processClientEvent(m_runtime.popNextClientEvent(), callbacks);
        }
    }
}

MultiplayerDialogAction MultiplayerRuntimeCoordinator::buildDialogAction(
    const MultiplayerAlertActionPlan& action,
    const MultiplayerAlertActionBindings& bindings) {
    switch (action.kind) {
        case MultiplayerAlertActionKind::ReturnToMainMenu:
            return MultiplayerDialogAction{
                action.label,
                [onReturnToMainMenu = bindings.onReturnToMainMenu]() {
                    if (onReturnToMainMenu) {
                        onReturnToMainMenu();
                    }
                }
            };

        case MultiplayerAlertActionKind::Reconnect:
            return MultiplayerDialogAction{
                action.label,
                [reconnectAttemptInProgress = bindings.reconnectAttemptInProgress,
                 setReconnectAttemptInProgress = bindings.setReconnectAttemptInProgress,
                 onReconnectToMultiplayerHost = bindings.onReconnectToMultiplayerHost,
                 onShowReconnectFailure = bindings.onShowReconnectFailure]() {
                    if (reconnectAttemptInProgress && reconnectAttemptInProgress()) {
                        return;
                    }

                    if (setReconnectAttemptInProgress) {
                        setReconnectAttemptInProgress(true);
                    }

                    std::string error;
                    if (!onReconnectToMultiplayerHost || !onReconnectToMultiplayerHost(&error)) {
                        if (setReconnectAttemptInProgress) {
                            setReconnectAttemptInProgress(false);
                        }
                        if (onShowReconnectFailure) {
                            onShowReconnectFailure(
                                "Reconnect Failed",
                                MultiplayerEventCoordinator::reconnectFailureMessage(error));
                        }
                    }
                }
            };

        case MultiplayerAlertActionKind::Continue:
        default:
            return MultiplayerDialogAction{action.label, {}};
    }
}

void MultiplayerRuntimeCoordinator::processServerEvent(const MultiplayerServer::Event& event,
                                                       const MultiplayerRuntimeCallbacks& callbacks) {
    const MultiplayerServerEventPlan plan = MultiplayerEventCoordinator::planServerEvent(
        event,
        MultiplayerHostEventState{
            m_engine.turnSystem().getTurnNumber(),
            m_runtime.lanHostRemoteSessionEstablished()});

    if (plan.hideAlert) {
        m_uiManager.hideMultiplayerAlert();
    }
    if (plan.logMessage.has_value()) {
        m_engine.eventLog().log(m_engine.turnSystem().getTurnNumber(), KingdomId::Black, *plan.logMessage);
    }
    if (plan.remoteSessionEstablished.has_value()) {
        m_runtime.setLanHostRemoteSessionEstablished(*plan.remoteSessionEstablished);
    }
    if (plan.pushSnapshotToRemote && callbacks.pushSnapshotToRemote) {
        std::string snapshotError;
        if (!callbacks.pushSnapshotToRemote(&snapshotError)) {
            if (!snapshotError.empty()) {
                m_engine.eventLog().log(m_engine.turnSystem().getTurnNumber(), KingdomId::Black, snapshotError);
            }
        } else {
            m_runtime.setLanHostRemoteSessionEstablished(true);
        }
    }
    if (plan.applyRemoteTurnSubmission && callbacks.applyRemoteTurnSubmission) {
        std::string error;
        if (!callbacks.applyRemoteTurnSubmission(event.commands, &error)) {
            const std::string rejectionMessage =
                error.empty() ? "The host rejected the submitted turn." : error;
            m_runtime.sendTurnRejected(rejectionMessage, nullptr);
            m_engine.eventLog().log(m_engine.turnSystem().getTurnNumber(), KingdomId::Black,
                                    error.empty() ? "Rejected remote multiplayer turn." : error);
            if (callbacks.pushSnapshotToRemote) {
                callbacks.pushSnapshotToRemote(nullptr);
            }
        }
    }
    if (plan.alert.has_value()) {
        showMultiplayerAlertPlan(m_runtime, m_uiManager, *plan.alert, callbacks);
    }
}

void MultiplayerRuntimeCoordinator::processClientEvent(const MultiplayerClient::Event& event,
                                                       const MultiplayerRuntimeCallbacks& callbacks) {
    const MultiplayerClientEventPlan plan = MultiplayerEventCoordinator::planClientEvent(event);
    switch (plan.type) {
        case MultiplayerClientEventPlan::Type::RestoreSnapshot: {
            const InputSelectionBookmark selectionBookmark = callbacks.captureSelectionBookmark
                ? callbacks.captureSelectionBookmark()
                : InputSelectionBookmark{};

            std::string restoreError;
            if (!MultiplayerEventCoordinator::restoreClientSnapshot(
                    plan.serializedSaveData,
                    m_saveManager,
                    m_engine,
                    m_config,
                    &restoreError)) {
                std::cerr << restoreError << std::endl;
                return;
            }

            MultiplayerEventCoordinator::applyClientSnapshotState(
                m_runtime,
                m_input,
                m_localPlayerContext,
                m_waitingForRemoteTurnResult);
            m_uiManager.hideMultiplayerAlert();
            if (callbacks.refreshTurnPhase) {
                callbacks.refreshTurnPhase();
            }
            if (callbacks.reconcileSelectionBookmark) {
                callbacks.reconcileSelectionBookmark(selectionBookmark);
            }
            if (callbacks.updateUI) {
                callbacks.updateUI();
            }
            break;
        }

        case MultiplayerClientEventPlan::Type::ShowAlert:
            if (plan.alert.has_value()) {
                showMultiplayerAlertPlan(m_runtime, m_uiManager, *plan.alert, callbacks);
            }
            break;

        case MultiplayerClientEventPlan::Type::Disconnect:
            std::cerr << plan.message << std::endl;
            MultiplayerEventCoordinator::applyClientDisconnectState(
                m_runtime,
                m_engine,
                m_input,
                m_localPlayerContext,
                m_waitingForRemoteTurnResult);
            if (callbacks.invalidateTurnDraft) {
                callbacks.invalidateTurnDraft();
            }
            showLanClientDisconnectAlert(m_runtime, m_uiManager, plan.title, plan.message, callbacks);
            break;

        case MultiplayerClientEventPlan::Type::None:
        default:
            break;
    }
}
