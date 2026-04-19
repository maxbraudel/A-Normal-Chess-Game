#include "Runtime/RenderCoordinator.hpp"

#include <algorithm>

#include "Assets/AssetManager.hpp"
#include "Board/Board.hpp"
#include "Board/CellTraversal.hpp"
#include "Buildings/Building.hpp"
#include "Buildings/StructurePlacementProfile.hpp"
#include "Config/GameConfig.hpp"
#include "Kingdom/Kingdom.hpp"
#include "Objects/MapObject.hpp"
#include "Render/Camera.hpp"
#include "Render/Renderer.hpp"
#include "Render/StructureOverlay.hpp"
#include "Runtime/WeatherVisibility.hpp"
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
                                       const Building* selectedBuilding,
                                       const WeatherMaskCache& weatherMaskCache,
                                       KingdomId localPerspective,
                                       bool drawOccludable,
                                       bool showOnlyBlockingBuildings) {
    for (const Building& building : buildings) {
        if (WeatherVisibility::isOccludableBuilding(building, localPerspective) != drawOccludable) {
            continue;
        }
        if (showOnlyBlockingBuildings) {
            bool hasBlockingCell = false;
            for (const sf::Vector2i& cellPos : building.getOccupiedCells()) {
                if (!board.isInBounds(cellPos.x, cellPos.y)) {
                    continue;
                }

                const Cell& cell = board.getCell(cellPos.x, cellPos.y);
                if (cell.building == &building && !isCellTerrainTraversable(cell)) {
                    hasBlockingCell = true;
                    break;
                }
            }

            if (!hasBlockingCell) {
                continue;
            }
        }
        if (drawOccludable
            && WeatherVisibility::shouldHideBuildingOverlay(building,
                                                            localPerspective,
                                                            weatherMaskCache)) {
            continue;
        }

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

    plan.showTacticalGrid = state.tacticalGridModeActive;
    plan.showOnlyBlockingBuildingOverlays = state.tacticalGridModeActive;
    plan.capturePreviewPieceIds = state.capturePreviewPieceIds;
    plan.selectedBuilding = state.selectedBuilding;

    const bool showActionOverlays = state.permissions.canShowActionOverlays;
    const bool showBuildPreview = state.permissions.canShowBuildPreview;
    const bool canShowSelectedPieceActions = showActionOverlays
        && state.selectedPiece != nullptr
        && state.selectedPiece->kingdom == state.activeKingdom;

    if (!plan.showTacticalGrid
        && state.activeTool == ToolState::Select
        && state.selectedPiece != nullptr) {
        plan.showOrientationCheckerboard = true;
    }

    if (canShowSelectedPieceActions && state.activeTool == ToolState::Select) {
        const TurnCommand* pendingMove = findPendingMoveForPiece(pendingCommands, state.selectedPiece->id);
        const sf::Vector2i highlightedOrigin = pendingMove != nullptr
            ? pendingMove->origin
            : state.selectedCell.value_or(state.selectedPiece->position);
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
            plan.selectionFrames.push_back(SelectionFrameSpec{state.selectedCell.value_or(state.selectedPiece->position), 1, 1});
        }
        if (state.selectedBuilding != nullptr) {
            plan.selectionFrames.push_back(SelectionFrameSpec{
                state.selectedBuilding->origin,
                state.selectedBuilding->getFootprintWidth(),
                state.selectedBuilding->getFootprintHeight()
            });
        } else if (state.selectedMapObject != nullptr) {
            plan.selectionFrames.push_back(SelectionFrameSpec{state.selectedMapObject->position, 1, 1});
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
    if (plan.showTacticalGrid) {
        bindings.renderer.getOverlay().drawTacticalGridCheckerboard(
            bindings.window,
            bindings.displayedBoard,
            bindings.config.getCellSizePx());
        bindings.renderer.drawTacticalGridBlockedTerrain(
            bindings.window,
            bindings.camera,
            bindings.displayedBoard);
        bindings.renderer.drawTacticalGridBlockedStructures(
            bindings.window,
            bindings.camera,
            bindings.displayedBoard,
            bindings.localPerspectiveKingdom,
            bindings.weatherMaskCache);
    } else {
        bindings.renderer.drawTerrainLayer(bindings.window,
                                           bindings.camera,
                                           bindings.displayedBoard);

        bindings.renderer.drawOccludableBuildings(bindings.window,
                                                  bindings.camera,
                                                  bindings.displayedKingdoms,
                                                  bindings.localPerspectiveKingdom,
                                                  bindings.weatherMaskCache);

        bindings.renderer.drawVisibleBuildings(bindings.window,
                                               bindings.camera,
                                               bindings.displayedKingdoms,
                                               bindings.displayedPublicBuildings,
                                               bindings.localPerspectiveKingdom);

        bindings.renderer.drawMapObjectsLayer(bindings.window,
                                              bindings.camera,
                                              bindings.displayedMapObjects);
    }

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

    const StructureOverlayPolicy overlayPolicy = makeWorldStructureOverlayPolicy();
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
                                          plan.selectedBuilding,
                                          bindings.weatherMaskCache,
                                          bindings.localPerspectiveKingdom,
                                          true,
                                          plan.showOnlyBlockingBuildingOverlays);
    }

    bindings.renderer.drawOccludablePieces(bindings.window,
                                           bindings.camera,
                                           bindings.displayedKingdoms,
                                           bindings.localPerspectiveKingdom,
                                           bindings.weatherMaskCache);
    bindings.renderer.drawAutonomousUnitsLayer(bindings.window,
                                               bindings.camera,
                                               bindings.displayedAutonomousUnits,
                                               bindings.weatherMaskCache);
    bindings.renderer.drawVisiblePieces(bindings.window,
                                        bindings.camera,
                                        bindings.displayedKingdoms,
                                        bindings.localPerspectiveKingdom);
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
                                      plan.selectedBuilding,
                                      bindings.weatherMaskCache,
                                      bindings.localPerspectiveKingdom,
                                      false,
                                      plan.showOnlyBlockingBuildingOverlays);
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
                                          plan.selectedBuilding,
                                          bindings.weatherMaskCache,
                                          bindings.localPerspectiveKingdom,
                                          false,
                                          plan.showOnlyBlockingBuildingOverlays);
    }
    bindings.renderer.drawWeatherLayer(bindings.window,
                                       bindings.camera,
                                       bindings.displayedBoard,
                                       bindings.weatherMaskCache);

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
}