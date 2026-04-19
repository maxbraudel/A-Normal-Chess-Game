#pragma once

#include <array>
#include <functional>
#include <optional>
#include <vector>

#include "Runtime/FrontendCoordinator.hpp"
#include "UI/GameMenuUI.hpp"

class Board;
class Building;
class GameConfig;
class GameEngine;
class Kingdom;
class MapObject;
struct AutonomousUnit;
class UIManager;
class TurnSystem;

struct InGameMenuOpenPlan {
    bool shouldOpen = false;
    std::optional<GameState> nextGameState;
    GameMenuPresentation presentation;
};

struct InGameMenuClosePlan {
    bool shouldClose = false;
    std::optional<GameState> nextGameState;
};

struct InGamePresentationState {
    FrontendRuntimeState runtimeState;
    bool usingProjectedDisplayState = false;
    ToolState activeTool = ToolState::Select;
    bool overviewVisible = false;
    const Piece* selectedPiece = nullptr;
    const AutonomousUnit* selectedAutonomousUnit = nullptr;
    const Building* selectedBuilding = nullptr;
    const MapObject* selectedMapObject = nullptr;
    const Cell* selectedCell = nullptr;
};

struct InGamePresentationBindings {
    const GameEngine& engine;
    const Board& displayedBoard;
    const std::array<Kingdom, kNumKingdoms>& displayedKingdoms;
    const std::vector<Building>& displayedPublicBuildings;
    const TurnSystem& turnSystem;
    const GameConfig& config;
};

class InGamePresentationCoordinator {
public:
    static GameMenuPresentation buildGameMenuPresentation(const FrontendRuntimeState& state);
    static InGameMenuOpenPlan planOpenInGameMenu(const FrontendRuntimeState& state);
    static InGameMenuClosePlan planCloseInGameMenu(const FrontendRuntimeState& state);

    static void updateInGameUi(UIManager& uiManager,
                               const InGamePresentationState& state,
                               const InGamePresentationBindings& bindings,
                               const InteractionPermissions& permissions,
                               const CheckTurnValidation& validation,
                               const InGameHudPresentation& hudPresentation,
                               const std::function<void()>& onReturnToMainMenu);

private:
    static void applyLeftPanelPresentation(UIManager& uiManager,
                                           const FrontendLeftPanelPresentation& presentation,
                                           const std::array<Kingdom, kNumKingdoms>& displayedKingdoms,
                                           const GameConfig& config);
};