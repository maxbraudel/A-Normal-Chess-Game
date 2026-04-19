#pragma once

#include <functional>
#include <string>

#include "Core/GameSessionConfig.hpp"
#include "Runtime/SessionFormRequest.hpp"

struct JoinMultiplayerRequest;
class UIManager;

struct GameMenuCallbackBindings {
    std::function<void()> onResume;
    std::function<void()> onSave;
    std::function<void()> onQuitToMainMenu;
};

struct MainMenuCallbackBindings {
    std::function<void()> onLoadSaves;
    std::function<void()> onExitGame;
    std::function<std::string(const SessionFormRequest&)> onCreateSave;
    std::function<void(const std::string&)> onPlaySave;
    std::function<std::string(const JoinMultiplayerRequest&)> onJoinMultiplayer;
    std::function<void(const std::string&)> onDeleteSave;
    std::function<std::string(const SessionFormRequest&)> onEditSave;
};

struct HudCallbackBindings {
    std::function<void()> onMenu;
    std::function<void()> onResetTurn;
    std::function<void()> onEndTurn;
};

struct ToolBarCallbackBindings {
    std::function<void()> onSelect;
    std::function<void()> onBuild;
    std::function<void()> onOverview;
};

struct PanelCallbackBindings {
    std::function<void(int)> onSelectBuildType;
    std::function<void(int)> onCancelBuildingConstruction;
    std::function<void(int)> onCancelBarracksConstruction;
    std::function<void(int, int)> onPieceUpgrade;
    std::function<void(int)> onPieceDisband;
    std::function<void(int, int)> onBarracksProduce;
};

struct UICallbackBindings {
    GameMenuCallbackBindings gameMenu;
    MainMenuCallbackBindings mainMenu;
    HudCallbackBindings hud;
    ToolBarCallbackBindings toolBar;
    PanelCallbackBindings panels;
};

class UICallbackBinder {
public:
    static void bind(UIManager& uiManager, const UICallbackBindings& bindings);
};