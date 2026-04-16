#include "AI/AIStrategy.hpp"
#include "AI/AITurnContext.hpp"
#include "AI/GameSnapshot.hpp"
#include "AI/ForwardModel.hpp"
#include "AI/AIEvaluator.hpp"
#include "Config/AIConfig.hpp"
#include "Kingdom/KingdomId.hpp"
#include "Systems/EconomySystem.hpp"
#include <algorithm>
#include <cmath>
#include <random>

// =========================================================================
//  Helpers
// =========================================================================

int AIStrategy::countFreeBarracks(const GameSnapshot& s, KingdomId k) {
    int n = 0;
    for (auto& b : s.kingdom(k).buildings)
        if (b.type == BuildingType::Barracks && !b.isDestroyed() && !b.isProducing) ++n;
    return n;
}

bool AIStrategy::hasMatingMaterial(const GameSnapshot& s, KingdomId k) {
    auto& kd = s.kingdom(k);
    int combatPieces = 0;
    bool hasHeavy = false;
    for (auto& p : kd.pieces) {
        if (p.type == PieceType::King) continue;
        ++combatPieces;
        if (p.type == PieceType::Rook || p.type == PieceType::Queen) hasHeavy = true;
    }
    // At least 2 combat pieces or 1 heavy piece
    return combatPieces >= 2 || hasHeavy;
}

int AIStrategy::countPiecesNearEnemyKing(const GameSnapshot& s, KingdomId k, int radius) {
    auto* eKing = s.enemyKingdom(k).getKing();
    if (!eKing) return 0;
    int n = 0;
    for (auto& p : s.kingdom(k).pieces) {
        if (p.type == PieceType::King) continue;
        int dist = std::abs(p.position.x - eKing->position.x)
                 + std::abs(p.position.y - eKing->position.y);
        if (dist <= radius) ++n;
    }
    return n;
}

int AIStrategy::countEnemyKingEscapeSquares(const GameSnapshot& s, KingdomId k,
                                              int globalMaxRange) {
    auto* eKing = s.enemyKingdom(k).getKing();
    if (!eKing) return 0;
    ThreatMap myThreats = ForwardModel::buildThreatMap(s, k, globalMaxRange);
    int safe = 0;
    for (int dy = -1; dy <= 1; ++dy) {
        for (int dx = -1; dx <= 1; ++dx) {
            if (dx == 0 && dy == 0) continue;
            int nx = eKing->position.x + dx, ny = eKing->position.y + dy;
            if (!s.isTraversable(nx, ny)) continue;
            if (s.enemyKingdom(k).getPieceAt({nx, ny})) continue;
            if (!myThreats.isSet(nx, ny)) ++safe;
        }
    }
    return safe;
}

int AIStrategy::countEnemyPiecesNearMyKing(const GameSnapshot& s, KingdomId k, int radius) {
    auto* myKing = s.kingdom(k).getKing();
    if (!myKing) return 0;
    int n = 0;
    for (auto& p : s.enemyKingdom(k).pieces) {
        int dist = std::abs(p.position.x - myKing->position.x)
                 + std::abs(p.position.y - myKing->position.y);
        if (dist <= radius) ++n;
    }
    return n;
}

int AIStrategy::countSelfKingEscapeSquares(const GameSnapshot& s, KingdomId k,
                                             int globalMaxRange) {
    auto* myKing = s.kingdom(k).getKing();
    if (!myKing) return 8;
    ThreatMap enemyThreats = ForwardModel::buildThreatMap(s, s.enemyKingdom(k).id,
                                                           globalMaxRange);
    int safe = 0;
    for (int dy = -1; dy <= 1; ++dy) {
        for (int dx = -1; dx <= 1; ++dx) {
            if (dx == 0 && dy == 0) continue;
            int nx = myKing->position.x + dx, ny = myKing->position.y + dy;
            if (!s.isTraversable(nx, ny)) continue;
            if (s.kingdom(k).getPieceAt({nx, ny})) continue;
            if (!enemyThreats.isSet(nx, ny)) ++safe;
        }
    }
    return safe;
}

