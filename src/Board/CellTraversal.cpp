#include "Board/CellTraversal.hpp"

#include "Board/Cell.hpp"
#include "Buildings/Building.hpp"
#include "Board/CellType.hpp"
#include "Systems/StructureIntegrityRules.hpp"

bool isCellTerrainTraversable(const Cell& cell) {
    if (!cell.isInCircle || cell.type == CellType::Water || cell.type == CellType::Void) {
        return false;
    }

    if (!cell.building) {
        return true;
    }

    const int localX = cell.position.x - cell.building->origin.x;
    const int localY = cell.position.y - cell.building->origin.y;
    return !StructureIntegrityRules::isWallCellBlocking(*cell.building, localX, localY);
}