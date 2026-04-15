#include "Systems/StructureIntegrityRules.hpp"

#include "Config/GameConfig.hpp"

bool StructureIntegrityRules::isWallType(BuildingType type) {
    return type == BuildingType::WoodWall || type == BuildingType::StoneWall;
}

bool StructureIntegrityRules::isRepairableOwnedStructureType(BuildingType type) {
    switch (type) {
        case BuildingType::Barracks:
        case BuildingType::Bridge:
        case BuildingType::Arena:
            return true;

        default:
            return false;
    }
}

bool StructureIntegrityRules::shouldRemoveWhenFullyDestroyed(BuildingType type) {
    return !isRepairableOwnedStructureType(type);
}

int StructureIntegrityRules::defaultCellHP(BuildingType type, const GameConfig& config) {
    switch (type) {
        case BuildingType::WoodWall:
            return config.getWoodWallHP();

        case BuildingType::StoneWall:
            return config.getStoneWallHP();

        case BuildingType::Barracks:
            return config.getBarracksCellHP();

        case BuildingType::Arena:
        case BuildingType::Bridge:
            return 1;

        default:
            return 1;
    }
}

int StructureIntegrityRules::repairCostPerCell(BuildingType type, const GameConfig& config) {
    switch (type) {
        case BuildingType::Barracks:
        case BuildingType::Arena:
        case BuildingType::Bridge:
            return config.getRepairCostPerCell(type);

        default:
            return 0;
    }
}