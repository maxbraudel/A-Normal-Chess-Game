#include "Systems/PublicBuildingOccupation.hpp"

#include "Board/Board.hpp"
#include "Board/Cell.hpp"
#include "Buildings/Building.hpp"
#include "Kingdom/KingdomId.hpp"
#include "Units/Piece.hpp"

PublicBuildingOccupationState resolvePublicBuildingOccupationState(const Building& building,
                                                                   const Board& board) {
    bool whitePresent = false;
    bool blackPresent = false;

    for (const sf::Vector2i& pos : building.getOccupiedCells()) {
        if (!board.isInBounds(pos.x, pos.y)) {
            continue;
        }

        const Cell& cell = board.getCell(pos.x, pos.y);
        if (!cell.piece) {
            continue;
        }

        if (cell.piece->kingdom == KingdomId::White) {
            whitePresent = true;
        } else {
            blackPresent = true;
        }

        if (whitePresent && blackPresent) {
            return PublicBuildingOccupationState::Contested;
        }
    }

    if (whitePresent) {
        return PublicBuildingOccupationState::WhiteOccupied;
    }
    if (blackPresent) {
        return PublicBuildingOccupationState::BlackOccupied;
    }

    return PublicBuildingOccupationState::Unoccupied;
}