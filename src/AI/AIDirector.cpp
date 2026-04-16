#include "AI/AIDirector.hpp"
#include "Board/Board.hpp"
#include "Kingdom/Kingdom.hpp"
#include "Buildings/Building.hpp"
#include "Config/GameConfig.hpp"
#include "Systems/EconomySystem.hpp"
#include <algorithm>
#include <array>
#include <iostream>
#include <limits>
#include <cmath>

// =========================================================================
//  Construction / Config
// =========================================================================

AIDirector::AIDirector() = default;

bool AIDirector::loadConfig(const std::string& filepath) {
    const bool loaded = m_config.loadFromFile(filepath);
    AIEvaluator::setConfig(&m_config);
    return loaded;
}

void AIDirector::applyPlanMetadata(const AIDirectorPlan& plan) {
    m_lastEnemyKingPos = plan.lastEnemyKingPos;
    m_enemyKingStaticTurns = plan.enemyKingStaticTurns;
    m_lastMovedPieceId = plan.lastMovedPieceId;
}

// =========================================================================
//  Helper: estimate current income per turn
// =========================================================================

static int estimateIncome(const GameSnapshot& s, KingdomId k,
                           int mineIncome, int farmIncome) {
    int income = 0;
    auto& kd = s.kingdom(k);
    auto& enemy = s.enemyKingdom(k);
    auto check = [&](const SnapBuilding& b) {
        if (b.isDestroyed()) return;
        if (b.type != BuildingType::Mine && b.type != BuildingType::Farm) return;
        int ipCell = (b.type == BuildingType::Mine) ? mineIncome : farmIncome;
        int myOccupiedCells = 0;
        int enemyOccupiedCells = 0;
        for (auto& cell : b.getOccupiedCells()) {
            if (kd.getPieceAt(cell)) ++myOccupiedCells;
            if (enemy.getPieceAt(cell)) ++enemyOccupiedCells;
        }
        const ResourceIncomeBreakdown breakdown = (k == KingdomId::White)
            ? EconomySystem::calculateResourceIncomeFromOccupation(myOccupiedCells, enemyOccupiedCells, ipCell)
            : EconomySystem::calculateResourceIncomeFromOccupation(enemyOccupiedCells, myOccupiedCells, ipCell);
        income += breakdown.incomeFor(k);
    };
    for (auto& b : kd.buildings)      check(b);
    for (auto& b : enemy.buildings)   check(b);
    for (auto& b : s.publicBuildings) check(b);
    return income;
}

static int countBarracks(const GameSnapshot& s, KingdomId k) {
    int count = 0;
    for (const auto& building : s.kingdom(k).buildings) {
        if (building.type == BuildingType::Barracks && !building.isDestroyed()) {
            ++count;
        }
    }
    return count;
}

static int countCombatPieces(const SnapKingdom& kingdom) {
    int count = 0;
    for (const auto& piece : kingdom.pieces) {
        if (piece.type != PieceType::King) {
            ++count;
        }
    }
    return count;
}

static bool hasSufficientMatingMaterial(const GameSnapshot& s, KingdomId k) {
    int knights = 0;
    int bishops = 0;
    int rooks = 0;
    int queens = 0;
    int pawns = 0;

    for (const auto& piece : s.kingdom(k).pieces) {
        switch (piece.type) {
            case PieceType::Knight: ++knights; break;
            case PieceType::Bishop: ++bishops; break;
            case PieceType::Rook: ++rooks; break;
            case PieceType::Queen: ++queens; break;
            case PieceType::Pawn: ++pawns; break;
            default: break;
        }
    }

    const int combatPieces = knights + bishops + rooks + queens + pawns;
    const int strongPieces = knights + bishops + rooks + queens;

    if (queens > 0 || rooks > 0) return true;
    if (bishops >= 2) return true;
    if (bishops >= 1 && knights >= 1) return true;
    if (strongPieces >= 3) return true;
    if (combatPieces >= 6) return true;
    return false;
}

static int countFriendlyNeighbors(const GameSnapshot& s, KingdomId k, sf::Vector2i pos, int radius) {
    int count = 0;
    for (const auto& piece : s.kingdom(k).pieces) {
        if (std::abs(piece.position.x - pos.x) <= radius &&
            std::abs(piece.position.y - pos.y) <= radius) {
            ++count;
        }
    }
    return count;
}

static int countStrongPieces(const SnapKingdom& kingdom) {
    int count = 0;
    for (const auto& piece : kingdom.pieces) {
        if (piece.type == PieceType::Knight || piece.type == PieceType::Bishop ||
            piece.type == PieceType::Rook || piece.type == PieceType::Queen) {
            ++count;
        }
    }
    return count;
}