float AIStrategy::avgDistanceToChurch(const GameSnapshot& s, KingdomId k) {
    // Find church center
    sf::Vector2i churchCenter{-1, -1};
    for (auto& b : s.publicBuildings) {
        if (b.type == BuildingType::Church && !b.isDestroyed()) {
            churchCenter = b.origin;
            break;
        }
    }
    if (churchCenter.x < 0) return 999.0f;

    auto& kd = s.kingdom(k);
    auto* king = kd.getKing();
    float sum = 0.0f;
    int count = 0;

    // Distance of king, first bishop, first pawn to church
    if (king) {
        sum += static_cast<float>(std::abs(king->position.x - churchCenter.x)
                                + std::abs(king->position.y - churchCenter.y));
        ++count;
    }
    for (auto& p : kd.pieces) {
        if (p.type == PieceType::Bishop) {
            sum += static_cast<float>(std::abs(p.position.x - churchCenter.x)
                                    + std::abs(p.position.y - churchCenter.y));
            ++count;
            break;
        }
    }
    for (auto& p : kd.pieces) {
        if (p.type == PieceType::Pawn) {
            sum += static_cast<float>(std::abs(p.position.x - churchCenter.x)
                                    + std::abs(p.position.y - churchCenter.y));
            ++count;
            break;
        }
    }

    return count > 0 ? sum / static_cast<float>(count) : 999.0f;
}

// =========================================================================
//  Scoring functions  (each returns 0..100)
// =========================================================================

float AIStrategy::scoreRushAttack(const AITurnContext& /*ctx*/, const GameSnapshot& s,
                                    KingdomId k) const {
    float score = 0.0f;
    int myCount = static_cast<int>(s.kingdom(k).pieces.size());
    int enemyCount = static_cast<int>(s.enemyKingdom(k).pieces.size());

    if (myCount > enemyCount)
        score += static_cast<float>(myCount - enemyCount) * 20.0f;
    if (enemyCount == 1) score += 80.0f; // only king left
    if (hasMatingMaterial(s, k)) score += 30.0f;
    if (ForwardModel::isInCheck(s, k, 8)) score -= 50.0f;

    return std::clamp(score, 0.0f, 100.0f);
}

float AIStrategy::scoreEconomyExpand(const AITurnContext& ctx, const GameSnapshot& s,
                                       KingdomId k) const {
    float score = 0.0f;

    // Estimate income per turn
    int income = 0;
    auto countIncome = [&](const SnapBuilding& b) {
        if (b.isDestroyed()) return;
        if (b.type != BuildingType::Mine && b.type != BuildingType::Farm) return;
        const int incPerCell = (b.type == BuildingType::Mine) ? ctx.mineIncomePerCell : ctx.farmIncomePerCell;
        int myOccupiedCells = 0;
        int enemyOccupiedCells = 0;
        for (auto& cell : b.getOccupiedCells()) {
            if (s.kingdom(k).getPieceAt(cell)) ++myOccupiedCells;
            if (s.enemyKingdom(k).getPieceAt(cell)) ++enemyOccupiedCells;
        }
        const ResourceIncomeBreakdown breakdown = (k == KingdomId::White)
            ? EconomySystem::calculateResourceIncomeFromOccupation(myOccupiedCells, enemyOccupiedCells, incPerCell)
            : EconomySystem::calculateResourceIncomeFromOccupation(enemyOccupiedCells, myOccupiedCells, incPerCell);
        income += breakdown.incomeFor(k);
    };
    for (auto& b : s.kingdom(k).buildings)       countIncome(b);
    for (auto& b : s.enemyKingdom(k).buildings)   countIncome(b);
    for (auto& b : s.publicBuildings)              countIncome(b);

    if (income == 0) score += 70.0f;
    else if (income < 20) score += 40.0f;
    else score += 10.0f;

    if (s.kingdom(k).gold < 30) score += 30.0f;
    score += static_cast<float>(ctx.freeResourceCells.size()) * 5.0f;
    if (income >= 40) score -= 20.0f;

    return std::clamp(score, 0.0f, 100.0f);
}

