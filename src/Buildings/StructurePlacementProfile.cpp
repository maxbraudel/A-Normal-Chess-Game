#include "Buildings/StructurePlacementProfile.hpp"

#include <algorithm>

#include "Buildings/Building.hpp"
#include "Config/GameConfig.hpp"

namespace {

const std::vector<StructurePlacementProfile> kStructurePlacementProfiles = {
    {BuildingType::Barracks, {1, 1}},
    {BuildingType::Church, {1, 1}},
    {BuildingType::Mine, {2, 2}},
    {BuildingType::Farm, {2, 1}},
    {BuildingType::Arena, {1, 1}},
    {BuildingType::WoodWall, {0, 0}},
    {BuildingType::StoneWall, {0, 0}},
    {BuildingType::Bridge, {0, 0}},
};

sf::Vector2i defaultCentralAnchorSourceLocal(int baseWidth, int baseHeight) {
    return {
        std::max(0, (baseWidth - 1) / 2),
        std::max(0, (baseHeight - 1) / 2)
    };
}

sf::Vector2i clampSourceLocal(sf::Vector2i sourceLocal, int baseWidth, int baseHeight) {
    if (baseWidth <= 0 || baseHeight <= 0) {
        return {0, 0};
    }

    return {
        std::clamp(sourceLocal.x, 0, baseWidth - 1),
        std::clamp(sourceLocal.y, 0, baseHeight - 1)
    };
}

sf::Vector2i mapSourceLocalToFootprintLocal(sf::Vector2i sourceLocal,
                                            int baseWidth,
                                            int baseHeight,
                                            int rotationQuarterTurns,
                                            int flipMask) {
    const int footprintWidth = Building::getFootprintWidthFor(baseWidth, baseHeight, rotationQuarterTurns);
    const int footprintHeight = Building::getFootprintHeightFor(baseWidth, baseHeight, rotationQuarterTurns);
    for (int localY = 0; localY < footprintHeight; ++localY) {
        for (int localX = 0; localX < footprintWidth; ++localX) {
            if (Building::mapFootprintToSourceLocalFor(
                    localX,
                    localY,
                    baseWidth,
                    baseHeight,
                    rotationQuarterTurns,
                    flipMask) == sourceLocal) {
                return {localX, localY};
            }
        }
    }

    return {0, 0};
}

int baseWidthFor(BuildingType type, const GameConfig& config) {
    return config.getBuildingWidth(type);
}

int baseHeightFor(BuildingType type, const GameConfig& config) {
    return config.getBuildingHeight(type);
}

} // namespace

const std::vector<StructurePlacementProfile>& StructurePlacementProfiles::all() {
    return kStructurePlacementProfiles;
}

const StructurePlacementProfile* StructurePlacementProfiles::find(BuildingType type) {
    for (const StructurePlacementProfile& profile : kStructurePlacementProfiles) {
        if (profile.type == type) {
            return &profile;
        }
    }

    return nullptr;
}

sf::Vector2i StructurePlacementProfiles::getAnchorSourceLocal(BuildingType type,
                                                              int baseWidth,
                                                              int baseHeight) {
    const StructurePlacementProfile* profile = find(type);
    const sf::Vector2i fallback = defaultCentralAnchorSourceLocal(baseWidth, baseHeight);
    return clampSourceLocal(profile ? profile->anchorSourceLocal : fallback, baseWidth, baseHeight);
}

sf::Vector2i StructurePlacementProfiles::getAnchorSourceLocal(BuildingType type,
                                                              const GameConfig& config) {
    return getAnchorSourceLocal(type, baseWidthFor(type, config), baseHeightFor(type, config));
}

sf::Vector2i StructurePlacementProfiles::getAnchorFootprintLocal(BuildingType type,
                                                                 int baseWidth,
                                                                 int baseHeight,
                                                                 int rotationQuarterTurns,
                                                                 int flipMask) {
    return mapSourceLocalToFootprintLocal(
        getAnchorSourceLocal(type, baseWidth, baseHeight),
        baseWidth,
        baseHeight,
        rotationQuarterTurns,
        flipMask);
}

sf::Vector2i StructurePlacementProfiles::getAnchorFootprintLocal(BuildingType type,
                                                                 int rotationQuarterTurns,
                                                                 const GameConfig& config,
                                                                 int flipMask) {
    return getAnchorFootprintLocal(
        type,
        baseWidthFor(type, config),
        baseHeightFor(type, config),
        rotationQuarterTurns,
        flipMask);
}

sf::Vector2i StructurePlacementProfiles::originFromAnchorCell(BuildingType type,
                                                              sf::Vector2i anchorCell,
                                                              int baseWidth,
                                                              int baseHeight,
                                                              int rotationQuarterTurns,
                                                              int flipMask) {
    const sf::Vector2i anchorFootprintLocal = getAnchorFootprintLocal(
        type,
        baseWidth,
        baseHeight,
        rotationQuarterTurns,
        flipMask);
    return {
        anchorCell.x - anchorFootprintLocal.x,
        anchorCell.y - anchorFootprintLocal.y
    };
}

sf::Vector2i StructurePlacementProfiles::originFromAnchorCell(BuildingType type,
                                                              sf::Vector2i anchorCell,
                                                              int rotationQuarterTurns,
                                                              const GameConfig& config,
                                                              int flipMask) {
    return originFromAnchorCell(
        type,
        anchorCell,
        baseWidthFor(type, config),
        baseHeightFor(type, config),
        rotationQuarterTurns,
        flipMask);
}

sf::Vector2i StructurePlacementProfiles::anchorCellFromOrigin(BuildingType type,
                                                              sf::Vector2i origin,
                                                              int baseWidth,
                                                              int baseHeight,
                                                              int rotationQuarterTurns,
                                                              int flipMask) {
    const sf::Vector2i anchorFootprintLocal = getAnchorFootprintLocal(
        type,
        baseWidth,
        baseHeight,
        rotationQuarterTurns,
        flipMask);
    return {
        origin.x + anchorFootprintLocal.x,
        origin.y + anchorFootprintLocal.y
    };
}

sf::Vector2i StructurePlacementProfiles::anchorCellFromOrigin(BuildingType type,
                                                              sf::Vector2i origin,
                                                              int rotationQuarterTurns,
                                                              const GameConfig& config,
                                                              int flipMask) {
    return anchorCellFromOrigin(
        type,
        origin,
        baseWidthFor(type, config),
        baseHeightFor(type, config),
        rotationQuarterTurns,
        flipMask);
}