static bool canUpgradeSnapshotPiece(const SnapPiece& piece, PieceType target, const GameConfig& config) {
    if (piece.type == PieceType::Pawn && (target == PieceType::Knight || target == PieceType::Bishop)) {
        return piece.xp >= config.getXPThresholdPawnToKnightOrBishop();
    }
    if ((piece.type == PieceType::Knight || piece.type == PieceType::Bishop) && target == PieceType::Rook) {
        return piece.xp >= config.getXPThresholdToRook();
    }
    return false;
}

static PieceType choosePawnUpgradeTarget(const SnapKingdom& kingdom) {
    int knights = 0;
    int bishops = 0;
    for (const auto& piece : kingdom.pieces) {
        if (piece.type == PieceType::Knight) ++knights;
        if (piece.type == PieceType::Bishop) ++bishops;
    }
    return (knights <= bishops) ? PieceType::Knight : PieceType::Bishop;
}

static PieceType nextUpgradeTarget(const SnapKingdom& kingdom, const SnapPiece& piece) {
    switch (piece.type) {
        case PieceType::Pawn:
            return choosePawnUpgradeTarget(kingdom);
        case PieceType::Knight:
            return PieceType::Rook;
        case PieceType::Bishop:
            return PieceType::Rook;
        default:
            return piece.type;
    }
}

static int countPiecesNearEnemyKing(const GameSnapshot& s, KingdomId aiKingdom, int radius) {
    const auto* enemyKing = s.enemyKingdom(aiKingdom).getKing();
    if (!enemyKing) return 0;

    int count = 0;
    for (const auto& piece : s.kingdom(aiKingdom).pieces) {
        if (piece.type == PieceType::King) continue;
        if (std::abs(piece.position.x - enemyKing->position.x)
            + std::abs(piece.position.y - enemyKing->position.y) <= radius) {
            ++count;
        }
    }
    return count;
}

static int countSafeKingEscapes(const GameSnapshot& s, KingdomId defendingKingdom,
                                int globalMaxRange) {
    const auto* king = s.kingdom(defendingKingdom).getKing();
    if (!king) return 0;

    const KingdomId attacker = s.enemyKingdom(defendingKingdom).id;
    const ThreatMap threats = ForwardModel::buildThreatMap(s, attacker, globalMaxRange);
    const auto* enemyKing = s.kingdom(attacker).getKing();

    int safe = 0;
    for (int dy = -1; dy <= 1; ++dy) {
        for (int dx = -1; dx <= 1; ++dx) {
            if (dx == 0 && dy == 0) continue;

            sf::Vector2i pos = king->position + sf::Vector2i{dx, dy};
            if (!s.isTraversable(pos.x, pos.y)) continue;
            if (s.kingdom(defendingKingdom).getPieceAt(pos)) continue;
            if (threats.isSet(pos)) continue;
            if (enemyKing && std::abs(pos.x - enemyKing->position.x) <= 1 &&
                std::abs(pos.y - enemyKing->position.y) <= 1) continue;
            ++safe;
        }
    }
    return safe;
}

static int manhattanDistance(sf::Vector2i a, sf::Vector2i b) {
    return std::abs(a.x - b.x) + std::abs(a.y - b.y);
}

static int octantIndex(sf::Vector2i center, sf::Vector2i pos) {
    const float dx = static_cast<float>(pos.x - center.x);
    const float dy = static_cast<float>(pos.y - center.y);
    float angle = std::atan2(dy, dx);
    if (angle < 0.0f) {
        angle += 6.28318530718f;
    }
    return static_cast<int>(std::floor((angle + 0.39269908169f) / 0.78539816339f)) % 8;
}

struct AssaultSlot {
    sf::Vector2i pos;
    bool isEscape = false;
    int sector = 0;
};

struct AssaultEval {
    float value = -std::numeric_limits<float>::max();
    int slotDistance = 999;
    int sectorLoad = 999;
    int uncoveredEscapePressure = 0;
};

static std::vector<AssaultSlot> buildAssaultSlots(const GameSnapshot& snapshot,
                                                  sf::Vector2i enemyKingPos) {
    std::vector<AssaultSlot> slots;

    auto addRing = [&](int radius, bool isEscape) {
        for (int dy = -radius; dy <= radius; ++dy) {
            for (int dx = -radius; dx <= radius; ++dx) {
                if (dx == 0 && dy == 0) continue;
                if (std::max(std::abs(dx), std::abs(dy)) != radius) continue;

                sf::Vector2i pos = enemyKingPos + sf::Vector2i{dx, dy};
                if (!snapshot.isTraversable(pos.x, pos.y)) continue;
                slots.push_back({pos, isEscape, octantIndex(enemyKingPos, pos)});
            }
        }
    };

    addRing(1, true);
    addRing(2, false);
    return slots;
}

