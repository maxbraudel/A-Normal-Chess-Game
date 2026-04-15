#include "Input/InputHandler.hpp"

#include "Render/Camera.hpp"
#include "Board/Board.hpp"
#include "Board/Cell.hpp"
#include "Kingdom/Kingdom.hpp"
#include "Units/Piece.hpp"
#include "Units/PieceType.hpp"
#include "Units/MovementRules.hpp"
#include "Buildings/Building.hpp"
#include "Systems/TurnSystem.hpp"
#include "Systems/TurnCommand.hpp"
#include "Systems/CheckResponseRules.hpp"
#include "Systems/BuildSystem.hpp"
#include "Config/GameConfig.hpp"
#include "UI/UIManager.hpp"

#include <algorithm>
#include <chrono>
#include <cmath>

namespace {

constexpr float kKeyboardPanSpeed = 900.f;
const auto kSelectionCycleThreshold = std::chrono::milliseconds(350);

} // namespace

InputHandler::InputHandler()
    : m_currentTool(ToolState::Select), m_selectedPiece(nullptr), m_selectedBuilding(nullptr),
    m_hasSelectedCell(false), m_selectedCell({0, 0}),
      m_hasMovePreview(false), m_hasBuildPreview(false),
      m_buildPreviewType(BuildingType::Barracks),
        m_buildPreviewRotationQuarterTurns(0),
            m_activeSelectionLayer(SelectionLayer::None), m_hasActiveSelectionCell(false),
            m_activeSelectionCell({0, 0}), m_isSelectionCycleArmed(false),
            m_selectionCycleCell({0, 0}),
      m_isDragging(false) {}

ToolState InputHandler::getCurrentTool() const { return m_currentTool; }
void InputHandler::setTool(ToolState tool) {
    m_currentTool = tool;
    clearSelection();
}

Piece* InputHandler::getSelectedPiece() const { return m_selectedPiece; }
Building* InputHandler::getSelectedBuilding() const { return m_selectedBuilding; }
bool InputHandler::hasSelectedCell() const { return m_hasSelectedCell; }
sf::Vector2i InputHandler::getSelectedCell() const { return m_selectedCell; }
const std::vector<sf::Vector2i>& InputHandler::getValidMoves() const { return m_validMoves; }
const std::vector<sf::Vector2i>& InputHandler::getDangerMoves() const { return m_dangerMoves; }
int InputHandler::getCapturePreviewPieceId() const { return m_capturePreviewPieceId; }
bool InputHandler::hasMovePreview() const { return m_hasMovePreview; }
sf::Vector2i InputHandler::getMoveFrom() const { return m_moveFrom; }
sf::Vector2i InputHandler::getMoveTo() const { return m_moveTo; }

BuildingType InputHandler::getBuildPreviewType() const { return m_buildPreviewType; }
sf::Vector2i InputHandler::getBuildPreviewOrigin() const { return m_buildPreviewOrigin; }
int InputHandler::getBuildPreviewRotationQuarterTurns() const { return m_buildPreviewRotationQuarterTurns; }
bool InputHandler::hasBuildPreview() const { return m_hasBuildPreview; }
void InputHandler::setBuildType(BuildingType type) {
    if (m_buildPreviewType != type) {
        m_buildPreviewRotationQuarterTurns = 0;
    }
    m_buildPreviewType = type;
}

void InputHandler::clearSelection() {
    m_selectedPiece = nullptr;
    m_selectedBuilding = nullptr;
    m_hasSelectedCell = false;
    m_selectedCell = {0, 0};
    m_activeSelectionLayer = SelectionLayer::None;
    m_hasActiveSelectionCell = false;
    m_activeSelectionCell = {0, 0};
    m_validMoves.clear();
    m_dangerMoves.clear();
    clearSelectionCycle();
    // NOTE: does NOT clear move preview — call cancelLiveMove() / clearMovePreview() separately
    m_hasBuildPreview = false;
}

