#include "Input/InputHandler.hpp"
#include "Render/Camera.hpp"
#include "Board/Board.hpp"
#include "Board/Cell.hpp"
#include "Kingdom/Kingdom.hpp"
#include "Units/Piece.hpp"
#include "Units/MovementRules.hpp"
#include "Buildings/Building.hpp"
#include "Systems/TurnSystem.hpp"
#include "Systems/TurnCommand.hpp"
#include "Systems/BuildSystem.hpp"
#include "Config/GameConfig.hpp"
#include "UI/UIManager.hpp"
#include <algorithm>

InputHandler::InputHandler()
    : m_currentTool(ToolState::Select), m_selectedPiece(nullptr), m_selectedBuilding(nullptr),
      m_hasMovePreview(false), m_hasBuildPreview(false),
      m_buildPreviewType(BuildingType::Barracks),
      m_isDragging(false) {}

ToolState InputHandler::getCurrentTool() const { return m_currentTool; }
void InputHandler::setTool(ToolState tool) {
    m_currentTool = tool;
    clearSelection();
}

Piece* InputHandler::getSelectedPiece() const { return m_selectedPiece; }
Building* InputHandler::getSelectedBuilding() const { return m_selectedBuilding; }
const std::vector<sf::Vector2i>& InputHandler::getValidMoves() const { return m_validMoves; }
bool InputHandler::hasMovePreview() const { return m_hasMovePreview; }
sf::Vector2i InputHandler::getMoveFrom() const { return m_moveFrom; }
sf::Vector2i InputHandler::getMoveTo() const { return m_moveTo; }

BuildingType InputHandler::getBuildPreviewType() const { return m_buildPreviewType; }
sf::Vector2i InputHandler::getBuildPreviewOrigin() const { return m_buildPreviewOrigin; }
bool InputHandler::hasBuildPreview() const { return m_hasBuildPreview; }
void InputHandler::setBuildType(BuildingType type) { m_buildPreviewType = type; }

void InputHandler::clearSelection() {
    m_selectedPiece = nullptr;
    m_selectedBuilding = nullptr;
    m_validMoves.clear();
    // NOTE: does NOT clear move preview — call cancelLiveMove() / clearMovePreview() separately
    m_hasBuildPreview = false;
}

void InputHandler::cancelLiveMove() {
    if (m_movedPiece) {
        m_movedPiece->position = m_moveFrom;  // restore visual position
        m_movedPiece = nullptr;
    }
    m_hasMovePreview = false;
}

void InputHandler::clearMovePreview() {
    m_movedPiece = nullptr;
    m_hasMovePreview = false;
}

void InputHandler::handleEvent(const sf::Event& event, sf::RenderWindow& window,
                                Camera& camera, Board& board,
                                TurnSystem& turnSystem, Kingdom& activeKingdom,
                                Kingdom& enemyKingdom,
                                const std::vector<Building>& publicBuildings,
                                UIManager& uiManager, const GameConfig& config) {
    // Camera input (always active)
    handleCameraInput(event, window, camera);

    // K: center on king
    if (event.type == sf::Event::KeyPressed && event.key.code == sf::Keyboard::K) {
        Piece* king = activeKingdom.getKing();
        if (king) {
            float cx = static_cast<float>(king->position.x * config.getCellSizePx() + config.getCellSizePx() / 2);
            float cy = static_cast<float>(king->position.y * config.getCellSizePx() + config.getCellSizePx() / 2);
            camera.centerOn({cx, cy});
        } else if (!activeKingdom.pieces.empty()) {
            auto& p = activeKingdom.pieces.front();
            float cx = static_cast<float>(p.position.x * config.getCellSizePx() + config.getCellSizePx() / 2);
            float cy = static_cast<float>(p.position.y * config.getCellSizePx() + config.getCellSizePx() / 2);
            camera.centerOn({cx, cy});
        }
    }

    switch (m_currentTool) {
        case ToolState::Select:
            handleSelectTool(event, window, camera, board, turnSystem,
                              activeKingdom, enemyKingdom, publicBuildings, config);
            break;
        case ToolState::Build:
            handleBuildTool(event, window, camera, board, turnSystem, activeKingdom, config);
            break;
        case ToolState::Journal:
            // Journal mode: no map interaction
            break;
    }
}

