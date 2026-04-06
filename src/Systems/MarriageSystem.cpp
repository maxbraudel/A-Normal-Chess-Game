#include "Systems/MarriageSystem.hpp"
#include "Kingdom/Kingdom.hpp"
#include "Board/Board.hpp"
#include "Board/Cell.hpp"
#include "Buildings/Building.hpp"
#include "Buildings/BuildingType.hpp"
#include "Units/Piece.hpp"
#include "Systems/EventLog.hpp"
#include <cmath>

bool MarriageSystem::canMarry(const Kingdom& kingdom, const Board& board, const Building& church) {
    if (church.type != BuildingType::Church) return false;
    if (kingdom.hasQueen()) return false;

    auto cells = church.getOccupiedCells();

    // Check if king, bishop, and pawn are all on church cells
    const Piece* king = nullptr;
    const Piece* bishop = nullptr;
    const Piece* pawn = nullptr;
    bool enemyPresent = false;

    for (const auto& pos : cells) {
        const Cell& cell = board.getCell(pos.x, pos.y);
        if (cell.piece) {
            if (cell.piece->kingdom != kingdom.id) {
                enemyPresent = true;
                continue;
            }
            if (cell.piece->type == PieceType::King) king = cell.piece;
            else if (cell.piece->type == PieceType::Bishop) bishop = cell.piece;
            else if (cell.piece->type == PieceType::Pawn) pawn = cell.piece;
        }
    }

    if (enemyPresent) return false;
    if (!king || !bishop || !pawn) return false;

    // King and pawn must be adjacent
    int dx = std::abs(king->position.x - pawn->position.x);
    int dy = std::abs(king->position.y - pawn->position.y);
    if (dx > 1 || dy > 1 || (dx == 0 && dy == 0)) return false;

    return true;
}

void MarriageSystem::performMarriage(Kingdom& kingdom, const Board& board, const Building& church,
                                      EventLog& log, int turnNumber) {
    if (!canMarry(kingdom, board, church)) return;

    auto cells = church.getOccupiedCells();
    for (const auto& pos : cells) {
        const Cell& cell = board.getCell(pos.x, pos.y);
        if (cell.piece && cell.piece->kingdom == kingdom.id && cell.piece->type == PieceType::Pawn) {
            // Find this piece in kingdom
            Piece* pawn = kingdom.getPieceById(cell.piece->id);
            if (pawn) {
                pawn->type = PieceType::Queen;
                log.log(turnNumber, kingdom.id, "Marriage! A pawn becomes Queen!");
                return;
            }
        }
    }
}
