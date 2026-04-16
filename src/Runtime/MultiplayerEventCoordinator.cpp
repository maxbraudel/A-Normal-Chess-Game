#include "Runtime/MultiplayerEventCoordinator.hpp"

#include "Config/GameConfig.hpp"
#include "Core/GameEngine.hpp"
#include "Core/LocalPlayerContext.hpp"
#include "Core/ToolState.hpp"
#include "Input/InputHandler.hpp"
#include "Multiplayer/MultiplayerRuntime.hpp"
#include "Save/SaveData.hpp"
#include "Save/SaveManager.hpp"

namespace {

void writeMultiplayerError(std::string* errorMessage, const std::string& message) {
    if (errorMessage) {
        *errorMessage = message;
    }
}

} // namespace

MultiplayerServerEventPlan MultiplayerEventCoordinator::planServerEvent(
    const MultiplayerServer::Event& event,
    const MultiplayerHostEventState& state) {
    MultiplayerServerEventPlan plan;
    switch (event.type) {
        case MultiplayerServer::Event::Type::ClientConnected:
            plan.hideAlert = true;
            plan.logMessage = event.message;
            plan.remoteSessionEstablished = false;
            plan.pushSnapshotToRemote = true;
            break;

        case MultiplayerServer::Event::Type::ClientDisconnected:
            plan.logMessage = event.message;
            plan.remoteSessionEstablished = false;
            if (state.remoteSessionEstablished) {
                plan.alert = MultiplayerAlertPlan{
                    "Black Disconnected",
                    event.message + "\n\nWhite cannot continue until Black reconnects.",
                    MultiplayerAlertActionPlan{MultiplayerAlertActionKind::Continue, "Continue"},
                    std::nullopt
                };
            }
            break;

        case MultiplayerServer::Event::Type::ClientConnectionInterrupted:
            plan.remoteSessionEstablished = false;
            if (!event.message.empty()) {
                plan.logMessage = event.message;
            }
            break;

        case MultiplayerServer::Event::Type::TurnSubmitted:
            plan.applyRemoteTurnSubmission = true;
            break;

        case MultiplayerServer::Event::Type::Error:
            plan.logMessage = event.message;
            plan.alert = MultiplayerAlertPlan{
                "LAN Host Error",
                event.message.empty() ? "The LAN host encountered a network error." : event.message,
                MultiplayerAlertActionPlan{MultiplayerAlertActionKind::Continue, "Continue"},
                std::nullopt
            };
            break;
    }

    return plan;
}

MultiplayerClientEventPlan MultiplayerEventCoordinator::planClientEvent(
    const MultiplayerClient::Event& event) {
    MultiplayerClientEventPlan plan;
    switch (event.type) {
        case MultiplayerClient::Event::Type::SnapshotReceived:
            plan.type = MultiplayerClientEventPlan::Type::RestoreSnapshot;
            plan.serializedSaveData = event.serializedSaveData;
            break;

        case MultiplayerClient::Event::Type::TurnRejected:
            plan.type = MultiplayerClientEventPlan::Type::ShowAlert;
            plan.alert = MultiplayerAlertPlan{
                "Turn Rejected",
                (event.message.empty() ? std::string{"The host rejected the submitted turn."} : event.message)
                    + "\n\nYour client stays synchronized with the host snapshot.",
                MultiplayerAlertActionPlan{MultiplayerAlertActionKind::Continue, "Continue"},
                std::nullopt
            };
            break;

        case MultiplayerClient::Event::Type::Disconnected:
        case MultiplayerClient::Event::Type::Error:
            plan.type = MultiplayerClientEventPlan::Type::Disconnect;
            plan.title = event.type == MultiplayerClient::Event::Type::Disconnected
                ? "Host Disconnected"
                : "Network Error";
            plan.message = event.message.empty()
                ? (event.type == MultiplayerClient::Event::Type::Disconnected
                    ? "Multiplayer host disconnected."
                    : "The multiplayer client encountered a network error.")
                : event.message;
            break;

        default:
            break;
    }

    return plan;
}

MultiplayerAlertPlan MultiplayerEventCoordinator::buildClientDisconnectAlert(
    const MultiplayerReconnectDialogState& state,
    const std::string& title,
    const std::string& message) {
    MultiplayerAlertPlan alert;
    alert.title = title;
    alert.message = message;
    if (!state.hasReconnectRequest) {
        alert.primaryAction = {MultiplayerAlertActionKind::ReturnToMainMenu, "Return to Main Menu"};
        return alert;
    }

    alert.primaryAction = {MultiplayerAlertActionKind::Reconnect, "Reconnect"};
    alert.secondaryAction = MultiplayerAlertActionPlan{
        MultiplayerAlertActionKind::ReturnToMainMenu,
        "Return to Main Menu"
    };
    return alert;
}

std::string MultiplayerEventCoordinator::reconnectFailureMessage(const std::string& errorMessage) {
    return errorMessage.empty()
        ? "Unable to reconnect to the previous multiplayer host."
        : "Unable to reconnect to the previous multiplayer host.\n\n" + errorMessage;
}

bool MultiplayerEventCoordinator::restoreClientSnapshot(const std::string& serializedSaveData,
                                                        SaveManager& saveManager,
                                                        GameEngine& engine,
                                                        const GameConfig& config,
                                                        std::string* errorMessage) {
    SaveData snapshotData;
    if (!saveManager.deserialize(serializedSaveData, snapshotData)) {
        writeMultiplayerError(errorMessage, "Received an invalid multiplayer snapshot from the host.");
        return false;
    }

    std::string restoreError;
    if (!engine.restoreFromSave(snapshotData, config, &restoreError)) {
        writeMultiplayerError(
            errorMessage,
            restoreError.empty()
                ? "Failed to restore multiplayer snapshot."
                : "Failed to restore multiplayer snapshot: " + restoreError);
        return false;
    }

    return true;
}

void MultiplayerEventCoordinator::applyClientSnapshotState(MultiplayerRuntime& runtime,
                                                           InputHandler& input,
                                                           LocalPlayerContext& localPlayerContext,
                                                           bool& waitingForRemoteTurnResult) {
    localPlayerContext = makeLanClientLocalPlayerContext();
    waitingForRemoteTurnResult = false;
    runtime.noteReconnectRecovered();
    input.clearMovePreview();
}

void MultiplayerEventCoordinator::applyClientDisconnectState(MultiplayerRuntime& runtime,
                                                             GameEngine& engine,
                                                             InputHandler& input,
                                                             LocalPlayerContext& localPlayerContext,
                                                             bool& waitingForRemoteTurnResult) {
    runtime.resetConnections();
    localPlayerContext = makeLanClientLocalPlayerContext();
    waitingForRemoteTurnResult = false;
    input.clearMovePreview();
    input.clearSelection();
    input.setTool(ToolState::Select);
    engine.resetPendingTurn();
}