void InputHandler::handleSelectTool(const sf::Event& event, sf::RenderWindow& window,
                                      Camera& camera, Board& board,
                                      TurnSystem& turnSystem, Kingdom& activeKingdom,
                                      Kingdom& enemyKingdom,
                                      const std::vector<Building>& publicBuildings,
                                      const GameConfig& config) {
    if (event.type == sf::Event::MouseButtonPressed && event.mouseButton.button == sf::Mouse::Left) {
        sf::Vector2f worldPos = camera.screenToWorld({event.mouseButton.x, event.mouseButton.y}, window);
        sf::Vector2i cellPos = camera.worldToCell(worldPos, config.getCellSizePx());

        if (!board.isInBounds(cellPos.x, cellPos.y)) return;
        Cell& cell = board.getCell(cellPos.x, cellPos.y);
        if (!cell.isInCircle) return;

        // ---- Move already applied: the moved piece must be selected to undo it ----
        if (m_hasMovePreview) {
            if (m_selectedPiece == m_movedPiece && cellPos == m_moveFrom) {
                m_movedPiece->position = m_moveFrom;
                turnSystem.cancelMoveCommand();

                Piece* restoredPiece = m_movedPiece;
                m_movedPiece = nullptr;
                m_hasMovePreview = false;
                m_moveTo = m_moveFrom;

                m_selectedPiece = restoredPiece;
                m_selectedBuilding = nullptr;
                m_validMoves = MovementRules::getValidMoves(*m_selectedPiece, board, config);
                return;
            }

            if (m_movedPiece && cellPos == m_movedPiece->position) {
                m_selectedPiece = m_movedPiece;
                m_selectedBuilding = nullptr;
                m_validMoves.clear();
                return;
            }

            clearSelection();
            return;
        }

        // ---- A piece is selected; player clicked a valid destination ----
        if (m_selectedPiece) {
            auto it = std::find(m_validMoves.begin(), m_validMoves.end(), cellPos);
            if (it != m_validMoves.end()) {
                // Store origin before moving
                sf::Vector2i origin = m_selectedPiece->position;

                // Apply move visually (piece draws at new position immediately)
                m_selectedPiece->position = cellPos;

                // Queue the command (includes origin for commitTurn)
                TurnCommand cmd;
                cmd.type = TurnCommand::Move;
                cmd.pieceId = m_selectedPiece->id;
                cmd.origin = origin;
                cmd.destination = cellPos;
                if (turnSystem.queueCommand(cmd)) {
                    m_movedPiece = m_selectedPiece;
                    m_hasMovePreview = true;
                    m_moveFrom = origin;
                    m_moveTo = cellPos;
                    m_selectedPiece = m_movedPiece;
                    m_selectedBuilding = nullptr;
                    m_validMoves.clear();
                } else {
                    // Couldn't queue (already moved this turn): revert visual move
                    m_selectedPiece->position = origin;
                }
                return;
            }
        }

        // ---- Try to select a piece ----
        Piece* piece = activeKingdom.getPieceAt(cellPos);
        if (piece) {
            m_selectedPiece = piece;
            m_selectedBuilding = nullptr;
            m_validMoves = MovementRules::getValidMoves(*piece, board, config);
            m_hasMovePreview = false;
            return;
        }

        // ---- Try to select a building ----
        Building* building = activeKingdom.getBuildingAt(cellPos);
        if (!building) {
            for (auto& b : const_cast<std::vector<Building>&>(publicBuildings)) {
                if (b.containsCell(cellPos.x, cellPos.y)) {
                    building = &b;
                    break;
                }
            }
        }
        if (building) {
            m_selectedBuilding = building;
            m_selectedPiece = nullptr;
            m_validMoves.clear();
            return;
        }

        // Clicked empty: deselect
        clearSelection();
    }
}

void InputHandler::handleBuildTool(const sf::Event& event, sf::RenderWindow& window,
                                     Camera& camera, Board& board,
                                     TurnSystem& turnSystem, Kingdom& activeKingdom,
                                     const GameConfig& config) {
    if (event.type == sf::Event::MouseMoved) {
        sf::Vector2f worldPos = camera.screenToWorld({event.mouseMove.x, event.mouseMove.y}, window);
        sf::Vector2i cellPos = camera.worldToCell(worldPos, config.getCellSizePx());
        m_buildPreviewOrigin = cellPos;
        m_hasBuildPreview = true;
    }

    if (event.type == sf::Event::MouseButtonPressed && event.mouseButton.button == sf::Mouse::Left) {
        sf::Vector2f worldPos = camera.screenToWorld({event.mouseButton.x, event.mouseButton.y}, window);
        sf::Vector2i cellPos = camera.worldToCell(worldPos, config.getCellSizePx());

        Piece* king = activeKingdom.getKing();
        if (!king) {
            // Check if initial pawn (first piece) can build
            if (!activeKingdom.pieces.empty()) {
                king = &activeKingdom.pieces.front();
            } else return;
        }

        if (BuildSystem::canBuild(m_buildPreviewType, cellPos, *king, board, activeKingdom, config)) {
            TurnCommand cmd;
            cmd.type = TurnCommand::Build;
            cmd.buildingType = m_buildPreviewType;
            cmd.buildOrigin = cellPos;
            turnSystem.queueCommand(cmd);
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
