#pragma once

#include <functional>
#include <string>

#include "Core/GameSessionConfig.hpp"
#include "Core/GameState.hpp"
#include "Core/InteractionPermissions.hpp"
#include "Runtime/UICallbackBinder.hpp"

class GameConfig;
class InputHandler;
class Kingdom;
class PanelActionCoordinator;
class SaveManager;
class UIManager;
struct JoinMultiplayerRequest;

struct UICallbackRuntimeState {
    GameState gameState = GameState::MainMenu;
    InteractionPermissions permissions{};
};

struct UICallbackCoordinatorDependencies {
    std::function<UICallbackRuntimeState()> runtimeState;
    UIManager& uiManager;
    SaveManager& saveManager;
    InputHandler& input;
    PanelActionCoordinator& panelActionCoordinator;
    const GameConfig& config;
    std::function<void()> closeInGameMenu;
    std::function<void()> saveGame;
    std::function<void()> returnToMainMenu;
    std::function<bool(const GameSessionConfig&, std::string*)> startNewGame;
    std::function<void(const std::string&)> loadGame;
    std::function<bool(const JoinMultiplayerRequest&, std::string*)> joinMultiplayer;
    std::function<void()> stopMultiplayer;
    std::function<void()> closeWindow;
    std::function<void()> toggleInGameMenu;
    std::function<void()> resetPlayerTurn;
    std::function<void()> commitPlayerTurn;
    std::function<void()> activateSelectTool;
    std::function<void()> toggleOverview;
    std::function<void()> ensureTurnDraftUpToDate;
    std::function<KingdomId()> activeKingdomId;
    std::function<KingdomId()> localPerspectiveKingdom;
    std::function<Kingdom&(KingdomId)> displayedKingdom;
};

class UICallbackCoordinator {
public:
    static UICallbackBindings buildBindings(const UICallbackCoordinatorDependencies& dependencies);
    static void configure(const UICallbackCoordinatorDependencies& dependencies);
};