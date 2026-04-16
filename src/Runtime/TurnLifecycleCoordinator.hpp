#pragma once

#include <functional>
#include <string>
#include <vector>

#include "Core/GameState.hpp"
#include "Input/InputSelectionBookmark.hpp"

class AITurnCoordinator;
class GameEngine;
class InputHandler;
class TurnCoordinator;
class UIManager;
struct TurnCommand;

struct CommitPlayerTurnDispatchPlan {
    bool submitClientTurn = false;
    bool commitAuthoritativeTurn = false;
};

struct ClientTurnSubmissionFailurePlan {
    bool showAlert = false;
    std::string alertTitle;
    std::string alertMessage;
};

struct TurnResetPlan {
    bool cancelLiveMove = false;
    bool resetPendingCommands = false;
    bool syncTurnDraftBeforeReconcile = false;
    bool reconcileSelection = false;
};

struct TurnLifecycleCallbacks {
    std::function<InputSelectionBookmark()> captureSelectionBookmark;
    std::function<void(const InputSelectionBookmark&)> reconcileSelectionBookmark;
    std::function<void()> ensureTurnDraftUpToDate;
    std::function<void()> refreshTurnPhase;
    std::function<void()> updateUI;
    std::function<void()> persistLanHostSnapshot;
};

class TurnLifecycleCoordinator {
public:
    TurnLifecycleCoordinator(GameState& gameState,
                             bool& waitingForRemoteTurnResult,
                             GameEngine& engine,
                             TurnCoordinator& turnCoordinator,
                             InputHandler& input,
                             UIManager& uiManager,
                             AITurnCoordinator& aiTurnCoordinator);

    static CommitPlayerTurnDispatchPlan buildCommitPlayerTurnDispatchPlan(bool lanClient);
    static ClientTurnSubmissionFailurePlan buildClientTurnSubmissionFailurePlan(
        const std::string& errorMessage,
        bool clientConnected);
    static TurnResetPlan buildResetPlan();

    void commitPlayerTurn(bool lanClient,
                          bool lanHost,
                          bool clientConnected,
                          const TurnLifecycleCallbacks& callbacks);
    void commitAuthoritativeTurn(bool lanHost, const TurnLifecycleCallbacks& callbacks);
    void resetPlayerTurn(const TurnLifecycleCallbacks& callbacks);
    bool applyRemoteTurnSubmission(bool lanHost,
                                   const std::vector<TurnCommand>& commands,
                                   const TurnLifecycleCallbacks& callbacks,
                                   std::string* errorMessage = nullptr);

private:
    bool submitClientTurn(bool lanClient,
                          const TurnLifecycleCallbacks& callbacks,
                          std::string* errorMessage = nullptr);

    GameState& m_gameState;
    bool& m_waitingForRemoteTurnResult;
    GameEngine& m_engine;
    TurnCoordinator& m_turnCoordinator;
    InputHandler& m_input;
    UIManager& m_uiManager;
    AITurnCoordinator& m_aiTurnCoordinator;
};