#include "AI/AIEvaluator.hpp"
#include "AI/GameSnapshot.hpp"
#include "AI/ForwardModel.hpp"
#include "AI/ThreatMap.hpp"
#include "AI/AIBrain.hpp"         // for AIPhase
#include "Board/Board.hpp"
#include "Board/Cell.hpp"
#include "Kingdom/Kingdom.hpp"
#include "Units/Piece.hpp"
#include "Units/PieceType.hpp"
#include "Units/MovementRules.hpp"
#include "Buildings/Building.hpp"
#include "Buildings/BuildingType.hpp"
#include "Config/GameConfig.hpp"
#include "Systems/EconomySystem.hpp"
#include <cmath>
#include <algorithm>

// =========================================================================
//  Piece value table (centipawn-style, used by both legacy and new API)
// =========================================================================

float AIEvaluator::pieceValue(PieceType type) {
    switch (type) {
        case PieceType::Pawn:   return 100.0f;
        case PieceType::Knight: return 320.0f;
        case PieceType::Bishop: return 330.0f;
        case PieceType::Rook:   return 500.0f;
        case PieceType::Queen:  return 900.0f;
        case PieceType::King:   return 10000.0f;
        default: return 0.0f;
    }
}

// Legacy piece value helper (old 1/3/5/9 scale, kept for backward compat)
static float legacyPieceValue(PieceType type) {
    switch (type) {
        case PieceType::Pawn:   return 1.0f;
        case PieceType::Knight: return 3.0f;
        case PieceType::Bishop: return 3.0f;
        case PieceType::Rook:   return 5.0f;
        case PieceType::Queen:  return 9.0f;
        case PieceType::King:   return 100.0f;
        default: return 0.0f;
    }
}

// =========================================================================
//  Phase-based weight tables
// =========================================================================

EvalWeights AIEvaluator::weightsForPhase(AIPhase phase) {
    EvalWeights w;
    switch (phase) {
        case AIPhase::EARLY_GAME:
            w = {1.0f, 3.0f, 2.0f, 1.0f, 2.0f, 0.5f, 0.3f}; break;
        case AIPhase::BUILD_UP:
            w = {1.5f, 2.0f, 1.5f, 1.0f, 1.5f, 1.0f, 0.5f}; break;
        case AIPhase::MID_GAME:
            w = {2.0f, 1.0f, 1.0f, 1.5f, 1.0f, 1.5f, 1.5f}; break;
        case AIPhase::AGGRESSION:
            w = {2.0f, 0.3f, 0.5f, 1.5f, 0.3f, 2.0f, 3.0f}; break;
        case AIPhase::ENDGAME:
            w = {1.5f, 0.2f, 0.3f, 2.0f, 0.2f, 2.0f, 4.0f}; break;
        case AIPhase::CRISIS:
            w = {0.5f, 0.1f, 0.1f, 5.0f, 0.1f, 0.5f, 0.5f}; break;
    }
    return w;
}

// =========================================================================
//  NEW: Full evaluation on GameSnapshot (main API for MCTS / AI Director)
// =========================================================================

float AIEvaluator::evaluate(const GameSnapshot& s, KingdomId perspective,
                             int globalMaxRange, const EvalWeights& w) {
    float score = 0.0f;
    score += w.material    * scoreMaterial(s, perspective);
    score += w.economy     * scoreEconomy(s, perspective);
    score += w.mapControl  * scoreMapControl(s, perspective);
    score += w.kingSafety  * scoreKingSafety(s, perspective, globalMaxRange);
    score += w.development * scoreDevelopment(s, perspective);
    score += w.threat      * scoreThreat(s, perspective, globalMaxRange);
    score += w.checkmate   * scoreCheckmateProximity(s, perspective, globalMaxRange);
    return score;
}

// =========================================================================
//  Component: Material
// =========================================================================