static AssaultEval evaluateAssaultPosition(const GameSnapshot& snapshot,
                                          KingdomId aiKingdom,
                                          const SnapPiece& piece,
                                          sf::Vector2i pos,
                                          sf::Vector2i enemyKingPos,
                                          const std::vector<AssaultSlot>& slots,
                                          const std::array<int, 8>& sectorLoads,
                                          const ThreatMap& currentThreats,
                                          const AIConfig& aiConfig) {
    (void)snapshot;
    (void)aiKingdom;
    AssaultEval best;

    const int currentSector = octantIndex(enemyKingPos, piece.position);
    const bool subtractCurrentSector =
        (manhattanDistance(piece.position, enemyKingPos) <= aiConfig.pressure.sectorLoadDistance);
    const int moveSector = octantIndex(enemyKingPos, pos);

    for (const auto& slot : slots) {
        int slotSectorLoad = sectorLoads[slot.sector];
        int moveSectorLoad = sectorLoads[moveSector];
        if (subtractCurrentSector && currentSector == slot.sector) {
            slotSectorLoad = std::max(0, slotSectorLoad - 1);
        }
        if (subtractCurrentSector && currentSector == moveSector) {
            moveSectorLoad = std::max(0, moveSectorLoad - 1);
        }

        const int slotDistance = manhattanDistance(pos, slot.pos);
        const bool exactSlot = (pos == slot.pos);
        const int uncoveredEscapePressure = (slot.isEscape && !currentThreats.isSet(slot.pos)) ? 1 : 0;

        float value = 0.0f;
        value += uncoveredEscapePressure
            ? aiConfig.pressure.assaultUncoveredEscapeWeight
            : (slot.isEscape
                ? aiConfig.pressure.assaultEscapeWeight
                : aiConfig.pressure.assaultNonEscapeWeight);
        value += exactSlot
            ? (slot.isEscape
                ? aiConfig.pressure.assaultExactEscapeBonus
                : aiConfig.pressure.assaultExactNonEscapeBonus)
            : 0.0f;
        value -= static_cast<float>(slotDistance) * aiConfig.pressure.assaultSlotDistancePenalty;
        value -= static_cast<float>(slotSectorLoad) * aiConfig.pressure.assaultSlotSectorLoadPenalty;
        value -= static_cast<float>(moveSectorLoad) * aiConfig.pressure.assaultMoveSectorLoadPenalty;

        if (value > best.value) {
            best.value = value;
            best.slotDistance = slotDistance;
            best.sectorLoad = moveSectorLoad;
            best.uncoveredEscapePressure = uncoveredEscapePressure;
        }
    }

    return best;
}

static bool moveTowardTarget(AIDirectorPlan& plan,
                             const GameSnapshot& snapshot,
                             KingdomId aiKingdom,
                             int pieceId,
                             sf::Vector2i target,
                             int globalMaxRange) {
    const auto* piece = snapshot.kingdom(aiKingdom).getPieceById(pieceId);
    if (!piece) return false;

    const auto moves = ForwardModel::getLegalMoves(snapshot, *piece, globalMaxRange);
    int currentDist = std::abs(piece->position.x - target.x) + std::abs(piece->position.y - target.y);
    int bestDist = currentDist;
    sf::Vector2i bestDest = piece->position;

    for (const auto& move : moves) {
        int dist = std::abs(move.x - target.x) + std::abs(move.y - target.y);
        if (dist < bestDist) {
            bestDist = dist;
            bestDest = move;
        }
    }

    if (bestDest == piece->position) return false;

    TurnCommand cmd;
    cmd.type = TurnCommand::Move;
    cmd.pieceId = pieceId;
    cmd.origin = piece->position;
    cmd.destination = bestDest;
    plan.commands.push_back(cmd);
    return true;
}

// =========================================================================
//  Main entry: computeTurn
// =========================================================================

