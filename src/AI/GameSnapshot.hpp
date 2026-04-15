#pragma once
#include <algorithm>
#include <memory>
#include <vector>
#include <SFML/System/Vector2.hpp>
#include "Buildings/Building.hpp"
#include "Units/PieceType.hpp"
#include "Buildings/BuildingType.hpp"
#include "Kingdom/KingdomId.hpp"
#include "Board/CellType.hpp"
#include "AI/ThreatMap.hpp"

// ---- Lightweight snapshot structs (no pointers, fully clonable) ----

struct SnapPiece {
    int id = -1;
    PieceType type = PieceType::Pawn;
    KingdomId kingdom = KingdomId::White;
    sf::Vector2i position{0, 0};
    int xp = 0;
};

struct SnapBuilding {
    int id = -1;
    BuildingType type = BuildingType::Barracks;
    KingdomId owner = KingdomId::White;
    bool isNeutral = false;
    sf::Vector2i origin{0, 0};
    int width = 1;
    int height = 1;
    int rotationQuarterTurns = 0;
    int flipMask = 0;
    std::vector<int> cellHP;
    bool isProducing = false;
    PieceType producingType = PieceType::Pawn;
    int turnsRemaining = 0;

    int getFootprintWidth() const {
        return Building::getFootprintWidthFor(width, height, rotationQuarterTurns);
    }

    int getFootprintHeight() const {
        return Building::getFootprintHeightFor(width, height, rotationQuarterTurns);
    }

    sf::Vector2i mapFootprintToSourceLocal(int localX, int localY) const {
        return Building::mapFootprintToSourceLocalFor(
            localX, localY, width, height, rotationQuarterTurns, flipMask);
    }

    bool containsCell(int x, int y) const {
        return x >= origin.x && x < origin.x + getFootprintWidth() &&
               y >= origin.y && y < origin.y + getFootprintHeight();
    }

    bool isDestroyed() const {
        if (isNeutral) return false;
        for (int hp : cellHP) if (hp > 0) return false;
        return true;
    }

    void damageCellAt(int localX, int localY) {
        const sf::Vector2i sourceLocal = mapFootprintToSourceLocal(localX, localY);
        if (sourceLocal.x < 0 || sourceLocal.y < 0) {
            return;
        }

        const int index = sourceLocal.y * width + sourceLocal.x;
        if (index >= 0 && index < static_cast<int>(cellHP.size())) {
            cellHP[index] = std::max(0, cellHP[index] - 1);
        }
    }

    std::vector<sf::Vector2i> getOccupiedCells() const {
        std::vector<sf::Vector2i> cells;
        for (int dy = 0; dy < getFootprintHeight(); ++dy)
            for (int dx = 0; dx < getFootprintWidth(); ++dx)
                cells.push_back({origin.x + dx, origin.y + dy});
        return cells;
    }
};

struct SnapKingdom {
    KingdomId id = KingdomId::White;
    int gold = 0;
    std::vector<SnapPiece> pieces;
    std::vector<SnapBuilding> buildings;

    SnapPiece* getKing() {
        for (auto& p : pieces) if (p.type == PieceType::King) return &p;
        return nullptr;
    }
    const SnapPiece* getKing() const {
        for (auto& p : pieces) if (p.type == PieceType::King) return &p;
        return nullptr;
    }
    bool hasQueen() const {
        for (auto& p : pieces) if (p.type == PieceType::Queen) return true;
        return false;
    }
    SnapPiece* getPieceAt(sf::Vector2i pos) {
        for (auto& p : pieces) if (p.position == pos) return &p;
        return nullptr;
    }
    const SnapPiece* getPieceAt(sf::Vector2i pos) const {
        for (auto& p : pieces) if (p.position == pos) return &p;
        return nullptr;
    }
    SnapPiece* getPieceById(int pid) {
        for (auto& p : pieces) if (p.id == pid) return &p;
        return nullptr;
    }
    const SnapPiece* getPieceById(int pid) const {
        for (auto& p : pieces) if (p.id == pid) return &p;
        return nullptr;
    }
    SnapBuilding* getBuildingById(int bid) {
        for (auto& b : buildings) if (b.id == bid) return &b;
        return nullptr;
    }
    void removePiece(int pid) {
        for (auto it = pieces.begin(); it != pieces.end(); ++it) {
            if (it->id == pid) { pieces.erase(it); return; }
        }
    }
};

