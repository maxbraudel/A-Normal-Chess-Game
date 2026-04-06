#pragma once
#include <SFML/Window/Event.hpp>
#include <SFML/System/Vector2.hpp>
#include <SFML/Graphics/RenderWindow.hpp>
#include <vector>
#include "Input/ToolState.hpp"
#include "Buildings/BuildingType.hpp"

class Camera;
class Board;
class TurnSystem;
class UIManager;
class GameConfig;
class Kingdom;
class Piece;
class Building;

class InputHandler {
public:
    InputHandler();

    void handleEvent(const sf::Event& event, sf::RenderWindow& window,
                      Camera& camera, Board& board,
                      TurnSystem& turnSystem, Kingdom& activeKingdom,
                      Kingdom& enemyKingdom,
                      const std::vector<Building>& publicBuildings,
                      UIManager& uiManager, const GameConfig& config);

    ToolState getCurrentTool() const;
    void setTool(ToolState tool);

    Piece* getSelectedPiece() const;
    Building* getSelectedBuilding() const;
    const std::vector<sf::Vector2i>& getValidMoves() const;
    bool hasMovePreview() const;
    sf::Vector2i getMoveFrom() const;
    sf::Vector2i getMoveTo() const;
    void cancelLiveMove();   // restores piece position and clears preview state
    void clearMovePreview(); // clears preview state without restoring (use after commit)

    // Build mode
    BuildingType getBuildPreviewType() const;
    sf::Vector2i getBuildPreviewOrigin() const;
    bool hasBuildPreview() const;
    void setBuildType(BuildingType type);

    void clearSelection();

private:
    ToolState m_currentTool;
    Piece* m_selectedPiece;
    Building* m_selectedBuilding;
    std::vector<sf::Vector2i> m_validMoves;

    bool m_hasMovePreview;
    sf::Vector2i m_moveFrom;
    sf::Vector2i m_moveTo;
    Piece* m_movedPiece = nullptr;

    bool m_hasBuildPreview;
    BuildingType m_buildPreviewType;
    sf::Vector2i m_buildPreviewOrigin;

    // Camera dragging
    bool m_isDragging;
    sf::Vector2i m_lastMousePos;

    void handleSelectTool(const sf::Event& event, sf::RenderWindow& window,
                           Camera& camera, Board& board,
                           TurnSystem& turnSystem, Kingdom& activeKingdom,
                           Kingdom& enemyKingdom,
                           const std::vector<Building>& publicBuildings,
                           const GameConfig& config);
    void handleBuildTool(const sf::Event& event, sf::RenderWindow& window,
                          Camera& camera, Board& board,
                          TurnSystem& turnSystem, Kingdom& activeKingdom,
                          const GameConfig& config);
    void handleCameraInput(const sf::Event& event, sf::RenderWindow& window, Camera& camera);
};
