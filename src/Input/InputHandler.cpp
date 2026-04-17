#include "Input/InputHandler.hpp"

#include "Render/Camera.hpp"
#include "Board/Board.hpp"
#include "Board/Cell.hpp"
#include "Kingdom/Kingdom.hpp"
#include "Units/Piece.hpp"
#include "Units/PieceType.hpp"
#include "Units/MovementRules.hpp"
#include "Buildings/Building.hpp"
#include "Objects/MapObject.hpp"
#include "Buildings/StructurePlacementProfile.hpp"
#include "Systems/TurnSystem.hpp"
#include "Systems/TurnCommand.hpp"
#include "Systems/CheckResponseRules.hpp"
#include "Systems/BuildSystem.hpp"
#include "Systems/PendingTurnProjection.hpp"
#include "Systems/SelectionMoveRules.hpp"
#include "Config/GameConfig.hpp"
#include "Runtime/WeatherVisibility.hpp"
#include "UI/UIManager.hpp"

#include <algorithm>
#include <chrono>
#include <cmath>

namespace {

constexpr float kKeyboardPanSpeed = 900.f;
const auto kSelectionCycleThreshold = std::chrono::milliseconds(350);

void rebuildLayerOrder(LayeredSelectionStack& stack) {
    stack.count = 0;
    if (stack.piece) {
        stack.layers[stack.count++] = SelectionLayer::Piece;
    }
    if (stack.building) {
        stack.layers[stack.count++] = SelectionLayer::Building;
    }
    if (stack.mapObject) {
        stack.layers[stack.count++] = SelectionLayer::Object;
    }
    if (stack.hasTerrain) {
        stack.layers[stack.count++] = SelectionLayer::Terrain;
    }
    while (stack.count < static_cast<int>(stack.layers.size())) {
        stack.layers[stack.count++] = SelectionLayer::None;
    }
    stack.count = std::min(stack.count, static_cast<int>(stack.layers.size()));
    while (stack.count > 0 && stack.layers[stack.count - 1] == SelectionLayer::None) {
        --stack.count;
    }
}

} // namespace

InputHandler::InputHandler()
    : m_currentTool(ToolState::Select), m_selectedPieceId(-1), m_selectedBuildingId(-1),
    m_selectedMapObjectId(-1),
    m_selectedBuildingType(BuildingType::Barracks),
    m_selectedMapObjectType(MapObjectType::Chest),
    m_selectedBuildingOwner(KingdomId::White), m_selectedBuildingIsNeutral(false),
    m_selectedBuildingRotationQuarterTurns(0),
    m_hasSelectedCell(false), m_selectedCell({0, 0}),
    m_selectedOriginDangerous(false),
    m_hasBuildPreview(false), m_buildPreviewType(BuildingType::Barracks),
    m_buildPreviewAnchorCell({0, 0}),
    m_buildPreviewRotationQuarterTurns(0),
    m_activeSelectionLayer(SelectionLayer::None), m_hasActiveSelectionCell(false),
    m_activeSelectionCell({0, 0}), m_isSelectionCycleArmed(false),
    m_selectionCycleCell({0, 0}),
    m_isDragging(false),
    m_panUpHeld(false),
    m_panDownHeld(false),
    m_panLeftHeld(false),
    m_panRightHeld(false) {}

ToolState InputHandler::getCurrentTool() const { return m_currentTool; }
void InputHandler::setTool(ToolState tool) {
    m_currentTool = tool;
    clearSelection();
}

int InputHandler::getSelectedPieceId() const { return m_selectedPieceId; }
int InputHandler::getSelectedBuildingId() const { return m_selectedBuildingId; }
int InputHandler::getSelectedMapObjectId() const { return m_selectedMapObjectId; }
bool InputHandler::hasSelectedCell() const { return m_hasSelectedCell; }
sf::Vector2i InputHandler::getSelectedCell() const { return m_selectedCell; }
bool InputHandler::hasSelectionAnchorCell() const { return m_hasActiveSelectionCell || m_hasSelectedCell; }
sf::Vector2i InputHandler::getSelectionAnchorCell() const {
    return m_hasActiveSelectionCell ? m_activeSelectionCell : m_selectedCell;
}
const std::vector<sf::Vector2i>& InputHandler::getValidMoves() const { return m_validMoves; }
const std::vector<sf::Vector2i>& InputHandler::getDangerMoves() const { return m_dangerMoves; }
bool InputHandler::isSelectedOriginDangerous() const { return m_selectedOriginDangerous; }
const std::set<int>& InputHandler::getCapturePreviewPieceIds() const { return m_capturePreviewPieceIds; }
bool InputHandler::hasMovePreview() const { return !m_movePreviewOrigins.empty(); }