float AIStrategy::scoreBuildArmy(const AITurnContext& /*ctx*/, const GameSnapshot& s,
                                   KingdomId k) const {
    float score = 0.0f;
    int pieceCount = static_cast<int>(s.kingdom(k).pieces.size());

    if (pieceCount <= 2) score += 50.0f;
    else if (pieceCount <= 4) score += 30.0f;
    else score += 10.0f;

    if (s.kingdom(k).gold >= 60) score += 20.0f;
    if (s.kingdom(k).gold >= 30) score += 10.0f;

    int freeBarr = countFreeBarracks(s, k);
    score += freeBarr * 15.0f;

    // Count units in production backlog
    int producing = 0;
    for (auto& b : s.kingdom(k).buildings)
        if (b.type == BuildingType::Barracks && b.isProducing) ++producing;
    if (producing >= 5) score -= 30.0f;

    return std::clamp(score, 0.0f, 100.0f);
}

float AIStrategy::scoreBuildInfra(const AITurnContext& /*ctx*/, const GameSnapshot& s,
                                    KingdomId k) const {
    float score = 0.0f;
    int barracksCount = 0;
    for (auto& b : s.kingdom(k).buildings)
        if (b.type == BuildingType::Barracks && !b.isDestroyed()) ++barracksCount;

    if (barracksCount == 0 && s.kingdom(k).gold >= 50) score += 80.0f;
    else if (barracksCount == 1 && s.kingdom(k).gold >= 50) score += 40.0f;
    else if (barracksCount >= 2) score += 5.0f;

    return std::clamp(score, 0.0f, 100.0f);
}

float AIStrategy::scorePursueQueen(const AITurnContext& /*ctx*/, const GameSnapshot& s,
                                     KingdomId k, int turnNumber) const {
    bool hasBishop = false;
    bool hasRook = false;
    for (auto& p : s.kingdom(k).pieces) {
        if (p.type == PieceType::Bishop) hasBishop = true;
        if (p.type == PieceType::Rook)   hasRook   = true;
    }
    if (!hasBishop || !hasRook) return 0.0f;

    float score = 40.0f;
    float dist = avgDistanceToChurch(s, k);
    score += std::max(0.0f, 30.0f - dist * 2.0f);

    if (turnNumber > 20) score += 15.0f;
    if (turnNumber > 40) score += 15.0f;

    return std::clamp(score, 0.0f, 100.0f);
}

float AIStrategy::scoreDefendKing(const AITurnContext& /*ctx*/, const GameSnapshot& s,
                                    KingdomId k, int globalMaxRange) const {
    float score = 0.0f;
    if (ForwardModel::isInCheck(s, k, globalMaxRange)) score += 90.0f;

    int myEscapes = countSelfKingEscapeSquares(s, k, globalMaxRange);
    if (myEscapes <= 2) score += 40.0f;
    else if (myEscapes <= 4) score += 15.0f;

    int nearbyEnemies = countEnemyPiecesNearMyKing(s, k, 4);
    score += nearbyEnemies * 12.0f;

    return std::clamp(score, 0.0f, 100.0f);
}

float AIStrategy::scoreCheckmatePress(const AITurnContext& /*ctx*/, const GameSnapshot& s,
                                        KingdomId k, int globalMaxRange) const {
    float score = 0.0f;
    int nearPieces = countPiecesNearEnemyKing(s, k, 4);
    score += nearPieces * 15.0f;

    int escapes = countEnemyKingEscapeSquares(s, k, globalMaxRange);
    int blocked = 8 - escapes;
    score += blocked * 12.0f;

    if (ForwardModel::isInCheck(s, s.enemyKingdom(k).id, globalMaxRange))
        score += 40.0f;

    if (!hasMatingMaterial(s, k)) score -= 60.0f;

    return std::clamp(score, 0.0f, 100.0f);
}

