#pragma once

#include <optional>

#include <SFML/System/Vector2.hpp>

#include "Buildings/BuildingType.hpp"
#include "Core/ToolState.hpp"
#include "Kingdom/KingdomId.hpp"
#include "Objects/MapObjectType.hpp"

struct InputSelectionBookmark {
    ToolState tool = ToolState::Select;
    int pieceId = -1;
    int autonomousUnitId = -1;
    int buildingId = -1;
    int mapObjectId = -1;
    // Canonical world cell where the current selection was anchored.
    std::optional<sf::Vector2i> selectedCell;
    std::optional<sf::Vector2i> selectedBuildingOrigin;
    std::optional<sf::Vector2i> selectedMapObjectPosition;
    BuildingType selectedBuildingType = BuildingType::Barracks;
    KingdomId selectedBuildingOwner = KingdomId::White;
    bool selectedBuildingIsNeutral = false;
    int selectedBuildingRotationQuarterTurns = 0;
    MapObjectType selectedMapObjectType = MapObjectType::Chest;
    BuildingType buildPreviewType = BuildingType::Barracks;
    int buildPreviewRotationQuarterTurns = 0;
    std::optional<sf::Vector2i> buildPreviewAnchorCell;
};