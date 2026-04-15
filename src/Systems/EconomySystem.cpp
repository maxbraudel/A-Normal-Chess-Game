#include "Systems/EconomySystem.hpp"
#include "Kingdom/Kingdom.hpp"
#include "Board/Board.hpp"
#include "Board/Cell.hpp"
#include "Buildings/Building.hpp"
#include "Buildings/BuildingType.hpp"
#include "Config/GameConfig.hpp"
#include "Systems/EventLog.hpp"

#include <algorithm>

namespace {

int incomePerCellForResource(BuildingType type, const GameConfig& config) {
    switch (type) {
        case BuildingType::Mine:
            return config.getMineIncomePerCellPerTurn();
        case BuildingType::Farm:
            return config.getFarmIncomePerCellPerTurn();
        default:
            return 0;
    }
}

} // namespace

ResourceIncomeBreakdown EconomySystem::calculateResourceIncomeFromOccupation(int whiteOccupiedCells,
                                                                             int blackOccupiedCells,
                                                                             int incomePerCell) {
    ResourceIncomeBreakdown breakdown;
    breakdown.isResourceBuilding = true;
    breakdown.incomePerCell = incomePerCell;
    breakdown.whiteOccupiedCells = std::max(0, whiteOccupiedCells);
    breakdown.blackOccupiedCells = std::max(0, blackOccupiedCells);
    breakdown.whiteIncome = std::max(breakdown.whiteOccupiedCells - breakdown.blackOccupiedCells, 0) * incomePerCell;
    breakdown.blackIncome = std::max(breakdown.blackOccupiedCells - breakdown.whiteOccupiedCells, 0) * incomePerCell;
    return breakdown;
}

ResourceIncomeBreakdown EconomySystem::calculateResourceIncomeBreakdown(const Building& building,
                                                                        const Board& board,
                                                                        const GameConfig& config) {
    if (building.type != BuildingType::Mine && building.type != BuildingType::Farm) {
        return {};
    }

    int whiteOccupiedCells = 0;
    int blackOccupiedCells = 0;
    for (const sf::Vector2i& pos : building.getOccupiedCells()) {
        const Cell& cell = board.getCell(pos.x, pos.y);
        if (!cell.piece) {
            continue;
        }

        if (cell.piece->kingdom == KingdomId::White) {
            ++whiteOccupiedCells;
        } else {
            ++blackOccupiedCells;
        }
    }

    return calculateResourceIncomeFromOccupation(
        whiteOccupiedCells,
        blackOccupiedCells,
        incomePerCellForResource(building.type, config));
}

int EconomySystem::calculateProjectedIncome(const Kingdom& kingdom, const Board& board,
                                            const std::vector<Building>& publicBuildings,
                                            const GameConfig& config) {
    int totalIncome = 0;

    for (const auto& building : publicBuildings) {
        const ResourceIncomeBreakdown breakdown = calculateResourceIncomeBreakdown(building, board, config);
        if (!breakdown.isResourceBuilding) {
            continue;
        }

        totalIncome += breakdown.incomeFor(kingdom.id);
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
