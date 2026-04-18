#include "Runtime/FrontendCoordinator.hpp"

#include "Board/Board.hpp"
#include "Core/GameEngine.hpp"
#include "Buildings/Building.hpp"
#include "Buildings/BuildingFactory.hpp"
#include "Config/GameConfig.hpp"
#include "Kingdom/Kingdom.hpp"
#include "Render/Camera.hpp"
#include "Systems/TurnSystem.hpp"
#include "UI/InGameViewModelBuilder.hpp"
#include "UI/UIManager.hpp"

namespace {

Kingdom& selectKingdom(std::array<Kingdom, kNumKingdoms>& kingdoms, KingdomId id) {
    return kingdoms[kingdomIndex(id)];
}

const Kingdom& selectKingdom(const std::array<Kingdom, kNumKingdoms>& kingdoms, KingdomId id) {
    return kingdoms[kingdomIndex(id)];
}

std::vector<InGameAlert> buildInGameAlerts(const FrontendRuntimeState& state,
                                           const CheckTurnValidation& validation) {
    std::vector<InGameAlert> alerts;
    if (state.gameState == GameState::GameOver && validation.activeKingInCheck && !validation.hasAnyLegalResponse) {
        const KingdomId winner = opponent(state.activeKingdom);
        const bool localHotseat = isLocalHotseatSession(state.localPlayerContext);
        if (localHotseat) {
            alerts.push_back(InGameAlert{
                std::string{"Checkmate - "}
                    + (winner == KingdomId::White ? "White" : "Black")
                    + " wins",
                InGameAlertTone::Danger
            });
        } else {
            const bool localWon = FrontendCoordinator::localPerspectiveKingdom(state) == winner;
            alerts.push_back(InGameAlert{
                localWon ? "Checkmate - You Win" : "Checkmate - You Lose",
                localWon ? InGameAlertTone::Success : InGameAlertTone::Danger
            });
        }
        return alerts;
    }

    if (validation.activeKingInCheck) {
        alerts.push_back(InGameAlert{"Check", InGameAlertTone::Danger});
    }
    if (validation.bankrupt) {
        alerts.push_back(InGameAlert{
            "Bankruptcy: end turn would reach " + std::to_string(validation.projectedEndingGold) + " gold.",
            InGameAlertTone::Warning
        });
    }

    return alerts;
}

} // namespace

bool FrontendCoordinator::isLocalPlayerTurn(const FrontendRuntimeState& state) {
    return state.localPlayerContext.isLocallyControlled(state.activeKingdom);
}

KingdomId FrontendCoordinator::localPerspectiveKingdom(const FrontendRuntimeState& state) {
    if (isLocalPlayerTurn(state)) {
        return state.activeKingdom;
    }

    return state.localPlayerContext.perspectiveKingdom;
}

bool FrontendCoordinator::isMultiplayerSessionReady(const FrontendRuntimeState& state) {
    if (state.localPlayerContext.mode == LocalSessionMode::LanHost) {
        return state.hostAuthenticated;
    }

    if (state.localPlayerContext.mode == LocalSessionMode::LanClient) {
        return state.clientAuthenticated;
    }

    return true;
}

bool FrontendCoordinator::shouldEvaluateTurnValidation(const FrontendRuntimeState& state) {
    return state.gameState == GameState::Playing
        && !state.overlaysVisible
        && !state.waitingForRemoteTurnResult
        && isMultiplayerSessionReady(state)
        && isLocalPlayerTurn(state);
}

InteractionPermissions FrontendCoordinator::currentInteractionPermissions(
    const FrontendRuntimeState& state,
    const std::optional<CheckTurnValidation>& validation) {
    InteractionPermissionInputs inputs;
    inputs.gameState = state.gameState;
    inputs.overlaysVisible = state.overlaysVisible;
    inputs.inGameMenuOpen = state.inGameMenuOpen;
    inputs.waitingForRemoteTurnResult = state.waitingForRemoteTurnResult;
    inputs.multiplayerSessionReady = isMultiplayerSessionReady(state);
    inputs.isLocalPlayerTurn = isLocalPlayerTurn(state);
    if (validation.has_value()) {
        inputs.activeKingInCheck = validation->activeKingInCheck;
        inputs.projectedKingInCheck = validation->projectedKingInCheck;
        inputs.hasAnyLegalResponse = validation->hasAnyLegalResponse;
    }

    return computeInteractionPermissions(inputs);
}