/// Shared immutable terrain grid (never changes during simulation)
struct TerrainGrid {
    int radius = 25;
    int diameter = 50;
    // grid[y][x]
    std::vector<std::vector<CellType>> types;
    std::vector<std::vector<bool>> inCircle;

    bool isInBounds(int x, int y) const {
        return x >= 0 && x < diameter && y >= 0 && y < diameter;
    }
    bool isTraversable(int x, int y) const {
        if (!isInBounds(x, y)) return false;
        if (!inCircle[y][x]) return false;
        return types[y][x] != CellType::Water && types[y][x] != CellType::Void;
    }
};

/// Full lightweight game-state snapshot for AI simulation
struct GameSnapshot {
    std::shared_ptr<const TerrainGrid> terrain; // shared across clones
    SnapKingdom white;
    SnapKingdom black;
    std::vector<SnapBuilding> publicBuildings;  // neutral/public buildings
    int turnNumber = 1;

    // ---- Accessors ----
    SnapKingdom& kingdom(KingdomId id) {
        return id == KingdomId::White ? white : black;
    }
    const SnapKingdom& kingdom(KingdomId id) const {
        return id == KingdomId::White ? white : black;
    }
    SnapKingdom& enemyKingdom(KingdomId id) {
        return id == KingdomId::White ? black : white;
    }
    const SnapKingdom& enemyKingdom(KingdomId id) const {
        return id == KingdomId::White ? black : white;
    }

    int getRadius() const { return terrain ? terrain->radius : 25; }
    int getDiameter() const { return terrain ? terrain->diameter : 50; }

    bool isInBounds(int x, int y) const { return terrain && terrain->isInBounds(x, y); }
    bool isTraversable(int x, int y) const { return terrain && terrain->isTraversable(x, y); }

    // Find ANY piece at a position (either kingdom)
    SnapPiece* pieceAt(sf::Vector2i pos) {
        if (auto* p = white.getPieceAt(pos)) return p;
        return black.getPieceAt(pos);
    }
    const SnapPiece* pieceAt(sf::Vector2i pos) const {
        if (auto* p = white.getPieceAt(pos)) return p;
        return black.getPieceAt(pos);
    }

    // Find building at position (kingdom-owned or public)
    const SnapBuilding* buildingAt(sf::Vector2i pos) const {
        for (auto& b : white.buildings) if (!b.isDestroyed() && b.containsCell(pos.x, pos.y)) return &b;
        for (auto& b : black.buildings) if (!b.isDestroyed() && b.containsCell(pos.x, pos.y)) return &b;
        for (auto& b : publicBuildings) if (!b.isDestroyed() && b.containsCell(pos.x, pos.y)) return &b;
        return nullptr;
    }
    SnapBuilding* buildingAt(sf::Vector2i pos) {
        for (auto& b : white.buildings) if (!b.isDestroyed() && b.containsCell(pos.x, pos.y)) return &b;
        for (auto& b : black.buildings) if (!b.isDestroyed() && b.containsCell(pos.x, pos.y)) return &b;
        for (auto& b : publicBuildings) if (!b.isDestroyed() && b.containsCell(pos.x, pos.y)) return &b;
        return nullptr;
    }

    // Deep clone (terrain is shared, everything else is copied)
    GameSnapshot clone() const {
        GameSnapshot s;
        s.terrain = terrain;
        s.white = white;
        s.black = black;
        s.publicBuildings = publicBuildings;
        s.turnNumber = turnNumber;
        return s;
    }
};
