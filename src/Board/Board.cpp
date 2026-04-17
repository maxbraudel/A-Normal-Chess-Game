#include "Board/Board.hpp"
#include "Buildings/Building.hpp"
#include "Buildings/BuildingType.hpp"
#include "Systems/StructureIntegrityRules.hpp"
#include "Units/Piece.hpp"
#include <cmath>

Board::Board() : m_radius(0), m_diameter(0) {
    m_voidCell.type = CellType::Void;
    m_voidCell.terrainFlipMask = 0;
    m_voidCell.terrainBrightness = 255;
    m_voidCell.isInCircle = false;
    m_voidCell.mapObject = nullptr;
    m_voidCell.autonomousUnit = nullptr;
}

void Board::init(int radius) {
    m_radius = radius;
    m_diameter = radius * 2;
    m_grid.assign(m_diameter, std::vector<Cell>(m_diameter));

    float cx = static_cast<float>(m_radius);
    float cy = static_cast<float>(m_radius);

    for (int y = 0; y < m_diameter; ++y) {
        for (int x = 0; x < m_diameter; ++x) {
            float dx = static_cast<float>(x) - cx + 0.5f;
            float dy = static_cast<float>(y) - cy + 0.5f;
            float dist = std::sqrt(dx * dx + dy * dy);

            Cell& cell = m_grid[y][x];
            cell.position = {x, y};
            if (dist <= static_cast<float>(m_radius)) {
                cell.type = CellType::Grass;
                cell.isInCircle = true;
            } else {
                cell.type = CellType::Void;
                cell.isInCircle = false;
            }
            cell.terrainFlipMask = 0;
            cell.terrainBrightness = 255;
            cell.building = nullptr;
            cell.mapObject = nullptr;
            cell.piece = nullptr;
            cell.autonomousUnit = nullptr;
        }
    }
}

Cell& Board::getCell(int x, int y) {
    if (!isInBounds(x, y)) return m_voidCell;
    return m_grid[y][x];
}

const Cell& Board::getCell(int x, int y) const {
    if (!isInBounds(x, y)) return m_voidCell;
    return m_grid[y][x];
}

bool Board::isInBounds(int x, int y) const {
    return x >= 0 && x < m_diameter && y >= 0 && y < m_diameter;
}

bool Board::isTraversable(int x, int y, KingdomId mover) const {
    if (!isInBounds(x, y)) return false;
    const Cell& cell = m_grid[y][x];
    if (!cell.isInCircle) return false;
    if (cell.type == CellType::Water) return false;
    if (cell.building) {
        const int localX = x - cell.building->origin.x;
        const int localY = y - cell.building->origin.y;
        if (StructureIntegrityRules::isWallCellBlocking(*cell.building, localX, localY)) {
            return false;
        }
    }
    // Friendly pieces block
    if (cell.piece && cell.piece->kingdom == mover) return false;
    return true;
}

int Board::getRadius() const { return m_radius; }
int Board::getDiameter() const { return m_diameter; }

std::vector<sf::Vector2i> Board::getAllValidCells() const {
    std::vector<sf::Vector2i> result;
    for (int y = 0; y < m_diameter; ++y) {
        for (int x = 0; x < m_diameter; ++x) {
            if (m_grid[y][x].isInCircle) {
                result.push_back({x, y});
            }
        }
    }
    return result;
}

std::vector<std::vector<Cell>>& Board::getGrid() { return m_grid; }
const std::vector<std::vector<Cell>>& Board::getGrid() const { return m_grid; }
