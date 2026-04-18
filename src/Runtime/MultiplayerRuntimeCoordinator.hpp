#pragma once

#include <functional>
#include <string>
#include <vector>

#include "Core/GameplayNotification.hpp"
#include "Input/InputSelectionBookmark.hpp"
#include "Runtime/MultiplayerEventCoordinator.hpp"
#include "Systems/TurnCommand.hpp"
#include "UI/UIManager.hpp"

class GameConfig;
class GameEngine;
class InputHandler;
class MultiplayerRuntime;
class SaveManager;
struct LocalPlayerContext;

struct MultiplayerRuntimeCallbacks {
    std::function<bool(const std::vector<GameplayNotification>&, std::string*)> pushSnapshotToRemote;
    std::function<bool(const std::vector<TurnCommand>&, std::string*)> applyRemoteTurnSubmission;
    std::function<InputSelectionBookmark()> captureSelectionBookmark;
    std::function<void(const InputSelectionBookmark&)> reconcileSelectionBookmark;
    std::function<void()> refreshTurnPhase;
    std::function<void()> updateUI;
    std::function<void(const GameplayNotification&)> showGameplayNotification;
    std::function<void()> invalidateTurnDraft;
    std::function<void()> returnToMainMenu;
    std::function<bool(std::string*)> reconnectToMultiplayerHost;
};

struct MultiplayerAlertActionBindings {
    std::function<void()> onReturnToMainMenu;
    std::function<bool(std::string*)> onReconnectToMultiplayerHost;
    std::function<void(const std::string&, const std::string&)> onShowReconnectFailure;
    std::function<bool()> reconnectAttemptInProgress;
    std::function<void(bool)> setReconnectAttemptInProgress;
};

class MultiplayerRuntimeCoordinator {
public:
    MultiplayerRuntimeCoordinator(MultiplayerRuntime& runtime,
                                  SaveManager& saveManager,
                                  GameEngine& engine,
                                  InputHandler& input,
                                  UIManager& uiManager,
                                  LocalPlayerContext& localPlayerContext,
                                  bool& waitingForRemoteTurnResult,
                                  const GameConfig& config);

    void update(const MultiplayerRuntimeCallbacks& callbacks);

    static MultiplayerDialogAction buildDialogAction(const MultiplayerAlertActionPlan& action,
                                                     const MultiplayerAlertActionBindings& bindings);

private:
    void processServerEvent(const MultiplayerServer::Event& event,
                            const MultiplayerRuntimeCallbacks& callbacks);
    void processClientEvent(const MultiplayerClient::Event& event,
                            const MultiplayerRuntimeCallbacks& callbacks);

    MultiplayerRuntime& m_runtime;
    SaveManager& m_saveManager;
    GameEngine& m_engine;
    InputHandler& m_input;
    UIManager& m_uiManager;
    LocalPlayerContext& m_localPlayerContext;
    bool& m_waitingForRemoteTurnResult;
    const GameConfig& m_config;
};