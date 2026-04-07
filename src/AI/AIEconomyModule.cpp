#include "AI/AIEconomyModule.hpp"
#include "AI/GameSnapshot.hpp"
#include "AI/AITurnContext.hpp"
#include "Buildings/BuildingType.hpp"
#include <algorithm>
#include <cmath>
#include <climits>

// =========================================================================
//  Recruit cost table
// =========================================================================

int AIEconomyModule::recruitCost(PieceType type) {
    switch (type) {
        case PieceType::Pawn:   return 10;
        case PieceType::Knight: return 30;
        case PieceType::Bishop: return 30;
        case PieceType::Rook:   return 60;
        default: return 999;
    }
}

// =========================================================================
//  Gold reserve
// =========================================================================

int AIEconomyModule::computeGoldReserve(int turnNumber, int barracksCount,
                                          StrategicObjective objective) {
    if (turnNumber <= 8) return 0;
    if (barracksCount <= 1) return 50; // save for a barracks
    if (objective == StrategicObjective::RUSH_ATTACK ||
        objective == StrategicObjective::CHECKMATE_PRESS)
        return 20;
    return 30; // default buffer
}

// =========================================================================
//  Resource gathering plan (greedy distance-based assignment)
// =========================================================================

ResourcePlan AIEconomyModule::planResourceGathering(const GameSnapshot& s,
                                                      const AITurnContext& ctx,
                                                      KingdomId k) const {
    ResourcePlan plan;
    auto& kd = s.kingdom(k);
    int barracksCount = 0;
    for (const auto& b : kd.buildings)
        if (b.type == BuildingType::Barracks && !b.isDestroyed()) ++barracksCount;

    // Collect free resource cells from context
    // (cells in mines/farms not occupied by us)
    struct ResCell {
        sf::Vector2i pos;
        int value; // 10 for mine, 5 for farm
    };
    std::vector<ResCell> freeRes;
    for (auto& cell : ctx.freeResourceCells) {
        auto* bld = s.buildingAt(cell);
        if (!bld) continue;
        int val = (bld->type == BuildingType::Mine) ? 10 : 5;
        freeRes.push_back({cell, val});
    }

    // Sort by value descending (prefer mines over farms)
    std::sort(freeRes.begin(), freeRes.end(),
              [](const ResCell& a, const ResCell& b) { return a.value > b.value; });

    // Collect idle pieces (not king, not on an income cell already)
    struct IdlePiece {
        int id;
        sf::Vector2i pos;
    };
    std::vector<IdlePiece> available;
    for (auto& p : kd.pieces) {
        const bool canUseKingForBootstrap = (barracksCount == 0);
        if (p.type == PieceType::King && !canUseKingForBootstrap) continue;
        // Check if already on an income cell
        auto* bld = s.buildingAt(p.position);
        bool alreadyOnIncome = bld && (bld->type == BuildingType::Mine ||
                                        bld->type == BuildingType::Farm);
        if (!alreadyOnIncome)
            available.push_back({p.id, p.position});
    }

    if (available.empty() && kd.pieces.size() == 1) {
        const auto& lonePiece = kd.pieces.front();
        auto* bld = s.buildingAt(lonePiece.position);
        const bool alreadyOnIncome = bld && (bld->type == BuildingType::Mine ||
                                             bld->type == BuildingType::Farm);
        if (!alreadyOnIncome)
            available.push_back({lonePiece.id, lonePiece.position});
    }

    // Greedy assignment: closest piece to highest-value cell
    for (auto& rc : freeRes) {
        if (available.empty()) break;

        int bestIdx = -1;
        int bestDist = INT_MAX;
        for (int i = 0; i < static_cast<int>(available.size()); ++i) {
            int dist = std::abs(available[i].pos.x - rc.pos.x)
                     + std::abs(available[i].pos.y - rc.pos.y);
            if (dist < bestDist) {
                bestDist = dist;
                bestIdx = i;
            }
        }

        if (bestIdx >= 0 && bestDist <= 15) { // don't send too far
            plan.assignments.push_back({available[bestIdx].id, rc.pos});
            available.erase(available.begin() + bestIdx);
        }
    }

    // Estimate expected income from current + planned occupation
    plan.expectedIncome = 0;
    for (auto& [pid, cell] : plan.assignments) {
        auto* bld = s.buildingAt(cell);
        if (bld) plan.expectedIncome += (bld->type == BuildingType::Mine) ? 10 : 5;
    }

    return plan;
}

