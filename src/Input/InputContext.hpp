#pragma once

#include <SFML/Graphics/RenderWindow.hpp>

#include <vector>

#include "Core/InteractionPermissions.hpp"
#include "Kingdom/KingdomId.hpp"
#include "Telemetry/BehavioralTelemetryTypes.hpp"
#include "Systems/WeatherTypes.hpp"
#include "Systems/TurnValidationContext.hpp"

class Camera;
class Board;
class TurnSystem;
class UIManager;
class GameConfig;
class Kingdom;
class Building;
class BuildingFactory;
class CheckEscapeSolver;
class AsyncSelectionMoveService;
class BehavioralTelemetryCollector;

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
    BehavioralTelemetryCollector* behavioralTelemetry = nullptr;
    BehavioralTelemetryOrigin behavioralTelemetryOrigin = BehavioralTelemetryOrigin::LocalHost;
    CheckEscapeSolver* sharedCheckEscapeSolver = nullptr;
    AsyncSelectionMoveService* asyncSelectionMoveService = nullptr;
    const WeatherMaskCache* weatherMaskCache = nullptr;
    KingdomId localPerspectiveKingdom = KingdomId::White;
    InteractionPermissions permissions;
    bool materializePendingStateLocally = false;
    bool useConcretePendingState = false;
};