float AIEvaluator::scoreMaterial(const GameSnapshot& s, KingdomId k) {
    float myMat = 0.0f, enemyMat = 0.0f;
    for (auto& p : s.kingdom(k).pieces)        myMat    += pieceValue(p.type);
    for (auto& p : s.enemyKingdom(k).pieces)   enemyMat += pieceValue(p.type);
    return myMat - enemyMat;
}

// =========================================================================
//  Component: Economy
// =========================================================================

// Helper: count exclusive income cells owned by kingdom
static int countIncomePerTurn(const GameSnapshot& s, KingdomId k,
                               int mineIncome, int farmIncome) {
    int income = 0;
    auto& kd = s.kingdom(k);
    auto& enemy = s.enemyKingdom(k);

    // Iterate buildings of both sides + public, check mines/farms
    auto checkBuilding = [&](const SnapBuilding& b) {
        if (b.isDestroyed()) return;
        if (b.type != BuildingType::Mine && b.type != BuildingType::Farm) return;
        int incPerCell = (b.type == BuildingType::Mine) ? mineIncome : farmIncome;
        int myOccupiedCells = 0;
        int enemyOccupiedCells = 0;
        for (auto& cell : b.getOccupiedCells()) {
            if (kd.getPieceAt(cell)) ++myOccupiedCells;
            if (enemy.getPieceAt(cell)) ++enemyOccupiedCells;
        }

        const ResourceIncomeBreakdown breakdown = (k == KingdomId::White)
            ? EconomySystem::calculateResourceIncomeFromOccupation(myOccupiedCells, enemyOccupiedCells, incPerCell)
            : EconomySystem::calculateResourceIncomeFromOccupation(enemyOccupiedCells, myOccupiedCells, incPerCell);
        income += breakdown.incomeFor(k);
    };

    for (auto& b : kd.buildings)      checkBuilding(b);
    for (auto& b : enemy.buildings)    checkBuilding(b);
    for (auto& b : s.publicBuildings)  checkBuilding(b);
    return income;
}

static int countBarracks(const GameSnapshot& s, KingdomId k) {
    int n = 0;
    for (auto& b : s.kingdom(k).buildings)
        if (b.type == BuildingType::Barracks && !b.isDestroyed()) ++n;
    return n;
}

float AIEvaluator::scoreEconomy(const GameSnapshot& s, KingdomId k,
                                  int mineIncome, int farmIncome) {
    float goldScore     = s.kingdom(k).gold * 0.5f;
    float myIncome      = countIncomePerTurn(s, k, mineIncome, farmIncome) * 5.0f;
    float enemyIncome   = countIncomePerTurn(s, s.enemyKingdom(k).id,
                                                mineIncome, farmIncome) * 5.0f;
    float barracksScore = countBarracks(s, k) * 30.0f;
    return goldScore + myIncome - enemyIncome + barracksScore;
}

// =========================================================================
//  Component: Map Control
// =========================================================================

// Count resource (Mine/Farm) cells exclusively occupied by kingdom
static int countOccupiedResourceCells(const GameSnapshot& s, KingdomId k) {
    int count = 0;
    auto& kd = s.kingdom(k);
    auto& enemy = s.enemyKingdom(k);

    auto checkBuilding = [&](const SnapBuilding& b) {
        if (b.isDestroyed()) return;
        if (b.type != BuildingType::Mine && b.type != BuildingType::Farm) return;
        for (auto& cell : b.getOccupiedCells())
            if (kd.getPieceAt(cell) && !enemy.getPieceAt(cell)) ++count;
    };

    for (auto& b : kd.buildings)       checkBuilding(b);
    for (auto& b : enemy.buildings)    checkBuilding(b);
    for (auto& b : s.publicBuildings)  checkBuilding(b);
    return count;
}

static int countContestedResourceCells(const GameSnapshot& s) {
    int count = 0;
    auto checkBuilding = [&](const SnapBuilding& b) {
        if (b.isDestroyed()) return;
        if (b.type != BuildingType::Mine && b.type != BuildingType::Farm) return;
        for (auto& cell : b.getOccupiedCells())
            if (s.white.getPieceAt(cell) && s.black.getPieceAt(cell)) ++count;
    };
    for (auto& b : s.white.buildings)  checkBuilding(b);
    for (auto& b : s.black.buildings)  checkBuilding(b);
    for (auto& b : s.publicBuildings)  checkBuilding(b);
    return count;
}

