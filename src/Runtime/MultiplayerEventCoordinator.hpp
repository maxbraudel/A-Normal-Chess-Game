#pragma once

#include <optional>
#include <string>

#include "Multiplayer/MultiplayerClient.hpp"
#include "Multiplayer/MultiplayerServer.hpp"

class GameConfig;
class GameEngine;
class InputHandler;
class MultiplayerRuntime;
class SaveManager;
struct LocalPlayerContext;

struct MultiplayerHostEventState {
    int turnNumber = 1;
    bool remoteSessionEstablished = false;
};

enum class MultiplayerAlertActionKind {
    Continue,
    ReturnToMainMenu,
    Reconnect
};

struct MultiplayerAlertActionPlan {
    MultiplayerAlertActionKind kind = MultiplayerAlertActionKind::Continue;
    std::string label;
};

struct MultiplayerAlertPlan {
    std::string title;
    std::string message;
    MultiplayerAlertActionPlan primaryAction;
    std::optional<MultiplayerAlertActionPlan> secondaryAction;
};

struct MultiplayerServerEventPlan {
    std::optional<std::string> logMessage;
    std::optional<bool> remoteSessionEstablished;
    bool hideAlert = false;
    bool pushSnapshotToRemote = false;
    bool applyRemoteTurnSubmission = false;
    std::optional<MultiplayerAlertPlan> alert;
};

struct MultiplayerClientEventPlan {
    enum class Type {
        None,
        RestoreSnapshot,
        ShowAlert,
        Disconnect
    };

    Type type = Type::None;
    std::string serializedSaveData;
    std::string title;
    std::string message;
    std::optional<MultiplayerAlertPlan> alert;
};

struct MultiplayerReconnectDialogState {
    bool hasReconnectRequest = false;
};

class MultiplayerEventCoordinator {
public:
    static MultiplayerServerEventPlan planServerEvent(const MultiplayerServer::Event& event,
                                                      const MultiplayerHostEventState& state);
    static MultiplayerClientEventPlan planClientEvent(const MultiplayerClient::Event& event);
    static MultiplayerAlertPlan buildClientDisconnectAlert(const MultiplayerReconnectDialogState& state,
                                                           const std::string& title,
                                                           const std::string& message);
    static std::string reconnectFailureMessage(const std::string& errorMessage);
    static bool restoreClientSnapshot(const std::string& serializedSaveData,
                                      SaveManager& saveManager,
                                      GameEngine& engine,
                                      const GameConfig& config,
                                      std::string* errorMessage = nullptr);
    static void applyClientSnapshotState(MultiplayerRuntime& runtime,
                                         InputHandler& input,
                                         LocalPlayerContext& localPlayerContext,
                                         bool& waitingForRemoteTurnResult);
    static void applyClientDisconnectState(MultiplayerRuntime& runtime,
                                           GameEngine& engine,
                                           InputHandler& input,
                                           LocalPlayerContext& localPlayerContext,
                                           bool& waitingForRemoteTurnResult);
};