BuildingType InputHandler::getBuildPreviewType() const { return m_buildPreviewType; }
sf::Vector2i InputHandler::getBuildPreviewAnchorCell() const { return m_buildPreviewAnchorCell; }
int InputHandler::getBuildPreviewRotationQuarterTurns() const { return m_buildPreviewRotationQuarterTurns; }
bool InputHandler::hasBuildPreview() const { return m_hasBuildPreview; }
void InputHandler::setBuildType(BuildingType type) {
    if (m_buildPreviewType != type) {
        m_buildPreviewRotationQuarterTurns = 0;
    }
    m_buildPreviewType = type;
}

Piece* InputHandler::resolveSelectedPiece(const InputContext& context) const {
    if (m_selectedPieceId < 0) {
        return nullptr;
    }

    if (Piece* piece = context.controlledKingdom.getPieceById(m_selectedPieceId)) {
        return piece;
    }
    if (Piece* piece = context.opposingKingdom.getPieceById(m_selectedPieceId)) {
        return piece;
    }
    if (Piece* piece = context.authoritativeControlledKingdom.getPieceById(m_selectedPieceId)) {
        return piece;
    }

    return context.authoritativeOpposingKingdom.getPieceById(m_selectedPieceId);
}

InputSelectionBookmark InputHandler::createSelectionBookmark() const {
    InputSelectionBookmark bookmark;
    bookmark.tool = m_currentTool;
    bookmark.buildPreviewType = m_buildPreviewType;
    bookmark.buildPreviewRotationQuarterTurns = m_buildPreviewRotationQuarterTurns;
    if (m_selectedPieceId >= 0) {
        bookmark.pieceId = m_selectedPieceId;
    }
    if (m_selectedBuildingId >= 0) {
        bookmark.buildingId = m_selectedBuildingId;
        bookmark.selectedBuildingOrigin = m_selectedBuildingOrigin;
        bookmark.selectedBuildingType = m_selectedBuildingType;
        bookmark.selectedBuildingOwner = m_selectedBuildingOwner;
        bookmark.selectedBuildingIsNeutral = m_selectedBuildingIsNeutral;
        bookmark.selectedBuildingRotationQuarterTurns = m_selectedBuildingRotationQuarterTurns;
    }
    if (m_selectedMapObjectId >= 0) {
        bookmark.mapObjectId = m_selectedMapObjectId;
        bookmark.selectedMapObjectPosition = m_selectedMapObjectPosition;
        bookmark.selectedMapObjectType = m_selectedMapObjectType;
    }
    if (m_hasActiveSelectionCell) {
        bookmark.selectedCell = m_activeSelectionCell;
    } else if (m_hasSelectedCell) {
        bookmark.selectedCell = m_selectedCell;
    }
    if (m_currentTool == ToolState::Build && m_hasBuildPreview) {
        bookmark.buildPreviewAnchorCell = m_buildPreviewAnchorCell;
    }
    return bookmark;
}

