#include "Runtime/UICallbackCoordinator.hpp"

#include <utility>

#include "Buildings/BuildingType.hpp"
#include "Config/GameConfig.hpp"
#include "Core/ToolState.hpp"
#include "Input/InputHandler.hpp"
#include "Kingdom/Kingdom.hpp"
#include "Runtime/FrontendCoordinator.hpp"
#include "Runtime/PanelActionCoordinator.hpp"
#include "Save/SaveManager.hpp"
#include "UI/MainMenuUI.hpp"
#include "UI/UIManager.hpp"
#include "Units/PieceType.hpp"

namespace {

constexpr const char* kSavesDirectory = "saves";

UICallbackRuntimeState currentRuntimeState(const UICallbackCoordinatorDependencies& dependencies) {
    if (dependencies.runtimeState) {
        return dependencies.runtimeState();
    }
    return UICallbackRuntimeState{};
}

bool canToggleInGameMenu(GameState state) {
    return state == GameState::Playing
        || state == GameState::Paused
        || state == GameState::GameOver;
}

} // namespace

UICallbackBindings UICallbackCoordinator::buildBindings(const UICallbackCoordinatorDependencies& dependencies) {
    UICallbackBindings bindings;

    bindings.gameMenu.onResume = dependencies.closeInGameMenu;
    bindings.gameMenu.onSave = dependencies.saveGame;
    bindings.gameMenu.onQuitToMainMenu = dependencies.returnToMainMenu;

    bindings.mainMenu.onLoadSaves = [dependencies]() {
        dependencies.uiManager.mainMenu().setSaves(
            dependencies.saveManager.listSaveSummaries(kSavesDirectory));
    };
    bindings.mainMenu.onExitGame = [dependencies]() {
        if (dependencies.stopMultiplayer) {
            dependencies.stopMultiplayer();
        }
        if (dependencies.closeWindow) {
            dependencies.closeWindow();
        }
    };
    bindings.mainMenu.onCreateSave = [dependencies](const GameSessionConfig& session) {
        std::string error;
        if (dependencies.startNewGame && !dependencies.startNewGame(session, &error)) {
            return error;
        }
        return std::string{};
    };
    bindings.mainMenu.onPlaySave = [dependencies](const std::string& saveName) {
        if (dependencies.loadGame) {
            dependencies.loadGame(saveName);
        }
    };
    bindings.mainMenu.onJoinMultiplayer = [dependencies](const JoinMultiplayerRequest& request) {
        std::string error;
        if (dependencies.joinMultiplayer && !dependencies.joinMultiplayer(request, &error)) {
            return error;
        }
        return std::string{};
    };
    bindings.mainMenu.onDeleteSave = [dependencies](const std::string& saveName) {
        if (saveName.empty()) {
            return;
        }

        dependencies.saveManager.deleteSave(std::string{kSavesDirectory} + "/" + saveName + ".json");
        dependencies.uiManager.mainMenu().setSaves(
            dependencies.saveManager.listSaveSummaries(kSavesDirectory));
    };

    bindings.hud.onMenu = [dependencies]() {
        if (!canToggleInGameMenu(currentRuntimeState(dependencies).gameState)) {
            return;
        }
        if (dependencies.toggleInGameMenu) {
            dependencies.toggleInGameMenu();
        }
    };
    bindings.hud.onResetTurn = [dependencies]() {
        if (!currentRuntimeState(dependencies).permissions.canIssueCommands) {
            return;
        }
        if (dependencies.resetPlayerTurn) {
            dependencies.resetPlayerTurn();
        }
    };
    bindings.hud.onEndTurn = [dependencies]() {
        if (!currentRuntimeState(dependencies).permissions.canIssueCommands) {
            return;
        }
        if (dependencies.commitPlayerTurn) {
            dependencies.commitPlayerTurn();
        }
    };

    bindings.toolBar.onSelect = [dependencies]() {
        if (!currentRuntimeState(dependencies).permissions.canUseToolbar) {
            return;
        }
        if (dependencies.activateSelectTool) {
            dependencies.activateSelectTool();
        }
    };
    bindings.toolBar.onBuild = [dependencies]() {
        const UICallbackRuntimeState runtimeState = currentRuntimeState(dependencies);
        const InteractionPermissions& permissions = runtimeState.permissions;
        if (!permissions.canUseToolbar || !permissions.canOpenBuildPanel) {
            return;
        }

        dependencies.input.setTool(ToolState::Build);
        dependencies.uiManager.buildToolPanel().setSelectedBuildType(dependencies.input.getBuildPreviewType());
        const KingdomId viewedKingdomId = permissions.canIssueCommands
            ? dependencies.activeKingdomId()
            : dependencies.localPerspectiveKingdom();
        if (dependencies.ensureTurnDraftUpToDate) {
            dependencies.ensureTurnDraftUpToDate();
        }
        dependencies.uiManager.showBuildToolPanel(
            dependencies.displayedKingdom(viewedKingdomId),
            dependencies.config,
            permissions.canQueueNonMoveActions,
            FrontendCoordinator::leftPanelTitle(FrontendLeftPanelKind::BuildTool));
    };
    bindings.toolBar.onOverview = [dependencies]() {
        if (!currentRuntimeState(dependencies).permissions.canUseToolbar) {
            return;
        }
        if (dependencies.toggleOverview) {
            dependencies.toggleOverview();
        }
    };

    bindings.panels.onSelectBuildType = [dependencies](int type) {
        const InteractionPermissions permissions = currentRuntimeState(dependencies).permissions;
        if (!permissions.canQueueNonMoveActions) {
            return;
        }
        const BuildingType buildingType = static_cast<BuildingType>(type);
        dependencies.input.setBuildType(buildingType);
        dependencies.uiManager.buildToolPanel().setSelectedBuildType(buildingType);
    };
    bindings.panels.onCancelBuildingConstruction = [dependencies](int buildingId) {
        dependencies.panelActionCoordinator.cancelQueuedConstructionSelection(
            currentRuntimeState(dependencies).permissions,
            buildingId);
    };
    bindings.panels.onCancelBarracksConstruction = [dependencies](int buildingId) {
        dependencies.panelActionCoordinator.cancelQueuedConstructionSelection(
            currentRuntimeState(dependencies).permissions,
            buildingId);
    };
    bindings.panels.onPieceUpgrade = [dependencies](int pieceId, int targetType) {
        dependencies.panelActionCoordinator.handlePieceUpgradeRequest(
            currentRuntimeState(dependencies).permissions,
            pieceId,
            static_cast<PieceType>(targetType));
    };
    bindings.panels.onPieceDisband = [dependencies](int pieceId) {
        dependencies.panelActionCoordinator.handlePieceDisbandRequest(
            currentRuntimeState(dependencies).permissions,
            pieceId);
    };
    bindings.panels.onBarracksProduce = [dependencies](int barracksId, int pieceType) {
        dependencies.panelActionCoordinator.handleBarracksProduceRequest(
            currentRuntimeState(dependencies).permissions,
            barracksId,
            static_cast<PieceType>(pieceType));
    };

    return bindings;
}

void UICallbackCoordinator::configure(const UICallbackCoordinatorDependencies& dependencies) {
    UICallbackBinder::bind(dependencies.uiManager, buildBindings(dependencies));
}