#include "AI/AISpecialModule.hpp"
#include "AI/GameSnapshot.hpp"
#include "Buildings/BuildingType.hpp"
#include "Units/PieceType.hpp"
#include <cmath>
#include <algorithm>
#include <climits>

// =========================================================================
//  Church cell helpers
// =========================================================================

std::vector<sf::Vector2i> AISpecialModule::getChurchCells(const GameSnapshot& s) {
    std::vector<sf::Vector2i> cells;
    for (auto& b : s.publicBuildings) {
        if (b.type == BuildingType::Church && !b.isDestroyed()) {
            for (auto& c : b.getOccupiedCells())
                cells.push_back(c);
        }
    }
    return cells;
}

sf::Vector2i AISpecialModule::closestChurchCell(sf::Vector2i from,
                                                   const std::vector<sf::Vector2i>& cells) {
    sf::Vector2i best = from;
    int bestDist = INT_MAX;
    for (auto& c : cells) {
        int d = std::abs(from.x - c.x) + std::abs(from.y - c.y);
        if (d < bestDist) { bestDist = d; best = c; }
    }
    return best;
}

// =========================================================================
//  evaluateMarriage
// =========================================================================

MarriagePlan AISpecialModule::evaluateMarriage(const GameSnapshot& s, KingdomId k) const {
    MarriagePlan plan;

    // Already have a queen → no marriage needed
    if (s.kingdom(k).hasQueen()) return plan;

    // Find required pieces
    auto& kd = s.kingdom(k);
    auto* king = kd.getKing();
    const SnapPiece* bishop = nullptr;
    const SnapPiece* pawn = nullptr;

    if (!king) return plan;

    auto churchCells = getChurchCells(s);
    if (churchCells.empty()) return plan;

    // Find the bishop closest to church
    int bestBishopDist = INT_MAX;
    for (auto& p : kd.pieces) {
        if (p.type == PieceType::Bishop) {
            sf::Vector2i target = closestChurchCell(p.position, churchCells);
            int d = std::abs(p.position.x - target.x) + std::abs(p.position.y - target.y);
            if (d < bestBishopDist) {
                bestBishopDist = d;
                bishop = &p;
            }
        }
    }

    // Find the pawn closest to church
    int bestPawnDist = INT_MAX;
    for (auto& p : kd.pieces) {
        if (p.type == PieceType::Pawn) {
            sf::Vector2i target = closestChurchCell(p.position, churchCells);
            int d = std::abs(p.position.x - target.x) + std::abs(p.position.y - target.y);
            if (d < bestPawnDist) {
                bestPawnDist = d;
                pawn = &p;
            }
        }
    }

    if (!bishop || !pawn) return plan;

    // Assign church cells for each piece
    plan.kingTarget   = closestChurchCell(king->position,   churchCells);
    plan.bishopTarget = closestChurchCell(bishop->position, churchCells);
    plan.pawnTarget   = closestChurchCell(pawn->position,   churchCells);

    // Ensure king and pawn target cells are adjacent (Chebyshev ≤ 1)
    int chebDist = std::max(std::abs(plan.kingTarget.x - plan.pawnTarget.x),
                             std::abs(plan.kingTarget.y - plan.pawnTarget.y));
    if (chebDist > 1) {
        // Try swapping pawn target to a church cell adjacent to king's target
        for (auto& c : churchCells) {
            int d = std::max(std::abs(plan.kingTarget.x - c.x),
                             std::abs(plan.kingTarget.y - c.y));
            if (d <= 1 && c != plan.bishopTarget) {
                plan.pawnTarget = c;
                break;
            }
        }
    }

    // Estimated turns = max distance across the three pieces
    int kingTurns   = std::abs(king->position.x - plan.kingTarget.x)
                    + std::abs(king->position.y - plan.kingTarget.y);
    int bishopTurns = std::abs(bishop->position.x - plan.bishopTarget.x)
                    + std::abs(bishop->position.y - plan.bishopTarget.y);
    int pawnTurns   = std::abs(pawn->position.x - plan.pawnTarget.x)
                    + std::abs(pawn->position.y - plan.pawnTarget.y);
    plan.estimatedTurns = std::max({kingTurns, bishopTurns, pawnTurns});

    // Cost-benefit: Queen value (900) vs lost turns (50 per turn)
    float marriageValue = 900.0f - plan.estimatedTurns * 50.0f;
    if (marriageValue > 0.0f) plan.pursuing = true;

    // Already on church cells and adjacent?
    auto isOnChurch = [&](const SnapPiece* p) {
        for (auto& c : churchCells)
            if (p->position == c) return true;
        return false;
    };
    bool kingOnChurch   = isOnChurch(king);
    bool bishopOnChurch = isOnChurch(bishop);
    bool pawnOnChurch   = isOnChurch(pawn);
    bool adjacent = std::max(std::abs(king->position.x - pawn->position.x),
                              std::abs(king->position.y - pawn->position.y)) <= 1;

    // Check no enemy pieces on church
    bool churchClear = true;
    for (auto& c : churchCells)
        if (s.enemyKingdom(k).getPieceAt(c)) { churchClear = false; break; }

    if (kingOnChurch && bishopOnChurch && pawnOnChurch && adjacent && churchClear) {
        plan.pursuing = true;
        plan.canMarryNow = true;
        plan.estimatedTurns = 0;
    }

    return plan;
}

// =========================================================================
//  getMarriageMoveTarget — what cell should a piece move toward?
// =========================================================================

sf::Vector2i AISpecialModule::getMarriageMoveTarget(const GameSnapshot& s, KingdomId k,
                                                       int pieceId) const {
    auto* piece = s.kingdom(k).getPieceById(pieceId);
    if (!piece) return {0, 0};

    MarriagePlan plan = evaluateMarriage(s, k);
    if (!plan.pursuing) return piece->position;

    auto* king = s.kingdom(k).getKing();
    if (king && piece->id == king->id) return plan.kingTarget;

    if (piece->type == PieceType::Bishop) return plan.bishopTarget;
    if (piece->type == PieceType::Pawn)   return plan.pawnTarget;

    return piece->position;
}