void InputHandler::selectCell(sf::Vector2i cellPos) {
    m_selectedPiece = nullptr;
    m_selectedBuilding = nullptr;
    m_selectedCell = cellPos;
    m_hasSelectedCell = true;
    m_validMoves.clear();
    m_dangerMoves.clear();
    setActiveSelectionMetadata(SelectionLayer::Terrain, cellPos);
}

void InputHandler::activatePieceSelection(Piece* piece, sf::Vector2i cellPos,
                                          const Board& board, const Kingdom& enemyKingdom,
                                          const GameConfig& config, bool allowCommands) {
    m_selectedPiece = piece;
    m_selectedBuilding = nullptr;
    m_hasSelectedCell = false;
    if (piece && allowCommands && piece->kingdom != enemyKingdom.id) {
        refreshPieceMoves(piece, const_cast<Board&>(board), enemyKingdom, config);
    } else {
        m_validMoves.clear();
        m_dangerMoves.clear();
    }
    setActiveSelectionMetadata(SelectionLayer::Piece, cellPos);
}

void InputHandler::activateBuildingSelection(Building* building, sf::Vector2i cellPos) {
    m_selectedPiece = nullptr;
    m_selectedBuilding = building;
    m_hasSelectedCell = false;
    m_validMoves.clear();
    m_dangerMoves.clear();
    setActiveSelectionMetadata(SelectionLayer::Building, cellPos);
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
    Piece* pieceOverride = nullptr;
    bool suppressCellPiece = false;

    if (m_hasMovePreview && m_movedPiece) {
        if (cellPos == m_moveTo) {
            pieceOverride = m_movedPiece;
            suppressCellPiece = true;
        } else if (cellPos == m_moveFrom) {
            suppressCellPiece = true;
        }
    }

    return resolveCellSelectionStack(cell, cellPos, pieceOverride, suppressCellPiece);
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
                                   context.board, context.opposingKingdom,
                                   context.config,
                                   context.allowCommands && stack.piece
                                       && stack.piece->kingdom == context.controlledKingdom.id);
            return;
        case SelectionLayer::Building:
            activateBuildingSelection(stack.building, stack.cellPos);
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
    if (m_hasMovePreview) {
        context.turnSystem.cancelMoveCommand();
        cancelLiveMove();
    }

    m_selectedPiece = nullptr;
    m_validMoves.clear();
    m_dangerMoves.clear();
}

void InputHandler::refreshPieceMoves(Piece* piece, Board& board, const Kingdom& enemyKingdom, const GameConfig& config) {
    m_validMoves.clear();
    m_dangerMoves.clear();
    std::vector<sf::Vector2i> allMoves = MovementRules::getValidMoves(*piece, board, config);
    std::vector<sf::Vector2i> legalMoves = CheckResponseRules::filterLegalMovesForPiece(*piece, board, config);
    if (piece->type == PieceType::King) {
        for (const auto& mv : allMoves) {
            const Cell& destinationCell = board.getCell(mv.x, mv.y);
            if (destinationCell.piece
                && destinationCell.piece->kingdom != piece->kingdom
                && destinationCell.piece->type == PieceType::King) {
                continue;
            }

            if (std::find(legalMoves.begin(), legalMoves.end(), mv) != legalMoves.end()) {
                m_validMoves.push_back(mv);
            } else {
                m_dangerMoves.push_back(mv);
            }
        }
    } else {
        m_validMoves = std::move(legalMoves);
    }
}

void InputHandler::cancelLiveMove() {
    if (m_movedPiece) {
        m_movedPiece->position = m_moveFrom;  // restore visual position
        m_movedPiece = nullptr;
    }
    m_hasMovePreview = false;
    m_capturePreviewPieceId = -1;
}