AIDirectorPlan AIDirector::computeTurn(Board& board, Kingdom& self, Kingdom& enemy,
                                         const std::vector<Building>& publicBuildings,
                                         int turnNumber, const GameConfig& config) {
    TimeBudget timer(m_config.maxTurnTimeMs);

    AIDirectorPlan plan;
    KingdomId aiKingdom = self.id;
    KingdomId enemyId = enemy.id;
    int globalMaxRange = config.getGlobalMaxRange();

    // --- 1. Build context ---
    AITurnContext ctx;
    ctx.build(board, self, enemy, config);

    // --- 2. Create snapshot for ForwardModel ---
    GameSnapshot snapshot = ForwardModel::createSnapshot(board, self, enemy,
                                                          publicBuildings, turnNumber);
    // Determine if either player uses White or Black for the snapshot accessor
    // self = kingdom(aiKingdom), enemy = enemyKingdom(aiKingdom)

    // --- Track enemy king position ---
    auto* eKing = snapshot.enemyKingdom(aiKingdom).getKing();
    if (eKing) {
        if (eKing->position == m_lastEnemyKingPos)
            ++m_enemyKingStaticTurns;
        else
            m_enemyKingStaticTurns = 0;
        plan.lastEnemyKingPos = eKing->position;
    }
    plan.enemyKingStaticTurns = m_enemyKingStaticTurns;

    // --- 3. Detect immediate mate-in-1 ---
    if (timer.hasAtLeast(m_config.mateInOneMinBudgetMs)) {
        auto mate = m_checkmateSolver.findMateIn1(snapshot, aiKingdom, globalMaxRange, config);
        if (mate.has_value()) {
            // Found checkmate! Queue the move.
            auto* piece = snapshot.kingdom(aiKingdom).getPieceById(mate->pieceId);
            if (piece) {
                TurnCommand cmd;
                cmd.type = TurnCommand::Move;
                cmd.pieceId = mate->pieceId;
                cmd.origin = piece->position;
                cmd.destination = mate->destination;
                plan.commands.push_back(cmd);
                plan.objectiveName = "CHECKMATE_IN_1";

                // Also queue all productions (free actions)
                executeProductions(plan, snapshot, aiKingdom, TurnPlan{}, turnNumber, config);
                return plan;
            }
        }
    }

    // --- 4. Deeper mate search if time permits ---
    if (timer.hasAtLeast(m_config.deepMateMinBudgetMs)) {
        auto deepMate = m_checkmateSolver.findMateInN(snapshot, aiKingdom, m_config.deepMateDepth,
                                globalMaxRange,
                                std::min(m_config.deepMateMaxBudgetMs,
                                         static_cast<int>(timer.remainingMs() * m_config.checkmateSolverBudgetFraction)),
                                config);
        if (deepMate.has_value() && deepMate->pieceId >= 0) {
            auto* piece = snapshot.kingdom(aiKingdom).getPieceById(deepMate->pieceId);
            if (piece) {
                TurnCommand cmd;
                cmd.type = TurnCommand::Move;
                cmd.pieceId = deepMate->pieceId;
                cmd.origin = piece->position;
                cmd.destination = deepMate->destination;
                plan.commands.push_back(cmd);
                plan.objectiveName = "CHECKMATE_IN_N";
                executeProductions(plan, snapshot, aiKingdom, TurnPlan{}, turnNumber, config);
                return plan;
            }
        }
    }

    // --- 5. Strategic layer: choose objective + composite plan ---
    TurnPlan stratPlan = m_strategy.computePlan(ctx, snapshot, aiKingdom,
                                                  turnNumber, m_config);
    const int myCombatPieces = countCombatPieces(snapshot.kingdom(aiKingdom));
    const int enemyCombatPieces = countCombatPieces(snapshot.enemyKingdom(aiKingdom));
    const int strongPieces = countStrongPieces(snapshot.kingdom(aiKingdom));
    const int nearbyPressurePieces = countPiecesNearEnemyKing(snapshot, aiKingdom, 8);
    const bool forcePressure = hasSufficientMatingMaterial(snapshot, aiKingdom)
        && ((enemyCombatPieces == 0 && myCombatPieces >= 3)
            || (enemyCombatPieces <= 1
                && m_enemyKingStaticTurns >= m_config.enemyKingStaticTurnsThreshold
                && myCombatPieces >= 4));
    const bool shouldContinuePressureProduction = !forcePressure
        || nearbyPressurePieces < 5
        || strongPieces < 3
        || myCombatPieces < 6;

    if (forcePressure) {
        stratPlan.primaryObjective = StrategicObjective::CHECKMATE_PRESS;
        stratPlan.shouldMarry = false;
    }
    plan.objectiveName = AIStrategy::objectiveName(stratPlan.primaryObjective);
    if (forcePressure) {
        plan.objectiveName = "FORCE_PRESSURE";
    }

    // Determine AIPhase for eval weights
    AIPhase phase = AIPhase::MID_GAME;
    if (turnNumber <= m_config.earlyGameMaxTurn) phase = AIPhase::EARLY_GAME;
    else if (turnNumber <= m_config.buildUpMaxTurn) phase = AIPhase::BUILD_UP;
    else if (stratPlan.primaryObjective == StrategicObjective::DEFEND_KING)
        phase = AIPhase::CRISIS;
    else if (stratPlan.primaryObjective == StrategicObjective::CHECKMATE_PRESS ||
             stratPlan.primaryObjective == StrategicObjective::RUSH_ATTACK)
        phase = AIPhase::AGGRESSION;
    else if (static_cast<int>(snapshot.kingdom(aiKingdom).pieces.size()) <= m_config.endgamePieceThreshold &&
             static_cast<int>(snapshot.enemyKingdom(aiKingdom).pieces.size()) <= m_config.endgamePieceThreshold)
        phase = AIPhase::ENDGAME;

    EvalWeights weights = AIEvaluator::weightsForPhase(phase);

    // --- 6. Tactical layer: MCTS for best move ---
    int mctsBudget = std::max(30, static_cast<int>(timer.remainingMs() * m_config.mctsBudgetFraction));
    executeMove(plan, snapshot, ctx, aiKingdom, stratPlan,
                globalMaxRange, weights, mctsBudget, config, forcePressure);

    // --- 7. Build ---
    int incomePerTurn = estimateIncome(snapshot, aiKingdom,
                                        config.getMineIncomePerCellPerTurn(),
                                        config.getFarmIncomePerCellPerTurn());
    executeBuild(plan, snapshot, aiKingdom, stratPlan, turnNumber, incomePerTurn, config);

    // --- 8. Upgrades ---
    executeUpgrades(plan, snapshot, aiKingdom, config);

    // --- 9. Produce in ALL free barracks ---
    if (shouldContinuePressureProduction)
        executeProductions(plan, snapshot, aiKingdom, stratPlan, turnNumber, config);

    return plan;
}

