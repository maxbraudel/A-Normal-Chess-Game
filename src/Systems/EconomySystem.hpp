#pragma once

class Kingdom;
class Board;
class Building;
class GameConfig;
class EventLog;
#include <vector>

class EconomySystem {
public:
    static void collectIncome(Kingdom& kingdom, const Board& board,
                               const std::vector<Building>& publicBuildings,
                               const GameConfig& config, EventLog& log, int turnNumber);
};
