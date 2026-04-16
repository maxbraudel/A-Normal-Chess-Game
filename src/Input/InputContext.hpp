#pragma once

#include <SFML/Graphics/RenderWindow.hpp>

#include <vector>

#include "Core/InteractionPermissions.hpp"
#include "Systems/TurnValidationContext.hpp"

class Camera;
class Board;
class TurnSystem;
class UIManager;
class GameConfig;
class Kingdom;
class Building;
class BuildingFactory;

struct InputContext {
    sf::RenderWindow& window;
    Camera& camera;
    Board& board;
    TurnSystem& turnSystem;
    BuildingFactory& buildingFactory;
    Kingdom& controlledKingdom;
    Kingdom& opposingKingdom;
    const std::vector<Building>& publicBuildings;
    Board& authoritativeBoard;
    Kingdom& authoritativeControlledKingdom;
    Kingdom& authoritativeOpposingKingdom;
    const std::vector<Building>& authoritativePublicBuildings;
    TurnValidationContext authoritativeTurnContext;
    UIManager& uiManager;
    const GameConfig& config;
    InteractionPermissions permissions;
    bool materializePendingStateLocally = false;
    bool useConcretePendingState = false;
};