InGameHudPresentation FrontendCoordinator::buildInGameHudPresentation(const FrontendRuntimeState& state) {
    InGameHudPresentation presentation;
    const bool singleLocalKingdom = hasSingleLocallyControlledKingdom(state.localPlayerContext);

    presentation.statsKingdom = singleLocalKingdom
        ? localPerspectiveKingdom(state)
        : state.activeKingdom;
    presentation.showTurnPointIndicators = !singleLocalKingdom || isLocalPlayerTurn(state);

    if (singleLocalKingdom
        && isLocalPlayerTurn(state)
        && !state.waitingForRemoteTurnResult
        && isMultiplayerSessionReady(state)
        && (state.gameState == GameState::Playing || state.gameState == GameState::Paused)) {
        presentation.turnIndicatorTone = InGameTurnIndicatorTone::LocalTurn;
    }

    return presentation;
}

InGameViewModel FrontendCoordinator::buildDashboardViewModel(const FrontendRuntimeState& state,
                                                             const FrontendDashboardBindings& bindings,
                                                             const InteractionPermissions& permissions,
                                                             const CheckTurnValidation& validation,
                                                             const InGameHudPresentation& presentation,
                                                             bool usingProjectedDisplayState) {
    InGameViewModel viewModel = buildInGameViewModel(bindings.engine,
                                                     bindings.config,
                                                     state.gameState,
                                                     permissions.canIssueCommands,
                                                     presentation);

    if (usingProjectedDisplayState) {
        const Kingdom& displayedHudKingdom = selectKingdom(bindings.displayedKingdoms, presentation.statsKingdom);
        const Kingdom& displayedWhiteKingdom = selectKingdom(bindings.displayedKingdoms, KingdomId::White);
        const Kingdom& displayedBlackKingdom = selectKingdom(bindings.displayedKingdoms, KingdomId::Black);

        viewModel.activeGold = displayedHudKingdom.gold;
        viewModel.activeOccupiedCells = countOccupiedBuildingCells(displayedHudKingdom);
        viewModel.activeTroops = displayedHudKingdom.pieceCount();
        viewModel.activeIncome = EconomySystem::calculateProjectedNetIncome(
            displayedHudKingdom,
            bindings.displayedBoard,
            bindings.displayedPublicBuildings,
            bindings.config);
        viewModel.balanceMetrics[0].whiteValue = displayedWhiteKingdom.gold;
        viewModel.balanceMetrics[0].blackValue = displayedBlackKingdom.gold;
        viewModel.balanceMetrics[1].whiteValue = countOccupiedBuildingCells(displayedWhiteKingdom);
        viewModel.balanceMetrics[1].blackValue = countOccupiedBuildingCells(displayedBlackKingdom);
        viewModel.balanceMetrics[3].whiteValue = EconomySystem::calculateProjectedNetIncome(
            displayedWhiteKingdom,
            bindings.displayedBoard,
            bindings.displayedPublicBuildings,
            bindings.config);
        viewModel.balanceMetrics[3].blackValue = EconomySystem::calculateProjectedNetIncome(
            displayedBlackKingdom,
            bindings.displayedBoard,
            bindings.displayedPublicBuildings,
            bindings.config);
    }

    viewModel.alerts = buildInGameAlerts(state, validation);
    viewModel.canEndTurn = permissions.canIssueCommands && validation.valid;
    return viewModel;
}

