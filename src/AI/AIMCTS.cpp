#include "AI/AIMCTS.hpp"
#include "AI/ForwardModel.hpp"
#include "AI/AIEvaluator.hpp"
#include "AI/TimeBudget.hpp"
#include "Config/GameConfig.hpp"
#include "Systems/BuildReachRules.hpp"
#include "Systems/StructureIntegrityRules.hpp"
#include <cmath>
#include <algorithm>
#include <random>

// Thread-local RNG for rollouts
static thread_local std::mt19937 s_rng{std::random_device{}()};

namespace {

int getBuildCost(BuildingType type, const GameConfig& config) {
    switch (type) {
        case BuildingType::Barracks:
            return config.getBarracksCost();
        case BuildingType::WoodWall:
            return config.getWoodWallCost();
        case BuildingType::StoneWall:
            return config.getStoneWallCost();
        case BuildingType::Arena:
            return config.getArenaCost();
        default:
            return 0;
    }
}

}

// =========================================================================
//  UCB1 formula
// =========================================================================

float AIMCTS::ucb1(const MCTSNode& child, int parentVisits, float C) {
    if (child.visits == 0) return 1e9f; // unvisited = explore first
    float exploitation = child.averageScore();
    float exploration  = C * std::sqrt(std::log(static_cast<float>(parentVisits))
                                      / static_cast<float>(child.visits));
    return exploitation + exploration;
}

// =========================================================================
//  Main search loop
// =========================================================================

MCTSAction AIMCTS::search(const GameSnapshot& rootState,
                            KingdomId aiKingdom,
                            StrategicObjective objective,
                            int globalMaxRange,
                            const EvalWeights& weights,
                            int budgetMs,
                            const GameConfig& config) {
    TimeBudget timer(budgetMs);

    auto root = std::make_unique<MCTSNode>();
    root->state = rootState;

    int iterations = 0;
    while (!timer.expired() && iterations < MAX_ITERATIONS) {
        // 1. Selection
        MCTSNode* leaf = selection(root.get());

        // 2. Expansion
        MCTSNode* expanded = expansion(leaf, aiKingdom, objective, globalMaxRange, config);

        // 3. Simulation (rollout)
        float score = simulation(expanded->state, aiKingdom,
                     globalMaxRange, weights, DEFAULT_ROLLOUT_DEPTH, config);

        // 4. Backpropagation
        backpropagation(expanded, score);

        ++iterations;
    }

    // Choose best child (most visits)
    MCTSNode* bestChild = nullptr;
    int bestVisits = -1;
    for (auto& child : root->children) {
        if (child->visits > bestVisits) {
            bestVisits = child->visits;
            bestChild = child.get();
        }
    }

    if (bestChild) return bestChild->action;
    return MCTSAction{MCTSAction::END_TURN};
}

// =========================================================================
//  Selection: walk down tree using UCB1
// =========================================================================

MCTSNode* AIMCTS::selection(MCTSNode* node) {
    while (!node->children.empty()) {
        MCTSNode* best = nullptr;
        float bestUCB = -1e9f;
        for (auto& child : node->children) {
            float u = ucb1(*child, node->visits);
            if (u > bestUCB) {
                bestUCB = u;
                best = child.get();
            }
        }
        node = best;
    }
    return node;
}

// =========================================================================
//  Expansion: create one child from an unexplored action
// =========================================================================

MCTSNode* AIMCTS::expansion(MCTSNode* node, KingdomId aiKingdom,
                              StrategicObjective objective, int globalMaxRange,
                              const GameConfig& config) {
    // If already expanded (or terminal), return self
    if (!node->children.empty() || node->visits > 0) {
        // Generate candidate actions
        auto actions = generateCandidateActions(node->state, aiKingdom,
                            objective, globalMaxRange, config);
        if (actions.empty()) return node;

        // Limit children
        int limit = std::min(static_cast<int>(actions.size()), MAX_CHILDREN_PER_NODE);

        for (int i = 0; i < limit; ++i) {
            auto child = std::make_unique<MCTSNode>();
            child->state = node->state.clone();
            child->parent = node;
            child->action = actions[i];
            applyAction(child->state, actions[i], aiKingdom, config);
            node->children.push_back(std::move(child));
        }

        // Return a random unvisited child
        std::uniform_int_distribution<int> dist(0, static_cast<int>(node->children.size()) - 1);
        return node->children[dist(s_rng)].get();
    }

    return node;
}

