#include "Systems/ProductionSystem.hpp"
#include "Buildings/Building.hpp"
#include "Buildings/BuildingType.hpp"
#include "Board/Board.hpp"
#include "Board/Cell.hpp"
#include "Kingdom/Kingdom.hpp"
#include "Config/GameConfig.hpp"
#include "Systems/ProductionSpawnRules.hpp"

bool ProductionSystem::canStartProduction(const Building& barracks, PieceType type,
                                           const Kingdom& kingdom, const GameConfig& config) {
    if (barracks.type != BuildingType::Barracks) return false;
    if (!barracks.isUsable()) return false;
    if (barracks.isProducing) return false;
    int cost = config.getRecruitCost(type);
    return kingdom.gold >= cost;
}

void ProductionSystem::startProduction(Building& barracks, PieceType type, const GameConfig& config) {
    barracks.isProducing = true;
    barracks.producingType = static_cast<int>(type);
    barracks.turnsRemaining = config.getProductionTurns(type);
}

void ProductionSystem::advanceProduction(Building& barracks) {
    if (barracks.isProducing && barracks.turnsRemaining > 0) {
        --barracks.turnsRemaining;
    }
}

bool ProductionSystem::isProductionComplete(const Building& barracks) {
    return barracks.isProducing && barracks.turnsRemaining <= 0;
}

sf::Vector2i ProductionSystem::findSpawnCell(const Building& barracks,
                                             const Board& board,
                                             PieceType type,
                                             const Kingdom& kingdom) {
    const std::optional<int> preferredParity = (type == PieceType::Bishop)
        ? kingdom.preferredNextBishopSpawnParity()
        : std::nullopt;

    return ProductionSpawnRules::findSpawnCell(
        barracks.origin,
        barracks.getFootprintWidth(),
        barracks.getFootprintHeight(),
        board.getDiameter(),
        [&board](const sf::Vector2i& pos) {
            const Cell& cell = board.getCell(pos.x, pos.y);
            return cell.isInCircle && cell.type != CellType::Water && !cell.piece && !cell.building;
        },
        preferredParity);
}
