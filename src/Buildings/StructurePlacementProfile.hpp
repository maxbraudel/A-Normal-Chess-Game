#pragma once

#include <SFML/System/Vector2.hpp>

#include <vector>

#include "Buildings/BuildingType.hpp"

class GameConfig;

struct StructurePlacementProfile {
    BuildingType type;
    sf::Vector2i anchorSourceLocal;
};

class StructurePlacementProfiles {
public:
    static const std::vector<StructurePlacementProfile>& all();
    static const StructurePlacementProfile* find(BuildingType type);

    static sf::Vector2i getAnchorSourceLocal(BuildingType type,
                                             int baseWidth,
                                             int baseHeight);
    static sf::Vector2i getAnchorSourceLocal(BuildingType type,
                                             const GameConfig& config);
    static sf::Vector2i getAnchorFootprintLocal(BuildingType type,
                                                int baseWidth,
                                                int baseHeight,
                                                int rotationQuarterTurns,
                                                int flipMask = 0);
    static sf::Vector2i getAnchorFootprintLocal(BuildingType type,
                                                int rotationQuarterTurns,
                                                const GameConfig& config,
                                                int flipMask = 0);
    static sf::Vector2i originFromAnchorCell(BuildingType type,
                                             sf::Vector2i anchorCell,
                                             int baseWidth,
                                             int baseHeight,
                                             int rotationQuarterTurns,
                                             int flipMask = 0);
    static sf::Vector2i originFromAnchorCell(BuildingType type,
                                             sf::Vector2i anchorCell,
                                             int rotationQuarterTurns,
                                             const GameConfig& config,
                                             int flipMask = 0);
    static sf::Vector2i anchorCellFromOrigin(BuildingType type,
                                             sf::Vector2i origin,
                                             int baseWidth,
                                             int baseHeight,
                                             int rotationQuarterTurns,
                                             int flipMask = 0);
    static sf::Vector2i anchorCellFromOrigin(BuildingType type,
                                             sf::Vector2i origin,
                                             int rotationQuarterTurns,
                                             const GameConfig& config,
                                             int flipMask = 0);
};