// =========================================================================
//  executeMove — MCTS or heuristic fallback
// =========================================================================

void AIDirector::executeMove(AIDirectorPlan& plan, const GameSnapshot& snapshot,
                               const AITurnContext& ctx, KingdomId aiKingdom,
                               const TurnPlan& stratPlan, int globalMaxRange,
                                const EvalWeights& weights, int mctsBudgetMs,
                                const GameConfig& config,
                               bool forcePressure) {
    // Check if we already have a move command
    for (auto& cmd : plan.commands)
        if (cmd.type == TurnCommand::Move) return;

    if (forcePressure && executePressureMove(plan, snapshot, aiKingdom, globalMaxRange, config)) {
        return;
    }

    const int income = estimateIncome(snapshot, aiKingdom, 10, 5);
    const int barracksCount = countBarracks(snapshot, aiKingdom);
    if (barracksCount == 0 && snapshot.kingdom(aiKingdom).gold < 50 && income > 0) {
        return;
    }
    if ((barracksCount == 0 && snapshot.kingdom(aiKingdom).gold < 50) || income == 0) {
        const ResourcePlan resourcePlan = m_economyModule.planResourceGathering(snapshot, ctx, aiKingdom);
        if (!resourcePlan.assignments.empty()) {
            const auto& [pieceId, targetCell] = resourcePlan.assignments.front();
            if (moveTowardTarget(plan, snapshot, aiKingdom, pieceId, targetCell, globalMaxRange)) {
                return;
            }
        }
    }

    if (mctsBudgetMs > 30) {
        MCTSAction best = m_mcts.search(snapshot, aiKingdom,
                                          stratPlan.primaryObjective,
                                          globalMaxRange, weights, mctsBudgetMs, config);
        if (best.type == MCTSAction::MOVE && best.pieceId >= 0) {
            auto* piece = snapshot.kingdom(aiKingdom).getPieceById(best.pieceId);
            if (piece) {
                TurnCommand cmd;
                cmd.type = TurnCommand::Move;
                cmd.pieceId = best.pieceId;
                cmd.origin = piece->position;
                cmd.destination = best.destination;
                plan.commands.push_back(cmd);
                return;
            }
        }
    }

    // Fallback: heuristic move
    heuristicMove(plan, snapshot, ctx, aiKingdom,
                   stratPlan.primaryObjective, globalMaxRange);
}

