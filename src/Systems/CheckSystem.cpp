#include "Systems/CheckSystem.hpp"
#include "Board/Board.hpp"
#include "Board/Cell.hpp"
#include "Units/Piece.hpp"
#include "Units/MovementRules.hpp"
#include "Config/GameConfig.hpp"

bool CheckSystem::isInCheck(KingdomId kingdomId, const Board& board, const GameConfig& config) {
    // Find the king
    const Piece* king = nullptr;
    KingdomId enemyId = (kingdomId == KingdomId::White) ? KingdomId::Black : KingdomId::White;

    int diameter = board.getDiameter();
    for (int y = 0; y < diameter && !king; ++y) {
        for (int x = 0; x < diameter && !king; ++x) {
            const Cell& cell = board.getCell(x, y);
            if (cell.piece && cell.piece->kingdom == kingdomId && cell.piece->type == PieceType::King) {
                king = cell.piece;
            }
        }
    }

    if (!king) return false; // No king = initial pawn case handled differently

    // Check if any enemy piece can reach the king
    auto threatened = getThreatenedSquares(enemyId, board, config);
    return threatened.count(king->position) > 0;
}

bool CheckSystem::isCheckmate(KingdomId kingdomId, Board& board, const GameConfig& config) {
    // Find the king
    const Piece* king = nullptr;
    int diameter = board.getDiameter();

    for (int y = 0; y < diameter && !king; ++y) {
        for (int x = 0; x < diameter && !king; ++x) {
            const Cell& cell = board.getCell(x, y);
            if (cell.piece && cell.piece->kingdom == kingdomId && cell.piece->type == PieceType::King) {
                king = cell.piece;
            }
        }
    }

    if (!king) return true; // No king means initial pawn was killed

    if (!isInCheck(kingdomId, board, config)) return false;

    // Collect all kingdom pieces
    std::vector<Piece*> pieces;
    for (int y = 0; y < diameter; ++y) {
        for (int x = 0; x < diameter; ++x) {
            Cell& cell = board.getCell(x, y);
            if (cell.piece && cell.piece->kingdom == kingdomId) {
                pieces.push_back(cell.piece);
            }
        }
    }

    // Try every possible move of every piece
    for (Piece* piece : pieces) {
        auto moves = MovementRules::getValidMoves(*piece, board, config);
        for (const auto& move : moves) {
            // Simulate the move
            sf::Vector2i oldPos = piece->position;
            Cell& oldCell = board.getCell(oldPos.x, oldPos.y);
            Cell& newCell = board.getCell(move.x, move.y);

            Piece* capturedPiece = newCell.piece;
            Building* capturedBuilding = newCell.building;

            // Apply move temporarily
            oldCell.piece = nullptr;
            Piece* oldPieceAtTarget = newCell.piece;
            newCell.piece = piece;
            piece->position = move;

            // Remove captured piece from check consideration
            if (capturedPiece && capturedPiece->kingdom != kingdomId) {
                newCell.piece = piece; // attacker takes the spot
            }

            bool stillInCheck = isInCheck(kingdomId, board, config);

            // Revert
            piece->position = oldPos;
            newCell.piece = oldPieceAtTarget;
            oldCell.piece = piece;

            if (!stillInCheck) return false; // Found an escape
        }
    }

    return true; // No move saves the king
}

bool CheckSystem::isSafeSquare(sf::Vector2i pos, KingdomId kingdomId, const Board& board, const GameConfig& config) {
    KingdomId enemyId = (kingdomId == KingdomId::White) ? KingdomId::Black : KingdomId::White;
    auto threatened = getThreatenedSquares(enemyId, board, config);
    return threatened.count(pos) == 0;
}

std::set<sf::Vector2i, Vec2iCompare> CheckSystem::getThreatenedSquares(
    KingdomId attackerKingdom, const Board& board, const GameConfig& config) {
    std::set<sf::Vector2i, Vec2iCompare> threatened;
    int diameter = board.getDiameter();

    for (int y = 0; y < diameter; ++y) {
        for (int x = 0; x < diameter; ++x) {
            const Cell& cell = board.getCell(x, y);
            if (cell.piece && cell.piece->kingdom == attackerKingdom) {
                auto moves = MovementRules::getValidMoves(*cell.piece, board, config);
                for (const auto& m : moves) {
                    threatened.insert(m);
                }
            }
        }
    }

    return threatened;
}