void InputHandler::clearMovePreview() {
    m_movedPiece = nullptr;
    m_hasMovePreview = false;
    m_capturePreviewPieceId = -1;
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
        && m_currentTool == ToolState::Build && context.allowCommands) {
        m_buildPreviewRotationQuarterTurns = (m_buildPreviewRotationQuarterTurns + 1) % 4;
    }

    switch (m_currentTool) {
        case ToolState::Select:
            handleSelectTool(event, context);
            break;
        case ToolState::Build:
            if (context.allowCommands) {
                handleBuildTool(event, context);
            } else {
                handleSelectTool(event, context);
            }
            break;
    }
}

void InputHandler::updateCameraMovement(float deltaTime, Camera& camera) {
    if (deltaTime <= 0.f) {
        return;
    }

    sf::Vector2f direction{0.f, 0.f};
    if (sf::Keyboard::isKeyPressed(sf::Keyboard::Z)) {
        direction.y -= 1.f;
    }
    if (sf::Keyboard::isKeyPressed(sf::Keyboard::S)) {
        direction.y += 1.f;
    }
    if (sf::Keyboard::isKeyPressed(sf::Keyboard::Q)) {
        direction.x -= 1.f;
    }
    if (sf::Keyboard::isKeyPressed(sf::Keyboard::D)) {
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

        if (!context.allowCommands) {
            applyResolvedSelection(stack, stack.top(), context);
            armSelectionCycle(cellPos);
            return;
        }

        // ---- Live move preview: allow retargeting to any original legal move or cancel on origin ----
        if (m_hasMovePreview) {
            if (m_selectedPiece == m_movedPiece && cellPos == m_moveFrom) {
                m_movedPiece->position = m_moveFrom;
                context.turnSystem.cancelMoveCommand();

                Piece* restoredPiece = m_movedPiece;
                m_movedPiece = nullptr;
                m_hasMovePreview = false;
                m_capturePreviewPieceId = -1;
                m_moveTo = m_moveFrom;

                m_selectedPiece = restoredPiece;
                m_selectedBuilding = nullptr;
                m_hasSelectedCell = false;
                setActiveSelectionMetadata(SelectionLayer::Piece, m_moveFrom);
                armSelectionCycle(m_moveFrom);
                return;
            }

            auto validMoveIt = std::find(m_validMoves.begin(), m_validMoves.end(), cellPos);
            if (m_movedPiece && validMoveIt != m_validMoves.end()) {
                if (cellPos == m_movedPiece->position) {
                    m_selectedPiece = m_movedPiece;
                    m_selectedBuilding = nullptr;
                    return;
                }

                m_movedPiece->position = cellPos;

                Piece* captured = context.opposingKingdom.getPieceAt(cellPos);
                m_capturePreviewPieceId = captured ? captured->id : -1;

                context.turnSystem.cancelMoveCommand();

                TurnCommand cmd;
                cmd.type = TurnCommand::Move;
                cmd.pieceId = m_movedPiece->id;
                cmd.origin = m_moveFrom;
                cmd.destination = cellPos;
                if (context.turnSystem.queueCommand(cmd)) {
                    m_moveTo = cellPos;
                    m_selectedPiece = m_movedPiece;
                    m_selectedBuilding = nullptr;
                    m_hasSelectedCell = false;
                    setActiveSelectionMetadata(SelectionLayer::Piece, cellPos);
                    armSelectionCycle(cellPos);
                } else {
                    m_movedPiece->position = m_moveTo;
                    Piece* previousCapture = context.opposingKingdom.getPieceAt(m_moveTo);
                    m_capturePreviewPieceId = previousCapture ? previousCapture->id : -1;
                }
                return;
            }

            cancelPieceSelectionContext(context);
        }

        // ---- A piece is selected; player clicked a valid destination ----
        if (m_selectedPiece) {
            auto it = std::find(m_validMoves.begin(), m_validMoves.end(), cellPos);
            if (it != m_validMoves.end()) {
                // Store origin before moving
                sf::Vector2i origin = m_selectedPiece->position;

                // Apply move visually (piece draws at new position immediately)
                m_selectedPiece->position = cellPos;

                // Track any enemy piece at the destination for preview-hide
                Piece* captured = context.opposingKingdom.getPieceAt(cellPos);
                m_capturePreviewPieceId = captured ? captured->id : -1;

                // Queue the command (includes origin for commitTurn)
                TurnCommand cmd;
                cmd.type = TurnCommand::Move;
                cmd.pieceId = m_selectedPiece->id;
                cmd.origin = origin;
                cmd.destination = cellPos;
                if (context.turnSystem.queueCommand(cmd)) {
                    m_movedPiece = m_selectedPiece;
                    m_hasMovePreview = true;
                    m_moveFrom = origin;
                    m_moveTo = cellPos;
                    m_selectedPiece = m_movedPiece;
                    m_selectedBuilding = nullptr;
                    m_hasSelectedCell = false;
                    setActiveSelectionMetadata(SelectionLayer::Piece, cellPos);
                    armSelectionCycle(cellPos);
                } else {
                    // Couldn't queue (already moved this turn): revert visual move
                    m_selectedPiece->position = origin;
                    m_capturePreviewPieceId = -1;
                }
                return;
            }
        }

        applyResolvedSelection(stack, stack.top(), context);
        armSelectionCycle(cellPos);
    }
}