float AIStrategy::scoreContestResources(const AITurnContext& ctx, const GameSnapshot& s,
                                          KingdomId k) const {
    float score = 0.0f;
    // Count contested cells (both sides have a piece)
    int contested = 0;
    auto check = [&](const SnapBuilding& b) {
        if (b.isDestroyed()) return;
        if (b.type != BuildingType::Mine && b.type != BuildingType::Farm) return;
        for (auto& cell : b.getOccupiedCells())
            if (s.white.getPieceAt(cell) && s.black.getPieceAt(cell)) ++contested;
    };
    for (auto& b : s.white.buildings)  check(b);
    for (auto& b : s.black.buildings)  check(b);
    for (auto& b : s.publicBuildings)  check(b);

    score += contested * 20.0f;
    // Also boost if enemy occupies resources we could take
    int enemyResource = 0;
    auto checkEnemy = [&](const SnapBuilding& b) {
        if (b.isDestroyed()) return;
        if (b.type != BuildingType::Mine && b.type != BuildingType::Farm) return;
        for (auto& cell : b.getOccupiedCells())
            if (s.enemyKingdom(k).getPieceAt(cell) && !s.kingdom(k).getPieceAt(cell))
                ++enemyResource;
    };
    for (auto& b : s.kingdom(k).buildings)       checkEnemy(b);
    for (auto& b : s.enemyKingdom(k).buildings)   checkEnemy(b);
    for (auto& b : s.publicBuildings)              checkEnemy(b);

    score += enemyResource * 10.0f;

    return std::clamp(score, 0.0f, 100.0f);
}

float AIStrategy::scoreRetreatRegroup(const AITurnContext& /*ctx*/, const GameSnapshot& s,
                                        KingdomId k, int globalMaxRange) const {
    float score = 0.0f;
    auto& kd = s.kingdom(k);
    ThreatMap enemyThreats = ForwardModel::buildThreatMap(s, s.enemyKingdom(k).id,
                                                           globalMaxRange);
    int threatened = 0;
    for (auto& p : kd.pieces)
        if (enemyThreats.isSet(p.position.x, p.position.y)) ++threatened;

    int total = static_cast<int>(kd.pieces.size());
    if (total > 0 && threatened > total / 2) score += 50.0f;
    else if (threatened >= 2) score += 25.0f;

    // If we have way fewer pieces than enemy, regroup
    int diff = static_cast<int>(s.enemyKingdom(k).pieces.size()) - total;
    if (diff >= 3) score += 30.0f;

    return std::clamp(score, 0.0f, 100.0f);
}

// =========================================================================
//  Main entry: compute TurnPlan
// =========================================================================

