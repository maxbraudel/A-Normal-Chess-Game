#include "Runtime/InGamePresentationCoordinator.hpp"

#include "Autonomous/AutonomousUnit.hpp"
#include "Board/Board.hpp"
#include "Buildings/Building.hpp"
#include "Config/GameConfig.hpp"
#include "Core/GameEngine.hpp"
#include "Kingdom/Kingdom.hpp"
#include "Systems/TurnSystem.hpp"
#include "UI/ToolBar.hpp"
#include "UI/UIManager.hpp"

namespace {

const Kingdom& displayedKingdomForOwner(const std::array<Kingdom, kNumKingdoms>& displayedKingdoms,
                                        KingdomId owner) {
    return displayedKingdoms[kingdomIndex(owner)];
}

}

GameMenuPresentation InGamePresentationCoordinator::buildGameMenuPresentation(const FrontendRuntimeState& state) {
    GameMenuPresentation presentation;
    presentation.pauseState = state.localPlayerContext.isNetworked()
        ? GameMenuPauseState::NotPaused
        : GameMenuPauseState::Paused;
    presentation.showSave = state.localPlayerContext.mode != LocalSessionMode::LanClient;
    return presentation;
}

InGameMenuOpenPlan InGamePresentationCoordinator::planOpenInGameMenu(const FrontendRuntimeState& state) {
    InGameMenuOpenPlan plan;
    if (state.gameState != GameState::Playing
        && state.gameState != GameState::Paused
        && state.gameState != GameState::GameOver) {
        return plan;
    }

    plan.shouldOpen = true;
    plan.presentation = buildGameMenuPresentation(state);
    if (!state.localPlayerContext.isNetworked()) {
        plan.nextGameState = GameState::Paused;
    }
    return plan;
}

InGameMenuClosePlan InGamePresentationCoordinator::planCloseInGameMenu(const FrontendRuntimeState& state) {
    InGameMenuClosePlan plan;
    plan.shouldClose = true;
    if (!state.localPlayerContext.isNetworked() && state.gameState == GameState::Paused) {
        plan.nextGameState = GameState::Playing;
    }
    return plan;
}

void InGamePresentationCoordinator::updateInGameUi(
    UIManager& uiManager,
    const InGamePresentationState& state,
    const InGamePresentationBindings& bindings,
    const InteractionPermissions& permissions,
    const CheckTurnValidation& validation,
    const InGameHudPresentation& hudPresentation,
    const std::function<void()>& onReturnToMainMenu) {
    const FrontendDashboardBindings dashboardBindings{
        bindings.engine,
        bindings.displayedBoard,
        bindings.displayedKingdoms,
        bindings.displayedPublicBuildings,
        bindings.config
    };
    InGameViewModel viewModel = FrontendCoordinator::buildDashboardViewModel(
        state.runtimeState,
        dashboardBindings,
        permissions,
        validation,
        hudPresentation,
        state.usingProjectedDisplayState);
    uiManager.updateDashboard(viewModel);

    uiManager.toolBar().applyPresentation(makeToolBarPresentation(
        state.activeTool,
        permissions.canUseToolbar,
        permissions.canOpenBuildPanel,
        state.overviewVisible));

    FrontendCoordinator::updateMultiplayerPresentation(
        uiManager,
        state.runtimeState,
        bindings.engine.sessionConfig().multiplayer.port,
        onReturnToMainMenu);

    const FrontendPanelBindings panelBindings{
        state.activeTool,
        bindings.displayedBoard,
        bindings.turnSystem,
        bindings.config,
        state.selectedPiece,
        state.selectedAutonomousUnit,
        state.selectedBuilding,
        state.selectedMapObject,
        state.selectedCell
    };
    applyLeftPanelPresentation(
        uiManager,
        FrontendCoordinator::buildLeftPanelPresentation(state.runtimeState, panelBindings, permissions),
        bindings.displayedKingdoms,
        bindings.config);
}

void InGamePresentationCoordinator::applyLeftPanelPresentation(
    UIManager& uiManager,
    const FrontendLeftPanelPresentation& presentation,
    const std::array<Kingdom, kNumKingdoms>& displayedKingdoms,
    const GameConfig& config) {
    switch (presentation.kind) {
        case FrontendLeftPanelKind::BuildTool:
            uiManager.showBuildToolPanel(displayedKingdomForOwner(displayedKingdoms, presentation.viewedKingdom),
                                         config,
                                         presentation.allowBuild,
                                         presentation.title);
            return;

        case FrontendLeftPanelKind::Piece:
            if (presentation.piece != nullptr) {
                uiManager.showPiecePanel(*presentation.piece,
                                         config,
                                         presentation.allowUpgrade,
                                         presentation.allowDisband,
                                         presentation.pendingUpgrade,
                                         presentation.pendingDisband,
                                         presentation.title);
                return;
            }
            break;

        case FrontendLeftPanelKind::AutonomousUnit:
            if (presentation.autonomousUnit != nullptr) {
                uiManager.showAutonomousUnitPanel(*presentation.autonomousUnit, presentation.title);
                return;
            }
            break;

        case FrontendLeftPanelKind::Barracks:
            if (presentation.building != nullptr) {
                uiManager.showBarracksPanel(*presentation.building,
                                            displayedKingdomForOwner(displayedKingdoms, presentation.building->owner),
                                            config,
                                            presentation.allowProduce,
                                            presentation.allowCancelConstruction,
                                            presentation.pendingProduce,
                                            presentation.title);
                return;
            }
            break;

        case FrontendLeftPanelKind::Building:
            if (presentation.building != nullptr) {
                uiManager.showBuildingPanel(*presentation.building,
                                            presentation.allowCancelConstruction,
                                            presentation.resourceIncome,
                                            presentation.publicOccupation,
                                            presentation.title);
                return;
            }
            break;

        case FrontendLeftPanelKind::MapObject:
            if (presentation.mapObject != nullptr) {
                uiManager.showMapObjectPanel(*presentation.mapObject, presentation.title);
                return;
            }
            break;

        case FrontendLeftPanelKind::Cell:
            if (presentation.cell != nullptr) {
                uiManager.showCellPanel(*presentation.cell, presentation.title);
                return;
            }
            break;

        case FrontendLeftPanelKind::EmptyState:
        default:
            break;
    }

    uiManager.showSelectionEmptyState();
}