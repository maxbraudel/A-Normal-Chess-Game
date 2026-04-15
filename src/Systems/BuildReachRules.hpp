#pragma once

#include <SFML/System/Vector2.hpp>

#include <vector>

#include "Units/PieceType.hpp"

bool isBuildSupportPieceType(PieceType type);
bool footprintHasAdjacentBuilder(sf::Vector2i origin,
                                 int width,
                                 int height,
                                 const std::vector<sf::Vector2i>& builderPositions);

template <typename PieceCollection>
std::vector<sf::Vector2i> collectBuilderPositions(const PieceCollection& pieces) {
    std::vector<sf::Vector2i> positions;
    for (const auto& piece : pieces) {
        if (isBuildSupportPieceType(piece.type)) {
            positions.push_back(piece.position);
        }
    }

    return positions;
}