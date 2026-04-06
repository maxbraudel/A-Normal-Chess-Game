#include "Systems/EconomySystem.hpp"
#include "Kingdom/Kingdom.hpp"
#include "Board/Board.hpp"
#include "Board/Cell.hpp"
#include "Buildings/Building.hpp"
#include "Buildings/BuildingType.hpp"
#include "Config/GameConfig.hpp"
#include "Systems/EventLog.hpp"

void EconomySystem::collectIncome(Kingdom& kingdom, const Board& board,
                                    const std::vector<Building>& publicBuildings,
                                    const GameConfig& config, EventLog& log, int turnNumber) {
    int totalIncome = 0;

    for (const auto& building : publicBuildings) {
        if (building.type != BuildingType::Mine && building.type != BuildingType::Farm) continue;

        int incomePerCell = 0;
        if (building.type == BuildingType::Mine)
            incomePerCell = config.getMineIncomePerCellPerTurn();
        else if (building.type == BuildingType::Farm)
            incomePerCell = config.getFarmIncomePerCellPerTurn();

        // Check if contested: both kingdoms have pieces on this building
        bool friendlyPresent = false;
        bool enemyPresent = false;
        int friendlyCells = 0;

        for (auto& pos : building.getOccupiedCells()) {
            const Cell& cell = board.getCell(pos.x, pos.y);
            if (cell.piece) {
                if (cell.piece->kingdom == kingdom.id) {
                    friendlyPresent = true;
                    ++friendlyCells;
                } else {
                    enemyPresent = true;
                }
            }
        }

        // Contested = no income
        if (friendlyPresent && enemyPresent) continue;
        if (!friendlyPresent) continue;

        int income = friendlyCells * incomePerCell;
        totalIncome += income;
    }

    if (totalIncome > 0) {
        kingdom.gold += totalIncome;
        log.log(turnNumber, kingdom.id, "Income: +" + std::to_string(totalIncome) + " gold");
    }
}
