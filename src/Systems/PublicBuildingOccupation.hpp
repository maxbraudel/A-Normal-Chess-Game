#pragma once

class Board;
class Building;

enum class PublicBuildingOccupationState {
    Unoccupied,
    WhiteOccupied,
    BlackOccupied,
    Contested
};

PublicBuildingOccupationState resolvePublicBuildingOccupationState(const Building& building,
                                                                   const Board& board);