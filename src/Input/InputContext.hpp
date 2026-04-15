#pragma once

#include <SFML/Graphics/RenderWindow.hpp>

#include <vector>

class Camera;
class Board;
class TurnSystem;
class UIManager;
class GameConfig;
class Kingdom;
class Building;

struct InputContext {
    sf::RenderWindow& window;
    Camera& camera;
    Board& board;
    TurnSystem& turnSystem;
    Kingdom& controlledKingdom;
    Kingdom& opposingKingdom;
    const std::vector<Building>& publicBuildings;
    UIManager& uiManager;
    const GameConfig& config;
    bool allowCommands = true;
    bool allowNonMoveActions = true;
};