FrontendLeftPanelPresentation FrontendCoordinator::buildLeftPanelPresentation(
    const FrontendRuntimeState& state,
    const FrontendPanelBindings& bindings,
    const InteractionPermissions& permissions) {
    FrontendLeftPanelPresentation presentation;
    presentation.viewedKingdom = permissions.canIssueCommands ? state.activeKingdom : localPerspectiveKingdom(state);

    switch (bindings.activeTool) {
        case ToolState::Build:
            if (permissions.canOpenBuildPanel) {
                presentation.kind = FrontendLeftPanelKind::BuildTool;
                presentation.allowBuild = permissions.canQueueNonMoveActions;
            }
            return presentation;

        case ToolState::Select:
        default:
            break;
    }

    if (bindings.selectedPiece != nullptr) {
        const bool ownsSelectedPiece = bindings.selectedPiece->kingdom == presentation.viewedKingdom;
        presentation.kind = FrontendLeftPanelKind::Piece;
        presentation.piece = bindings.selectedPiece;
        presentation.pendingUpgrade = bindings.turnSystem.getPendingUpgradeCommand(bindings.selectedPiece->id);
        presentation.pendingDisband = bindings.turnSystem.getPendingDisbandCommand(bindings.selectedPiece->id);
        presentation.allowUpgrade = permissions.canQueueNonMoveActions
            && ownsSelectedPiece
            && presentation.pendingDisband == nullptr;
        presentation.allowDisband = permissions.canQueueNonMoveActions
            && ownsSelectedPiece
            && bindings.selectedPiece->type != PieceType::King
            && (presentation.pendingDisband != nullptr
                || (!bindings.turnSystem.hasPendingMoveForPiece(bindings.selectedPiece->id)
                    && presentation.pendingUpgrade == nullptr));
        return presentation;
    }

    if (bindings.selectedBuilding != nullptr) {
        presentation.building = bindings.selectedBuilding;
        presentation.allowCancelConstruction = permissions.canQueueNonMoveActions
            && bindings.selectedBuilding->isUnderConstruction()
            && bindings.selectedBuilding->owner == bindings.turnSystem.getActiveKingdom();

        if (bindings.selectedBuilding->type == BuildingType::Barracks) {
            presentation.kind = FrontendLeftPanelKind::Barracks;
            presentation.allowProduce = permissions.canQueueNonMoveActions
                && !bindings.selectedBuilding->isUnderConstruction()
                && bindings.selectedBuilding->owner == presentation.viewedKingdom;
            presentation.pendingProduce = bindings.turnSystem.getPendingProduceCommand(bindings.selectedBuilding->id);
            return presentation;
        }

        presentation.kind = FrontendLeftPanelKind::Building;
        if (bindings.selectedBuilding->hasActiveGameplayEffects()
            && (bindings.selectedBuilding->type == BuildingType::Mine
                || bindings.selectedBuilding->type == BuildingType::Farm)) {
            presentation.resourceIncome = EconomySystem::calculateResourceIncomeBreakdown(
                *bindings.selectedBuilding,
                bindings.displayedBoard,
                bindings.config);
        }
        if (bindings.selectedBuilding->isPublic()) {
            presentation.publicOccupation = resolvePublicBuildingOccupationState(
                *bindings.selectedBuilding,
                bindings.displayedBoard);
        }
        return presentation;
    }

    if (bindings.selectedMapObject != nullptr) {
        presentation.kind = FrontendLeftPanelKind::MapObject;
        presentation.mapObject = bindings.selectedMapObject;
        return presentation;
    }

    if (bindings.selectedCell != nullptr) {
        presentation.kind = FrontendLeftPanelKind::Cell;
        presentation.cell = bindings.selectedCell;
    }

    return presentation;
}

