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
    std::vector<int> cellHP;

    bool isProducing;
    int producingType; // PieceType cast
    int turnsRemaining;

    Building();

    bool isPublic() const;
    bool isDestroyed() const;
    bool isCellDamaged(int localX, int localY) const;
    int getCellHP(int localX, int localY) const;
    void damageCellAt(int localX, int localY);
    std::vector<sf::Vector2i> getOccupiedCells() const;
    std::vector<sf::Vector2i> getAdjacentCells(const Board& board) const;
    bool containsCell(int x, int y) const;
};