// =========================================================================
//  Simulation: fast rollout
// =========================================================================

float AIMCTS::simulation(const GameSnapshot& state, KingdomId aiKingdom,
                           int globalMaxRange, const EvalWeights& weights,
                           int rolloutDepth, const GameConfig& config) {
    GameSnapshot s = state.clone();
    KingdomId enemyId = (aiKingdom == KingdomId::White) ? KingdomId::Black : KingdomId::White;

    for (int d = 0; d < rolloutDepth; ++d) {
        KingdomId active = (d % 2 == 0) ? aiKingdom : enemyId;

        // Check terminal state
        if (ForwardModel::isCheckmate(s, aiKingdom, globalMaxRange))
            return -10000.0f + static_cast<float>(d);
        if (ForwardModel::isCheckmate(s, enemyId, globalMaxRange))
            return 10000.0f - static_cast<float>(d);

        // Select a rollout action using heuristic
        MCTSAction action = selectRolloutAction(s, active, globalMaxRange);
        applyAction(s, action, active, config);
        ForwardModel::advanceTurn(s, active, 10, 5, 2, config);
    }

    return AIEvaluator::evaluate(s, aiKingdom, globalMaxRange, weights);
}

// =========================================================================
//  Backpropagation
// =========================================================================

void AIMCTS::backpropagation(MCTSNode* node, float score) {
    while (node) {
        node->visits++;
        node->totalScore += score;
        node = node->parent;
    }
}

// =========================================================================
//  Action generation with relevance-based pruning
// =========================================================================

std::vector<MCTSAction> AIMCTS::generateCandidateActions(
    const GameSnapshot& s, KingdomId k,
    StrategicObjective objective, int globalMaxRange,
    const GameConfig& config)
{
    struct ScoredAction {
        MCTSAction action;
        float relevance;
    };
    std::vector<ScoredAction> scored;

    // --- MOVES (largest factor) ---
    for (auto& piece : s.kingdom(k).pieces) {
        auto moves = ForwardModel::getLegalMoves(s, piece, globalMaxRange);
        for (auto& dest : moves) {
            float rel = scoreMoveRelevance(s, piece, dest, objective);
            if (rel > RELEVANCE_THRESHOLD) {
                MCTSAction a;
                a.type = MCTSAction::MOVE;
                a.pieceId = piece.id;
                a.destination = dest;
                scored.push_back({a, rel});
            }
        }
    }

    // --- PRODUCTIONS (1 per free barracks) ---
    for (auto& b : s.kingdom(k).buildings) {
        if (b.type != BuildingType::Barracks || !b.isUsable() || b.isProducing) continue;
        for (PieceType pt : {PieceType::Pawn, PieceType::Knight, PieceType::Bishop, PieceType::Rook}) {
            const int cost = config.getRecruitCost(pt);
            if (s.kingdom(k).gold >= cost) {
                MCTSAction a;
                a.type = MCTSAction::PRODUCE;
                a.prodType = pt;
                a.barracksId = b.id;
                scored.push_back({a, 20.0f}); // medium relevance
            }
        }
    }

    // --- BUILD (if objective demands it) ---
    if (objective == StrategicObjective::BUILD_INFRASTRUCTURE ||
        objective == StrategicObjective::ECONOMY_EXPAND) {
        const int barracksCost = config.getBarracksCost();
        const int barracksWidth = config.getBuildingWidth(BuildingType::Barracks);
        const int barracksHeight = config.getBuildingHeight(BuildingType::Barracks);
        if (s.kingdom(k).gold >= barracksCost) {
            const auto builderPositions = collectBuilderPositions(s.kingdom(k).pieces);
            for (const sf::Vector2i& builderPos : builderPositions) {
                bool foundCandidate = false;
                for (int dy = -2; dy <= 2 && !foundCandidate; ++dy) {
                    for (int dx = -2; dx <= 2; ++dx) {
                        sf::Vector2i pos = builderPos + sf::Vector2i{dx, dy};
                        bool valid = footprintHasAdjacentBuilder(pos, barracksWidth, barracksHeight, builderPositions);
                        for (int fy = 0; valid && fy < barracksHeight; ++fy) {
                            for (int fx = 0; fx < barracksWidth; ++fx) {
                                const sf::Vector2i cellPos{pos.x + fx, pos.y + fy};
                                if (!s.isTraversable(cellPos.x, cellPos.y) || s.pieceAt(cellPos) || s.buildingAt(cellPos)) {
                                    valid = false;
                                    break;
                                }
                            }
                        }
                        if (!valid) {
                            continue;
                        }

                        MCTSAction a;
                        a.type = MCTSAction::BUILD;
                        a.destination = pos;
                        a.bldType = BuildingType::Barracks;
                        scored.push_back({a, 15.0f});
                        foundCandidate = true;
                        break;
                    }
                }
                if (foundCandidate) {
                    break;
                }
            }
        }
    }

    // --- END TURN (always available) ---
    scored.push_back({{MCTSAction::END_TURN}, 1.0f});

    // Sort by relevance, keep top MAX_CHILDREN
    std::sort(scored.begin(), scored.end(),
              [](const ScoredAction& a, const ScoredAction& b) {
                  return a.relevance > b.relevance;
              });

    std::vector<MCTSAction> actions;
    int limit = std::min(static_cast<int>(scored.size()), MAX_CHILDREN_PER_NODE);
    for (int i = 0; i < limit; ++i)
        actions.push_back(scored[i].action);

    return actions;
}

