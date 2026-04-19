#include "Systems/BuildSystem.hpp"
#include "Units/Piece.hpp"
#include "Board/Board.hpp"
#include "Board/Cell.hpp"
#include "Kingdom/Kingdom.hpp"
#include "Buildings/Building.hpp"
#include "Config/GameConfig.hpp"
#include "Systems/BuildReachRules.hpp"
#include "Systems/StructureIntegrityRules.hpp"

bool BuildSystem::canBuild(BuildingType type, sf::Vector2i origin,
                           const Board& board,
                           const Kingdom& kingdom, const GameConfig& config,
                           int rotationQuarterTurns) {
    const int baseWidth = config.getBuildingWidth(type);
    const int baseHeight = config.getBuildingHeight(type);
    const int w = Building::getFootprintWidthFor(baseWidth, baseHeight, rotationQuarterTurns);
    const int h = Building::getFootprintHeightFor(baseWidth, baseHeight, rotationQuarterTurns);

    // Check budget
    int cost = 0;
    switch (type) {
        case BuildingType::Barracks: cost = config.getBarracksCost(); break;
        case BuildingType::WoodWall: cost = config.getWoodWallCost(); break;
        case BuildingType::StoneWall: cost = config.getStoneWallCost(); break;
        case BuildingType::Arena: cost = config.getArenaCost(); break;
        default: return false;
    }
    if (kingdom.gold < cost) return false;

    // Check all cells free
    for (int dy = 0; dy < h; ++dy) {
        for (int dx = 0; dx < w; ++dx) {
            int x = origin.x + dx;
            int y = origin.y + dy;
            if (!board.isInBounds(x, y)) return false;
            const Cell& cell = board.getCell(x, y);
            if (!cell.isInCircle) return false;
            if (cell.type == CellType::Water) return false;
            if (cell.building) return false;
            if (cell.piece) return false;
        }
    }

    return footprintHasAdjacentBuilder(origin, w, h, collectBuilderPositions(kingdom.pieces));
}

Building BuildSystem::place(BuildingType type, sf::Vector2i origin,
                             KingdomId owner, Board& board, const GameConfig& config,
                             int rotationQuarterTurns) {
    Building b;
    b.type = type;
    b.owner = owner;
    b.isNeutral = false;
    b.origin = origin;
    b.width = config.getBuildingWidth(type);
    b.height = config.getBuildingHeight(type);
    b.rotationQuarterTurns = rotationQuarterTurns;
    b.flipMask = 0;
    b.isProducing = false;
    b.producingType = 0;
    b.turnsRemaining = 0;

    const int hp = StructureIntegrityRules::defaultCellHP(type, config);
    b.cellHP.assign(b.width * b.height, hp);
    b.cellBreachState.assign(b.width * b.height, 0);
    b.setDestroyedCellsRequired(StructureIntegrityRules::destroyedCellsRequired(type, config));

    return b;
}