void InputHandler::reconcileSelection(const InputSelectionBookmark& bookmark,
                                      Piece* selectedPiece,
                                      Building* selectedBuilding,
                                      MapObject* selectedMapObject,
                                      const InputContext& context) {
    m_currentTool = bookmark.tool;
    m_buildPreviewType = bookmark.buildPreviewType;
    m_buildPreviewRotationQuarterTurns = bookmark.buildPreviewRotationQuarterTurns;

    const auto restoreSelectionAtBookmarkedCell = [&]() -> bool {
        if (!bookmark.selectedCell.has_value()) {
            return false;
        }

        const sf::Vector2i cellPos = *bookmark.selectedCell;
        if (!context.board.isInBounds(cellPos.x, cellPos.y)) {
            return false;
        }

        const Cell& cell = context.board.getCell(cellPos.x, cellPos.y);
        if (!cell.isInCircle) {
            return false;
        }

        const LayeredSelectionStack stack = resolveSelectionStackAtCell(context, cellPos);
        if (bookmark.pieceId >= 0) {
            if (stack.piece != nullptr && stack.piece->id == bookmark.pieceId) {
                activatePieceSelection(stack.piece,
                                       cellPos,
                                       context,
                                       context.permissions.canIssueCommands
                                           && stack.piece->kingdom == context.controlledKingdom.id);
            } else {
                activateTerrainSelection(cellPos);
            }
            restoreBuildPreviewState(bookmark);
            return true;
        }

        if (bookmark.buildingId >= 0) {
            const bool sameBuildingId = stack.building != nullptr && stack.building->id == bookmark.buildingId;
            const bool sameBuildingMetadata = stack.building != nullptr
                && bookmark.selectedBuildingOrigin.has_value()
                && stack.building->origin == *bookmark.selectedBuildingOrigin
                && stack.building->type == bookmark.selectedBuildingType
                && stack.building->owner == bookmark.selectedBuildingOwner
                && stack.building->isNeutral == bookmark.selectedBuildingIsNeutral
                && stack.building->rotationQuarterTurns == bookmark.selectedBuildingRotationQuarterTurns;

            if (sameBuildingId || sameBuildingMetadata) {
                activateBuildingSelection(stack.building, cellPos);
            } else {
                activateTerrainSelection(cellPos);
            }
            restoreBuildPreviewState(bookmark);
            return true;
        }

        if (bookmark.mapObjectId >= 0) {
            const bool sameObjectId = stack.mapObject != nullptr && stack.mapObject->id == bookmark.mapObjectId;
            const bool sameObjectMetadata = stack.mapObject != nullptr
                && bookmark.selectedMapObjectPosition.has_value()
                && stack.mapObject->position == *bookmark.selectedMapObjectPosition
                && stack.mapObject->type == bookmark.selectedMapObjectType;

            if (sameObjectId || sameObjectMetadata) {
                activateMapObjectSelection(stack.mapObject, cellPos);
            } else {
                activateTerrainSelection(cellPos);
            }
            restoreBuildPreviewState(bookmark);
            return true;
        }

        activateTerrainSelection(cellPos);
        restoreBuildPreviewState(bookmark);
        return true;
    };

    if (restoreSelectionAtBookmarkedCell()) {
        return;
    }

    if (selectedPiece) {
        activatePieceSelection(selectedPiece,
                               selectedPiece->position,
                               context,
                               context.permissions.canIssueCommands
                                   && selectedPiece->kingdom == context.controlledKingdom.id);
        restoreBuildPreviewState(bookmark);
        return;
    }

    if (selectedBuilding) {
        activateBuildingSelection(selectedBuilding, selectedBuilding->origin);
        restoreBuildPreviewState(bookmark);
        return;
    }

    if (selectedMapObject) {
        activateMapObjectSelection(selectedMapObject, selectedMapObject->position);
        restoreBuildPreviewState(bookmark);
        return;
    }

    if (bookmark.selectedCell.has_value()) {
        const sf::Vector2i cellPos = *bookmark.selectedCell;
        if (context.board.isInBounds(cellPos.x, cellPos.y)) {
            const Cell& cell = context.board.getCell(cellPos.x, cellPos.y);
            if (cell.isInCircle) {
                activateTerrainSelection(cellPos);
                restoreBuildPreviewState(bookmark);
                return;
            }
        }
    }

    clearSelection();
    restoreBuildPreviewState(bookmark);
}

void InputHandler::restoreBuildPreviewState(const InputSelectionBookmark& bookmark) {
    m_buildPreviewType = bookmark.buildPreviewType;
    m_buildPreviewRotationQuarterTurns = bookmark.buildPreviewRotationQuarterTurns;

    if (bookmark.tool == ToolState::Build && bookmark.buildPreviewAnchorCell.has_value()) {
        m_buildPreviewAnchorCell = *bookmark.buildPreviewAnchorCell;
        m_hasBuildPreview = true;
        return;
    }

    m_hasBuildPreview = false;
}

void InputHandler::clearSelection() {
    m_selectedPieceId = -1;
    m_selectedBuildingId = -1;
    m_selectedMapObjectId = -1;
    m_selectedBuildingOrigin.reset();
    m_selectedMapObjectPosition.reset();
    m_selectedBuildingType = BuildingType::Barracks;
    m_selectedMapObjectType = MapObjectType::Chest;
    m_selectedBuildingOwner = KingdomId::White;
    m_selectedBuildingIsNeutral = false;
    m_selectedBuildingRotationQuarterTurns = 0;
    m_hasSelectedCell = false;
    m_selectedCell = {0, 0};
    m_activeSelectionLayer = SelectionLayer::None;
    m_hasActiveSelectionCell = false;
    m_activeSelectionCell = {0, 0};
    m_validMoves.clear();
    m_dangerMoves.clear();
    m_selectedOriginDangerous = false;
    clearSelectionCycle();
    // NOTE: does NOT clear queued move previews — call cancelLiveMove() / clearMovePreview() separately
    m_hasBuildPreview = false;
}

void InputHandler::selectCell(sf::Vector2i cellPos) {
    m_selectedPieceId = -1;
    m_selectedBuildingId = -1;
    m_selectedMapObjectId = -1;
    m_selectedBuildingOrigin.reset();
    m_selectedMapObjectPosition.reset();
    m_selectedBuildingType = BuildingType::Barracks;
    m_selectedMapObjectType = MapObjectType::Chest;
    m_selectedBuildingOwner = KingdomId::White;
    m_selectedBuildingIsNeutral = false;
    m_selectedBuildingRotationQuarterTurns = 0;
    m_selectedCell = cellPos;
    m_hasSelectedCell = true;
    m_validMoves.clear();
    m_dangerMoves.clear();
    m_selectedOriginDangerous = false;
    setActiveSelectionMetadata(SelectionLayer::Terrain, cellPos);
}