// =========================================================================
//  Production planning
// =========================================================================

PieceType AIEconomyModule::chooseBestUnit(const GameSnapshot& s, KingdomId k,
                                            StrategicObjective objective, int budget) {
    // Count current composition
    int pawns = 0, knights = 0, bishops = 0, rooks = 0;
    for (auto& p : s.kingdom(k).pieces) {
        switch (p.type) {
            case PieceType::Pawn:   ++pawns;   break;
            case PieceType::Knight: ++knights;  break;
            case PieceType::Bishop: ++bishops;  break;
            case PieceType::Rook:   ++rooks;    break;
            default: break;
        }
    }

    // Desired composition depends on objective
    struct Desired { int p, n, b, r; };
    Desired desired;
    switch (objective) {
        case StrategicObjective::RUSH_ATTACK:
            desired = {3, 1, 0, 0}; break;
        case StrategicObjective::ECONOMY_EXPAND:
            desired = {4, 0, 1, 0}; break;
        case StrategicObjective::BUILD_ARMY:
            desired = {2, 2, 1, 1}; break;
        case StrategicObjective::CHECKMATE_PRESS:
            desired = {1, 2, 1, 2}; break;
        case StrategicObjective::PURSUE_QUEEN:
            desired = {2, 1, 1, 1}; break;
        default:
            desired = {2, 1, 1, 1}; break;
    }

    // Find the type with the biggest deficit that we can afford
    struct TypeDef { PieceType type; int deficit; int cost; };
    std::vector<TypeDef> candidates = {
        {PieceType::Pawn,   desired.p - pawns,   10},
        {PieceType::Knight, desired.n - knights,  30},
        {PieceType::Bishop, desired.b - bishops,  30},
        {PieceType::Rook,   desired.r - rooks,    60},
    };

    std::sort(candidates.begin(), candidates.end(),
              [](const TypeDef& a, const TypeDef& b) { return a.deficit > b.deficit; });

    for (auto& c : candidates)
        if (c.deficit > 0 && budget >= c.cost) return c.type;

    if (objective == StrategicObjective::CHECKMATE_PRESS) {
        if (rooks < 2 && budget >= 60) return PieceType::Rook;
        if (knights + bishops < 4 && budget >= 30) {
            return (bishops <= knights) ? PieceType::Bishop : PieceType::Knight;
        }
        if (budget >= 60) return PieceType::Rook;
        if (budget >= 30) return (bishops <= knights) ? PieceType::Bishop : PieceType::Knight;
        if (pawns < 2 && budget >= 10) return PieceType::Pawn;
    }

    // Fallback: cheapest we can afford
    if (budget >= 10) return PieceType::Pawn;
    return PieceType::Pawn; // even if can't afford, caller checks
}

ProductionPlan AIEconomyModule::planProduction(const GameSnapshot& s,
                                                 KingdomId k,
                                                 StrategicObjective objective,
                                                 int turnNumber) const {
    ProductionPlan plan;
    int barracksCount = 0;
    for (auto& b : s.kingdom(k).buildings)
        if (b.type == BuildingType::Barracks && !b.isDestroyed()) ++barracksCount;

    int reserve = computeGoldReserve(turnNumber, barracksCount, objective);
    int budget = std::max(0, s.kingdom(k).gold - reserve);

    for (auto& b : s.kingdom(k).buildings) {
        if (b.type != BuildingType::Barracks || b.isDestroyed() || b.isProducing) continue;

        PieceType best = chooseBestUnit(s, k, objective, budget);
        int cost = recruitCost(best);
        if (budget >= cost) {
            plan.orders.push_back({b.id, best});
            budget -= cost;
        }
    }

    return plan;
}
