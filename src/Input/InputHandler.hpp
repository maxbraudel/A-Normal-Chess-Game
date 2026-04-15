#pragma once
#include <chrono>
#include <map>
#include <set>
#include <SFML/System/Vector2.hpp>
#include <SFML/Window/Event.hpp>
#include <vector>
#include "Input/InputContext.hpp"
#include "Input/LayeredSelection.hpp"
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

    void handleEvent(const sf::Event& event, const InputContext& context);
    void updateCameraMovement(float deltaTime, Camera& camera);

    ToolState getCurrentTool() const;
    void setTool(ToolState tool);

    Piece* getSelectedPiece() const;
    Building* getSelectedBuilding() const;
    bool hasSelectedCell() const;
    sf::Vector2i getSelectedCell() const;
    const std::vector<sf::Vector2i>& getValidMoves() const;
    const std::vector<sf::Vector2i>& getDangerMoves() const; // king squares under enemy threat
    const std::set<int>& getCapturePreviewPieceIds() const; // enemy pieces visually hidden during queued capture previews
    bool hasMovePreview() const;
    void cancelLiveMove(Kingdom& controlledKingdom, const TurnSystem& turnSystem);   // restores queued preview positions and clears preview state
    void clearMovePreview(); // clears preview state without restoring (use after commit)

    // Build mode
    BuildingType getBuildPreviewType() const;
    sf::Vector2i getBuildPreviewOrigin() const;
    int getBuildPreviewRotationQuarterTurns() const;
    bool hasBuildPreview() const;
    void setBuildType(BuildingType type);

    void clearSelection();

private:
    ToolState m_currentTool;
    Piece* m_selectedPiece;
    Building* m_selectedBuilding;
    bool m_hasSelectedCell;
    sf::Vector2i m_selectedCell;
    std::vector<sf::Vector2i> m_validMoves;
    std::vector<sf::Vector2i> m_dangerMoves; // king moves onto threatened squares (shown red, blocked)
    std::set<int> m_capturePreviewPieceIds;
    std::map<int, sf::Vector2i> m_movePreviewOrigins;

    bool m_hasBuildPreview;
    BuildingType m_buildPreviewType;
    sf::Vector2i m_buildPreviewOrigin;
    int m_buildPreviewRotationQuarterTurns;

    SelectionLayer m_activeSelectionLayer;
    bool m_hasActiveSelectionCell;
    sf::Vector2i m_activeSelectionCell;
    bool m_isSelectionCycleArmed;
    sf::Vector2i m_selectionCycleCell;
    std::chrono::steady_clock::time_point m_selectionCycleArmTime;

    // Camera dragging
    bool m_isDragging;
    sf::Vector2i m_lastMousePos;

    void handleSelectTool(const sf::Event& event, const InputContext& context);
    void handleBuildTool(const sf::Event& event, const InputContext& context);
    void handleCameraInput(const sf::Event& event, sf::RenderWindow& window, Camera& camera);
    // Recompute move targets for piece given the projected queued turn state.
    void refreshPieceMoves(Piece* piece, const InputContext& context);
    void syncQueuedMovePreviewState(const InputContext& context);
    void selectCell(sf::Vector2i cellPos);
    void activatePieceSelection(Piece* piece, sf::Vector2i cellPos,
                                const InputContext& context, bool allowCommands);
    void activateBuildingSelection(Building* building, sf::Vector2i cellPos);
    void activateTerrainSelection(sf::Vector2i cellPos);
    void setActiveSelectionMetadata(SelectionLayer layer, sf::Vector2i cellPos);
    void clearSelectionCycle();
    void armSelectionCycle(sf::Vector2i cellPos);
    bool canCycleSelection(sf::Vector2i cellPos) const;
    LayeredSelectionStack resolveSelectionStackAtCell(const InputContext& context, sf::Vector2i cellPos) const;
    void applyResolvedSelection(const LayeredSelectionStack& stack, SelectionLayer layer,
                                const InputContext& context);
    void cancelPieceSelectionContext(const InputContext& context);
};
