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

    // Find required pieces
    auto& kd = s.kingdom(k);
    auto* king = kd.getKing();
    const SnapPiece* bishop = nullptr;
    const SnapPiece* rook = nullptr;

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

    // Find the rook closest to church
    int bestRookDist = INT_MAX;
    for (auto& p : kd.pieces) {
        if (p.type == PieceType::Rook) {
            sf::Vector2i target = closestChurchCell(p.position, churchCells);
            int d = std::abs(p.position.x - target.x) + std::abs(p.position.y - target.y);
            if (d < bestRookDist) {
                bestRookDist = d;
                rook = &p;
            }
        }
    }

    if (!bishop || !rook) return plan;

    // Assign church cells for each piece
    plan.kingTarget   = closestChurchCell(king->position,   churchCells);
    plan.bishopTarget = closestChurchCell(bishop->position, churchCells);
    plan.rookTarget   = closestChurchCell(rook->position,   churchCells);

    // Estimated turns = max distance across the three pieces
    int kingTurns   = std::abs(king->position.x - plan.kingTarget.x)
                    + std::abs(king->position.y - plan.kingTarget.y);
    int bishopTurns = std::abs(bishop->position.x - plan.bishopTarget.x)
                    + std::abs(bishop->position.y - plan.bishopTarget.y);
    int rookTurns   = std::abs(rook->position.x - plan.rookTarget.x)
                    + std::abs(rook->position.y - plan.rookTarget.y);
    plan.estimatedTurns = std::max({kingTurns, bishopTurns, rookTurns});

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
    bool rookOnChurch   = isOnChurch(rook);

    // Check no enemy pieces on church
    bool churchClear = true;
    for (auto& c : churchCells)
        if (s.enemyKingdom(k).getPieceAt(c)) { churchClear = false; break; }

    if (kingOnChurch && bishopOnChurch && rookOnChurch && churchClear) {
        plan.pursuing = true;
        plan.canCoronateNow = true;
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
    if (piece->type == PieceType::Rook)   return plan.rookTarget;

    return piece->position;
}
