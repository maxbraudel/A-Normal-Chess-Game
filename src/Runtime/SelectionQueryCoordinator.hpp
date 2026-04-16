#pragma once

#include <array>
#include <vector>

#include "Input/InputSelectionBookmark.hpp"
#include "Kingdom/Kingdom.hpp"

struct SelectionQueryView {
    std::array<Kingdom, kNumKingdoms>& kingdoms;
    std::vector<Building>& publicBuildings;
};

class SelectionQueryCoordinator {
public:
    static Piece* findPieceById(const SelectionQueryView& view, int pieceId);
    static Building* findBuildingById(const SelectionQueryView& view, int buildingId);
    static Building* findBuildingForBookmark(const SelectionQueryView& view,
                                             const InputSelectionBookmark& bookmark);
};