#include "AI/CheckmateSolver.hpp"
#include "AI/ForwardModel.hpp"
#include "AI/ThreatMap.hpp"
#include <cmath>

// Forward declaration of local helper
static bool s_isLegalKingEscape(const GameSnapshot& sim, sf::Vector2i kingPos,
                                  sf::Vector2i esc, KingdomId defender,
                                  KingdomId attacker, const ThreatMap& attackerThreats,
                                  int globalMaxRange);

// =========================================================================
//  wouldGiveCheck — fast pre-filter before full simulation
// =========================================================================

bool CheckmateSolver::wouldGiveCheck(const GameSnapshot& s, const SnapPiece& piece,
                                       sf::Vector2i dest, sf::Vector2i eKingPos,
                                       int globalMaxRange) {
    int dx = eKingPos.x - dest.x;
    int dy = eKingPos.y - dest.y;

    switch (piece.type) {
        case PieceType::Pawn:
            // Pawn attacks 4-directionally at distance 1
            return (std::abs(dx) + std::abs(dy) == 1);

        case PieceType::Knight: {
            int adx = std::abs(dx), ady = std::abs(dy);
            return (adx == 1 && ady == 2) || (adx == 2 && ady == 1);
        }

        case PieceType::Bishop:
            if (std::abs(dx) != std::abs(dy) || dx == 0) return false;
            // Check line of sight
            {
                int sdx = (dx > 0) ? 1 : -1;
                int sdy = (dy > 0) ? 1 : -1;
                int dist = std::abs(dx);
                if (dist > globalMaxRange) return false;
                for (int i = 1; i < dist; ++i) {
                    int cx = dest.x + sdx * i, cy = dest.y + sdy * i;
                    if (s.pieceAt({cx, cy})) return false;
                    if (!s.isTraversable(cx, cy)) return false;
                }
            }
            return true;

        case PieceType::Rook:
            if (dx != 0 && dy != 0) return false;
            {
                int dir = (dx != 0) ? ((dx > 0) ? 1 : -1) : 0;
                int dirY = (dy != 0) ? ((dy > 0) ? 1 : -1) : 0;
                int dist = std::max(std::abs(dx), std::abs(dy));
                if (dist > globalMaxRange) return false;
                for (int i = 1; i < dist; ++i) {
                    int cx = dest.x + dir * i, cy = dest.y + dirY * i;
                    if (s.pieceAt({cx, cy})) return false;
                    if (!s.isTraversable(cx, cy)) return false;
                }
            }
            return true;

        case PieceType::Queen:
            // Queen = Rook + Bishop
            if (dx == 0 && dy == 0) return false;
            if (dx == 0 || dy == 0 || std::abs(dx) == std::abs(dy)) {
                int sdx = (dx != 0) ? ((dx > 0) ? 1 : -1) : 0;
                int sdy = (dy != 0) ? ((dy > 0) ? 1 : -1) : 0;
                int dist = std::max(std::abs(dx), std::abs(dy));
                if (dist > globalMaxRange) return false;
                for (int i = 1; i < dist; ++i) {
                    int cx = dest.x + sdx * i, cy = dest.y + sdy * i;
                    if (s.pieceAt({cx, cy})) return false;
                    if (!s.isTraversable(cx, cy)) return false;
                }
                return true;
            }
            return false;

        case PieceType::King:
            return false; // King doesn't give check

        default:
            return false;
    }
}

// =========================================================================
//  Escape squares computation
// =========================================================================

std::vector<sf::Vector2i> CheckmateSolver::computeEscapeSquares(
    const GameSnapshot& s, sf::Vector2i eKingPos, KingdomId enemyKingdom)
{
    std::vector<sf::Vector2i> escapes;
    for (int dy = -1; dy <= 1; ++dy) {
        for (int dx = -1; dx <= 1; ++dx) {
            if (dx == 0 && dy == 0) continue;
            int nx = eKingPos.x + dx, ny = eKingPos.y + dy;
            if (!s.isTraversable(nx, ny)) continue;
            // Not blocked by own piece
            if (s.kingdom(enemyKingdom).getPieceAt({nx, ny})) continue;
            escapes.push_back({nx, ny});
        }
    }
    return escapes;
}

// =========================================================================
//  Can a defender block/capture to stop the check?
// =========================================================================

bool CheckmateSolver::canDefendCheck(const GameSnapshot& s, KingdomId defender,
                                       int globalMaxRange) {
    // For each defender piece (not king, king escape already checked),
    // try all moves. If any results in the king NOT being in check, return true.
    for (auto& piece : s.kingdom(defender).pieces) {
        if (piece.type == PieceType::King) continue;
        auto moves = ForwardModel::getLegalMoves(s, piece, globalMaxRange);
        for (auto& dest : moves) {
            GameSnapshot sim = s.clone();
            ForwardModel::applyMove(sim, piece.id, dest, defender);
            if (!ForwardModel::isInCheck(sim, defender, globalMaxRange))
                return true;
        }
    }
    return false;
}

// =========================================================================
//  Helper: check if a king escape to 'esc' is legal
// =========================================================================

static bool s_isLegalKingEscape(const GameSnapshot& sim, sf::Vector2i /*kingPos*/,
                                  sf::Vector2i esc, KingdomId defender,
                                  KingdomId attacker, const ThreatMap& attackerThreats,
                                  int /*globalMaxRange*/) {
    if (!sim.isTraversable(esc.x, esc.y)) return false;
    if (sim.kingdom(defender).getPieceAt(esc)) return false;
    if (attackerThreats.isSet(esc.x, esc.y)) return false;
    auto* aKing = sim.kingdom(attacker).getKing();
    if (aKing) {
        int dx = std::abs(esc.x - aKing->position.x);
        int dy = std::abs(esc.y - aKing->position.y);
        if (dx <= 1 && dy <= 1) return false;
    }
    return true;
}