bool AIDirector::executePressureMove(AIDirectorPlan& plan, const GameSnapshot& snapshot,
                                     KingdomId aiKingdom, int globalMaxRange,
                                     const GameConfig& config) {
    const auto* enemyKing = snapshot.enemyKingdom(aiKingdom).getKing();
    if (!enemyKing) return false;
    const int myCombatPieces = countCombatPieces(snapshot.kingdom(aiKingdom));
    const KingdomId defendingKingdom = snapshot.enemyKingdom(aiKingdom).id;

    const ThreatMap currentThreats = ForwardModel::buildThreatMap(snapshot, aiKingdom, globalMaxRange);
    std::vector<sf::Vector2i> escapeSquares;
    for (int dy = -1; dy <= 1; ++dy) {
        for (int dx = -1; dx <= 1; ++dx) {
            if (dx == 0 && dy == 0) continue;
            sf::Vector2i square = enemyKing->position + sf::Vector2i{dx, dy};
            if (!snapshot.isTraversable(square.x, square.y)) continue;
            escapeSquares.push_back(square);
        }
    }

    const std::vector<AssaultSlot> assaultSlots = buildAssaultSlots(snapshot, enemyKing->position);
    std::array<int, 8> sectorLoads{};
    int nonKingCount = 0;
    bool allNonKingsNear = true;
    int currentCoveredEscapes = 0;
    const int currentSafeEscapes = countSafeKingEscapes(snapshot, defendingKingdom, globalMaxRange);
    for (const auto& escape : escapeSquares) {
        if (currentThreats.isSet(escape)) ++currentCoveredEscapes;
    }

    for (const auto& piece : snapshot.kingdom(aiKingdom).pieces) {
        if (piece.type == PieceType::King) continue;
        ++nonKingCount;
        if (manhattanDistance(piece.position, enemyKing->position) > m_config.pressure.nonKingNearDistance) {
            allNonKingsNear = false;
        }
        if (manhattanDistance(piece.position, enemyKing->position) <= m_config.pressure.sectorLoadDistance) {
            ++sectorLoads[octantIndex(enemyKing->position, piece.position)];
        }
    }

    float bestScore = -1e9f;
    int bestPieceId = -1;
    sf::Vector2i bestDest{0, 0};

    for (const auto& piece : snapshot.kingdom(aiKingdom).pieces) {
        const bool isKing = piece.type == PieceType::King;
        auto moves = ForwardModel::getLegalMoves(snapshot, piece, globalMaxRange);
        if (moves.empty()) continue;
        if (isKing && nonKingCount > 0) continue;

        const int currentDist = manhattanDistance(piece.position, enemyKing->position);
        const int crowdAtOrigin = countFriendlyNeighbors(snapshot, aiKingdom, piece.position, 2);
        const AssaultEval currentAssault = evaluateAssaultPosition(
            snapshot, aiKingdom, piece, piece.position, enemyKing->position,
            assaultSlots, sectorLoads, currentThreats, m_config);
        const bool pieceInPosition = (currentDist <= m_config.pressure.pieceInPositionDistance);

        for (const auto& dest : moves) {
            const int nextDist = manhattanDistance(dest, enemyKing->position);
            const int crowdAtDest = countFriendlyNeighbors(snapshot, aiKingdom, dest, 2);

            GameSnapshot sim = snapshot.clone();
            if (!ForwardModel::applyMove(sim, piece.id, dest, aiKingdom, config)) continue;
            const ThreatMap simThreats = ForwardModel::buildThreatMap(sim, aiKingdom, globalMaxRange);
            const bool givesCheck = ForwardModel::isInCheck(sim, snapshot.enemyKingdom(aiKingdom).id, globalMaxRange);
            const bool isMate = ForwardModel::isCheckmate(sim, defendingKingdom, globalMaxRange);
            const int safeEscapesAfter = countSafeKingEscapes(sim, defendingKingdom, globalMaxRange);

            int coveredEscapes = 0;
            int newCoverage = 0;
            for (const auto& escape : escapeSquares) {
                if (simThreats.isSet(escape)) {
                    ++coveredEscapes;
                    if (!currentThreats.isSet(escape)) ++newCoverage;
                }
            }

            const AssaultEval moveAssault = evaluateAssaultPosition(
                snapshot, aiKingdom, piece, dest, enemyKing->position,
                assaultSlots, sectorLoads, simThreats, m_config);

            float score = 0.0f;
            score += static_cast<float>(currentDist - nextDist) * m_config.pressure.approachDistanceWeight;
            score += static_cast<float>(crowdAtOrigin - crowdAtDest) * m_config.pressure.crowdReductionWeight;
            score += static_cast<float>(newCoverage) * m_config.pressure.newCoverageWeight;
            score += static_cast<float>(coveredEscapes - currentCoveredEscapes) * m_config.pressure.coverageDeltaWeight;
            score += static_cast<float>(currentSafeEscapes - safeEscapesAfter) * m_config.pressure.safeEscapeReductionWeight;
            score -= static_cast<float>(safeEscapesAfter) * m_config.pressure.safeEscapePenaltyWeight;
            score += (moveAssault.value - currentAssault.value) * m_config.pressure.assaultDeltaMultiplier;
            if (givesCheck) score += m_config.pressure.givesCheckBonus;
            if (isMate) score += m_config.pressure.mateBonus;

            switch (piece.type) {
                case PieceType::Rook: score += m_config.pressure.pieceTypeBonusRook; break;
                case PieceType::Bishop: score += m_config.pressure.pieceTypeBonusBishop; break;
                case PieceType::Knight: score += m_config.pressure.pieceTypeBonusKnight; break;
                case PieceType::Pawn: score += m_config.pressure.pieceTypeBonusPawn; break;
                case PieceType::Queen: score += m_config.pressure.pieceTypeBonusQueen; break;
                case PieceType::King: score -= m_config.pressure.kingMovePenalty; break;
            }

            if (piece.id == m_lastMovedPieceId) {
                score -= m_config.pressure.lastMovedPiecePenalty;
            }

            auto* victim = snapshot.enemyKingdom(aiKingdom).getPieceAt(dest);
            if (victim) {
                score += AIEvaluator::pieceValue(victim->type) * m_config.pressure.captureValueMultiplier;
            }

            if (pieceInPosition && !allNonKingsNear) {
                const bool improvesNet = givesCheck || newCoverage > 0 ||
                    safeEscapesAfter < currentSafeEscapes ||
                    moveAssault.value > currentAssault.value + m_config.pressure.inPositionAssaultImproveThreshold ||
                    moveAssault.slotDistance + 1 < currentAssault.slotDistance;
                if (!improvesNet) {
                    score -= m_config.pressure.noNetImprovePenalty;
                } else {
                    score += m_config.pressure.netImproveBonus;
                }
            }

            if (nextDist <= 2 && !isKing) {
                score += m_config.pressure.closeDistanceBonus;
            }
            if (currentDist <= 3 && nextDist >= currentDist && !isKing && newCoverage == 0 && !givesCheck) {
                score -= m_config.pressure.driftPenalty;
            }
            if (piece.type == PieceType::Pawn && myCombatPieces >= 8 && !givesCheck && newCoverage == 0) {
                score -= m_config.pressure.pawnOvercrowdPenalty;
            }

            if (score > bestScore) {
                bestScore = score;
                bestPieceId = piece.id;
                bestDest = dest;
            }
        }
    }

    if (bestPieceId < 0) return false;

    const auto* piece = snapshot.kingdom(aiKingdom).getPieceById(bestPieceId);
    if (!piece) return false;

    TurnCommand cmd;
    cmd.type = TurnCommand::Move;
    cmd.pieceId = bestPieceId;
    cmd.origin = piece->position;
    cmd.destination = bestDest;
    plan.lastMovedPieceId = bestPieceId;
    plan.commands.push_back(cmd);
    return true;
}

