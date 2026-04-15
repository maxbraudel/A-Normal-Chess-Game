#include "Systems/EconomySystem.hpp"
#include "Kingdom/Kingdom.hpp"
#include "Board/Board.hpp"
#include "Board/Cell.hpp"
#include "Buildings/Building.hpp"
#include "Buildings/BuildingType.hpp"
#include "Config/GameConfig.hpp"
#include "Systems/EventLog.hpp"

int EconomySystem::calculateProjectedIncome(const Kingdom& kingdom, const Board& board,
                                            const std::vector<Building>& publicBuildings,
                                            const GameConfig& config) {
    int totalIncome = 0;

    for (const auto& building : publicBuildings) {
        if (building.type != BuildingType::Mine && building.type != BuildingType::Farm) {
            continue;
        }

        const int incomePerCell = (building.type == BuildingType::Mine)
            ? config.getMineIncomePerCellPerTurn()
            : config.getFarmIncomePerCellPerTurn();

        bool friendlyPresent = false;
        bool enemyPresent = false;
        int friendlyCells = 0;

        for (const auto& pos : building.getOccupiedCells()) {
            const Cell& cell = board.getCell(pos.x, pos.y);
            if (!cell.piece) {
                continue;
            }

            if (cell.piece->kingdom == kingdom.id) {
                friendlyPresent = true;
                ++friendlyCells;
            } else {
                enemyPresent = true;
            }
        }

        if (friendlyPresent && enemyPresent) {
            continue;
        }
        if (!friendlyPresent) {
            continue;
        }

        totalIncome += friendlyCells * incomePerCell;
    }

    return totalIncome;
}

void EconomySystem::collectIncome(Kingdom& kingdom, const Board& board,
                                    const std::vector<Building>& publicBuildings,
                                    const GameConfig& config, EventLog& log, int turnNumber) {
    const int totalIncome = calculateProjectedIncome(kingdom, board, publicBuildings, config);

    if (totalIncome > 0) {
        kingdom.gold += totalIncome;
        log.log(turnNumber, kingdom.id, "Income: +" + std::to_string(totalIncome) + " gold");
    }
}
