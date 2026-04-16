#include "Runtime/RenderCoordinator.hpp"

#include <algorithm>

#include "Assets/AssetManager.hpp"
#include "Board/Board.hpp"
#include "Buildings/Building.hpp"
#include "Buildings/StructurePlacementProfile.hpp"
#include "Config/GameConfig.hpp"
#include "Kingdom/Kingdom.hpp"
#include "Render/Camera.hpp"
#include "Render/Renderer.hpp"
#include "Render/StructureOverlay.hpp"
#include "Systems/TurnCommand.hpp"

namespace {

void drawStructureOverlaysForBuildings(sf::RenderWindow& window,
                                       Renderer& renderer,
                                       const Camera& camera,
                                       const sf::View& hudView,
                                       sf::Vector2u windowSize,
                                       const std::vector<Building>& buildings,
                                       const Board& board,
                                       const GameConfig& config,
                                       const AssetManager& assets,
                                       const StructureOverlayPolicy& overlayPolicy,
                                       const Building* selectedBuilding) {
    for (const Building& building : buildings) {
        StructureOverlayContext overlayContext;
        overlayContext.isSelected = selectedBuilding == &building;

        const StructureOverlayStack overlay = buildStructureOverlay(
            building,
            board,
            config,
            overlayContext,
            overlayPolicy);
        if (overlay.isEmpty()) {
            continue;
        }

        renderer.getOverlay().drawStructureOverlay(
            window,
            camera,
            hudView,
            windowSize,
            building,
            overlay,
            config.getCellSizePx(),
            assets);
    }
}

const TurnCommand* findPendingMoveForPiece(const std::vector<TurnCommand>& pendingCommands, int pieceId) {
    for (const TurnCommand& command : pendingCommands) {
        if (command.type == TurnCommand::Move && command.pieceId == pieceId) {
            return &command;
        }
    }

    return nullptr;
}

} // namespace

bool RenderCoordinator::shouldRenderWorld(GameState state) {
    return state == GameState::Playing
        || state == GameState::Paused
        || state == GameState::GameOver;
}