// =========================================================================
//  findMateIn1 — optimized with wouldGiveCheck pre-filter
// =========================================================================

std::optional<MateMove> CheckmateSolver::findMateIn1(const GameSnapshot& snapshot,
                                                       KingdomId aiKingdom,
                                                       int globalMaxRange) const {
    auto* eKing = snapshot.enemyKingdom(aiKingdom).getKing();
    if (!eKing) return std::nullopt;
    sf::Vector2i eKingPos = eKing->position;
    KingdomId enemyId = snapshot.enemyKingdom(aiKingdom).id;

    // Pre-compute escape squares once
    auto escapes = computeEscapeSquares(snapshot, eKingPos, enemyId);

    for (auto& piece : snapshot.kingdom(aiKingdom).pieces) {
        if (piece.type == PieceType::King) continue; // King rarely mates alone

        auto moves = ForwardModel::getLegalMoves(snapshot, piece, globalMaxRange);
        for (auto& dest : moves) {
            if (dest == eKingPos) continue; // Can't capture king directly

            // Pre-filter: does this move give check?
            // We need to check on the post-move state since the piece moves
            // Create a temporary piece at dest to check
            if (!wouldGiveCheck(snapshot, piece, dest, eKingPos, globalMaxRange))
                continue;

            // Full simulation
            GameSnapshot sim = snapshot.clone();
            ForwardModel::applyMove(sim, piece.id, dest, aiKingdom);

            // Verify king is actually in check after the move
            if (!ForwardModel::isInCheck(sim, enemyId, globalMaxRange))
                continue;

            // Can the enemy escape?
            // 1. Try king moves
            bool canEscape = false;
            auto* simEKing = sim.kingdom(enemyId).getKing();
            if (simEKing) {
                ThreatMap threats = ForwardModel::buildThreatMap(sim, aiKingdom, globalMaxRange);
                for (auto& esc : escapes) {
                    if (s_isLegalKingEscape(sim, simEKing->position, esc,
                                             enemyId, aiKingdom, threats, globalMaxRange)) {
                        canEscape = true;
                        break;
                    }
                }
            }

            // 2. Try interpositions / captures by other pieces
            if (!canEscape)
                canEscape = canDefendCheck(sim, enemyId, globalMaxRange);

            if (!canEscape)
                return MateMove{piece.id, dest};
        }
    }

    return std::nullopt;
}

// =========================================================================
//  findMateInN — deeper alpha-beta search on check-giving moves
// =========================================================================

std::optional<MateMove> CheckmateSolver::findMateInN(const GameSnapshot& snapshot,
                                                       KingdomId aiKingdom,
                                                       int maxDepth,
                                                       int globalMaxRange,
                                                       int budgetMs) const {
    TimeBudget timer(budgetMs);
    return alphaBetaMate(snapshot, aiKingdom, 0, maxDepth, globalMaxRange, timer);
}

std::optional<MateMove> CheckmateSolver::alphaBetaMate(const GameSnapshot& s,
                                                         KingdomId ai,
                                                         int depth, int maxDepth,
                                                         int globalMaxRange,
                                                         TimeBudget& timer) const {
    if (timer.expired()) return std::nullopt;
    if (depth >= maxDepth) return std::nullopt;

    KingdomId enemyId = (ai == KingdomId::White) ? KingdomId::Black : KingdomId::White;

    if (depth % 2 == 0) {
        // AI's turn: try check-giving or escape-blocking moves
        for (auto& piece : s.kingdom(ai).pieces) {
            if (piece.type == PieceType::King) continue;
            if (timer.expired()) return std::nullopt;

            auto moves = ForwardModel::getLegalMoves(s, piece, globalMaxRange);
            for (auto& dest : moves) {
                GameSnapshot sim = s.clone();
                ForwardModel::applyMove(sim, piece.id, dest, ai);

                // Check if it's checkmate
                if (ForwardModel::isCheckmate(sim, enemyId, globalMaxRange))
                    return MateMove{piece.id, dest};

                // Only continue if this gives check (prune non-check moves for deeper search)
                if (!ForwardModel::isInCheck(sim, enemyId, globalMaxRange))
                    continue;

                // Recurse as enemy's turn
                auto result = alphaBetaMate(sim, ai, depth + 1, maxDepth,
                                             globalMaxRange, timer);
                if (result.has_value() && depth == 0)
                    return MateMove{piece.id, dest};
                if (result.has_value())
                    return result; // pass up
            }
        }
    } else {
        // Enemy's turn: try all escape moves. If ALL lead to mate, the parent move is good.
        bool allLeadToMate = true;
        for (auto& piece : s.kingdom(enemyId).pieces) {
            if (timer.expired()) return std::nullopt;
            auto moves = ForwardModel::getLegalMoves(s, piece, globalMaxRange);
            for (auto& dest : moves) {
                GameSnapshot sim = s.clone();
                ForwardModel::applyMove(sim, piece.id, dest, enemyId);

                if (!ForwardModel::isInCheck(sim, enemyId, globalMaxRange)) {
                    // Enemy found a way out, try to continue from here
                    auto result = alphaBetaMate(sim, ai, depth + 1, maxDepth,
                                                 globalMaxRange, timer);
                    if (!result.has_value()) {
                        allLeadToMate = false;
                        break;
                    }
                }
            }
            if (!allLeadToMate) break;
        }
        if (allLeadToMate)
            return MateMove{-1, {0, 0}}; // Sentinel: parent's move leads to forced mate
    }

    return std::nullopt;
}
