#include "Systems/CheckSystem.hpp"
#include "Systems/CheckResponseRules.hpp"
#include "Board/Board.hpp"
#include "Board/Cell.hpp"
#include "Kingdom/Kingdom.hpp"
#include "Units/Piece.hpp"
#include "Units/MovementRules.hpp"
#include "Config/GameConfig.hpp"

// ---- Fast overloads (O(pieces) instead of O(diameter^2)) ----

ThreatMap CheckSystem::buildThreatMap(const Kingdom& attacker,
                                       const Board& board, const GameConfig& config) {
    ThreatMap map;
    for (const auto& piece : attacker.pieces) {
        auto moves = MovementRules::getValidMoves(piece, board, config);
        for (const auto& m : moves) {
            map.mark(m);
        }
    }
    return map;
}

bool CheckSystem::isInCheckFast(const Kingdom& kingdom, const Kingdom& enemy,
                                 const Board& board, const GameConfig& config) {
    const Piece* king = kingdom.getKing();
    if (!king) return false;

    // Check directly: can any enemy piece reach the king?
    for (const auto& ep : enemy.pieces) {
        auto moves = MovementRules::getValidMoves(ep, board, config);
        for (const auto& m : moves) {
            if (m == king->position) return true;
        }
    }
    return false;
}

// ---- Legacy signatures (delegate or use Board scan for Game.cpp) ----

bool CheckSystem::isInCheck(KingdomId kingdomId, const Board& board, const GameConfig& config) {
    sf::Vector2i kingPos{-1, -1};
    KingdomId enemyId = (kingdomId == KingdomId::White) ? KingdomId::Black : KingdomId::White;

    int diameter = board.getDiameter();
    for (int y = 0; y < diameter && kingPos.x < 0; ++y) {
        for (int x = 0; x < diameter && kingPos.x < 0; ++x) {
            const Cell& cell = board.getCell(x, y);
            if (cell.piece && cell.piece->kingdom == kingdomId && cell.piece->type == PieceType::King) {
                kingPos = {x, y};
            }
        }
    }

    if (kingPos.x < 0) return false;

    // Check if any enemy piece can reach the king (no full threat map needed)
    for (int y = 0; y < diameter; ++y) {
        for (int x = 0; x < diameter; ++x) {
            const Cell& cell = board.getCell(x, y);
            if (cell.piece && cell.piece->kingdom == enemyId) {
                auto moves = MovementRules::getValidMoves(*cell.piece, board, config);
                for (const auto& m : moves) {
                    if (m == kingPos) return true;
                }
            }
        }
    }
    return false;
}

bool CheckSystem::isCheckmate(KingdomId kingdomId, Board& board, const GameConfig& config) {
    int diameter = board.getDiameter();
    // Board-driven checkmate: in check and no king-safe response move remains.
    if (!isInCheck(kingdomId, board, config)) {
        return false;
    }

    std::vector<Piece*> pieces;
    for (int y = 0; y < diameter; ++y) {
        for (int x = 0; x < diameter; ++x) {
            Cell& cell = board.getCell(x, y);
            if (cell.piece && cell.piece->kingdom == kingdomId) {
                pieces.push_back(cell.piece);
            }
        }
    }

    for (Piece* piece : pieces) {
        if (!CheckResponseRules::filterLegalMovesForPiece(*piece, board, config).empty()) {
            return false;
        }
    }

    return true;
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
