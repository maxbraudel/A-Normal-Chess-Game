#pragma once

#include <algorithm>

#include "Buildings/BuildingType.hpp"

class GameConfig;

enum class StructureOccupancyResult {
    None,
    Damaged,
    Breached,
    Destroyed
};

class StructureIntegrityRules {
public:
    static bool isWallType(BuildingType type);
    static bool isRepairableOwnedStructureType(BuildingType type);
    static bool shouldRemoveWhenFullyDestroyed(BuildingType type);
    static int defaultCellHP(BuildingType type, const GameConfig& config);
    static int repairCostPerCell(BuildingType type, const GameConfig& config);

    template <typename BuildingLike>
    static bool isWallCellBlocking(const BuildingLike& building, int localX, int localY) {
        if (!isWallType(building.type)) {
            return false;
        }

        return !building.isCellDestroyed(localX, localY);
    }

    template <typename BuildingLike>
    static StructureOccupancyResult applyEnemyOccupancy(BuildingLike& building,
                                                        int localX,
                                                        int localY,
                                                        const GameConfig& config) {
        if (building.isNeutral || building.isCellDestroyed(localX, localY)) {
            return StructureOccupancyResult::None;
        }

        switch (building.type) {
            case BuildingType::WoodWall:
                building.destroyCellAt(localX, localY);
                return StructureOccupancyResult::Destroyed;

            case BuildingType::StoneWall: {
                if (building.isCellBreached(localX, localY)) {
                    building.destroyCellAt(localX, localY);
                    return StructureOccupancyResult::Destroyed;
                }

                const int currentHP = building.getCellHP(localX, localY);
                building.setCellHP(localX, localY, std::max(1, currentHP - 1));
                building.setCellBreached(localX, localY, true);
                return StructureOccupancyResult::Breached;
            }

            default: {
                const int beforeHP = building.getCellHP(localX, localY);
                if (beforeHP <= 0) {
                    return StructureOccupancyResult::None;
                }

                building.damageCellAt(localX, localY);
                return building.isCellDestroyed(localX, localY)
                    ? StructureOccupancyResult::Destroyed
                    : StructureOccupancyResult::Damaged;
            }
        }
    }

    template <typename BuildingLike>
    static bool repairDestroyedCell(BuildingLike& building,
                                    int localX,
                                    int localY,
                                    const GameConfig& config) {
        if (!isRepairableOwnedStructureType(building.type) || !building.isCellDestroyed(localX, localY)) {
            return false;
        }

        building.repairCellAt(localX, localY, defaultCellHP(building.type, config));
        return true;
    }
};