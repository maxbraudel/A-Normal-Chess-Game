#include "Runtime/SelectionQueryCoordinator.hpp"

namespace {

bool matchesBookmark(const Building& building, const InputSelectionBookmark& bookmark) {
    return bookmark.selectedBuildingOrigin.has_value()
        && building.origin == *bookmark.selectedBuildingOrigin
        && building.type == bookmark.selectedBuildingType
        && building.isNeutral == bookmark.selectedBuildingIsNeutral
        && building.owner == bookmark.selectedBuildingOwner
        && building.rotationQuarterTurns == bookmark.selectedBuildingRotationQuarterTurns;
}

bool matchesBookmark(const MapObject& mapObject, const InputSelectionBookmark& bookmark) {
    return bookmark.selectedMapObjectPosition.has_value()
        && mapObject.position == *bookmark.selectedMapObjectPosition
        && mapObject.type == bookmark.selectedMapObjectType;
}

} // namespace

Piece* SelectionQueryCoordinator::findPieceById(const SelectionQueryView& view, int pieceId) {
    if (pieceId < 0) {
        return nullptr;
    }

    for (Kingdom& kingdom : view.kingdoms) {
        if (Piece* piece = kingdom.getPieceById(pieceId)) {
            return piece;
        }
    }

    return nullptr;
}

AutonomousUnit* SelectionQueryCoordinator::findAutonomousUnitById(const SelectionQueryView& view,
                                                                  int autonomousUnitId) {
    if (autonomousUnitId < 0) {
        return nullptr;
    }

    for (AutonomousUnit& autonomousUnit : view.autonomousUnits) {
        if (autonomousUnit.id == autonomousUnitId) {
            return &autonomousUnit;
        }
    }

    return nullptr;
}

AutonomousUnit* SelectionQueryCoordinator::findAutonomousUnitForBookmark(
    const SelectionQueryView& view,
    const InputSelectionBookmark& bookmark) {
    return findAutonomousUnitById(view, bookmark.autonomousUnitId);
}

Building* SelectionQueryCoordinator::findBuildingById(const SelectionQueryView& view, int buildingId) {
    if (buildingId < 0) {
        return nullptr;
    }

    for (Building& building : view.publicBuildings) {
        if (building.id == buildingId) {
            return &building;
        }
    }

    for (Kingdom& kingdom : view.kingdoms) {
        for (Building& building : kingdom.buildings) {
            if (building.id == buildingId) {
                return &building;
            }
        }
    }

    return nullptr;
}

Building* SelectionQueryCoordinator::findBuildingForBookmark(const SelectionQueryView& view,
                                                             const InputSelectionBookmark& bookmark) {
    if (Building* building = findBuildingById(view, bookmark.buildingId)) {
        return building;
    }

    for (Building& building : view.publicBuildings) {
        if (matchesBookmark(building, bookmark)) {
            return &building;
        }
    }

    for (Kingdom& kingdom : view.kingdoms) {
        for (Building& building : kingdom.buildings) {
            if (matchesBookmark(building, bookmark)) {
                return &building;
            }
        }
    }

    return nullptr;
}

MapObject* SelectionQueryCoordinator::findMapObjectById(const SelectionQueryView& view, int mapObjectId) {
    if (mapObjectId < 0) {
        return nullptr;
    }

    for (MapObject& object : view.mapObjects) {
        if (object.id == mapObjectId) {
            return &object;
        }
    }

    return nullptr;
}

MapObject* SelectionQueryCoordinator::findMapObjectForBookmark(const SelectionQueryView& view,
                                                               const InputSelectionBookmark& bookmark) {
    if (MapObject* object = findMapObjectById(view, bookmark.mapObjectId)) {
        return object;
    }

    for (MapObject& object : view.mapObjects) {
        if (matchesBookmark(object, bookmark)) {
            return &object;
        }
    }

    return nullptr;
}