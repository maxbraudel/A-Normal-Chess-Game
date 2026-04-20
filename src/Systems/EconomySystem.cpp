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
#include <optional>

namespace {

std::optional<ResourceIncomeProfile> incomeProfileForResource(BuildingType type, const GameConfig& config) {
    switch (type) {
        case BuildingType::Mine:
            return config.getMineIncomeProfile();
        case BuildingType::Farm:
            return config.getFarmIncomeProfile();
        default:
            return std::nullopt;
    }
}

int calculateControlledIncome(int controlledCells, const ResourceIncomeProfile& incomeProfile) {
    const int clampedControlledCells = std::max(0, controlledCells);
    int totalIncome = 0;
    for (int cellIndex = 0; cellIndex < clampedControlledCells; ++cellIndex) {
        totalIncome += std::max(
            incomeProfile.firstCellIncomePerTurn - (cellIndex * incomeProfile.additionalCellDecrement),
            incomeProfile.minimumCellIncomePerTurn);
    }

    return totalIncome;
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
                                                                             const ResourceIncomeProfile& incomeProfile) {
    ResourceIncomeBreakdown breakdown;
    breakdown.isResourceBuilding = true;
    breakdown.whiteOccupiedCells = std::max(0, whiteOccupiedCells);
    breakdown.blackOccupiedCells = std::max(0, blackOccupiedCells);
    breakdown.whiteIncome = calculateControlledIncome(
        breakdown.whiteOccupiedCells - breakdown.blackOccupiedCells,
        incomeProfile);
    breakdown.blackIncome = calculateControlledIncome(
        breakdown.blackOccupiedCells - breakdown.whiteOccupiedCells,
        incomeProfile);
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

    const std::optional<ResourceIncomeProfile> incomeProfile = incomeProfileForResource(building.type, config);
    if (!incomeProfile.has_value()) {
        return {};
    }

    return calculateResourceIncomeFromOccupation(
        whiteOccupiedCells,
        blackOccupiedCells,
        incomeProfile.value());
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

        const std::optional<ResourceIncomeProfile> incomeProfile = incomeProfileForResource(building.type, config);
        if (!incomeProfile.has_value()) {
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
            incomeProfile.value());
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
