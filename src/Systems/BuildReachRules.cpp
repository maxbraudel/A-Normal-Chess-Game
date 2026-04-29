#include "Systems/BuildReachRules.hpp"

#include <cmath>

bool isBuildSupportPieceType(PieceType type) {
    return type == PieceType::King || type == PieceType::Pawn;
}

bool canPieceTypeSupportBuild(PieceType type, BuildingType buildingType) {
    if (type == PieceType::Pawn) {
        return buildingType == BuildingType::Barracks
            || buildingType == BuildingType::WoodWall
            || buildingType == BuildingType::StoneWall;
    }

    if (type == PieceType::King) {
        return buildingType == BuildingType::Barracks;
    }

    return false;
}

bool footprintHasAdjacentBuilder(sf::Vector2i origin,
                                 int width,
                                 int height,
                                 const std::vector<sf::Vector2i>& builderPositions) {
    for (const sf::Vector2i& builderPos : builderPositions) {
        for (int dy = 0; dy < height; ++dy) {
            for (int dx = 0; dx < width; ++dx) {
                const int cellX = origin.x + dx;
                const int cellY = origin.y + dy;
                const int diffX = std::abs(builderPos.x - cellX);
                const int diffY = std::abs(builderPos.y - cellY);
                if (diffX <= 1 && diffY <= 1 && (diffX + diffY > 0)) {
                    return true;
                }
            }
        }
    }

    return false;
}