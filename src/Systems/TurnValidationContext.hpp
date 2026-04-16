#pragma once

#include <vector>

class Board;
class Building;
class GameConfig;
class Kingdom;

struct TurnValidationContext {
    const Board& board;
    const Kingdom& activeKingdom;
    const Kingdom& enemyKingdom;
    const std::vector<Building>& publicBuildings;
    int turnNumber;
    const GameConfig& config;
};