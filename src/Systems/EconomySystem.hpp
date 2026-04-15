#pragma once

#include <vector>

#include "Kingdom/KingdomId.hpp"

class Kingdom;
class Board;
class Building;
class GameConfig;
class EventLog;

struct ResourceIncomeBreakdown {
    bool isResourceBuilding = false;
    int incomePerCell = 0;
    int whiteOccupiedCells = 0;
    int blackOccupiedCells = 0;
    int whiteIncome = 0;
    int blackIncome = 0;

    int occupiedCellsFor(KingdomId kingdom) const {
        return kingdom == KingdomId::White ? whiteOccupiedCells : blackOccupiedCells;
    }

    int incomeFor(KingdomId kingdom) const {
        return kingdom == KingdomId::White ? whiteIncome : blackIncome;
    }
};

class EconomySystem {
public:
    static ResourceIncomeBreakdown calculateResourceIncomeFromOccupation(int whiteOccupiedCells,
                                                                        int blackOccupiedCells,
                                                                        int incomePerCell);
    static ResourceIncomeBreakdown calculateResourceIncomeBreakdown(const Building& building,
                                                                    const Board& board,
                                                                    const GameConfig& config);
    static int calculateProjectedIncome(const Kingdom& kingdom, const Board& board,
                                        const std::vector<Building>& publicBuildings,
                                        const GameConfig& config);
    static void collectIncome(Kingdom& kingdom, const Board& board,
                               const std::vector<Building>& publicBuildings,
                               const GameConfig& config, EventLog& log, int turnNumber);
};
