#pragma once
#include <chrono>
#include <map>
#include <optional>
#include <set>
#include <SFML/System/Vector2.hpp>
#include <SFML/Window/Event.hpp>
#include "Core/ToolState.hpp"
#include <vector>
#include "Input/InputContext.hpp"
#include "Input/InputSelectionBookmark.hpp"
#include "Input/LayeredSelection.hpp"
#include "Buildings/BuildingType.hpp"
#include "Objects/MapObjectType.hpp"

class Camera;
class Board;
class TurnSystem;
class UIManager;
class GameConfig;
class Kingdom;
class Piece;
struct AutonomousUnit;
class Building;
class MapObject;

class InputHandler {
public:
    InputHandler();

    void handleEvent(const sf::Event& event, const InputContext& context);
    void updateCameraMovement(float deltaTime, Camera& camera);

    ToolState getCurrentTool() const;
    void setTool(ToolState tool);

    int getSelectedPieceId() const;
    int getSelectedAutonomousUnitId() const;
    int getSelectedBuildingId() const;
    int getSelectedMapObjectId() const;
    bool hasSelectedCell() const;
    sf::Vector2i getSelectedCell() const;
    bool hasSelectionAnchorCell() const;
    sf::Vector2i getSelectionAnchorCell() const;
    const std::vector<sf::Vector2i>& getValidMoves() const;
    const std::vector<sf::Vector2i>& getDangerMoves() const; // selectable moves that still leave the active king in check
    bool isSelectedOriginDangerous() const;
    bool isSelectedOriginSelectable() const;
    const std::set<int>& getCapturePreviewPieceIds() const; // enemy pieces visually hidden during queued capture previews
    bool hasMovePreview() const;
    void cancelLiveMove(Kingdom& controlledKingdom, const TurnSystem& turnSystem);   // restores queued preview positions and clears preview state
    void clearMovePreview(); // clears preview state without restoring (use after commit)

    // Build mode
    BuildingType getBuildPreviewType() const;
    sf::Vector2i getBuildPreviewAnchorCell() const;
    int getBuildPreviewRotationQuarterTurns() const;
    bool hasBuildPreview() const;
    void setBuildType(BuildingType type);

    InputSelectionBookmark createSelectionBookmark() const;
    void reconcileSelection(const InputSelectionBookmark& bookmark,
                            Piece* selectedPiece,
                            AutonomousUnit* selectedAutonomousUnit,
                            Building* selectedBuilding,
                            MapObject* selectedMapObject,
                            const InputContext& context);
    void reconcileSelection(const InputSelectionBookmark& bookmark,
                            Piece* selectedPiece,
                            Building* selectedBuilding,
                            MapObject* selectedMapObject,
                            const InputContext& context) {
        reconcileSelection(bookmark,
                           selectedPiece,
                           nullptr,
                           selectedBuilding,
                           selectedMapObject,
                           context);
    }
    void reconcileSelection(const InputSelectionBookmark& bookmark,
                            Piece* selectedPiece,
                            Building* selectedBuilding,
                            const InputContext& context) {
        reconcileSelection(bookmark, selectedPiece, nullptr, selectedBuilding, nullptr, context);
    }
    void clearSelection();

private:
    ToolState m_currentTool;
    int m_selectedPieceId;
    int m_selectedAutonomousUnitId;
    int m_selectedBuildingId;
    int m_selectedMapObjectId;
    std::optional<sf::Vector2i> m_selectedBuildingOrigin;
    std::optional<sf::Vector2i> m_selectedMapObjectPosition;
    BuildingType m_selectedBuildingType;
    MapObjectType m_selectedMapObjectType;
    KingdomId m_selectedBuildingOwner;
    bool m_selectedBuildingIsNeutral;
    int m_selectedBuildingRotationQuarterTurns;
    bool m_hasSelectedCell;
    sf::Vector2i m_selectedCell;
    std::vector<sf::Vector2i> m_validMoves;
    std::vector<sf::Vector2i> m_dangerMoves; // selectable moves shown red because they leave the active king in check
    bool m_selectedOriginDangerous;
    bool m_selectedOriginSelectable;
    std::set<int> m_capturePreviewPieceIds;
    std::map<int, sf::Vector2i> m_movePreviewOrigins;

    bool m_hasBuildPreview;
    BuildingType m_buildPreviewType;
    sf::Vector2i m_buildPreviewAnchorCell;
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
    bool m_panUpHeld;
    bool m_panDownHeld;
    bool m_panLeftHeld;
    bool m_panRightHeld;

    void handleSelectTool(const sf::Event& event, const InputContext& context);
    void handleBuildTool(const sf::Event& event, const InputContext& context);
    void handleCameraInput(const sf::Event& event, sf::RenderWindow& window, Camera& camera);
    Piece* resolveSelectedPiece(const InputContext& context) const;
    // Recompute move targets for piece given the projected queued turn state.
    void refreshPieceMoves(Piece* piece, const InputContext& context);
    bool isSelectableMoveDestination(sf::Vector2i cellPos) const;
    void syncQueuedMovePreviewState(const InputContext& context);
    void selectCell(sf::Vector2i cellPos);
    void activatePieceSelection(Piece* piece, sf::Vector2i cellPos,
                                const InputContext& context, bool allowCommands);
    void activateAutonomousSelection(AutonomousUnit* autonomousUnit, sf::Vector2i cellPos);
    void activateBuildingSelection(Building* building, sf::Vector2i cellPos);
    void activateMapObjectSelection(MapObject* mapObject, sf::Vector2i cellPos);
    void activateTerrainSelection(sf::Vector2i cellPos);
    void setActiveSelectionMetadata(SelectionLayer layer, sf::Vector2i cellPos);
    void clearSelectionCycle();
    void armSelectionCycle(sf::Vector2i cellPos);
    bool canCycleSelection(sf::Vector2i cellPos) const;
    LayeredSelectionStack resolveSelectionStackAtCell(const InputContext& context, sf::Vector2i cellPos) const;
    void applyResolvedSelection(const LayeredSelectionStack& stack, SelectionLayer layer,
                                const InputContext& context);
    void cancelPieceSelectionContext(const InputContext& context);
    void restoreBuildPreviewState(const InputSelectionBookmark& bookmark);
};
