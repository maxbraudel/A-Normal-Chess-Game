#include "Systems/ProductionSystem.hpp"
#include "Buildings/Building.hpp"
#include "Buildings/BuildingType.hpp"
#include "Board/Board.hpp"
#include "Board/Cell.hpp"
#include "Kingdom/Kingdom.hpp"
#include "Config/GameConfig.hpp"

bool ProductionSystem::canStartProduction(const Building& barracks, PieceType type,
                                           const Kingdom& kingdom, const GameConfig& config) {
    if (barracks.type != BuildingType::Barracks) return false;
    if (barracks.isProducing) return false;
    if (barracks.isDestroyed()) return false;
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

sf::Vector2i ProductionSystem::findSpawnCell(const Building& barracks, const Board& board) {
    // Try adjacent cells first (radius 1), then expand to radius 2
    auto adjacent = barracks.getAdjacentCells(board);
    for (const auto& pos : adjacent) {
        const Cell& cell = board.getCell(pos.x, pos.y);
        if (cell.isInCircle && cell.type != CellType::Water && !cell.piece && !cell.building) {
            return pos;
        }
    }
    // Expand search: check cells at radius 2 from barracks center
    auto occupied = barracks.getOccupiedCells();
    if (!occupied.empty()) {
        sf::Vector2i center = occupied[0];
        for (int dy = -2; dy <= 2; ++dy) {
            for (int dx = -2; dx <= 2; ++dx) {
                if (std::abs(dx) <= 1 && std::abs(dy) <= 1) continue; // Already checked
                int nx = center.x + dx;
                int ny = center.y + dy;
                if (nx < 0 || ny < 0 || nx >= board.getDiameter() || ny >= board.getDiameter()) continue;
                const Cell& cell = board.getCell(nx, ny);
                if (cell.isInCircle && cell.type != CellType::Water && !cell.piece && !cell.building) {
                    return {nx, ny};
                }
            }
        }
    }
    return {-1, -1};
}