void InputHandler::activatePieceSelection(Piece* piece, sf::Vector2i cellPos,
                                          const InputContext& context, bool allowCommands) {
    m_selectedPieceId = piece ? piece->id : -1;
    m_selectedBuildingId = -1;
    m_selectedMapObjectId = -1;
    m_selectedBuildingOrigin.reset();
    m_selectedMapObjectPosition.reset();
    m_selectedBuildingType = BuildingType::Barracks;
    m_selectedMapObjectType = MapObjectType::Chest;
    m_selectedBuildingOwner = KingdomId::White;
    m_selectedBuildingIsNeutral = false;
    m_selectedBuildingRotationQuarterTurns = 0;
    m_hasSelectedCell = false;
    if (piece && allowCommands && piece->kingdom == context.controlledKingdom.id) {
        refreshPieceMoves(piece, context);
    } else {
        m_validMoves.clear();
        m_dangerMoves.clear();
        m_selectedOriginDangerous = false;
    }
    setActiveSelectionMetadata(SelectionLayer::Piece, cellPos);
}

void InputHandler::activateBuildingSelection(Building* building, sf::Vector2i cellPos) {
    m_selectedPieceId = -1;
    m_selectedBuildingId = building ? building->id : -1;
    m_selectedMapObjectId = -1;
    m_selectedBuildingOrigin = building ? std::optional<sf::Vector2i>{building->origin} : std::nullopt;
    m_selectedMapObjectPosition.reset();
    m_selectedBuildingType = building ? building->type : BuildingType::Barracks;
    m_selectedMapObjectType = MapObjectType::Chest;
    m_selectedBuildingOwner = building ? building->owner : KingdomId::White;
    m_selectedBuildingIsNeutral = building ? building->isNeutral : false;
    m_selectedBuildingRotationQuarterTurns = building ? building->rotationQuarterTurns : 0;
    m_hasSelectedCell = false;
    m_validMoves.clear();
    m_dangerMoves.clear();
    m_selectedOriginDangerous = false;
    setActiveSelectionMetadata(SelectionLayer::Building, cellPos);
}

void InputHandler::activateMapObjectSelection(MapObject* mapObject, sf::Vector2i cellPos) {
    m_selectedPieceId = -1;
    m_selectedBuildingId = -1;
    m_selectedMapObjectId = mapObject ? mapObject->id : -1;
    m_selectedBuildingOrigin.reset();
    m_selectedMapObjectPosition = mapObject ? std::optional<sf::Vector2i>{mapObject->position} : std::nullopt;
    m_selectedBuildingType = BuildingType::Barracks;
    m_selectedMapObjectType = mapObject ? mapObject->type : MapObjectType::Chest;
    m_selectedBuildingOwner = KingdomId::White;
    m_selectedBuildingIsNeutral = false;
    m_selectedBuildingRotationQuarterTurns = 0;
    m_hasSelectedCell = false;
    m_validMoves.clear();
    m_dangerMoves.clear();
    m_selectedOriginDangerous = false;
    setActiveSelectionMetadata(SelectionLayer::Object, cellPos);
}

void InputHandler::activateTerrainSelection(sf::Vector2i cellPos) {
    selectCell(cellPos);
}

void InputHandler::setActiveSelectionMetadata(SelectionLayer layer, sf::Vector2i cellPos) {
    m_activeSelectionLayer = layer;
    m_hasActiveSelectionCell = (layer != SelectionLayer::None);
    m_activeSelectionCell = m_hasActiveSelectionCell ? cellPos : sf::Vector2i{0, 0};
}

void InputHandler::clearSelectionCycle() {
    m_isSelectionCycleArmed = false;
    m_selectionCycleCell = {0, 0};
}

void InputHandler::armSelectionCycle(sf::Vector2i cellPos) {
    m_isSelectionCycleArmed = true;
    m_selectionCycleCell = cellPos;
    m_selectionCycleArmTime = std::chrono::steady_clock::now();
}

bool InputHandler::canCycleSelection(sf::Vector2i cellPos) const {
    if (m_currentTool != ToolState::Select || !m_isSelectionCycleArmed || !m_hasActiveSelectionCell) {
        return false;
    }

    if (cellPos != m_selectionCycleCell || cellPos != m_activeSelectionCell) {
        return false;
    }

    return (std::chrono::steady_clock::now() - m_selectionCycleArmTime) <= kSelectionCycleThreshold;
}

