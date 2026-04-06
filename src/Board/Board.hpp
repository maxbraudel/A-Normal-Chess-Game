#pragma once
#include <vector>
#include <SFML/System/Vector2.hpp>
#include "Board/Cell.hpp"
#include "Kingdom/KingdomId.hpp"

class GameConfig;

class Board {
public:
    Board();
    void init(int radius);

    Cell& getCell(int x, int y);
    const Cell& getCell(int x, int y) const;
    bool isInBounds(int x, int y) const;
    bool isTraversable(int x, int y, KingdomId mover) const;
    int getRadius() const;
    int getDiameter() const;

    std::vector<sf::Vector2i> getAllValidCells() const;

    // Direct access for generation
    std::vector<std::vector<Cell>>& getGrid();
    const std::vector<std::vector<Cell>>& getGrid() const;

private:
    int m_radius;
    int m_diameter;
    std::vector<std::vector<Cell>> m_grid;
    Cell m_voidCell; // returned for out-of-bounds
};
