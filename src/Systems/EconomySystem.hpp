#pragma once

#include <vector>

#include "Kingdom/KingdomId.hpp"

class Kingdom;
class Board;
class Building;
class GameConfig;
class EventLog;
struct GameSnapshot;

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

struct TurnEconomyBreakdown {
    int currentGold = 0;
    int grossIncome = 0;
    int upkeepCost = 0;
    int netIncome = 0;
    int endingGold = 0;

    bool wouldBeBankrupt() const {
        return endingGold < 0;
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
    static int calculateProjectedGrossIncome(const Kingdom& kingdom, const Board& board,
                                             const std::vector<Building>& publicBuildings,
                                             const GameConfig& config);
    static int calculateProjectedIncome(const Kingdom& kingdom, const Board& board,
                                        const std::vector<Building>& publicBuildings,
                                        const GameConfig& config);
    static int calculateProjectedUpkeep(const Kingdom& kingdom, const GameConfig& config);
    static int calculateProjectedNetIncome(const Kingdom& kingdom, const Board& board,
                                          const std::vector<Building>& publicBuildings,
                                          const GameConfig& config);
    static TurnEconomyBreakdown calculateTurnEconomy(const Kingdom& kingdom, const Board& board,
                                                     const std::vector<Building>& publicBuildings,
                                                     const GameConfig& config);
    static TurnEconomyBreakdown calculateTurnEconomy(const GameSnapshot& snapshot,
                                                     KingdomId kingdom,
                                                     const GameConfig& config);
    static void collectIncome(Kingdom& kingdom, const Board& board,
                               const std::vector<Building>& publicBuildings,
                               const GameConfig& config, EventLog& log, int turnNumber);
};