LayeredSelectionStack InputHandler::resolveSelectionStackAtCell(const InputContext& context,
                                                                sf::Vector2i cellPos) const {
    const Cell& cell = context.board.getCell(cellPos.x, cellPos.y);
    if (context.useConcretePendingState) {
        LayeredSelectionStack stack = resolveCellSelectionStack(cell, cellPos, nullptr, false);
        if (context.weatherMaskCache != nullptr) {
            if (stack.piece != nullptr && WeatherVisibility::shouldHidePiece(
                    *stack.piece,
                    context.localPerspectiveKingdom,
                    *context.weatherMaskCache)) {
                stack.piece = nullptr;
            }
            if (stack.building != nullptr && WeatherVisibility::shouldHideBuildingSelection(
                    *stack.building,
                    cellPos,
                    context.localPerspectiveKingdom,
                    *context.weatherMaskCache)) {
                stack.building = nullptr;
            }
            rebuildLayerOrder(stack);
        }
        return stack;
    }

    Piece* pieceOverride = nullptr;
    bool suppressCellPiece = false;

    for (const TurnCommand& command : context.turnSystem.getPendingCommands()) {
        if (command.type != TurnCommand::Move) {
            continue;
        }

        if (cellPos == command.destination) {
            if (cell.piece == nullptr) {
                pieceOverride = context.controlledKingdom.getPieceById(command.pieceId);
                suppressCellPiece = (pieceOverride != nullptr);
            }
        } else if (cellPos == command.origin
                   && cell.piece != nullptr
                   && cell.piece->id == command.pieceId) {
            suppressCellPiece = true;
        }
    }

    LayeredSelectionStack stack = resolveCellSelectionStack(cell, cellPos, pieceOverride, suppressCellPiece);
    if (context.weatherMaskCache != nullptr) {
        if (stack.piece != nullptr && WeatherVisibility::shouldHidePiece(
                *stack.piece,
                context.localPerspectiveKingdom,
                *context.weatherMaskCache)) {
            stack.piece = nullptr;
        }
        if (stack.building != nullptr && WeatherVisibility::shouldHideBuildingSelection(
                *stack.building,
                cellPos,
                context.localPerspectiveKingdom,
                *context.weatherMaskCache)) {
            stack.building = nullptr;
        }
        rebuildLayerOrder(stack);
    }
    return stack;
}

void InputHandler::applyResolvedSelection(const LayeredSelectionStack& stack,
                                          SelectionLayer layer,
                                          const InputContext& context) {
    if (layer != SelectionLayer::Piece && m_activeSelectionLayer == SelectionLayer::Piece) {
        cancelPieceSelectionContext(context);
    }

    switch (layer) {
        case SelectionLayer::Piece:
            activatePieceSelection(stack.piece, stack.cellPos,
                                   context,
                                   context.permissions.canIssueCommands && stack.piece
                                       && stack.piece->kingdom == context.controlledKingdom.id);
            return;
        case SelectionLayer::Building:
            activateBuildingSelection(stack.building, stack.cellPos);
            return;
        case SelectionLayer::Object:
            activateMapObjectSelection(stack.mapObject, stack.cellPos);
            return;
        case SelectionLayer::Terrain:
            activateTerrainSelection(stack.cellPos);
            return;
        case SelectionLayer::None:
        default:
            clearSelection();
            return;
    }
}

void InputHandler::cancelPieceSelectionContext(const InputContext& context) {
    (void) context;
    m_selectedPieceId = -1;
    m_validMoves.clear();
    m_dangerMoves.clear();
    m_selectedOriginDangerous = false;
}

void InputHandler::refreshPieceMoves(Piece* piece, const InputContext& context) {
    m_validMoves.clear();
    m_dangerMoves.clear();
    m_selectedOriginDangerous = false;
    if (!piece) {
        return;
    }

    Board& moveBoard = context.useConcretePendingState ? context.authoritativeBoard : context.board;
    Kingdom& moveControlledKingdom = context.useConcretePendingState
        ? context.authoritativeControlledKingdom
        : context.controlledKingdom;
    Kingdom& moveOpposingKingdom = context.useConcretePendingState
        ? context.authoritativeOpposingKingdom
        : context.opposingKingdom;
    const std::vector<Building>& movePublicBuildings = context.useConcretePendingState
        ? context.authoritativePublicBuildings
        : context.publicBuildings;
    const TurnValidationContext moveTurnContext{
        moveBoard,
        moveControlledKingdom,
        moveOpposingKingdom,
        movePublicBuildings,
        context.turnSystem.getTurnNumber(),
        context.config};

    const SelectionMoveOptions moveOptions = SelectionMoveRules::classifyPieceMoves(
        moveTurnContext,
        context.turnSystem.getPendingCommands(),
        piece->id);

    m_validMoves = moveOptions.safeMoves;
    m_dangerMoves = moveOptions.unsafeMoves;
    m_selectedOriginDangerous = (piece->type == PieceType::King) && moveOptions.originUnsafe;
}

bool InputHandler::isSelectableMoveDestination(sf::Vector2i cellPos) const {
    return std::find(m_validMoves.begin(), m_validMoves.end(), cellPos) != m_validMoves.end()
        || std::find(m_dangerMoves.begin(), m_dangerMoves.end(), cellPos) != m_dangerMoves.end();
}