WorldRenderPlan RenderCoordinator::buildWorldRenderPlan(
    const WorldRenderState& state,
    const std::vector<TurnCommand>& pendingCommands,
    const std::vector<sf::Vector2i>& buildableAnchorCellsOverlay,
    const std::vector<sf::Vector2i>& buildableCellsOverlay,
    const GameConfig& config) {
    WorldRenderPlan plan;
    plan.renderWorld = shouldRenderWorld(state.gameState);
    if (!plan.renderWorld) {
        return plan;
    }

    plan.capturePreviewPieceIds = state.capturePreviewPieceIds;
    plan.selectedBuilding = state.selectedBuilding;

    const bool showActionOverlays = state.permissions.canShowActionOverlays;
    const bool showBuildPreview = state.permissions.canShowBuildPreview;
    const bool canShowSelectedPieceActions = showActionOverlays
        && state.selectedPiece != nullptr
        && state.selectedPiece->kingdom == state.activeKingdom;

    if (state.activeTool == ToolState::Select && state.selectedPiece != nullptr) {
        plan.showOrientationCheckerboard = true;
    }

    if (canShowSelectedPieceActions && state.activeTool == ToolState::Select) {
        const TurnCommand* pendingMove = findPendingMoveForPiece(pendingCommands, state.selectedPiece->id);
        const sf::Vector2i highlightedOrigin = pendingMove != nullptr
            ? pendingMove->origin
            : state.selectedPiece->position;
        const bool shouldShowOriginOverlay = state.selectedPiece->type == PieceType::King || pendingMove != nullptr;
        if (shouldShowOriginOverlay) {
            plan.selectedOriginCell = OriginCellSpec{
                highlightedOrigin,
                state.selectedOriginDangerous ? sf::Color(255, 40, 40, 90) : sf::Color(40, 120, 255, 130)
            };
        }
        plan.highlightedCells = state.validMoves;
        plan.dangerCells = state.dangerMoves;
    } else if (state.activeTool == ToolState::Build && !buildableCellsOverlay.empty()) {
        plan.highlightedCells = buildableCellsOverlay;
    }

    if (state.activeTool == ToolState::Select) {
        if (state.selectedPiece != nullptr) {
            plan.selectionFrames.push_back(SelectionFrameSpec{state.selectedPiece->position, 1, 1});
        }
        if (state.selectedBuilding != nullptr) {
            plan.selectionFrames.push_back(SelectionFrameSpec{
                state.selectedBuilding->origin,
                state.selectedBuilding->getFootprintWidth(),
                state.selectedBuilding->getFootprintHeight()
            });
        } else if (state.selectedCell.has_value()) {
            plan.selectionFrames.push_back(SelectionFrameSpec{*state.selectedCell, 1, 1});
        }
    }

    if (showBuildPreview && state.activeTool == ToolState::Build && state.hasBuildPreview) {
        const sf::Vector2i previewOrigin = StructurePlacementProfiles::originFromAnchorCell(
            state.buildPreviewType,
            state.buildPreviewAnchorCell,
            state.buildPreviewRotationQuarterTurns,
            config);
        const int width = config.getBuildingWidth(state.buildPreviewType);
        const int height = config.getBuildingHeight(state.buildPreviewType);
        const bool valid = state.permissions.canQueueNonMoveActions
            && std::find(buildableAnchorCellsOverlay.begin(),
                         buildableAnchorCellsOverlay.end(),
                         state.buildPreviewAnchorCell) != buildableAnchorCellsOverlay.end();
        plan.liveBuildPreview = BuildPreviewSpec{
            previewOrigin,
            state.buildPreviewType,
            width,
            height,
            state.buildPreviewRotationQuarterTurns,
            valid
        };
    }

    if (showActionOverlays && !state.usingConcretePendingState) {
        for (const TurnCommand& pendingCommand : pendingCommands) {
            if (pendingCommand.type != TurnCommand::Build) {
                continue;
            }

            const int width = config.getBuildingWidth(pendingCommand.buildingType);
            const int height = config.getBuildingHeight(pendingCommand.buildingType);
            plan.pendingBuildPreviews.push_back(BuildPreviewSpec{
                pendingCommand.buildOrigin,
                pendingCommand.buildingType,
                width,
                height,
                pendingCommand.buildRotationQuarterTurns,
                true
            });
            plan.actionMarkers.push_back(ActionMarkerSpec{
                pendingCommand.buildOrigin,
                Building::getFootprintWidthFor(width, height, pendingCommand.buildRotationQuarterTurns),
                Building::getFootprintHeightFor(width, height, pendingCommand.buildRotationQuarterTurns),
                "build_ongoing"
            });
        }
    }

    if (showActionOverlays) {
        for (const TurnCommand& pendingCommand : pendingCommands) {
            if (pendingCommand.type != TurnCommand::Move) {
                continue;
            }

            plan.actionMarkers.push_back(ActionMarkerSpec{
                pendingCommand.destination,
                1,
                1,
                "move_ongoing"
            });
        }
    }

    return plan;
}

