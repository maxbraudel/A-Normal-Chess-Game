#include "Buildings/Building.hpp"
#include "Board/Board.hpp"
#include "Board/Cell.hpp"

#include <algorithm>

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

int getCellIndex(const Building& building, int localX, int localY) {
    const sf::Vector2i sourceLocal = building.mapFootprintToSourceLocal(localX, localY);
    if (sourceLocal.x < 0 || sourceLocal.y < 0) {
        return -1;
    }

    const int index = sourceLocal.y * building.width + sourceLocal.x;
    if (index < 0 || index >= static_cast<int>(building.cellHP.size())) {
        return -1;
    }

    return index;
}

} // namespace

Building::Building()
    : id(-1), type(BuildingType::Barracks), owner(KingdomId::White), isNeutral(false),
    origin(0, 0), width(0), height(0), rotationQuarterTurns(0), flipMask(0),
    state(BuildingState::Completed), destroyedCellsRequired(1),
      isProducing(false), producingType(0), turnsRemaining(0) {}

bool Building::isPublic() const {
    return type == BuildingType::Church || type == BuildingType::Mine || type == BuildingType::Farm;
}

bool Building::isDestroyed() const {
    if (isPublic()) return false;
    return destroyedCellCount() >= effectiveDestroyedCellsRequired();
}

bool Building::isUnderConstruction() const {
    return state == BuildingState::UnderConstruction;
}

bool Building::isUsable() const {
    return !isUnderConstruction() && !isDestroyed();
}

bool Building::hasActiveGameplayEffects() const {
    return !isUnderConstruction() && !isDestroyed();
}

int Building::destroyedCellCount() const {
    return static_cast<int>(std::count_if(cellHP.begin(), cellHP.end(), [](int hp) {
        return hp <= 0;
    }));
}

int Building::effectiveDestroyedCellsRequired() const {
    const int maxDestroyedCells = std::max(1, static_cast<int>(cellHP.size()));
    return std::clamp(destroyedCellsRequired, 1, maxDestroyedCells);
}

void Building::setDestroyedCellsRequired(int required) {
    const int maxDestroyedCells = std::max(1, static_cast<int>(cellHP.size()));
    destroyedCellsRequired = std::clamp(required, 1, maxDestroyedCells);
}

void Building::setConstructionState(BuildingState newState) {
    state = newState;
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

bool Building::isWall() const {
    return type == BuildingType::WoodWall || type == BuildingType::StoneWall;
}

sf::Vector2i Building::mapFootprintToSourceLocal(int localX, int localY) const {
    return mapFootprintToSourceLocalFor(localX, localY, width, height, rotationQuarterTurns, flipMask);
}

bool Building::isCellDestroyed(int localX, int localY) const {
    return getCellHP(localX, localY) <= 0;
}

bool Building::isCellBreached(int localX, int localY) const {
    if (type != BuildingType::StoneWall) {
        return false;
    }

    const int index = getCellIndex(*this, localX, localY);
    if (index < 0 || index >= static_cast<int>(cellBreachState.size())) {
        return false;
    }

    return cellBreachState[index] != 0 && !isCellDestroyed(localX, localY);
}

void Building::setCellBreached(int localX, int localY, bool breached) {
    const int index = getCellIndex(*this, localX, localY);
    if (index < 0) {
        return;
    }

    if (cellBreachState.size() < cellHP.size()) {
        cellBreachState.resize(cellHP.size(), 0);
    }

    cellBreachState[index] = breached ? 1 : 0;
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
    const int index = getCellIndex(*this, localX, localY);
    if (index < 0) return false;
    const int maxHP = cellHP.empty() ? 0 : cellHP[0];
    return cellHP[index] < maxHP || isCellBreached(localX, localY);
}

int Building::getCellHP(int localX, int localY) const {
    const int index = getCellIndex(*this, localX, localY);
    if (index < 0) return 0;
    return cellHP[index];
}

void Building::setCellHP(int localX, int localY, int hp) {
    const int index = getCellIndex(*this, localX, localY);
    if (index < 0) {
        return;
    }

    cellHP[index] = std::max(0, hp);
    if (cellBreachState.size() < cellHP.size()) {
        cellBreachState.resize(cellHP.size(), 0);
    }
    if (cellHP[index] <= 0) {
        cellBreachState[index] = 0;
    }
}

void Building::destroyCellAt(int localX, int localY) {
    setCellHP(localX, localY, 0);
    setCellBreached(localX, localY, false);
}

void Building::repairCellAt(int localX, int localY, int hp) {
    setCellHP(localX, localY, hp);
    setCellBreached(localX, localY, false);
}

void Building::damageCellAt(int localX, int localY) {
    const int index = getCellIndex(*this, localX, localY);
    if (index < 0 || cellHP[index] <= 0) {
        return;
    }

    cellHP[index]--;
    if (cellHP[index] <= 0 && index < static_cast<int>(cellBreachState.size())) {
        cellBreachState[index] = 0;
    }
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
