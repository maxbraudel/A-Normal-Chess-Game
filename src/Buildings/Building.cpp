#include "Buildings/Building.hpp"
#include "Board/Board.hpp"
#include "Board/Cell.hpp"

namespace {

constexpr int kFlipHorizontalMask = 1;
constexpr int kFlipVerticalMask = 2;

int normalizeRotation(int rotationQuarterTurns) {
    if (rotationQuarterTurns < 0) {
        return 0;
    }

    return rotationQuarterTurns % 4;
}

int normalizeFlipMask(int flipMask) {
    if (flipMask < 0) {
        return 0;
    }

    return flipMask & (kFlipHorizontalMask | kFlipVerticalMask);
}

} // namespace

Building::Building()
    : id(-1), type(BuildingType::Barracks), owner(KingdomId::White), isNeutral(false),
      origin(0, 0), width(0), height(0), rotationQuarterTurns(0), flipMask(0),
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

int Building::getFootprintWidth() const {
    return getFootprintWidthFor(width, height, rotationQuarterTurns);
}

int Building::getFootprintHeight() const {
    return getFootprintHeightFor(width, height, rotationQuarterTurns);
}

bool Building::hasHorizontalFlip() const {
    return (normalizeFlipMask(flipMask) & kFlipHorizontalMask) != 0;
}

bool Building::hasVerticalFlip() const {
    return (normalizeFlipMask(flipMask) & kFlipVerticalMask) != 0;
}

sf::Vector2i Building::mapFootprintToSourceLocal(int localX, int localY) const {
    return mapFootprintToSourceLocalFor(localX, localY, width, height, rotationQuarterTurns, flipMask);
}

int Building::getFootprintWidthFor(int baseWidth, int baseHeight, int rotationQuarterTurns) {
    const int normalizedRotation = normalizeRotation(rotationQuarterTurns);
    return (normalizedRotation % 2 == 0) ? baseWidth : baseHeight;
}

int Building::getFootprintHeightFor(int baseWidth, int baseHeight, int rotationQuarterTurns) {
    const int normalizedRotation = normalizeRotation(rotationQuarterTurns);
    return (normalizedRotation % 2 == 0) ? baseHeight : baseWidth;
}

sf::Vector2i Building::mapFootprintToSourceLocalFor(int localX, int localY,
                                                    int baseWidth, int baseHeight,
                                                    int rotationQuarterTurns,
                                                    int flipMask) {
    const int normalizedRotation = normalizeRotation(rotationQuarterTurns);
    const int footprintWidth = getFootprintWidthFor(baseWidth, baseHeight, normalizedRotation);
    const int footprintHeight = getFootprintHeightFor(baseWidth, baseHeight, normalizedRotation);
    if (localX < 0 || localY < 0 || localX >= footprintWidth || localY >= footprintHeight) {
        return {-1, -1};
    }

    int sourceX = 0;
    int sourceY = 0;
    switch (normalizedRotation) {
        case 0:
            sourceX = localX;
            sourceY = localY;
            break;
        case 1:
            sourceX = localY;
            sourceY = baseHeight - 1 - localX;
            break;
        case 2:
            sourceX = baseWidth - 1 - localX;
            sourceY = baseHeight - 1 - localY;
            break;
        case 3:
            sourceX = baseWidth - 1 - localY;
            sourceY = localX;
            break;
        default:
            break;
    }

    const int normalizedFlipMask = normalizeFlipMask(flipMask);
    if ((normalizedFlipMask & kFlipHorizontalMask) != 0) {
        sourceX = baseWidth - 1 - sourceX;
    }
    if ((normalizedFlipMask & kFlipVerticalMask) != 0) {
        sourceY = baseHeight - 1 - sourceY;
    }

    return {sourceX, sourceY};
}

bool Building::isCellDamaged(int localX, int localY) const {
    const sf::Vector2i sourceLocal = mapFootprintToSourceLocal(localX, localY);
    if (sourceLocal.x < 0 || sourceLocal.y < 0) return false;
    int idx = sourceLocal.y * width + sourceLocal.x;
    if (idx < 0 || idx >= static_cast<int>(cellHP.size())) return false;
    int maxHP = cellHP.empty() ? 0 : cellHP[0];
    return cellHP[idx] < maxHP;
}

int Building::getCellHP(int localX, int localY) const {
    const sf::Vector2i sourceLocal = mapFootprintToSourceLocal(localX, localY);
    if (sourceLocal.x < 0 || sourceLocal.y < 0) return 0;
    int idx = sourceLocal.y * width + sourceLocal.x;
    if (idx < 0 || idx >= static_cast<int>(cellHP.size())) return 0;
    return cellHP[idx];
}

void Building::damageCellAt(int localX, int localY) {
    const sf::Vector2i sourceLocal = mapFootprintToSourceLocal(localX, localY);
    if (sourceLocal.x < 0 || sourceLocal.y < 0) return;
    int idx = sourceLocal.y * width + sourceLocal.x;
    if (idx >= 0 && idx < static_cast<int>(cellHP.size()) && cellHP[idx] > 0)
        cellHP[idx]--;
}

std::vector<sf::Vector2i> Building::getOccupiedCells() const {
    std::vector<sf::Vector2i> cells;
    const int footprintWidth = getFootprintWidth();
    const int footprintHeight = getFootprintHeight();
    for (int dy = 0; dy < footprintHeight; ++dy)
        for (int dx = 0; dx < footprintWidth; ++dx)
            cells.push_back({origin.x + dx, origin.y + dy});
    return cells;
}

std::vector<sf::Vector2i> Building::getAdjacentCells(const Board& board) const {
    std::vector<sf::Vector2i> adjacent;
    const int footprintWidth = getFootprintWidth();
    const int footprintHeight = getFootprintHeight();
    for (int dx = -1; dx <= footprintWidth; ++dx) {
        for (int dy = -1; dy <= footprintHeight; ++dy) {
            if (dx >= 0 && dx < footprintWidth && dy >= 0 && dy < footprintHeight) continue;
            int x = origin.x + dx;
            int y = origin.y + dy;
            if (board.isInBounds(x, y) && board.getCell(x, y).isInCircle)
                adjacent.push_back({x, y});
        }
    }
    return adjacent;
}

bool Building::containsCell(int x, int y) const {
    const int footprintWidth = getFootprintWidth();
    const int footprintHeight = getFootprintHeight();
    return x >= origin.x && x < origin.x + footprintWidth &&
           y >= origin.y && y < origin.y + footprintHeight;
}