// Check if kingdom has a piece on any Church cell
static bool hasChurchControl(const GameSnapshot& s, KingdomId k) {
    for (auto& b : s.publicBuildings) {
        if (b.type != BuildingType::Church || b.isDestroyed()) continue;
        for (auto& cell : b.getOccupiedCells())
            if (s.kingdom(k).getPieceAt(cell)) return true;
    }
    return false;
}

static int countArenaControl(const GameSnapshot& s, KingdomId k) {
    int count = 0;
    auto check = [&](const SnapBuilding& b) {
        if (b.type != BuildingType::Arena || b.isDestroyed()) return;
        for (auto& cell : b.getOccupiedCells())
            if (s.kingdom(k).getPieceAt(cell)) { ++count; return; }
    };
    for (auto& b : s.kingdom(k).buildings)       check(b);
    for (auto& b : s.enemyKingdom(k).buildings)  check(b);
    for (auto& b : s.publicBuildings)             check(b);
    return count;
}

float AIEvaluator::scoreMapControl(const GameSnapshot& s, KingdomId k) {
    int myRes     = countOccupiedResourceCells(s, k);
    int enemyRes  = countOccupiedResourceCells(s, s.enemyKingdom(k).id);
    int contested = countContestedResourceCells(s);
    float church  = hasChurchControl(s, k) ? 50.0f : 0.0f;
    float arena   = countArenaControl(s, k) * 20.0f;
    return (myRes - enemyRes) * 15.0f
           - contested * 5.0f
           + church
           + arena;
}

// =========================================================================
//  Component: King Safety
// =========================================================================

// Count safe adjacent squares for a king
static int countSafeAdjacentSquares(const GameSnapshot& s, sf::Vector2i kingPos,
                                     KingdomId k, int globalMaxRange) {
    int safe = 0;
    ThreatMap enemyThreats = ForwardModel::buildThreatMap(s, s.enemyKingdom(k).id,
                                                           globalMaxRange);
    for (int dy = -1; dy <= 1; ++dy) {
        for (int dx = -1; dx <= 1; ++dx) {
            if (dx == 0 && dy == 0) continue;
            int nx = kingPos.x + dx, ny = kingPos.y + dy;
            if (!s.isTraversable(nx, ny)) continue;
            if (s.kingdom(k).getPieceAt({nx, ny})) continue;  // blocked by own piece
            if (!enemyThreats.isSet(nx, ny)) ++safe;
        }
    }
    return safe;
}

// Count friendly pieces within Manhattan distance of a position
static int countPiecesInRadius(const GameSnapshot& s, KingdomId k,
                                sf::Vector2i center, int radius) {
    int n = 0;
    for (auto& p : s.kingdom(k).pieces) {
        if (p.type == PieceType::King) continue;
        int dist = std::abs(p.position.x - center.x)
                 + std::abs(p.position.y - center.y);
        if (dist <= radius) ++n;
    }
    return n;
}

float AIEvaluator::scoreKingSafety(const GameSnapshot& s, KingdomId k,
                                     int globalMaxRange) {
    float score = 0.0f;
    auto* myKing = s.kingdom(k).getKing();
    if (!myKing) return -10000.0f;

    sf::Vector2i kPos = myKing->position;

    // In check = very bad
    if (ForwardModel::isInCheck(s, k, globalMaxRange)) score -= 500.0f;

    // Escape squares
    int escapes = countSafeAdjacentSquares(s, kPos, k, globalMaxRange);
    score += escapes * 30.0f;

    // Defenders near king (Manhattan ≤ 3)
    int defenders = countPiecesInRadius(s, k, kPos, 3);
    score += defenders * 40.0f;

    // Enemy king in check = good for us
    auto* eKing = s.enemyKingdom(k).getKing();
    if (eKing && ForwardModel::isInCheck(s, s.enemyKingdom(k).id, globalMaxRange))
        score += 400.0f;

    // Enemy escape squares (fewer = better)
    if (eKing) {
        int enemyEscapes = countSafeAdjacentSquares(s, eKing->position,
                                                      s.enemyKingdom(k).id, globalMaxRange);
        score -= enemyEscapes * 25.0f;
    }

    return score;
}