// =========================================================================
//  Move relevance scoring
// =========================================================================

float AIMCTS::scoreMoveRelevance(const GameSnapshot& s, const SnapPiece& piece,
                                   sf::Vector2i dest, StrategicObjective obj) {
    float score = 0.0f;

    // Captures are always relevant
    auto* victim = s.enemyKingdom(piece.kingdom).getPieceAt(dest);
    if (victim) score += AIEvaluator::pieceValue(victim->type) * 0.02f; // scale down

    auto* eKing = s.enemyKingdom(piece.kingdom).getKing();
    sf::Vector2i eKingPos = eKing ? eKing->position : sf::Vector2i{-1, -1};
    int currentDistToEK = (eKing) ? std::abs(piece.position.x - eKingPos.x)
                                    + std::abs(piece.position.y - eKingPos.y)
                                  : 999;
    int distToEK = (eKing) ? std::abs(dest.x - eKingPos.x) + std::abs(dest.y - eKingPos.y)
                           : 999;

    switch (obj) {
        case StrategicObjective::RUSH_ATTACK:
        case StrategicObjective::CHECKMATE_PRESS:
            if (currentDistToEK < 999 && distToEK < 999) {
                score += static_cast<float>(currentDistToEK - distToEK) * 25.0f;
            }
            score += std::max(0.0f, 30.0f - static_cast<float>(distToEK) * 0.5f);
            if (eKing && std::abs(dest.x - eKingPos.x) <= 1 && std::abs(dest.y - eKingPos.y) <= 1)
                score += 30.0f;
            break;

        case StrategicObjective::BUILD_ARMY:
            if (currentDistToEK < 999 && distToEK < 999) {
                score += static_cast<float>(currentDistToEK - distToEK) * 14.0f;
            }
            if (piece.type != PieceType::King)
                score += 8.0f;
            break;

        case StrategicObjective::ECONOMY_EXPAND:
        case StrategicObjective::CONTEST_RESOURCES: {
            auto* bld = s.buildingAt(dest);
            if (bld && (bld->type == BuildingType::Mine || bld->type == BuildingType::Farm))
                score += 40.0f;
            int bestDist = 999;
            for (const auto& publicBld : s.publicBuildings) {
                if (publicBld.isDestroyed()) continue;
                if (publicBld.type != BuildingType::Mine && publicBld.type != BuildingType::Farm) continue;
                for (const auto& cell : publicBld.getOccupiedCells()) {
                    if (s.kingdom(piece.kingdom).getPieceAt(cell)) continue;
                    int dist = std::abs(dest.x - cell.x) + std::abs(dest.y - cell.y);
                    if (dist < bestDist) bestDist = dist;
                }
            }
            if (bestDist < 999)
                score += std::max(0.0f, 30.0f - static_cast<float>(bestDist) * 2.0f);
            break;
        }

        case StrategicObjective::PURSUE_QUEEN: {
            // Moves toward church
            for (auto& b : s.publicBuildings) {
                if (b.type != BuildingType::Church || b.isDestroyed()) continue;
                int distToChurch = std::abs(dest.x - b.origin.x) + std::abs(dest.y - b.origin.y);
                score += std::max(0.0f, 50.0f - static_cast<float>(distToChurch) * 2.0f);
                break;
            }
            break;
        }

        case StrategicObjective::DEFEND_KING: {
            auto* myKing = s.kingdom(piece.kingdom).getKing();
            if (myKing) {
                int currentDistToKing = std::abs(piece.position.x - myKing->position.x)
                                      + std::abs(piece.position.y - myKing->position.y);
                int nextDistToKing = std::abs(dest.x - myKing->position.x)
                                   + std::abs(dest.y - myKing->position.y);
                score += static_cast<float>(currentDistToKing - nextDistToKing) * 12.0f;
                if (nextDistToKing <= 2) score += 20.0f;
            }
            break;
        }

        case StrategicObjective::RETREAT_REGROUP: {
            auto* myKing = s.kingdom(piece.kingdom).getKing();
            if (myKing) {
                int currentDistToKing = std::abs(piece.position.x - myKing->position.x)
                                      + std::abs(piece.position.y - myKing->position.y);
                int distToKing = std::abs(dest.x - myKing->position.x)
                               + std::abs(dest.y - myKing->position.y);
                score += static_cast<float>(currentDistToKing - distToKing) * 10.0f;
                score += std::max(0.0f, 20.0f - static_cast<float>(distToKing));
            }
            break;
        }

        default:
            score += 5.0f; // base relevance
            break;
    }

    return score;
}

