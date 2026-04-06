#include "Systems/BuildSystem.hpp"
#include "Units/Piece.hpp"
#include "Board/Board.hpp"
#include "Board/Cell.hpp"
#include "Kingdom/Kingdom.hpp"
#include "Buildings/Building.hpp"
#include "Config/GameConfig.hpp"

bool BuildSystem::canBuild(BuildingType type, sf::Vector2i origin,
                            const Piece& king, const Board& board,
                            const Kingdom& kingdom, const GameConfig& config) {
    int w = config.getBuildingWidth(type);
    int h = config.getBuildingHeight(type);

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

    // Check king adjacency: king must be adjacent to at least one cell of the building area
    bool kingAdjacent = false;
    for (int dy = 0; dy < h && !kingAdjacent; ++dy) {
        for (int dx = 0; dx < w && !kingAdjacent; ++dx) {
            int bx = origin.x + dx;
            int by = origin.y + dy;
            int diffX = std::abs(king.position.x - bx);
            int diffY = std::abs(king.position.y - by);
            if (diffX <= 1 && diffY <= 1 && (diffX + diffY > 0)) {
                kingAdjacent = true;
            }
        }
    }

    return kingAdjacent;
}

Building BuildSystem::place(BuildingType type, sf::Vector2i origin,
                             KingdomId owner, Board& board, const GameConfig& config) {
    Building b;
    b.type = type;
    b.owner = owner;
    b.isNeutral = false;
    b.origin = origin;
    b.width = config.getBuildingWidth(type);
    b.height = config.getBuildingHeight(type);
    b.isProducing = false;
    b.producingType = 0;
    b.turnsRemaining = 0;

    int hp = 1;
    if (type == BuildingType::StoneWall) hp = config.getStoneWallHP();
    else if (type == BuildingType::WoodWall) hp = config.getWoodWallHP();
    else if (type == BuildingType::Barracks) hp = config.getBarracksCellHP();
    b.cellHP.assign(b.width * b.height, hp);

    return b;
}