// =========================================================================
//  Component: Development
// =========================================================================

float AIEvaluator::scoreDevelopment(const GameSnapshot& s, KingdomId k) {
    float score = 0.0f;
    // Production in progress = future value
    for (auto& b : s.kingdom(k).buildings) {
        if (b.isProducing && !b.isDestroyed())
            score += pieceValue(b.producingType) * 0.3f;
    }
    // Cumulative XP
    for (auto& p : s.kingdom(k).pieces)
        score += p.xp * 0.1f;
    // Having a queen = big bonus
    if (s.kingdom(k).hasQueen()) score += 200.0f;
    return score;
}

// =========================================================================
//  Component: Threat
// =========================================================================

float AIEvaluator::scoreThreat(const GameSnapshot& s, KingdomId k,
                                 int globalMaxRange) {
    float score = 0.0f;
    ThreatMap myThreats    = ForwardModel::buildThreatMap(s, k, globalMaxRange);
    ThreatMap enemyThreats = ForwardModel::buildThreatMap(s, s.enemyKingdom(k).id,
                                                           globalMaxRange);
    // Enemy pieces under our threat = good
    for (auto& ep : s.enemyKingdom(k).pieces)
        if (myThreats.isSet(ep.position.x, ep.position.y))
            score += pieceValue(ep.type) * 0.3f;

    // Our pieces under enemy threat = bad
    for (auto& mp : s.kingdom(k).pieces)
        if (enemyThreats.isSet(mp.position.x, mp.position.y))
            score -= pieceValue(mp.type) * 0.4f;

    return score;
}

// =========================================================================
//  Component: Checkmate Proximity  (the most critical for finishing games)
// =========================================================================

float AIEvaluator::scoreCheckmateProximity(const GameSnapshot& s, KingdomId k,
                                             int globalMaxRange) {
    float score = 0.0f;
    auto* eKing = s.enemyKingdom(k).getKing();
    if (!eKing) return 100000.0f;  // enemy king gone = we won

    sf::Vector2i eKingPos = eKing->position;

    // Enemy in check
    if (ForwardModel::isInCheck(s, s.enemyKingdom(k).id, globalMaxRange))
        score += 300.0f;

    // Enemy in checkmate
    if (ForwardModel::isCheckmate(s, s.enemyKingdom(k).id, globalMaxRange))
        score += 100000.0f;

    // We are checkmated
    if (ForwardModel::isCheckmate(s, k, globalMaxRange))
        score -= 100000.0f;

    // Blocked escape squares of enemy king
    int blockedEscapes = 8 - countSafeAdjacentSquares(s, eKingPos,
                                                        s.enemyKingdom(k).id,
                                                        globalMaxRange);
    score += blockedEscapes * 40.0f;

    // Average Manhattan distance of our pieces to enemy king
    float totalDist = 0.0f;
    int pieceCount = 0;
    for (auto& p : s.kingdom(k).pieces) {
        if (p.type == PieceType::King) continue;
        totalDist += static_cast<float>(std::abs(p.position.x - eKingPos.x)
                                      + std::abs(p.position.y - eKingPos.y));
        ++pieceCount;
    }
    if (pieceCount > 0) {
        float avgDist = totalDist / static_cast<float>(pieceCount);
        score += std::max(0.0f, 100.0f - avgDist * 3.0f);
    }

    // Pieces in assault ring (Manhattan ≤ 3 of enemy king)
    int assaultPieces = countPiecesInRadius(s, k, eKingPos, 3);
    score += assaultPieces * 50.0f;

    return score;
}

