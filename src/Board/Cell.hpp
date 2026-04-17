#pragma once
#include <cstdint>
#include <SFML/System/Vector2.hpp>
#include "Board/CellType.hpp"

class Building;
class MapObject;
class Piece;

struct Cell {
    CellType type = CellType::Void;
    int terrainFlipMask = 0;
    std::uint8_t terrainBrightness = 255;
    Building* building = nullptr;
    MapObject* mapObject = nullptr;
    Piece* piece = nullptr;
    bool isInCircle = false;
    sf::Vector2i position{0, 0};
};
