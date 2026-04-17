#include "Systems/EconomySystem.hpp"
#include "Kingdom/Kingdom.hpp"
#include "Board/Board.hpp"
#include "Board/Cell.hpp"
#include "Buildings/Building.hpp"
#include "Buildings/BuildingType.hpp"
#include "Config/GameConfig.hpp"
#include "Projection/GameSnapshot.hpp"
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

template <typename PieceRange>
int calculateUpkeepForPieces(const PieceRange& pieces, const GameConfig& config) {
    int totalUpkeep = 0;
    for (const auto& piece : pieces) {
        totalUpkeep += config.getPieceUpkeepCost(piece.type);
    }

    return totalUpkeep;
}

TurnEconomyBreakdown buildTurnEconomyBreakdown(int currentGold, int grossIncome, int upkeepCost) {
    TurnEconomyBreakdown breakdown;
    breakdown.currentGold = currentGold;
    breakdown.grossIncome = grossIncome;
    breakdown.upkeepCost = upkeepCost;
    breakdown.netIncome = grossIncome - upkeepCost;
    breakdown.endingGold = currentGold + breakdown.netIncome;
    return breakdown;
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

    if (!building.hasActiveGameplayEffects()) {
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

int EconomySystem::calculateProjectedGrossIncome(const Kingdom& kingdom, const Board& board,
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

int EconomySystem::calculateProjectedIncome(const Kingdom& kingdom, const Board& board,
                                            const std::vector<Building>& publicBuildings,
                                            const GameConfig& config) {
    return calculateProjectedGrossIncome(kingdom, board, publicBuildings, config);
}

int EconomySystem::calculateProjectedUpkeep(const Kingdom& kingdom, const GameConfig& config) {
    return calculateUpkeepForPieces(kingdom.pieces, config);
}

int EconomySystem::calculateProjectedNetIncome(const Kingdom& kingdom, const Board& board,
                                               const std::vector<Building>& publicBuildings,
                                               const GameConfig& config) {
    return calculateProjectedGrossIncome(kingdom, board, publicBuildings, config)
        - calculateProjectedUpkeep(kingdom, config);
}

TurnEconomyBreakdown EconomySystem::calculateTurnEconomy(const Kingdom& kingdom,
                                                         const Board& board,
                                                         const std::vector<Building>& publicBuildings,
                                                         const GameConfig& config) {
    return buildTurnEconomyBreakdown(
        kingdom.gold,
        calculateProjectedGrossIncome(kingdom, board, publicBuildings, config),
        calculateProjectedUpkeep(kingdom, config));
}

TurnEconomyBreakdown EconomySystem::calculateTurnEconomy(const GameSnapshot& snapshot,
                                                         KingdomId kingdomId,
                                                         const GameConfig& config) {
    const SnapKingdom& kingdom = snapshot.kingdom(kingdomId);
    int grossIncome = 0;

    for (const auto& building : snapshot.publicBuildings) {
        if (!building.hasActiveGameplayEffects()) {
            continue;
        }

        const int incomePerCell = incomePerCellForResource(building.type, config);
        if (incomePerCell <= 0) {
            continue;
        }

        int whiteOccupiedCells = 0;
        int blackOccupiedCells = 0;
        for (const sf::Vector2i& pos : building.getOccupiedCells()) {
            if (const SnapPiece* piece = snapshot.pieceAt(pos)) {
                if (piece->kingdom == KingdomId::White) {
                    ++whiteOccupiedCells;
                } else {
                    ++blackOccupiedCells;
                }
            }
        }

        const ResourceIncomeBreakdown breakdown = calculateResourceIncomeFromOccupation(
            whiteOccupiedCells,
            blackOccupiedCells,
            incomePerCell);
        grossIncome += breakdown.incomeFor(kingdomId);
    }

    return buildTurnEconomyBreakdown(
        kingdom.gold,
        grossIncome,
        calculateUpkeepForPieces(kingdom.pieces, config));
}

void EconomySystem::collectIncome(Kingdom& kingdom, const Board& board,
                                    const std::vector<Building>& publicBuildings,
                                    const GameConfig& config, EventLog& log, int turnNumber) {
    const TurnEconomyBreakdown breakdown = calculateTurnEconomy(
        kingdom,
        board,
        publicBuildings,
        config);

    if (breakdown.grossIncome > 0) {
        log.log(turnNumber, kingdom.id,
                "Income: +" + std::to_string(breakdown.grossIncome) + " gold");
    }
    if (breakdown.upkeepCost > 0) {
        log.log(turnNumber, kingdom.id,
                "Upkeep: -" + std::to_string(breakdown.upkeepCost) + " gold");
    }

    kingdom.gold = std::max(0, breakdown.endingGold);
}