// =========================================================================
//  Rollout action selection (fast heuristic)
// =========================================================================

MCTSAction AIMCTS::selectRolloutAction(const GameSnapshot& s, KingdomId active,
                                         int globalMaxRange) {
    auto& kd = s.kingdom(active);
    if (kd.pieces.empty()) return {MCTSAction::END_TURN};

    // Try to find a good capture first
    for (auto& piece : kd.pieces) {
        auto moves = ForwardModel::getLegalMoves(s, piece, globalMaxRange);
        for (auto& dest : moves) {
            auto* victim = s.enemyKingdom(active).getPieceAt(dest);
            if (victim && AIEvaluator::pieceValue(victim->type) >= AIEvaluator::pieceValue(piece.type)) {
                return {MCTSAction::MOVE, piece.id, dest};
            }
        }
    }

    // Otherwise pick a random piece and random move
    std::uniform_int_distribution<int> pDist(0, static_cast<int>(kd.pieces.size()) - 1);
    for (int attempt = 0; attempt < 5; ++attempt) {
        auto& piece = kd.pieces[pDist(s_rng)];
        auto moves = ForwardModel::getLegalMoves(s, piece, globalMaxRange);
        if (!moves.empty()) {
            std::uniform_int_distribution<int> mDist(0, static_cast<int>(moves.size()) - 1);
            return {MCTSAction::MOVE, piece.id, moves[mDist(s_rng)]};
        }
    }

    return {MCTSAction::END_TURN};
}

// =========================================================================
//  Apply action to a snapshot
// =========================================================================

void AIMCTS::applyAction(GameSnapshot& s, const MCTSAction& action, KingdomId k,
                         const GameConfig& config) {
    switch (action.type) {
        case MCTSAction::MOVE:
            ForwardModel::applyMove(s, action.pieceId, action.destination, k, config);
            break;
        case MCTSAction::BUILD:
            ForwardModel::applyBuild(s, k, action.bldType, action.destination,
                                     config.getBuildingWidth(action.bldType),
                                     config.getBuildingHeight(action.bldType),
                                     0,
                                     getBuildCost(action.bldType, config),
                                     StructureIntegrityRules::defaultCellHP(action.bldType, config),
                                     config);
            break;
        case MCTSAction::PRODUCE:
            ForwardModel::applyProduce(s, action.barracksId, action.prodType,
                                       config.getRecruitCost(action.prodType),
                                       config.getProductionTurns(action.prodType), k);
            break;
        case MCTSAction::MARRY:
            break;
        case MCTSAction::END_TURN:
            break;
    }
}
