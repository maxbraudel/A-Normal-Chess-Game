#pragma once

#include <SFML/System/Vector2.hpp>

#include "Buildings/BuildingType.hpp"

struct PendingBuildSelection {
    BuildingType type = BuildingType::Barracks;
    sf::Vector2i origin{0, 0};
    int rotationQuarterTurns = 0;
    int footprintWidth = 0;
    int footprintHeight = 0;
    int flipMask = 0;
};