TurnPlan AIStrategy::computePlan(const AITurnContext& ctx,
                                  const GameSnapshot& snapshot,
                                  KingdomId aiKingdom,
                                  int turnNumber,
                                  const AIConfig& aiConfig) {
    int gmr = 8; // globalMaxRange default

    struct Candidate {
        StrategicObjective obj;
        float score;
    };

    std::vector<Candidate> candidates = {
        {StrategicObjective::RUSH_ATTACK,          scoreRushAttack(ctx, snapshot, aiKingdom)},
        {StrategicObjective::ECONOMY_EXPAND,       scoreEconomyExpand(ctx, snapshot, aiKingdom)},
        {StrategicObjective::BUILD_ARMY,           scoreBuildArmy(ctx, snapshot, aiKingdom)},
        {StrategicObjective::BUILD_INFRASTRUCTURE, scoreBuildInfra(ctx, snapshot, aiKingdom)},
        {StrategicObjective::PURSUE_QUEEN,         scorePursueQueen(ctx, snapshot, aiKingdom, turnNumber)},
        {StrategicObjective::DEFEND_KING,          scoreDefendKing(ctx, snapshot, aiKingdom, gmr)},
        {StrategicObjective::CHECKMATE_PRESS,      scoreCheckmatePress(ctx, snapshot, aiKingdom, gmr)},
        {StrategicObjective::CONTEST_RESOURCES,    scoreContestResources(ctx, snapshot, aiKingdom)},
        {StrategicObjective::RETREAT_REGROUP,       scoreRetreatRegroup(ctx, snapshot, aiKingdom, gmr)},
    };

    std::sort(candidates.begin(), candidates.end(),
              [](const Candidate& a, const Candidate& b) { return a.score > b.score; });

    // Slight stochastic variation on top-2
    StrategicObjective chosen = candidates[0].obj;
    if (candidates.size() >= 2 && aiConfig.randomness > 0.0f) {
        float diff = candidates[0].score - candidates[1].score;
        if (diff < 10.0f) {
            // Simple deterministic-ish selection based on turn parity
            if (turnNumber % 3 == 0) chosen = candidates[1].obj;
        }
    }

    m_lastObjective = chosen;

    // Build composite TurnPlan
    TurnPlan plan;
    plan.primaryObjective = chosen;

    // Always produce if we have barracks and gold
    int freeBarr = countFreeBarracks(snapshot, aiKingdom);
    plan.shouldProduce = freeBarr > 0 && snapshot.kingdom(aiKingdom).gold >= 10;

    // Choose production type based on objective
    switch (chosen) {
        case StrategicObjective::RUSH_ATTACK:
        case StrategicObjective::ECONOMY_EXPAND:
            plan.preferredProduction = PieceType::Pawn;
            break;
        case StrategicObjective::CHECKMATE_PRESS:
            plan.preferredProduction = (snapshot.kingdom(aiKingdom).gold >= 60)
                                        ? PieceType::Rook : PieceType::Knight;
            break;
        case StrategicObjective::BUILD_ARMY:
            plan.preferredProduction = (snapshot.kingdom(aiKingdom).gold >= 30)
                                        ? PieceType::Knight : PieceType::Pawn;
            break;
        default:
            plan.preferredProduction = PieceType::Pawn;
            break;
    }

    // Build decision
    int barracksCount = 0;
    for (auto& b : snapshot.kingdom(aiKingdom).buildings)
        if (b.type == BuildingType::Barracks && !b.isDestroyed()) ++barracksCount;

    if (barracksCount == 0 && snapshot.kingdom(aiKingdom).gold >= 50) {
        plan.shouldBuild = true;
        plan.preferredBuilding = BuildingType::Barracks;
    } else if (chosen == StrategicObjective::BUILD_INFRASTRUCTURE) {
        plan.shouldBuild = true;
        plan.preferredBuilding = BuildingType::Barracks;
    } else if (chosen == StrategicObjective::DEFEND_KING &&
               snapshot.kingdom(aiKingdom).gold >= 20) {
        plan.shouldBuild = true;
        plan.preferredBuilding = BuildingType::WoodWall;
    }

    // Marriage
    plan.shouldMarry = (chosen == StrategicObjective::PURSUE_QUEEN);

    return plan;
}

// =========================================================================
//  Name helper
// =========================================================================

std::string AIStrategy::objectiveName(StrategicObjective obj) {
    switch (obj) {
        case StrategicObjective::RUSH_ATTACK:          return "RUSH_ATTACK";
        case StrategicObjective::ECONOMY_EXPAND:       return "ECONOMY_EXPAND";
        case StrategicObjective::BUILD_ARMY:           return "BUILD_ARMY";
        case StrategicObjective::BUILD_INFRASTRUCTURE: return "BUILD_INFRASTRUCTURE";
        case StrategicObjective::PURSUE_QUEEN:         return "PURSUE_QUEEN";
        case StrategicObjective::DEFEND_KING:          return "DEFEND_KING";
        case StrategicObjective::CHECKMATE_PRESS:      return "CHECKMATE_PRESS";
        case StrategicObjective::CONTEST_RESOURCES:    return "CONTEST_RESOURCES";
        case StrategicObjective::RETREAT_REGROUP:      return "RETREAT_REGROUP";
    }
    return "UNKNOWN";
}
