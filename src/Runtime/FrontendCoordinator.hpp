#pragma once

#include <array>
#include <functional>
#include <optional>
#include <string>
#include <vector>

#include <SFML/Graphics/RenderWindow.hpp>

#include "Core/GameState.hpp"
#include "Core/InteractionPermissions.hpp"
#include "Core/LocalPlayerContext.hpp"
#include "Core/ToolState.hpp"
#include "Input/InputContext.hpp"
#include "Systems/CheckResponseRules.hpp"
#include "Systems/EconomySystem.hpp"
#include "Systems/PublicBuildingOccupation.hpp"
#include "UI/InGameViewModel.hpp"

class Board;
class Building;
class BuildingFactory;
class Camera;
struct Cell;
class GameConfig;
class GameEngine;
class Kingdom;
class Piece;
struct TurnCommand;
class TurnSystem;
class UIManager;

struct FrontendRuntimeState {
    GameState gameState = GameState::MainMenu;
    LocalPlayerContext localPlayerContext{};
    bool overlaysVisible = false;
    bool inGameMenuOpen = false;
    bool waitingForRemoteTurnResult = false;
    bool hostAuthenticated = false;
    bool clientAuthenticated = false;
    bool awaitingReconnect = false;
    bool hasReconnectRequest = false;
    std::string hostJoinHint;
    KingdomId activeKingdom = KingdomId::White;
};

struct FrontendDisplayBindings {
    sf::RenderWindow& window;
    Camera& camera;
    UIManager& uiManager;
    Board& displayedBoard;
    std::array<Kingdom, kNumKingdoms>& displayedKingdoms;
    const std::vector<Building>& displayedPublicBuildings;
    Board& authoritativeBoard;
    std::array<Kingdom, kNumKingdoms>& authoritativeKingdoms;
    const std::vector<Building>& authoritativePublicBuildings;
    TurnSystem& turnSystem;
    BuildingFactory& buildingFactory;
    TurnValidationContext authoritativeTurnContext;
    const GameConfig& config;
};

struct FrontendDashboardBindings {
    const GameEngine& engine;
    const Board& displayedBoard;
    const std::array<Kingdom, kNumKingdoms>& displayedKingdoms;
    const std::vector<Building>& displayedPublicBuildings;
    const GameConfig& config;
};

struct FrontendPanelBindings {
    ToolState activeTool = ToolState::Select;
    const Board& displayedBoard;
    const TurnSystem& turnSystem;
    const GameConfig& config;
    const Piece* selectedPiece = nullptr;
    const Building* selectedBuilding = nullptr;
    const Cell* selectedCell = nullptr;
};

enum class FrontendLeftPanelKind {
    EmptyState,
    Piece,
    Building,
    Barracks,
    BuildTool,
    Cell
};

struct FrontendLeftPanelPresentation {
    FrontendLeftPanelKind kind = FrontendLeftPanelKind::EmptyState;
    const Piece* piece = nullptr;
    const Building* building = nullptr;
    const Cell* cell = nullptr;
    KingdomId viewedKingdom = KingdomId::White;
    bool allowBuild = false;
    bool allowUpgrade = false;
    bool allowDisband = false;
    bool allowProduce = false;
    bool allowCancelConstruction = false;
    const TurnCommand* pendingUpgrade = nullptr;
    const TurnCommand* pendingDisband = nullptr;
    const TurnCommand* pendingProduce = nullptr;
    std::optional<ResourceIncomeBreakdown> resourceIncome;
    std::optional<PublicBuildingOccupationState> publicOccupation;
};

class FrontendCoordinator {
public:
    static bool isLocalPlayerTurn(const FrontendRuntimeState& state);
    static KingdomId localPerspectiveKingdom(const FrontendRuntimeState& state);
    static bool isMultiplayerSessionReady(const FrontendRuntimeState& state);
    static bool shouldEvaluateTurnValidation(const FrontendRuntimeState& state);
    static InteractionPermissions currentInteractionPermissions(
        const FrontendRuntimeState& state,
        const std::optional<CheckTurnValidation>& validation = std::nullopt);
    static InGameHudPresentation buildInGameHudPresentation(const FrontendRuntimeState& state);
    static InGameViewModel buildDashboardViewModel(const FrontendRuntimeState& state,
                                                   const FrontendDashboardBindings& bindings,
                                                   const InteractionPermissions& permissions,
                                                   const CheckTurnValidation& validation,
                                                   const InGameHudPresentation& presentation,
                                                   bool usingProjectedDisplayState);
    static FrontendLeftPanelPresentation buildLeftPanelPresentation(
        const FrontendRuntimeState& state,
        const FrontendPanelBindings& bindings,
        const InteractionPermissions& permissions);
    static InputContext buildInputContext(FrontendRuntimeState state,
                                          FrontendDisplayBindings& bindings,
                                          const InteractionPermissions& permissions,
                                          bool useConcretePendingState);
    static void updateMultiplayerPresentation(UIManager& uiManager,
                                              const FrontendRuntimeState& state,
                                              int multiplayerPort,
                                              const std::function<void()>& onReturnToMainMenu);
};