// =========================================================================
//  LEGACY API (unchanged behavior, kept for backward compatibility)
// =========================================================================

float AIEvaluator::evaluate(const Board& board, const Kingdom& self, const Kingdom& enemy,
                              const GameConfig& config) {
    float score = 0.0f;
    score += scoreMaterial(self) - scoreMaterial(enemy);
    score += scoreIncome(self, board, config) * 2.0f;
    score += scoreKingSafety(self, board, config) * 1.5f;
    score += scoreTerritorialControl(self, board) * 0.5f;
    score += scoreBuildings(self) - scoreBuildings(enemy) * 0.5f;
    score -= scoreThreats(self, enemy, board, config) * 1.0f;
    return score;
}

float AIEvaluator::scoreMaterial(const Kingdom& kingdom) {
    float score = 0.0f;
    for (const auto& p : kingdom.pieces) {
        score += legacyPieceValue(p.type);
    }
    return score;
}

float AIEvaluator::scoreIncome(const Kingdom& kingdom, const Board& board, const GameConfig& config) {
    float income = 0.0f;
    for (const auto& p : kingdom.pieces) {
        const Cell& cell = board.getCell(p.position.x, p.position.y);
        if (cell.building && (cell.building->type == BuildingType::Mine || cell.building->type == BuildingType::Farm)) {
            income += 1.0f;
        }
    }
    return income;
}

float AIEvaluator::scoreKingSafety(const Kingdom& kingdom, const Board& board, const GameConfig& config) {
    const Piece* king = kingdom.getKing();
    if (!king) return -100.0f;

    float safety = 0.0f;
    for (const auto& p : kingdom.pieces) {
        if (p.type == PieceType::King) continue;
        int dx = std::abs(p.position.x - king->position.x);
        int dy = std::abs(p.position.y - king->position.y);
        if (dx <= 3 && dy <= 3) {
            safety += 1.0f;
        }
    }
    for (const auto& b : kingdom.buildings) {
        if (b.type == BuildingType::WoodWall || b.type == BuildingType::StoneWall) {
            for (auto& cell : b.getOccupiedCells()) {
                int dx = std::abs(cell.x - king->position.x);
                int dy = std::abs(cell.y - king->position.y);
                if (dx <= 2 && dy <= 2) {
                    safety += 0.5f;
                }
            }
        }
    }
    return safety;
}

float AIEvaluator::scoreTerritorialControl(const Kingdom& kingdom, const Board& board) {
    float control = 0.0f;
    int center = board.getRadius();
    for (const auto& p : kingdom.pieces) {
        int dx = std::abs(p.position.x - center);
        int dy = std::abs(p.position.y - center);
        float dist = std::sqrt(static_cast<float>(dx * dx + dy * dy));
        control += std::max(0.0f, 1.0f - dist / static_cast<float>(board.getRadius()));
    }
    return control;
}

float AIEvaluator::scoreBuildings(const Kingdom& kingdom) {
    float score = 0.0f;
    for (const auto& b : kingdom.buildings) {
        if (b.isDestroyed()) continue;
        switch (b.type) {
            case BuildingType::Barracks:  score += 5.0f; break;
            case BuildingType::WoodWall:  score += 1.0f; break;
            case BuildingType::StoneWall: score += 2.0f; break;
            case BuildingType::Arena:     score += 3.0f; break;
            default: break;
        }
    }
    return score;
}

float AIEvaluator::scoreThreats(const Kingdom& kingdom, const Kingdom& enemy,
                                  const Board& board, const GameConfig& config) {
    float threat = 0.0f;
    for (const auto& enemyPiece : enemy.pieces) {
        auto moves = MovementRules::getValidMoves(enemyPiece, board, config);
        for (const auto& m : moves) {
            const Piece* ourPiece = kingdom.getPieceAt(m);
            if (ourPiece) {
                threat += legacyPieceValue(ourPiece->type);
            }
        }
    }
    return threat;
}
