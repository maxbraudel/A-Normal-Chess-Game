#pragma once

#include <array>
#include <optional>

#include <SFML/System/Vector2.hpp>

#include "Input/PendingBuildSelection.hpp"

class Piece;
class Building;
struct Cell;

enum class SelectionLayer {
    None,
    Piece,
    Building,
    PendingBuild,
    Terrain
};

struct LayeredSelectionStack {
    sf::Vector2i cellPos{0, 0};
    Piece* piece = nullptr;
    Building* building = nullptr;
    std::optional<PendingBuildSelection> pendingBuild;
    bool hasTerrain = false;
    std::array<SelectionLayer, 4> layers{
        SelectionLayer::None,
        SelectionLayer::None,
        SelectionLayer::None
        ,SelectionLayer::None
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
                                                bool suppressCellPiece = false,
                                                const std::optional<PendingBuildSelection>& pendingBuild = std::nullopt);