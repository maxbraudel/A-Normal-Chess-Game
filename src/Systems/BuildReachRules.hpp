#pragma once

#include <SFML/System/Vector2.hpp>

#include <vector>

#include "Buildings/BuildingType.hpp"
#include "Units/PieceType.hpp"

bool isBuildSupportPieceType(PieceType type);
bool canPieceTypeSupportBuild(PieceType type, BuildingType buildingType);
bool footprintHasAdjacentBuilder(sf::Vector2i origin,
                                 int width,
                                 int height,
                                 const std::vector<sf::Vector2i>& builderPositions);

template <typename PieceCollection>
std::vector<sf::Vector2i> collectBuilderPositions(const PieceCollection& pieces,
                                                  BuildingType buildingType) {
    std::vector<sf::Vector2i> positions;
    for (const auto& piece : pieces) {
        if (canPieceTypeSupportBuild(piece.type, buildingType)) {
            positions.push_back(piece.position);
        }
    }

    return positions;
}