void InputHandler::syncQueuedMovePreviewState(const InputContext& context) {
    const auto restorePreviewPositions = [&]() {
        for (const auto& [pieceId, origin] : m_movePreviewOrigins) {
            if (Piece* piece = context.controlledKingdom.getPieceById(pieceId)) {
                piece->position = origin;
            }
        }
    };

    if (context.materializePendingStateLocally) {
        restorePreviewPositions();
        m_movePreviewOrigins.clear();
        m_capturePreviewPieceIds.clear();
        return;
    }

    restorePreviewPositions();

    m_movePreviewOrigins.clear();
    m_capturePreviewPieceIds.clear();

    for (const TurnCommand& command : context.turnSystem.getPendingCommands()) {
        if (command.type != TurnCommand::Move) {
            continue;
        }

        Piece* piece = context.controlledKingdom.getPieceById(command.pieceId);
        if (!piece) {
            continue;
        }

        m_movePreviewOrigins.emplace(command.pieceId, command.origin);
        piece->position = command.destination;

        if (Piece* captured = context.opposingKingdom.getPieceAt(command.destination)) {
            m_capturePreviewPieceIds.insert(captured->id);
        }
    }
}

void InputHandler::cancelLiveMove(Kingdom& controlledKingdom, const TurnSystem& turnSystem) {
    (void) turnSystem;
    for (const auto& [pieceId, origin] : m_movePreviewOrigins) {
        if (Piece* piece = controlledKingdom.getPieceById(pieceId)) {
            piece->position = origin;
        }
    }

    m_movePreviewOrigins.clear();
    m_capturePreviewPieceIds.clear();
}

void InputHandler::clearMovePreview() {
    m_movePreviewOrigins.clear();
    m_capturePreviewPieceIds.clear();
}

void InputHandler::handleEvent(const sf::Event& event, const InputContext& context) {
    (void) context.uiManager;

    // Camera input (always active)
    handleCameraInput(event, context.window, context.camera);

    // K: center on king
    if (event.type == sf::Event::KeyPressed && event.key.code == sf::Keyboard::K) {
        Piece* king = context.controlledKingdom.getKing();
        if (king) {
            float cx = static_cast<float>(king->position.x * context.config.getCellSizePx()
                                          + context.config.getCellSizePx() / 2);
            float cy = static_cast<float>(king->position.y * context.config.getCellSizePx()
                                          + context.config.getCellSizePx() / 2);
            context.camera.centerOn({cx, cy});
        } else if (!context.controlledKingdom.pieces.empty()) {
            auto& p = context.controlledKingdom.pieces.front();
            float cx = static_cast<float>(p.position.x * context.config.getCellSizePx()
                                          + context.config.getCellSizePx() / 2);
            float cy = static_cast<float>(p.position.y * context.config.getCellSizePx()
                                          + context.config.getCellSizePx() / 2);
            context.camera.centerOn({cx, cy});
        }
    }

    if (event.type == sf::Event::KeyPressed && event.key.code == sf::Keyboard::R
        && m_currentTool == ToolState::Build && context.permissions.canQueueNonMoveActions) {
        m_buildPreviewRotationQuarterTurns = (m_buildPreviewRotationQuarterTurns + 1) % 4;
    }

    switch (m_currentTool) {
        case ToolState::Select:
            handleSelectTool(event, context);
            break;
        case ToolState::Build:
            handleBuildTool(event, context);
            break;
    }
}

void InputHandler::updateCameraMovement(float deltaTime, Camera& camera) {
    if (deltaTime <= 0.f) {
        return;
    }

    sf::Vector2f direction{0.f, 0.f};
    if (m_panUpHeld) {
        direction.y -= 1.f;
    }
    if (m_panDownHeld) {
        direction.y += 1.f;
    }
    if (m_panLeftHeld) {
        direction.x -= 1.f;
    }
    if (m_panRightHeld) {
        direction.x += 1.f;
    }

    const float length = std::sqrt(direction.x * direction.x + direction.y * direction.y);
    if (length <= 0.f) {
        return;
    }

    const float speed = kKeyboardPanSpeed * camera.getZoomLevel() * deltaTime;
    camera.pan({(direction.x / length) * speed, (direction.y / length) * speed});
}