void RenderCoordinator::renderWorldFrame(WorldRenderBindings& bindings,
                                         const WorldRenderPlan& plan) {
    if (!plan.renderWorld) {
        return;
    }

    bindings.camera.applyTo(bindings.window);
    bindings.renderer.setSkipPieceIds(plan.capturePreviewPieceIds);
    bindings.renderer.drawWorldBase(bindings.window,
                                    bindings.camera,
                                    bindings.displayedBoard,
                                    bindings.displayedKingdoms,
                                    bindings.displayedPublicBuildings);

    if (plan.showOrientationCheckerboard) {
        bindings.renderer.getOverlay().drawOrientationCheckerboard(
            bindings.window,
            bindings.displayedBoard,
            bindings.config.getCellSizePx());
    }
    if (plan.selectedOriginCell.has_value()) {
        bindings.renderer.getOverlay().drawOriginCell(bindings.window,
                                                      bindings.camera,
                                                      plan.selectedOriginCell->origin,
                                                      bindings.config.getCellSizePx(),
                                                      plan.selectedOriginCell->color);
    }
    if (!plan.highlightedCells.empty()) {
        bindings.renderer.getOverlay().drawReachableCells(bindings.window,
                                                          bindings.camera,
                                                          plan.highlightedCells,
                                                          bindings.config.getCellSizePx());
    }
    if (!plan.dangerCells.empty()) {
        bindings.renderer.getOverlay().drawDangerCells(bindings.window,
                                                       bindings.camera,
                                                       plan.dangerCells,
                                                       bindings.config.getCellSizePx());
    }

    bindings.renderer.drawPiecesLayer(bindings.window, bindings.camera, bindings.displayedKingdoms);

    for (const SelectionFrameSpec& frame : plan.selectionFrames) {
        bindings.renderer.getOverlay().drawSelectionFrame(bindings.window,
                                                          bindings.camera,
                                                          bindings.hudView,
                                                          bindings.windowSize,
                                                          frame.origin,
                                                          frame.width,
                                                          frame.height,
                                                          bindings.config.getCellSizePx());
    }

    if (plan.liveBuildPreview.has_value()) {
        bindings.renderer.getOverlay().drawBuildPreview(bindings.window,
                                                        bindings.camera,
                                                        bindings.hudView,
                                                        bindings.windowSize,
                                                        plan.liveBuildPreview->origin,
                                                        plan.liveBuildPreview->type,
                                                        plan.liveBuildPreview->width,
                                                        plan.liveBuildPreview->height,
                                                        plan.liveBuildPreview->rotationQuarterTurns,
                                                        0,
                                                        bindings.config.getCellSizePx(),
                                                        plan.liveBuildPreview->valid,
                                                        bindings.assets);
    }

    for (const BuildPreviewSpec& preview : plan.pendingBuildPreviews) {
        bindings.renderer.getOverlay().drawBuildPreview(bindings.window,
                                                        bindings.camera,
                                                        bindings.hudView,
                                                        bindings.windowSize,
                                                        preview.origin,
                                                        preview.type,
                                                        preview.width,
                                                        preview.height,
                                                        preview.rotationQuarterTurns,
                                                        0,
                                                        bindings.config.getCellSizePx(),
                                                        preview.valid,
                                                        bindings.assets);
    }

    for (const ActionMarkerSpec& marker : plan.actionMarkers) {
        bindings.renderer.getOverlay().drawActionMarker(bindings.window,
                                                        bindings.camera,
                                                        bindings.hudView,
                                                        bindings.windowSize,
                                                        marker.origin,
                                                        marker.width,
                                                        marker.height,
                                                        marker.iconName,
                                                        bindings.config.getCellSizePx(),
                                                        bindings.assets);
    }

    const StructureOverlayPolicy overlayPolicy = makeWorldStructureOverlayPolicy();
    drawStructureOverlaysForBuildings(bindings.window,
                                      bindings.renderer,
                                      bindings.camera,
                                      bindings.hudView,
                                      bindings.windowSize,
                                      bindings.displayedPublicBuildings,
                                      bindings.displayedBoard,
                                      bindings.config,
                                      bindings.assets,
                                      overlayPolicy,
                                      plan.selectedBuilding);
    for (const Kingdom& kingdomState : bindings.displayedKingdoms) {
        drawStructureOverlaysForBuildings(bindings.window,
                                          bindings.renderer,
                                          bindings.camera,
                                          bindings.hudView,
                                          bindings.windowSize,
                                          kingdomState.buildings,
                                          bindings.displayedBoard,
                                          bindings.config,
                                          bindings.assets,
                                          overlayPolicy,
                                          plan.selectedBuilding);
    }
}