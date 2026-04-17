#pragma once

#include <array>
#include <vector>

#include "Input/InputSelectionBookmark.hpp"
#include "Kingdom/Kingdom.hpp"
#include "Objects/MapObject.hpp"

struct SelectionQueryView {
    std::array<Kingdom, kNumKingdoms>& kingdoms;
    std::vector<Building>& publicBuildings;
    std::vector<MapObject>& mapObjects;

    SelectionQueryView(std::array<Kingdom, kNumKingdoms>& kingdomsRef,
                       std::vector<Building>& publicBuildingsRef)
        : kingdoms(kingdomsRef),
          publicBuildings(publicBuildingsRef),
          mapObjects(emptyMapObjects()) {}

    SelectionQueryView(std::array<Kingdom, kNumKingdoms>& kingdomsRef,
                       std::vector<Building>& publicBuildingsRef,
                       std::vector<MapObject>& mapObjectsRef)
        : kingdoms(kingdomsRef),
          publicBuildings(publicBuildingsRef),
          mapObjects(mapObjectsRef) {}

private:
    static std::vector<MapObject>& emptyMapObjects() {
        static std::vector<MapObject> empty;
        return empty;
    }
};

class SelectionQueryCoordinator {
public:
    static Piece* findPieceById(const SelectionQueryView& view, int pieceId);
    static Building* findBuildingById(const SelectionQueryView& view, int buildingId);
    static Building* findBuildingForBookmark(const SelectionQueryView& view,
                                             const InputSelectionBookmark& bookmark);
    static MapObject* findMapObjectById(const SelectionQueryView& view, int mapObjectId);
    static MapObject* findMapObjectForBookmark(const SelectionQueryView& view,
                                               const InputSelectionBookmark& bookmark);
};