void InputHandler::handleSelectTool(const sf::Event& event, const InputContext& context) {
    if (event.type == sf::Event::MouseButtonPressed && event.mouseButton.button == sf::Mouse::Left) {
        sf::Vector2f worldPos = context.camera.screenToWorld({event.mouseButton.x, event.mouseButton.y}, context.window);
        sf::Vector2i cellPos = context.camera.worldToCell(worldPos, context.config.getCellSizePx());

        if (!context.board.isInBounds(cellPos.x, cellPos.y)) {
            if (m_activeSelectionLayer == SelectionLayer::Piece) {
                cancelPieceSelectionContext(context);
            }
            clearSelection();
            return;
        }
        const Cell& cell = context.board.getCell(cellPos.x, cellPos.y);
        if (!cell.isInCircle) {
            if (m_activeSelectionLayer == SelectionLayer::Piece) {
                cancelPieceSelectionContext(context);
            }
            clearSelection();
            return;
        }

        const LayeredSelectionStack stack = resolveSelectionStackAtCell(context, cellPos);

        if (m_hasActiveSelectionCell && cellPos == m_activeSelectionCell) {
            if (canCycleSelection(cellPos)) {
                const SelectionLayer nextLayer = stack.nextBelow(m_activeSelectionLayer);
                applyResolvedSelection(stack, nextLayer, context);
                clearSelectionCycle();
                return;
            }

            armSelectionCycle(cellPos);
            return;
        }

        if (!context.permissions.canIssueCommands) {
            applyResolvedSelection(stack, stack.top(), context);
            armSelectionCycle(cellPos);
            return;
        }

        if (Piece* selectedPiece = resolveSelectedPiece(context)) {
            const TurnCommand* pendingMove = context.turnSystem.getPendingMoveCommand(selectedPiece->id);
            if (pendingMove && cellPos == pendingMove->origin) {
                const sf::Vector2i pendingOrigin = pendingMove->origin;
                Piece* restoredPiece = context.controlledKingdom.getPieceById(selectedPiece->id);
                if (Piece* authoritativePiece = context.authoritativeControlledKingdom.getPieceById(selectedPiece->id)) {
                    authoritativePiece->position = pendingOrigin;
                }
                if (restoredPiece && !context.useConcretePendingState) {
                    restoredPiece->position = pendingOrigin;
                }

                if (context.turnSystem.cancelMoveCommand(selectedPiece->id,
                                                         context.authoritativeTurnContext)) {
                    syncQueuedMovePreviewState(context);
                }

                m_selectedPieceId = restoredPiece ? restoredPiece->id : -1;
                m_selectedBuildingId = -1;
                m_selectedMapObjectId = -1;
                m_selectedBuildingOrigin.reset();
                m_selectedMapObjectPosition.reset();
                m_selectedBuildingType = BuildingType::Barracks;
                m_selectedMapObjectType = MapObjectType::Chest;
                m_selectedBuildingOwner = KingdomId::White;
                m_selectedBuildingIsNeutral = false;
                m_selectedBuildingRotationQuarterTurns = 0;
                m_hasSelectedCell = false;
                refreshPieceMoves(restoredPiece, context);
                setActiveSelectionMetadata(SelectionLayer::Piece, pendingOrigin);
                armSelectionCycle(pendingOrigin);
                return;
            }

            if (isSelectableMoveDestination(cellPos)) {
                const bool hadPendingMove = (pendingMove != nullptr);
                const sf::Vector2i moveOrigin = hadPendingMove
                    ? pendingMove->origin
                    : selectedPiece->position;
                Piece* restoredPiece = context.controlledKingdom.getPieceById(selectedPiece->id);
                if (!restoredPiece) {
                    return;
                }

                if (hadPendingMove && !context.useConcretePendingState) {
                    restoredPiece->position = pendingMove->origin;
                }
                if (hadPendingMove) {
                    if (Piece* authoritativePiece = context.authoritativeControlledKingdom.getPieceById(selectedPiece->id)) {
                        authoritativePiece->position = pendingMove->origin;
                    }
                }

                TurnCommand cmd;
                cmd.type = TurnCommand::Move;
                cmd.pieceId = restoredPiece->id;
                cmd.origin = moveOrigin;
                cmd.destination = cellPos;

                const bool moveQueued = hadPendingMove
                    ? context.turnSystem.replaceMoveCommand(cmd,
                                                            context.authoritativeTurnContext)
                    : context.turnSystem.queueCommand(cmd,
                                                      context.authoritativeTurnContext,
                                                      &context.buildingFactory);
                if (!moveQueued) {
                    syncQueuedMovePreviewState(context);
                    refreshPieceMoves(restoredPiece, context);
                    return;
                }

                syncQueuedMovePreviewState(context);
                m_selectedPieceId = restoredPiece->id;
                m_selectedBuildingId = -1;
                m_selectedMapObjectId = -1;
                m_selectedBuildingOrigin.reset();
                m_selectedMapObjectPosition.reset();
                m_selectedBuildingType = BuildingType::Barracks;
                m_selectedMapObjectType = MapObjectType::Chest;
                m_selectedBuildingOwner = KingdomId::White;
                m_selectedBuildingIsNeutral = false;
                m_selectedBuildingRotationQuarterTurns = 0;
                m_hasSelectedCell = false;
                setActiveSelectionMetadata(SelectionLayer::Piece, cellPos);
                armSelectionCycle(cellPos);
                refreshPieceMoves(restoredPiece, context);
                return;
            }
        }

        applyResolvedSelection(stack, stack.top(), context);
        armSelectionCycle(cellPos);
    }
}