void InputHandler::handleBuildTool(const sf::Event& event, const InputContext& context) {
    if (!context.allowCommands || !context.allowNonMoveActions) {
        m_hasBuildPreview = false;
        return;
    }

    if (event.type == sf::Event::MouseMoved) {
        sf::Vector2f worldPos = context.camera.screenToWorld({event.mouseMove.x, event.mouseMove.y}, context.window);
        sf::Vector2i cellPos = context.camera.worldToCell(worldPos, context.config.getCellSizePx());
        m_buildPreviewOrigin = cellPos;
        m_hasBuildPreview = true;
    }

    if (event.type == sf::Event::MouseButtonPressed && event.mouseButton.button == sf::Mouse::Left) {
        sf::Vector2f worldPos = context.camera.screenToWorld({event.mouseButton.x, event.mouseButton.y}, context.window);
        sf::Vector2i cellPos = context.camera.worldToCell(worldPos, context.config.getCellSizePx());

        Piece* king = context.controlledKingdom.getKing();
        if (!king) {
            // Check if initial pawn (first piece) can build
            if (!context.controlledKingdom.pieces.empty()) {
                king = &context.controlledKingdom.pieces.front();
            } else return;
        }

        if (BuildSystem::canBuild(m_buildPreviewType,
                                  cellPos,
                                  *king,
                                  context.board,
                                  context.controlledKingdom,
                                  context.config,
                                  m_buildPreviewRotationQuarterTurns)) {
            const TurnCommand* pendingBuild = context.turnSystem.getPendingBuildCommand();
            if (pendingBuild) {
                bool samePlacement = pendingBuild->buildingType == m_buildPreviewType
                    && pendingBuild->buildOrigin == cellPos
                    && pendingBuild->buildRotationQuarterTurns == m_buildPreviewRotationQuarterTurns;
                context.turnSystem.cancelBuildCommand();
                if (samePlacement) {
                    return;
                }
            }

            TurnCommand cmd;
            cmd.type = TurnCommand::Build;
            cmd.buildingType = m_buildPreviewType;
            cmd.buildOrigin = cellPos;
            cmd.buildRotationQuarterTurns = m_buildPreviewRotationQuarterTurns;
            context.turnSystem.queueCommand(cmd);
        }
    }
}

void InputHandler::handleCameraInput(const sf::Event& event, sf::RenderWindow& window, Camera& camera) {
    // Mouse wheel zoom
    if (event.type == sf::Event::MouseWheelScrolled) {
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
