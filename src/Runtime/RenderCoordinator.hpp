#pragma once

#include <array>
#include <optional>
#include <set>
#include <string>
#include <vector>

#include <SFML/Graphics/Color.hpp>
#include <SFML/Graphics/RenderWindow.hpp>
#include <SFML/Graphics/View.hpp>
#include <SFML/System/Vector2.hpp>

#include "Buildings/BuildingType.hpp"
#include "Core/GameState.hpp"
#include "Core/InteractionPermissions.hpp"
#include "Core/ToolState.hpp"
#include "Kingdom/KingdomId.hpp"
#include "Objects/MapObject.hpp"

class AssetManager;
class Board;
class Building;
class Camera;
class GameConfig;
class Kingdom;
class Piece;
class Renderer;
struct TurnCommand;

struct SelectionFrameSpec {
    sf::Vector2i origin{0, 0};
    int width = 1;
    int height = 1;
};

struct OriginCellSpec {
    sf::Vector2i origin{0, 0};
    sf::Color color = sf::Color::Transparent;
};

struct BuildPreviewSpec {
    sf::Vector2i origin{0, 0};
    BuildingType type = BuildingType::Barracks;
    int width = 0;
    int height = 0;
    int rotationQuarterTurns = 0;
    bool valid = false;
};

struct ActionMarkerSpec {
    sf::Vector2i origin{0, 0};
    int width = 1;
    int height = 1;
    std::string iconName;
};

struct WorldRenderState {
    GameState gameState = GameState::MainMenu;
    ToolState activeTool = ToolState::Select;
    InteractionPermissions permissions{};
    bool usingConcretePendingState = false;
    KingdomId activeKingdom = KingdomId::White;
    const Piece* selectedPiece = nullptr;
    const Building* selectedBuilding = nullptr;
    const MapObject* selectedMapObject = nullptr;
    std::optional<sf::Vector2i> selectedCell;
    bool selectedOriginDangerous = false;
    std::vector<sf::Vector2i> validMoves;
    std::vector<sf::Vector2i> dangerMoves;
    std::set<int> capturePreviewPieceIds;
    bool hasBuildPreview = false;
    BuildingType buildPreviewType = BuildingType::Barracks;
    sf::Vector2i buildPreviewAnchorCell{0, 0};
    int buildPreviewRotationQuarterTurns = 0;
};

struct WorldRenderPlan {
    bool renderWorld = false;
    std::set<int> capturePreviewPieceIds;
    bool showOrientationCheckerboard = false;
    std::optional<OriginCellSpec> selectedOriginCell;
    std::vector<sf::Vector2i> highlightedCells;
    std::vector<sf::Vector2i> dangerCells;
    std::vector<SelectionFrameSpec> selectionFrames;
    std::optional<BuildPreviewSpec> liveBuildPreview;
    std::vector<BuildPreviewSpec> pendingBuildPreviews;
    std::vector<ActionMarkerSpec> actionMarkers;
    const Building* selectedBuilding = nullptr;
};

struct WorldRenderBindings {
    sf::RenderWindow& window;
    const sf::View& hudView;
    sf::Vector2u windowSize;
    Camera& camera;
    Renderer& renderer;
    const AssetManager& assets;
    const Board& displayedBoard;
    const std::array<Kingdom, kNumKingdoms>& displayedKingdoms;
    const std::vector<Building>& displayedPublicBuildings;
    const std::vector<MapObject>& displayedMapObjects;
    const GameConfig& config;
};

class RenderCoordinator {
public:
    static bool shouldRenderWorld(GameState state);
    static WorldRenderPlan buildWorldRenderPlan(const WorldRenderState& state,
                                                const std::vector<TurnCommand>& pendingCommands,
                                                const std::vector<sf::Vector2i>& buildableAnchorCellsOverlay,
                                                const std::vector<sf::Vector2i>& buildableCellsOverlay,
                                                const GameConfig& config);
    static void renderWorldFrame(WorldRenderBindings& bindings,
                                 const WorldRenderPlan& plan);
};