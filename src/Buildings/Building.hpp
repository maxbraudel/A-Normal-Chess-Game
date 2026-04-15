#pragma once
#include <vector>
#include <SFML/System/Vector2.hpp>
#include "Buildings/BuildingType.hpp"
#include "Kingdom/KingdomId.hpp"

class Board;

class Building {
public:
    int id;
    BuildingType type;
    KingdomId owner;
    bool isNeutral;
    sf::Vector2i origin;
    int width;
    int height;
    int rotationQuarterTurns;
    int flipMask;
    std::vector<int> cellHP;
    std::vector<int> cellBreachState;

    bool isProducing;
    int producingType; // PieceType cast
    int turnsRemaining;

    Building();

    bool isPublic() const;
    bool isDestroyed() const;
    int getFootprintWidth() const;
    int getFootprintHeight() const;
    bool hasHorizontalFlip() const;
    bool hasVerticalFlip() const;
    sf::Vector2i mapFootprintToSourceLocal(int localX, int localY) const;
    bool isWall() const;
    bool isCellDestroyed(int localX, int localY) const;
    bool isCellBreached(int localX, int localY) const;
    void setCellBreached(int localX, int localY, bool breached);
    bool isCellDamaged(int localX, int localY) const;
    int getCellHP(int localX, int localY) const;
    void setCellHP(int localX, int localY, int hp);
    void destroyCellAt(int localX, int localY);
    void repairCellAt(int localX, int localY, int hp);
    void damageCellAt(int localX, int localY);
    std::vector<sf::Vector2i> getOccupiedCells() const;
    std::vector<sf::Vector2i> getAdjacentCells(const Board& board) const;
    bool containsCell(int x, int y) const;

    static int getFootprintWidthFor(int baseWidth, int baseHeight, int rotationQuarterTurns);
    static int getFootprintHeightFor(int baseWidth, int baseHeight, int rotationQuarterTurns);
    static sf::Vector2i mapFootprintToSourceLocalFor(int localX, int localY,
                                                     int baseWidth, int baseHeight,
                                                     int rotationQuarterTurns,
                                                     int flipMask);
};
