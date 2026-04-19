#pragma once

#include <array>
#include <vector>

#include "Autonomous/AutonomousUnit.hpp"
#include "Input/InputSelectionBookmark.hpp"
#include "Kingdom/Kingdom.hpp"
#include "Objects/MapObject.hpp"

struct SelectionQueryView {
    std::array<Kingdom, kNumKingdoms>& kingdoms;
    std::vector<Building>& publicBuildings;
    std::vector<MapObject>& mapObjects;
    std::vector<AutonomousUnit>& autonomousUnits;

    SelectionQueryView(std::array<Kingdom, kNumKingdoms>& kingdomsRef,
                       std::vector<Building>& publicBuildingsRef)
        : kingdoms(kingdomsRef),
          publicBuildings(publicBuildingsRef),
          mapObjects(emptyMapObjects()),
          autonomousUnits(emptyAutonomousUnits()) {}

    SelectionQueryView(std::array<Kingdom, kNumKingdoms>& kingdomsRef,
                       std::vector<Building>& publicBuildingsRef,
                       std::vector<MapObject>& mapObjectsRef)
        : kingdoms(kingdomsRef),
          publicBuildings(publicBuildingsRef),
          mapObjects(mapObjectsRef),
          autonomousUnits(emptyAutonomousUnits()) {}

    SelectionQueryView(std::array<Kingdom, kNumKingdoms>& kingdomsRef,
                       std::vector<Building>& publicBuildingsRef,
                       std::vector<MapObject>& mapObjectsRef,
                       std::vector<AutonomousUnit>& autonomousUnitsRef)
        : kingdoms(kingdomsRef),
          publicBuildings(publicBuildingsRef),
          mapObjects(mapObjectsRef),
          autonomousUnits(autonomousUnitsRef) {}

private:
    static std::vector<MapObject>& emptyMapObjects() {
        static std::vector<MapObject> empty;
        return empty;
    }

    static std::vector<AutonomousUnit>& emptyAutonomousUnits() {
        static std::vector<AutonomousUnit> empty;
        return empty;
    }
};

class SelectionQueryCoordinator {
public:
    static Piece* findPieceById(const SelectionQueryView& view, int pieceId);
    static AutonomousUnit* findAutonomousUnitById(const SelectionQueryView& view, int autonomousUnitId);
    static AutonomousUnit* findAutonomousUnitForBookmark(const SelectionQueryView& view,
                                                         const InputSelectionBookmark& bookmark);
    static Building* findBuildingById(const SelectionQueryView& view, int buildingId);
    static Building* findBuildingForBookmark(const SelectionQueryView& view,
                                             const InputSelectionBookmark& bookmark);
    static MapObject* findMapObjectById(const SelectionQueryView& view, int mapObjectId);
    static MapObject* findMapObjectForBookmark(const SelectionQueryView& view,
                                               const InputSelectionBookmark& bookmark);
};