void InputHandler::handleBuildTool(const sf::Event& event, const InputContext& context) {
    if (event.type == sf::Event::MouseMoved) {
        sf::Vector2f worldPos = context.camera.screenToWorld({event.mouseMove.x, event.mouseMove.y}, context.window);
        sf::Vector2i cellPos = context.camera.worldToCell(worldPos, context.config.getCellSizePx());
        m_buildPreviewAnchorCell = cellPos;
        m_hasBuildPreview = true;
    }

    if (!context.permissions.canIssueCommands || !context.permissions.canQueueNonMoveActions) {
        return;
    }

    if (event.type == sf::Event::MouseButtonPressed && event.mouseButton.button == sf::Mouse::Left) {
        sf::Vector2f worldPos = context.camera.screenToWorld({event.mouseButton.x, event.mouseButton.y}, context.window);
        const sf::Vector2i anchorCell = context.camera.worldToCell(worldPos, context.config.getCellSizePx());
        const sf::Vector2i canonicalOrigin = StructurePlacementProfiles::originFromAnchorCell(
            m_buildPreviewType,
            anchorCell,
            m_buildPreviewRotationQuarterTurns,
            context.config);

        TurnCommand cmd;
        cmd.type = TurnCommand::Build;
        cmd.buildingType = m_buildPreviewType;
        cmd.buildOrigin = canonicalOrigin;
        cmd.buildRotationQuarterTurns = m_buildPreviewRotationQuarterTurns;

        for (const TurnCommand& pendingCommand : context.turnSystem.getPendingCommands()) {
            if (pendingCommand.type != TurnCommand::Build) {
                continue;
            }

            const bool samePlacement = pendingCommand.buildingType == m_buildPreviewType
                && pendingCommand.buildOrigin == canonicalOrigin
                && pendingCommand.buildRotationQuarterTurns == m_buildPreviewRotationQuarterTurns;
            if (!samePlacement) {
                continue;
            }

            context.turnSystem.cancelBuildCommand(pendingCommand.buildId,
                                                  context.authoritativeTurnContext);
            return;
        }

        if (PendingTurnProjection::canAppendCommand(context.authoritativeTurnContext,
                                                    context.turnSystem.getPendingCommands(),
                                                    cmd)
            && context.turnSystem.queueCommand(cmd,
                                               context.authoritativeTurnContext,
                                               &context.buildingFactory)) {
            syncQueuedMovePreviewState(context);
        }
    }
}

void InputHandler::handleCameraInput(const sf::Event& event, sf::RenderWindow& window, Camera& camera) {
    if (event.type == sf::Event::KeyPressed || event.type == sf::Event::KeyReleased) {
        const bool pressed = event.type == sf::Event::KeyPressed;
        switch (event.key.code) {
            case sf::Keyboard::Z:
                m_panUpHeld = pressed;
                break;
            case sf::Keyboard::S:
                m_panDownHeld = pressed;
                break;
            case sf::Keyboard::Q:
                m_panLeftHeld = pressed;
                break;
            case sf::Keyboard::D:
                m_panRightHeld = pressed;
                break;
            default:
                break;
        }
    }

    if (event.type == sf::Event::LostFocus) {
        m_panUpHeld = false;
        m_panDownHeld = false;
        m_panLeftHeld = false;
        m_panRightHeld = false;
        m_isDragging = false;
    }

    // Mouse wheel zoom
    if (event.type == sf::Event::MouseWheelScrolled) {
        if (std::abs(event.mouseWheelScroll.delta) <= 0.001f) {
            return;
        }

        float factor = (event.mouseWheelScroll.delta > 0) ? 0.9f : 1.1f;
        camera.zoom(factor);
    }

    // Middle mouse drag for panning
    if (event.type == sf::Event::MouseButtonPressed && event.mouseButton.button == sf::Mouse::Middle) {
        m_isDragging = true;
        m_lastMousePos = {event.mouseButton.x, event.mouseButton.y};
    }

    if (event.type == sf::Event::MouseButtonReleased && event.mouseButton.button == sf::Mouse::Middle) {
        m_isDragging = false;
    }

    if (event.type == sf::Event::MouseMoved && m_isDragging) {
        sf::Vector2i currentPos = {event.mouseMove.x, event.mouseMove.y};
        sf::Vector2f oldWorld = camera.screenToWorld(m_lastMousePos, window);
        sf::Vector2f newWorld = camera.screenToWorld(currentPos, window);
        camera.pan(oldWorld - newWorld);
        m_lastMousePos = currentPos;
    }
}
