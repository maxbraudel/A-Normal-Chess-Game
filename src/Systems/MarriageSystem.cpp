#include "Systems/MarriageSystem.hpp"

#include "Projection/GameSnapshot.hpp"
#include "Kingdom/Kingdom.hpp"
#include "Board/Board.hpp"
#include "Board/Cell.hpp"
#include "Buildings/Building.hpp"
#include "Buildings/BuildingType.hpp"
#include "Units/Piece.hpp"
#include "Systems/EventLog.hpp"

namespace {

template <typename PieceLookup>
bool churchCoronationState(const std::vector<sf::Vector2i>& cells,
                           PieceLookup&& lookupPiece,
                           KingdomId kingdomId,
                           bool& hasKing,
                           bool& hasBishop,
                           int& rookId) {
    hasKing = false;
    hasBishop = false;
    rookId = -1;

    for (const sf::Vector2i& pos : cells) {
        const auto* piece = lookupPiece(pos);
        if (!piece) {
            continue;
        }

        if (piece->kingdom != kingdomId) {
            return false;
        }

        if (piece->type == PieceType::King) {
            hasKing = true;
        } else if (piece->type == PieceType::Bishop) {
            hasBishop = true;
        } else if (piece->type == PieceType::Rook && rookId < 0) {
            rookId = piece->id;
        }
    }

    return true;
}

template <typename PieceLookup>
bool canPerformChurchCoronationOnCells(const std::vector<sf::Vector2i>& cells,
                                       KingdomId kingdomId,
                                       PieceLookup&& lookupPiece,
                                       int& rookId) {
    bool hasKing = false;
    bool hasBishop = false;
    rookId = -1;
    if (!churchCoronationState(cells, lookupPiece, kingdomId, hasKing, hasBishop, rookId)) {
        return false;
    }

    return hasKing && hasBishop && rookId >= 0;
}

}

bool MarriageSystem::canPerformChurchCoronation(const Kingdom& kingdom,
                                                const Board& board,
                                                const Building& church) {
    if (church.type != BuildingType::Church || church.isDestroyed()) {
        return false;
    }

    int rookId = -1;
    return canPerformChurchCoronationOnCells(
        church.getOccupiedCells(),
        kingdom.id,
        [&board](const sf::Vector2i& pos) -> const Piece* {
            return board.getCell(pos.x, pos.y).piece;
        },
        rookId);
}

bool MarriageSystem::performChurchCoronation(Kingdom& kingdom,
                                             const Board& board,
                                             const Building& church,
                                             EventLog& log,
                                             int turnNumber) {
    int rookId = -1;
    if (church.type != BuildingType::Church || church.isDestroyed()) {
        return false;
    }

    if (!canPerformChurchCoronationOnCells(
            church.getOccupiedCells(),
            kingdom.id,
            [&board](const sf::Vector2i& pos) -> const Piece* {
                return board.getCell(pos.x, pos.y).piece;
            },
            rookId)) {
        return false;
    }

    Piece* rook = kingdom.getPieceById(rookId);
    if (!rook) {
        return false;
    }

    rook->type = PieceType::Queen;
    log.log(turnNumber, kingdom.id, "Coronation! A rook becomes Queen at the church!");
    return true;
}

bool MarriageSystem::canPerformChurchCoronation(const GameSnapshot& snapshot, KingdomId kingdomId) {
    for (const SnapBuilding& building : snapshot.publicBuildings) {
        if (building.type != BuildingType::Church || building.isDestroyed()) {
            continue;
        }

        int rookId = -1;
        if (canPerformChurchCoronationOnCells(
                building.getOccupiedCells(),
                kingdomId,
                [&snapshot](const sf::Vector2i& pos) -> const SnapPiece* {
                    return snapshot.pieceAt(pos);
                },
                rookId)) {
            return true;
        }
    }

    return false;
}

bool MarriageSystem::applyChurchCoronation(GameSnapshot& snapshot, KingdomId kingdomId) {
    for (const SnapBuilding& building : snapshot.publicBuildings) {
        if (building.type != BuildingType::Church || building.isDestroyed()) {
            continue;
        }

        int rookId = -1;
        if (!canPerformChurchCoronationOnCells(
                building.getOccupiedCells(),
                kingdomId,
                [&snapshot](const sf::Vector2i& pos) -> const SnapPiece* {
                    return snapshot.pieceAt(pos);
                },
                rookId)) {
            continue;
        }

        SnapPiece* rook = snapshot.kingdom(kingdomId).getPieceById(rookId);
        if (!rook) {
            return false;
        }

        rook->type = PieceType::Queen;
        return true;
    }

    return false;
}

bool MarriageSystem::canMarry(const Kingdom& kingdom, const Board& board, const Building& church) {
    return canPerformChurchCoronation(kingdom, board, church);
}

void MarriageSystem::performMarriage(Kingdom& kingdom, const Board& board, const Building& church,
                                      EventLog& log, int turnNumber) {
    performChurchCoronation(kingdom, board, church, log, turnNumber);
}
