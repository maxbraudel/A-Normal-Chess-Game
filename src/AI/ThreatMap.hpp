#pragma once
#include <cstring>
#include <SFML/System/Vector2.hpp>

// O(1) lookup threat grid — replaces std::set<Vector2i> for massive speedup.
// 10KB footprint, fits in L1 cache.
static constexpr int THREAT_MAP_SIZE = 100;

struct ThreatMap {
    bool grid[THREAT_MAP_SIZE][THREAT_MAP_SIZE]; // [y][x]

    ThreatMap() { clear(); }
    void clear() { std::memset(grid, 0, sizeof(grid)); }

    void mark(int x, int y) {
        if (x >= 0 && x < THREAT_MAP_SIZE && y >= 0 && y < THREAT_MAP_SIZE)
            grid[y][x] = true;
    }

    void mark(sf::Vector2i pos) { mark(pos.x, pos.y); }

    bool isSet(int x, int y) const {
        if (x < 0 || x >= THREAT_MAP_SIZE || y < 0 || y >= THREAT_MAP_SIZE)
            return false;
        return grid[y][x];
    }

    bool isSet(sf::Vector2i pos) const { return isSet(pos.x, pos.y); }
};