InputContext FrontendCoordinator::buildInputContext(FrontendRuntimeState state,
                                                    FrontendDisplayBindings& bindings,
                                                    const InteractionPermissions& permissions,
                                                    bool useConcretePendingState) {
    const KingdomId perspectiveKingdom = permissions.canIssueCommands
        ? state.activeKingdom
        : localPerspectiveKingdom(state);
    const bool materializePendingStateLocally = (state.gameState == GameState::Playing
            || state.gameState == GameState::Paused
            || state.gameState == GameState::GameOver)
        && isLocalPlayerTurn(state)
        && !state.waitingForRemoteTurnResult;

    return {
        bindings.window,
        bindings.camera,
        bindings.displayedBoard,
        bindings.turnSystem,
        bindings.buildingFactory,
        permissions.canIssueCommands
            ? selectKingdom(bindings.displayedKingdoms, state.activeKingdom)
            : selectKingdom(bindings.displayedKingdoms, perspectiveKingdom),
        permissions.canIssueCommands
            ? selectKingdom(bindings.displayedKingdoms, opponent(state.activeKingdom))
            : selectKingdom(bindings.displayedKingdoms, opponent(perspectiveKingdom)),
        bindings.displayedPublicBuildings,
        bindings.authoritativeBoard,
        selectKingdom(bindings.authoritativeKingdoms, state.activeKingdom),
        selectKingdom(bindings.authoritativeKingdoms, opponent(state.activeKingdom)),
        bindings.authoritativePublicBuildings,
        bindings.authoritativeTurnContext,
        bindings.uiManager,
        bindings.config,
        &bindings.weatherMaskCache,
        perspectiveKingdom,
        permissions,
        materializePendingStateLocally,
        useConcretePendingState
    };
}

void FrontendCoordinator::updateMultiplayerPresentation(UIManager& uiManager,
                                                        const FrontendRuntimeState& state,
                                                        int multiplayerPort,
                                                        const std::function<void()>& onReturnToMainMenu) {
    if (!state.localPlayerContext.isNetworked()) {
        uiManager.hideMultiplayerWaitingOverlay();
        uiManager.clearMultiplayerStatus();
        return;
    }

    if (state.localPlayerContext.mode == LocalSessionMode::LanHost) {
        if (!state.hostAuthenticated) {
            if (uiManager.isMultiplayerAlertVisible()) {
                uiManager.hideMultiplayerWaitingOverlay();
                uiManager.setMultiplayerStatus("LAN Host | Waiting for Black", MultiplayerStatusTone::Waiting);
                return;
            }

            std::string waitingMessage = "Share this endpoint with Black:\n";
            if (!state.hostJoinHint.empty()) {
                waitingMessage += state.hostJoinHint;
            } else {
                waitingMessage += "Port " + std::to_string(multiplayerPort);
            }
            waitingMessage += "\n\nWhite stays locked until the remote player finishes joining.";
            uiManager.showMultiplayerWaitingOverlay(
                "Waiting for Black Player",
                waitingMessage,
                "Return to Main Menu",
                onReturnToMainMenu);
            uiManager.setMultiplayerStatus("LAN Host | Waiting for Black", MultiplayerStatusTone::Waiting);
            return;
        }

        uiManager.hideMultiplayerWaitingOverlay();
        if (state.activeKingdom == KingdomId::Black) {
            uiManager.setMultiplayerStatus("LAN Host | Waiting for Black turn", MultiplayerStatusTone::Waiting);
        } else {
            uiManager.setMultiplayerStatus("LAN Host | Black connected", MultiplayerStatusTone::Connected);
        }
        return;
    }

    uiManager.hideMultiplayerWaitingOverlay();
    if (!state.clientAuthenticated) {
        if (state.awaitingReconnect) {
            uiManager.setMultiplayerStatus(
                state.hasReconnectRequest
                    ? "LAN Client | Disconnected - reconnect available"
                    : "LAN Client | Connection lost",
                MultiplayerStatusTone::Waiting);
            return;
        }

        uiManager.setMultiplayerStatus("LAN Client | Finalizing connection", MultiplayerStatusTone::Waiting);
        return;
    }

    if (state.waitingForRemoteTurnResult) {
        uiManager.setMultiplayerStatus("LAN Client | Waiting for host confirmation", MultiplayerStatusTone::Waiting);
    } else if (isLocalPlayerTurn(state)) {
        uiManager.setMultiplayerStatus("LAN Client | Connected - your turn", MultiplayerStatusTone::Connected);
    } else {
        uiManager.setMultiplayerStatus("LAN Client | Connected - White is playing", MultiplayerStatusTone::Neutral);
    }
}