// =========================================================================
//  heuristicMove — fast greedy fallback
// =========================================================================

void AIDirector::heuristicMove(AIDirectorPlan& plan, const GameSnapshot& snapshot,
                                 const AITurnContext& ctx, KingdomId aiKingdom,
                                 StrategicObjective objective, int globalMaxRange) {
    float bestScore = -1e9f;
    int bestPieceId = -1;
    sf::Vector2i bestDest{0, 0};

    for (auto& piece : snapshot.kingdom(aiKingdom).pieces) {
        auto moves = ForwardModel::getLegalMoves(snapshot, piece, globalMaxRange);
        for (auto& dest : moves) {
            float score = 0.0f;

            // Capture value
            auto* victim = snapshot.enemyKingdom(aiKingdom).getPieceAt(dest);
            if (victim) score += AIEvaluator::pieceValue(victim->type);

            auto* eKing = snapshot.enemyKingdom(aiKingdom).getKing();
            if (eKing) {
                int currentDistToEK = std::abs(piece.position.x - eKing->position.x)
                                    + std::abs(piece.position.y - eKing->position.y);
                int distToEK = std::abs(dest.x - eKing->position.x)
                             + std::abs(dest.y - eKing->position.y);

                if (objective == StrategicObjective::CHECKMATE_PRESS ||
                    objective == StrategicObjective::RUSH_ATTACK) {
                    score += static_cast<float>(currentDistToEK - distToEK) * m_config.heuristic.checkmateApproachWeight;
                    score += std::max(0.0f, m_config.heuristic.checkmateProximityBase - static_cast<float>(distToEK));
                    if (piece.type != PieceType::King) score += m_config.heuristic.checkmatePieceBonus;
                }

                if (objective == StrategicObjective::BUILD_ARMY) {
                    score += static_cast<float>(currentDistToEK - distToEK) * m_config.heuristic.buildArmyApproachWeight;
                    if (piece.type != PieceType::King) score += m_config.heuristic.buildArmyPieceBonus;
                }
            }

            if (objective == StrategicObjective::ECONOMY_EXPAND) {
                auto* bld = snapshot.buildingAt(dest);
                if (bld && (bld->type == BuildingType::Mine || bld->type == BuildingType::Farm))
                    score += m_config.heuristic.economyResourceBonus;

                int bestResourceDist = 999;
                for (const auto& cell : ctx.freeResourceCells) {
                    int dist = std::abs(dest.x - cell.x) + std::abs(dest.y - cell.y);
                    if (dist < bestResourceDist) bestResourceDist = dist;
                }
                if (bestResourceDist < 999) {
                    score += std::max(0.0f, m_config.heuristic.economyResourceDistBase
                                         - static_cast<float>(bestResourceDist) * m_config.heuristic.economyResourceDistScale);
                }
            }

            if (objective == StrategicObjective::DEFEND_KING) {
                auto* myKing = snapshot.kingdom(aiKingdom).getKing();
                if (myKing && piece.type != PieceType::King) {
                    int currentDistToMyKing = std::abs(piece.position.x - myKing->position.x)
                                            + std::abs(piece.position.y - myKing->position.y);
                    int distToMyKing = std::abs(dest.x - myKing->position.x)
                                     + std::abs(dest.y - myKing->position.y);
                    score += static_cast<float>(currentDistToMyKing - distToMyKing) * m_config.heuristic.defendKingApproachWeight;
                    score += std::max(0.0f, m_config.heuristic.defendKingProximityBase
                                         - static_cast<float>(distToMyKing) * m_config.heuristic.defendKingProximityScale);
                }
            }

            if (score > bestScore) {
                bestScore = score;
                bestPieceId = piece.id;
                bestDest = dest;
            }
        }
    }

    if (bestPieceId >= 0) {
        auto* piece = snapshot.kingdom(aiKingdom).getPieceById(bestPieceId);
        if (piece) {
            TurnCommand cmd;
            cmd.type = TurnCommand::Move;
            cmd.pieceId = bestPieceId;
            cmd.origin = piece->position;
            cmd.destination = bestDest;
            plan.lastMovedPieceId = bestPieceId;
            plan.commands.push_back(cmd);
        }
    }
}

