#pragma once

#include <cstdint>
#include <vector>

#include "Systems/XPTypes.hpp"

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
    std::uint32_t worldSeed = 0;
    XPSystemState xpSystemState{};
};