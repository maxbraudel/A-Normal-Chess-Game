#pragma once

#include <functional>
#include <string>

#include "Core/GameSessionConfig.hpp"
#include "Input/InputSelectionBookmark.hpp"
#include "Runtime/SessionPresentationCoordinator.hpp"

class GameEngine;
enum class GameState;
struct JoinMultiplayerRequest;
struct LocalPlayerContext;
class MultiplayerJoinCoordinator;
class MultiplayerRuntime;
class SessionFlow;

struct SessionRuntimeCallbacks {
    std::function<void()> stopMultiplayer;
    std::function<void()> clearMovePreview;
    std::function<void()> activateSelectTool;
    std::function<void()> resetPendingTurn;
    std::function<void()> resetPendingCommands;
    std::function<void()> invalidateTurnDraft;
    std::function<void(KingdomId)> centerCameraOnKingdom;
    std::function<void()> refreshTurnPhase;
    std::function<void()> showHud;
    std::function<void()> updateUI;
    std::function<void()> saveGame;
    std::function<InputSelectionBookmark()> captureSelectionBookmark;
    std::function<void(const InputSelectionBookmark&)> reconcileSelectionBookmark;
    std::function<void()> hideGameMenu;
    std::function<void()> hideMultiplayerAlert;
    std::function<void()> hideMultiplayerWaitingOverlay;
    std::function<void()> clearMultiplayerStatus;
    std::function<void()> showMainMenu;
};

class SessionRuntimeCoordinator {
public:
    SessionRuntimeCoordinator(GameState& gameState,
                              bool& waitingForRemoteTurnResult,
                              LocalPlayerContext& localPlayerContext,
                              GameEngine& engine,
                              MultiplayerRuntime& multiplayer,
                              SessionFlow& sessionFlow,
                              MultiplayerJoinCoordinator& multiplayerJoinCoordinator);

    bool startNewGame(const GameSessionConfig& session,
                      const SessionRuntimeCallbacks& callbacks,
                      std::string* errorMessage = nullptr);
    bool loadGame(const std::string& saveName,
                  const SessionRuntimeCallbacks& callbacks,
                  std::string* errorMessage = nullptr);
    bool joinMultiplayer(const JoinMultiplayerRequest& request,
                         const SessionRuntimeCallbacks& callbacks,
                         std::string* errorMessage = nullptr);
    bool reconnectToMultiplayerHost(const SessionRuntimeCallbacks& callbacks,
                                    std::string* errorMessage = nullptr);
    void returnToMainMenu(const SessionRuntimeCallbacks& callbacks);

private:
    void applySessionResetPlan(const SessionResetPlan& plan,
                               const SessionRuntimeCallbacks& callbacks);
    void applySessionPresentationPlan(const SessionPresentationPlan& plan,
                                      const SessionRuntimeCallbacks& callbacks);
    bool joinMultiplayerInternal(const JoinMultiplayerRequest& request,
                                 bool preserveLanClientContext,
                                 const SessionRuntimeCallbacks& callbacks,
                                 std::string* errorMessage = nullptr);

    GameState& m_gameState;
    bool& m_waitingForRemoteTurnResult;
    LocalPlayerContext& m_localPlayerContext;
    GameEngine& m_engine;
    MultiplayerRuntime& m_multiplayer;
    SessionFlow& m_sessionFlow;
    MultiplayerJoinCoordinator& m_multiplayerJoinCoordinator;
};