#include "Buildings/Building.hpp"
#include "Board/Board.hpp"
#include "Board/Cell.hpp"

Building::Building()
    : id(-1), type(BuildingType::Barracks), owner(KingdomId::White), isNeutral(false),
      origin(0, 0), width(0), height(0),
      isProducing(false), producingType(0), turnsRemaining(0) {}

bool Building::isPublic() const {
    return type == BuildingType::Church || type == BuildingType::Mine || type == BuildingType::Farm;
}

bool Building::isDestroyed() const {
    if (isPublic()) return false;
    for (int hp : cellHP) {
        if (hp > 0) return false;
    }
    return true;
}

bool Building::isCellDamaged(int localX, int localY) const {
    int idx = localY * width + localX;
    if (idx < 0 || idx >= static_cast<int>(cellHP.size())) return false;
    int maxHP = cellHP.empty() ? 0 : cellHP[0];
    return cellHP[idx] < maxHP;
}

int Building::getCellHP(int localX, int localY) const {
    int idx = localY * width + localX;
    if (idx < 0 || idx >= static_cast<int>(cellHP.size())) return 0;
    return cellHP[idx];
}

void Building::damageCellAt(int localX, int localY) {
    int idx = localY * width + localX;
    if (idx >= 0 && idx < static_cast<int>(cellHP.size()) && cellHP[idx] > 0)
        cellHP[idx]--;
}

std::vector<sf::Vector2i> Building::getOccupiedCells() const {
    std::vector<sf::Vector2i> cells;
    for (int dy = 0; dy < height; ++dy)
        for (int dx = 0; dx < width; ++dx)
            cells.push_back({origin.x + dx, origin.y + dy});
    return cells;
}

std::vector<sf::Vector2i> Building::getAdjacentCells(const Board& board) const {
    std::vector<sf::Vector2i> adjacent;
    for (int dx = -1; dx <= width; ++dx) {
        for (int dy = -1; dy <= height; ++dy) {
            if (dx >= 0 && dx < width && dy >= 0 && dy < height) continue;
            int x = origin.x + dx;
            int y = origin.y + dy;
            if (board.isInBounds(x, y) && board.getCell(x, y).isInCircle)
                adjacent.push_back({x, y});
        }
    }
    return adjacent;
}

bool Building::containsCell(int x, int y) const {
    return x >= origin.x && x < origin.x + width &&
           y >= origin.y && y < origin.y + height;
}
