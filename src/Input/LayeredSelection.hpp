#pragma once

#include <array>

#include <SFML/System/Vector2.hpp>

class Piece;
struct AutonomousUnit;
class Building;
class MapObject;
struct Cell;

enum class SelectionLayer {
    None,
    Piece,
    AutonomousUnit,
    Building,
    Object,
    Terrain
};

struct LayeredSelectionStack {
    sf::Vector2i cellPos{0, 0};
    Piece* piece = nullptr;
    AutonomousUnit* autonomousUnit = nullptr;
    Building* building = nullptr;
    MapObject* mapObject = nullptr;
    bool hasTerrain = false;
    std::array<SelectionLayer, 5> layers{
        SelectionLayer::None,
        SelectionLayer::None,
        SelectionLayer::None,
        SelectionLayer::None,
        SelectionLayer::None
    };
    int count = 0;

    bool isEmpty() const;
    bool contains(SelectionLayer layer) const;
    int indexOf(SelectionLayer layer) const;
    SelectionLayer top() const;
    SelectionLayer nextBelow(SelectionLayer currentLayer) const;
};

LayeredSelectionStack resolveCellSelectionStack(const Cell& cell, sf::Vector2i cellPos,
                                                Piece* pieceOverride = nullptr,
                                                bool suppressCellPiece = false);