// =========================================================================
//  executeBuild
// =========================================================================

void AIDirector::executeBuild(AIDirectorPlan& plan, const GameSnapshot& snapshot,
                                KingdomId aiKingdom, const TurnPlan& stratPlan,
                                int turnNumber, int incomePerTurn,
                                const GameConfig& config) {
    // Already have a build command?
    for (auto& cmd : plan.commands)
        if (cmd.type == TurnCommand::Build) return;

    const int gold = snapshot.kingdom(aiKingdom).gold;
    const int barracksCost = config.getBarracksCost();
    const int barracksCount = countBarracks(snapshot, aiKingdom);
    const bool forceExpandBarracks =
        (barracksCount == 0 && gold >= barracksCost) ||
        (barracksCount == 1 && gold >= barracksCost * 2) ||
        (barracksCount == 2 && gold >= barracksCost * 3 && turnNumber >= 12);

    if (!stratPlan.shouldBuild) {
        if (!forceExpandBarracks) {
            if (barracksCount > 0) return;
            if (gold < barracksCost) return;
        }
    }

    StrategicObjective buildObjective = stratPlan.primaryObjective;
    if (forceExpandBarracks) {
        buildObjective = StrategicObjective::BUILD_INFRASTRUCTURE;
    }

    auto buildSuggestion = m_buildModule.suggestBuild(
        snapshot, aiKingdom, buildObjective,
        turnNumber, incomePerTurn, config);

    if (buildSuggestion.has_value()) {
        TurnCommand cmd;
        cmd.type = TurnCommand::Build;
        cmd.buildingType = buildSuggestion->type;
        cmd.buildOrigin = buildSuggestion->position;
        plan.commands.push_back(cmd);
    }
}

// =========================================================================
//  executeUpgrades
// =========================================================================

void AIDirector::executeUpgrades(AIDirectorPlan& plan, const GameSnapshot& snapshot,
                                 KingdomId aiKingdom, const GameConfig& config) {
    int availableGold = snapshot.kingdom(aiKingdom).gold;

    for (const auto& command : plan.commands) {
        if (command.type != TurnCommand::Build) {
            continue;
        }

        switch (command.buildingType) {
            case BuildingType::Barracks:
                availableGold -= config.getBarracksCost();
                break;
            case BuildingType::WoodWall:
                availableGold -= config.getWoodWallCost();
                break;
            case BuildingType::StoneWall:
                availableGold -= config.getStoneWallCost();
                break;
            case BuildingType::Arena:
                availableGold -= config.getArenaCost();
                break;
            default:
                break;
        }
    }

    std::vector<const SnapPiece*> candidates;
    candidates.reserve(snapshot.kingdom(aiKingdom).pieces.size());
    for (const auto& piece : snapshot.kingdom(aiKingdom).pieces) {
        if (piece.type == PieceType::King || piece.type == PieceType::Queen) {
            continue;
        }
        candidates.push_back(&piece);
    }

    std::sort(candidates.begin(), candidates.end(), [](const SnapPiece* lhs, const SnapPiece* rhs) {
        return AIEvaluator::pieceValue(lhs->type) > AIEvaluator::pieceValue(rhs->type);
    });

    SnapKingdom simulatedKingdom = snapshot.kingdom(aiKingdom);
    for (const SnapPiece* piece : candidates) {
        const PieceType target = nextUpgradeTarget(simulatedKingdom, *piece);
        if (target == piece->type) {
            continue;
        }
        if (!canUpgradeSnapshotPiece(*piece, target, config)) {
            continue;
        }

        const int cost = config.getUpgradeCost(piece->type, target);
        if (cost <= 0 || cost > availableGold) {
            continue;
        }

        TurnCommand cmd;
        cmd.type = TurnCommand::Upgrade;
        cmd.upgradePieceId = piece->id;
        cmd.upgradeTarget = target;
        plan.commands.push_back(cmd);
        availableGold -= cost;

        if (SnapPiece* simulatedPiece = simulatedKingdom.getPieceById(piece->id)) {
            simulatedPiece->type = target;
        }
    }
}

// =========================================================================
//  executeProductions — produce in ALL free barracks
// =========================================================================

void AIDirector::executeProductions(AIDirectorPlan& plan, const GameSnapshot& snapshot,
                                      KingdomId aiKingdom, const TurnPlan& stratPlan,
                                      int turnNumber, const GameConfig& config) {
    ProductionPlan prodPlan = m_economyModule.planProduction(
        snapshot, aiKingdom, stratPlan.primaryObjective, turnNumber);

    for (auto& [barracksId, pieceType] : prodPlan.orders) {
        TurnCommand cmd;
        cmd.type = TurnCommand::Produce;
        cmd.barracksId = barracksId;
        cmd.produceType = pieceType;
        plan.commands.push_back(cmd);
    }
}

