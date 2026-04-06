#include "AI/AIStrategyMove.hpp"
#include "AI/AIBrain.hpp"
#include "AI/AITacticalEngine.hpp"
#include "AI/AITurnContext.hpp"
#include "Board/Board.hpp"
#include "Board/Cell.hpp"
#include "Kingdom/Kingdom.hpp"
#include "Units/Piece.hpp"
#include "Units/PieceType.hpp"
#include "Units/MovementRules.hpp"
#include "Buildings/Building.hpp"
#include "Buildings/BuildingType.hpp"
#include "Config/GameConfig.hpp"
#include "Config/AIConfig.hpp"
#include "Systems/CheckSystem.hpp"
#include <cmath>
#include <limits>
#include <algorithm>
#include <iostream>
#include <array>
#include <unordered_map>
#include <deque>

// =====================================================================
// GLOBAL LOOP DETECTION SYSTEM
// Track last N moves. If a (pieceId, destination) pair appears in recent
// history, penalize it heavily. This catches ALL loop types (2,3,4+ turns).
// =====================================================================
struct MoveRecord {
    int pieceId;
    sf::Vector2i from;
    sf::Vector2i to;
};
static std::deque<MoveRecord> s_moveHistory;
static constexpr int HISTORY_SIZE = 12;

// Track which piece moved last turn — to force army rotation
static int s_lastMovedPieceId = -1;

// Helper: check how many times a (pieceId, dest) appears in recent history
static int countRecentOccurrences(int pieceId, sf::Vector2i dest) {
    int count = 0;
    for (const auto& rec : s_moveHistory) {
        if (rec.pieceId == pieceId && rec.to == dest) ++count;
    }
    return count;
}

// Helper: record a move into history
static void recordMove(int pieceId, sf::Vector2i from, sf::Vector2i to) {
    s_moveHistory.push_back({pieceId, from, to});
    if (static_cast<int>(s_moveHistory.size()) > HISTORY_SIZE) {
        s_moveHistory.pop_front();
    }
    s_lastMovedPieceId = pieceId;
}

// Helper: make a move command
static TurnCommand makeMoveCmd(int pieceId, sf::Vector2i origin, sf::Vector2i dest) {
    TurnCommand cmd;
    cmd.type = TurnCommand::Move;
    cmd.pieceId = pieceId;
    cmd.origin = origin;
    cmd.destination = dest;
    recordMove(pieceId, origin, dest);
    return cmd;
}

// Helper: is piece productively sitting on a resource?
static bool isOnResource(const Piece& piece, const Board& board) {
    const Cell& cell = board.getCell(piece.position.x, piece.position.y);
    return cell.building &&
           (cell.building->type == BuildingType::Mine || cell.building->type == BuildingType::Farm);
}

// Helper: count pieces currently on resources
static int countPiecesOnResources(const Kingdom& self, const Board& board) {
    int count = 0;
    for (const auto& p : self.pieces) {
        if (isOnResource(p, board)) ++count;
    }
    return count;
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

static std::vector<AssaultSlot> buildAssaultSlots(const Piece& enemyKing, const Board& board) {
    std::vector<AssaultSlot> slots;

    auto addRing = [&](int radius, bool isEscape) {
        for (int dy = -radius; dy <= radius; ++dy) {
            for (int dx = -radius; dx <= radius; ++dx) {
                if (dx == 0 && dy == 0) continue;
                if (std::max(std::abs(dx), std::abs(dy)) != radius) continue;

                sf::Vector2i pos = enemyKing.position + sf::Vector2i(dx, dy);
                if (!board.isInBounds(pos.x, pos.y)) continue;

                const Cell& cell = board.getCell(pos.x, pos.y);
                if (!cell.isInCircle || cell.type == CellType::Water) continue;

                slots.push_back({pos, isEscape, octantIndex(enemyKing.position, pos)});
            }
        }
    };

    addRing(1, true);
    addRing(2, false);
    return slots;
}

static AssaultEval evaluateAssaultPosition(const Piece& piece,
                                          sf::Vector2i pos,
                                          const Kingdom& self,
                                          const Piece& enemyKing,
                                          const std::vector<AssaultSlot>& slots,
                                          const std::array<int, 8>& sectorLoads,
                                          const AITurnContext& ctx) {
    AssaultEval best;

    const int currentSector = octantIndex(enemyKing.position, piece.position);
    const bool subtractCurrentSector = (manhattanDistance(piece.position, enemyKing.position) <= 6);
    const int moveSector = octantIndex(enemyKing.position, pos);

    for (const auto& slot : slots) {
        int slotSectorLoad = sectorLoads[slot.sector];
        int moveSectorLoad = sectorLoads[moveSector];
        if (subtractCurrentSector && currentSector == slot.sector) {
            slotSectorLoad = std::max(0, slotSectorLoad - 1);
        }
        if (subtractCurrentSector && currentSector == moveSector) {
            moveSectorLoad = std::max(0, moveSectorLoad - 1);
        }

        const Piece* occupant = self.getPieceAt(slot.pos);
        const bool occupiedByFriend = occupant && occupant->id != piece.id;
        const int slotDistance = manhattanDistance(pos, slot.pos);
        const bool exactSlot = (pos == slot.pos);
        const int uncoveredEscapePressure = (slot.isEscape && !ctx.selfThreats.isSet(slot.pos)) ? 1 : 0;

        float value = 0.0f;
        value += uncoveredEscapePressure ? 80.0f : (slot.isEscape ? 26.0f : 14.0f);
        value += exactSlot ? (slot.isEscape ? 24.0f : 10.0f) : 0.0f;
        value -= static_cast<float>(slotDistance) * 8.0f;
        value -= static_cast<float>(slotSectorLoad) * 20.0f;
        value -= static_cast<float>(moveSectorLoad) * 12.0f;
        if (occupiedByFriend) value -= 140.0f;

        if (value > best.value) {
            best.value = value;
            best.slotDistance = slotDistance;
            best.sectorLoad = moveSectorLoad;
            best.uncoveredEscapePressure = uncoveredEscapePressure;
        }
    }

    return best;
}

std::vector<TurnCommand> AIStrategyMove::decide(Board& board, Kingdom& self,
                                                  Kingdom& enemy, const GameConfig& config,
                                                  const AIConfig& aiConfig, const AIBrain& brain,
                                                  AITacticalEngine& engine, const AITurnContext& ctx,
                                                  const std::vector<Building>& publicBuildings,
                                                  bool hasMoved) {
    std::vector<TurnCommand> commands;
    if (hasMoved) { std::cerr << "    [Move] hasMoved=true, skipping" << std::endl; return commands; }

    AIPhase phase = brain.getPhase();
    const auto& priorities = brain.getPriorities();
    Piece* king = self.getKing();
    std::cerr << "    [Move] phase=" << brain.getPhaseName() << " atk=" << priorities.attack << " def=" << priorities.defense << " econ=" << priorities.economy << std::endl;

    // Useful stats
    int totalPieces = static_cast<int>(self.pieces.size());
    int nonKingPieces = totalPieces - (king ? 1 : 0);
    int piecesOnResources = countPiecesOnResources(self, board);
    // How many workers we want on resources
    int desiredWorkers = 2;
    if (phase == AIPhase::AGGRESSION || phase == AIPhase::ENDGAME) desiredWorkers = 1;
    if (phase == AIPhase::MID_GAME) desiredWorkers = 2;
    if (nonKingPieces <= 3) desiredWorkers = 1;
    if (nonKingPieces <= 1) desiredWorkers = 0;

    // Enemy target
    const Piece* enemyKing = enemy.getKing();
    sf::Vector2i targetPos = enemyKing ? enemyKing->position
                           : (!enemy.pieces.empty() ? enemy.pieces.front().position
                           : sf::Vector2i{board.getRadius(), board.getRadius()});

    std::cerr << "    [Move] pieces=" << totalPieces << " nonKing=" << nonKingPieces
              << " onRes=" << piecesOnResources << " wantWorkers=" << desiredWorkers
              << " target=(" << targetPos.x << "," << targetPos.y << ")" << std::endl;

    // =========================================================================
    // 1. CRISIS: escape check
    // =========================================================================
    if (phase == AIPhase::CRISIS) {
        std::cerr << "    [Move] CRISIS" << std::endl;
        ScoredMove best = engine.findBestMove(board, self, enemy, config, ctx);
        if (best.pieceId >= 0) {
            Piece* p = self.getPieceById(best.pieceId);
            commands.push_back(makeMoveCmd(best.pieceId, p ? p->position : best.from, best.to));
            return commands;
        }
        if (king) {
            auto it = ctx.selfMoves.find(king->id);
            if (it != ctx.selfMoves.end()) {
                sf::Vector2i bestDest = king->position;
                int bestThreatCount = 999;
                for (const auto& move : it->second) {
                    if (ctx.enemyThreats.isSet(move)) continue;
                    int threats = 0;
                    for (int dy2 = -1; dy2 <= 1; ++dy2)
                        for (int dx2 = -1; dx2 <= 1; ++dx2)
                            if (ctx.enemyThreats.isSet(move.x + dx2, move.y + dy2))
                                ++threats;
                    if (threats < bestThreatCount) {
                        bestThreatCount = threats;
                        bestDest = move;
                    }
                }
                if (bestDest != king->position) {
                    commands.push_back(makeMoveCmd(king->id, king->position, bestDest));
                    return commands;
                }
                if (!it->second.empty()) {
                    commands.push_back(makeMoveCmd(king->id, king->position, it->second[0]));
                    return commands;
                }
            }
        }
        return commands;
    }

    // =========================================================================
    // 2. CHECKMATE-IN-1
    // =========================================================================
    if (priorities.attack >= 0.5f) {
        std::cerr << "    [Move] Checking checkmate-in-1" << std::endl;
        ScoredMove mate = engine.findCheckmateIn1(board, self, enemy, config, ctx);
        if (mate.pieceId >= 0 && mate.score > 90000.0f) {
            Piece* p = self.getPieceById(mate.pieceId);
            std::cerr << "    [Move] CHECKMATE FOUND!" << std::endl;
            commands.push_back(makeMoveCmd(mate.pieceId, p ? p->position : mate.from, mate.to));
            return commands;
        }
    }

    // =========================================================================
    // 3. TACTICAL CAPTURES — always take profitable safe captures
    // =========================================================================
    {
        ScoredMove best = engine.findBestMove(board, self, enemy, config, ctx);
        if (best.pieceId >= 0) {
            Piece* p = self.getPieceById(best.pieceId);
            if (p) {
                const Cell& dc = board.getCell(best.to.x, best.to.y);
                bool isCap = dc.piece && dc.piece->kingdom != self.id;
                // Never try to capture the enemy king — must use checkmate
                bool isKingTarget = isCap && dc.piece->type == PieceType::King;
                bool isSafe = engine.isMoveSafe(best.to, ctx);
                // Take captures: always if safe, or if winning trade (score > 500)
                if (isCap && !isKingTarget && (isSafe || best.score > 500.0f)) {
                    std::cerr << "    [Move] Capture: piece=" << best.pieceId
                              << " score=" << best.score << std::endl;
                    commands.push_back(makeMoveCmd(best.pieceId, p->position, best.to));
                    return commands;
                }
            }
        }
    }

    // =========================================================================
    // 4. DEFEND THREATENED KING
    // =========================================================================
    if (king && ctx.enemyThreats.isSet(king->position)) {
        std::cerr << "    [Move] King threatened!" << std::endl;
        auto it = ctx.selfMoves.find(king->id);
        if (it != ctx.selfMoves.end()) {
            sf::Vector2i bestSafe = king->position;
            float bestSafeDist = 0.0f;
            for (const auto& move : it->second) {
                if (!ctx.enemyThreats.isSet(move)) {
                    float dx = static_cast<float>(move.x - king->position.x);
                    float dy = static_cast<float>(move.y - king->position.y);
                    float dist = dx * dx + dy * dy;
                    if (dist > bestSafeDist) {
                        bestSafeDist = dist;
                        bestSafe = move;
                    }
                }
            }
            if (bestSafe != king->position) {
                commands.push_back(makeMoveCmd(king->id, king->position, bestSafe));
                return commands;
            }
        }
    }

    // =========================================================================
    // 5. RETREAT THREATENED PIECES (or counter-capture the attacker)
    //    Non-king pieces sitting on enemy-threatened squares are at risk of
    //    being captured for free. Evaluate each one:
    //      - Find the cheapest enemy attacker that can reach our piece.
    //      - If undefended, or if the cheapest attacker is cheaper than us,
    //        try to: (a) retreat to a safe square, (b) capture the attacker
    //        with a DIFFERENT friendly piece, or (c) interpose a cheap piece.
    //    Prioritize by piece value (save the most valuable first).
    // =========================================================================
    {
        struct ThreatenedInfo {
            int pieceId;
            float value;
            sf::Vector2i pos;
            int cheapestAttackerId = -1; // enemy piece that threatens us most cheaply
            float cheapestAttackerValue = 99999.0f;
        };
        std::vector<ThreatenedInfo> threatened;

        for (const auto& piece : self.pieces) {
            if (piece.type == PieceType::King) continue; // king handled in section 4
            if (!ctx.enemyThreats.isSet(piece.position)) continue;

            float myVal = AITacticalEngine::pieceValue(piece.type);

            // Check if piece is defended (our own threats cover the square)
            bool defended = ctx.selfThreats.isSet(piece.position);

            // Find cheapest enemy attacker that can reach this square
            float cheapestAttacker = 99999.0f;
            int cheapestAttackerId = -1;
            for (const auto& [enemyId, moves] : ctx.enemyMoves) {
                for (const auto& m : moves) {
                    if (m == piece.position) {
                        const Piece* ep = enemy.getPieceById(enemyId);
                        if (ep) {
                            float av = AITacticalEngine::pieceValue(ep->type);
                            if (av < cheapestAttacker) {
                                cheapestAttacker = av;
                                cheapestAttackerId = enemyId;
                            }
                        }
                        break; // this enemy piece can reach us, check next enemy
                    }
                }
            }

            // Decide if at risk:
            //  - Undefended piece → always at risk
            //  - Defended but cheapest attacker is cheaper → at risk (bad trade)
            //  - Defended and attacker is equal or more expensive → probably safe
            bool atRisk = false;
            if (!defended) {
                atRisk = true;
            } else if (cheapestAttacker < myVal * 0.9f) {
                // Attacker is significantly cheaper — trade is losing for us
                atRisk = true;
            }

            if (atRisk) {
                threatened.push_back({piece.id, myVal, piece.position,
                                      cheapestAttackerId, cheapestAttacker});
            }
        }

        // Sort by value descending — retreat the most valuable piece first
        std::sort(threatened.begin(), threatened.end(),
                  [](const ThreatenedInfo& a, const ThreatenedInfo& b) {
                      return a.value > b.value;
                  });

        for (const auto& t : threatened) {
            auto it = ctx.selfMoves.find(t.pieceId);
            if (it == ctx.selfMoves.end()) continue;

            sf::Vector2i bestRetreat = t.pos;
            float bestRetreatScore = -std::numeric_limits<float>::max();

            for (const auto& move : it->second) {
                if (ctx.enemyThreats.isSet(move)) continue; // not safe either

                float score = 0.0f;

                // Prefer staying close to the action (don't run to the corner)
                if (enemyKing) {
                    float dx = static_cast<float>(move.x - targetPos.x);
                    float dy = static_cast<float>(move.y - targetPos.y);
                    float dist = std::sqrt(dx * dx + dy * dy);
                    score -= dist * 0.5f; // slight penalty for running too far
                }

                // Bonus for landing on a resource
                const Cell& mc = board.getCell(move.x, move.y);
                if (mc.building) {
                    if (mc.building->type == BuildingType::Mine) score += 50.0f;
                    if (mc.building->type == BuildingType::Farm) score += 25.0f;
                }

                // Bonus for defended squares (our threats cover it)
                if (ctx.selfThreats.isSet(move)) score += 30.0f;

                // Loop penalty
                Piece* p = self.getPieceById(t.pieceId);
                if (p) {
                    int repeats = countRecentOccurrences(t.pieceId, move);
                    if (repeats > 0) score -= 200.0f * repeats;
                }

                if (score > bestRetreatScore) {
                    bestRetreatScore = score;
                    bestRetreat = move;
                }
            }

            if (bestRetreat != t.pos) {
                Piece* p = self.getPieceById(t.pieceId);
                if (p) {
                    std::cerr << "    [Move] RETREAT: piece " << t.pieceId
                              << " (val=" << t.value << ") from (" << t.pos.x << "," << t.pos.y
                              << ") to (" << bestRetreat.x << "," << bestRetreat.y << ")" << std::endl;
                    commands.push_back(makeMoveCmd(t.pieceId, p->position, bestRetreat));
                    return commands;
                }
            }

            // Retreat failed (no safe square). Try counter-capture:
            // Can a DIFFERENT friendly piece safely capture the attacker?
            if (t.cheapestAttackerId >= 0) {
                const Piece* attacker = enemy.getPieceById(t.cheapestAttackerId);
                if (attacker) {
                    sf::Vector2i attackerPos = attacker->position;
                    float bestCounterScore = -std::numeric_limits<float>::max();
                    int bestCounterId = -1;
                    sf::Vector2i bestCounterFrom = {0, 0};

                    for (const auto& [friendId, friendMoves] : ctx.selfMoves) {
                        if (friendId == t.pieceId) continue; // can't counter with the threatened piece itself
                        const Piece* fp = self.getPieceById(friendId);
                        if (!fp || fp->type == PieceType::King) continue;

                        for (const auto& fm : friendMoves) {
                            if (fm != attackerPos) continue; // can't reach the attacker
                            // Check if the counter-capture square is safe after capture
                            // (enemy threats minus the attacker itself; approximate)
                            float counterVal = AITacticalEngine::pieceValue(fp->type);
                            float capturedVal = t.cheapestAttackerValue;
                            // Only counter if we don't lose material:
                            // safe trade or winning trade
                            bool counterSafe = !ctx.enemyThreats.isSet(attackerPos) ||
                                               capturedVal >= counterVal;
                            if (!counterSafe) continue;

                            float score = capturedVal - counterVal * 0.1f;
                            if (score > bestCounterScore) {
                                bestCounterScore = score;
                                bestCounterId = friendId;
                                bestCounterFrom = fp->position;
                            }
                        }
                    }

                    if (bestCounterId >= 0) {
                        std::cerr << "    [Move] COUNTER-CAPTURE: piece " << bestCounterId
                                  << " captures attacker at (" << attackerPos.x << "," << attackerPos.y
                                  << ") to protect piece " << t.pieceId << std::endl;
                        commands.push_back(makeMoveCmd(bestCounterId, bestCounterFrom, attackerPos));
                        return commands;
                    }
                }
            }
        }
    }

    // =========================================================================
    // 6. MARRIAGE PURSUIT — move pieces toward church to create a Queen
    //    Only in non-attack phases, and only if pieces are CLOSE to the church.
    //    In AGGRESSION/ENDGAME, the army should be attacking, not chasing marriage.
    //    (was section 5 before retreat logic was added)
    // =========================================================================
    if (!self.hasQueen() && nonKingPieces >= 2
        && phase != AIPhase::AGGRESSION && phase != AIPhase::ENDGAME) {
        // Find a church
        const Building* church = nullptr;
        for (const auto& b : publicBuildings) {
            if (b.type == BuildingType::Church && !b.isDestroyed()) {
                church = &b;
                break;
            }
        }

        if (church) {
            auto churchCells = church->getOccupiedCells();
            sf::Vector2i churchCenter = {0, 0};
            for (const auto& cc : churchCells) {
                churchCenter.x += cc.x;
                churchCenter.y += cc.y;
            }
            churchCenter.x /= static_cast<int>(churchCells.size());
            churchCenter.y /= static_cast<int>(churchCells.size());

            // Check which pieces we need on the church
            bool kingOnChurch = false, bishopOnChurch = false, pawnOnChurch = false;
            for (const auto& cc : churchCells) {
                const Cell& cell = board.getCell(cc.x, cc.y);
                if (cell.piece && cell.piece->kingdom == self.id) {
                    if (cell.piece->type == PieceType::King) kingOnChurch = true;
                    else if (cell.piece->type == PieceType::Bishop) bishopOnChurch = true;
                    else if (cell.piece->type == PieceType::Pawn) pawnOnChurch = true;
                }
            }

            // Find the best piece to move toward church
            // Priority: move the piece that isn't yet on the church
            auto needsToReachChurch = [&](const Piece& p) -> bool {
                if (p.type == PieceType::King && !kingOnChurch) return true;
                if (p.type == PieceType::Bishop && !bishopOnChurch) return true;
                if (p.type == PieceType::Pawn && !pawnOnChurch) return true;
                return false;
            };

            // Check if we have all required pieces
            bool hasBishop = false, hasPawn = false;
            for (const auto& p : self.pieces) {
                if (p.type == PieceType::Bishop) hasBishop = true;
                if (p.type == PieceType::Pawn) hasPawn = true;
            }

            if (hasBishop && hasPawn && king) {
                // Only pursue marriage if the needed pieces are reasonably close
                // (within 8 Manhattan distance of church center)
                bool closeEnough = true;
                for (const auto& piece : self.pieces) {
                    if (!needsToReachChurch(piece)) continue;
                    float minDist = std::numeric_limits<float>::max();
                    for (const auto& cc : churchCells) {
                        float d = static_cast<float>(std::abs(piece.position.x - cc.x) + std::abs(piece.position.y - cc.y));
                        minDist = std::min(minDist, d);
                    }
                    if (minDist > 8.0f) { closeEnough = false; break; }
                }

                if (!closeEnough) goto skip_marriage;
                float bestScore = -std::numeric_limits<float>::max();
                int bestPId = -1;
                sf::Vector2i bestDest = {0, 0};

                for (const auto& piece : self.pieces) {
                    if (!needsToReachChurch(piece)) continue;

                    auto it = ctx.selfMoves.find(piece.id);
                    if (it == ctx.selfMoves.end()) continue;

                    float curDist = std::numeric_limits<float>::max();
                    for (const auto& cc : churchCells) {
                        float d = static_cast<float>(std::abs(piece.position.x - cc.x) + std::abs(piece.position.y - cc.y));
                        curDist = std::min(curDist, d);
                    }

                    for (const auto& move : it->second) {
                        if (!engine.isMoveSafe(move, ctx)) continue;

                        // For king, also check it's not threatened
                        if (piece.type == PieceType::King && ctx.enemyThreats.isSet(move)) continue;

                        float moveDist = std::numeric_limits<float>::max();
                        for (const auto& cc : churchCells) {
                            float d = static_cast<float>(std::abs(move.x - cc.x) + std::abs(move.y - cc.y));
                            moveDist = std::min(moveDist, d);
                        }

                        float progress = curDist - moveDist;
                        if (progress > bestScore) {
                            bestScore = progress;
                            bestPId = piece.id;
                            bestDest = move;
                        }
                    }
                }

                if (bestPId >= 0 && bestScore > 0.0f) {
                    Piece* p = self.getPieceById(bestPId);
                    if (p) {
                        std::cerr << "    [Move] Marriage pursuit: piece " << bestPId
                                  << " toward church at (" << bestDest.x << "," << bestDest.y << ")" << std::endl;
                        commands.push_back(makeMoveCmd(bestPId, p->position, bestDest));
                        return commands;
                    }
                }
            }
            skip_marriage:;
        }
    }

    // =========================================================================
    // 7. ADVANCE ARMY TOWARD ENEMY — SURROUND & BLOCKADE STRATEGY
    //    Key insight: pieces already near the enemy king HOLD POSITION
    //    and let far-away pieces catch up. Only move a nearby piece if it
    //    can give check or cover a new escape square it doesn't already cover.
    //    SKIP if we don't have mating material — pointless to advance.
    // =========================================================================
    if (priorities.attack >= 0.2f && !enemy.pieces.empty()
        && brain.hasSufficientMatingMaterial(self)) {
        std::cerr << "    [Move] Advance toward enemy (surround)" << std::endl;

        const Piece* enemyK = enemy.getKing();

        // Compute escape squares around the enemy king (1-cell radius)
        std::vector<sf::Vector2i> escapeSquares;
        std::vector<AssaultSlot> assaultSlots;
        if (enemyK) {
            for (int dy = -1; dy <= 1; ++dy) {
                for (int dx = -1; dx <= 1; ++dx) {
                    if (dx == 0 && dy == 0) continue;
                    sf::Vector2i sq = enemyK->position + sf::Vector2i(dx, dy);
                    if (sq.x >= 0 && sq.y >= 0 && sq.x < board.getDiameter() && sq.y < board.getDiameter()) {
                        const Cell& c = board.getCell(sq.x, sq.y);
                        if (c.isInCircle && c.type != CellType::Water) {
                            escapeSquares.push_back(sq);
                        }
                    }
                }
            }

            assaultSlots = buildAssaultSlots(*enemyK, board);
        }

        std::array<int, 8> sectorLoads{};
        if (enemyK) {
            for (const auto& p : self.pieces) {
                if (p.type == PieceType::King) continue;
                if (manhattanDistance(p.position, enemyK->position) > 6) continue;
                ++sectorLoads[octantIndex(enemyK->position, p.position)];
            }
        }

        // Count pieces already "in position" (Manhattan ≤ 4 of enemy king)
        // These pieces should HOLD and let others advance
        int inPositionCount = 0;
        bool allInPosition = true;
        if (enemyK) {
            for (const auto& p : self.pieces) {
                if (p.type == PieceType::King) continue;
                int md = std::abs(p.position.x - enemyK->position.x)
                       + std::abs(p.position.y - enemyK->position.y);
                if (md <= 4) ++inPositionCount;
                else allInPosition = false;
            }
        }

        float bestScore = -std::numeric_limits<float>::max();
        int bestPieceId = -1;
        sf::Vector2i bestMovePos = {0, 0};
        int bestNewCoverage = -1;
        int bestSectorLoad = std::numeric_limits<int>::max();
        int bestSlotDistance = std::numeric_limits<int>::max();
        float bestProgress = -std::numeric_limits<float>::max();

        for (const auto& piece : self.pieces) {
            // King: only advance if it's the sole piece
            if (piece.type == PieceType::King) {
                if (nonKingPieces >= 1) continue;
            }

            // Workers on resources: only mobilize excess workers
            if (isOnResource(piece, board) && piece.type != PieceType::King) {
                if (piecesOnResources <= desiredWorkers) continue;
            }

            float currentDist = 0.0f;
            int currentMd = 0;
            AssaultEval currentAssault;
            if (enemyK) {
                currentDist = std::sqrt(
                    static_cast<float>((piece.position.x - targetPos.x) * (piece.position.x - targetPos.x) +
                                       (piece.position.y - targetPos.y) * (piece.position.y - targetPos.y)));
                currentMd = std::abs(piece.position.x - enemyK->position.x)
                          + std::abs(piece.position.y - enemyK->position.y);
                currentAssault = evaluateAssaultPosition(piece, piece.position, self,
                    *enemyK, assaultSlots, sectorLoads, ctx);
            }

            // === KEY FIX: Pieces already near the king HOLD POSITION ===
            // Pieces at Manhattan ≤ 4 of enemy king should NOT shuffle around.
            // They hold and wait for reinforcements. Only move if:
            //   (a) ALL non-king pieces are already in position (final assault), OR
            //   (b) This move gives CHECK (sets up checkmate)
            bool pieceInPosition = (currentMd <= 4);
            if (pieceInPosition && !allInPosition) {
                std::cerr << "    [Move] Piece " << piece.id << " HOLDING at md=" << currentMd << std::endl;
                continue;
            }

            // If this piece is currently under threat and retreat section
            // couldn't save it, don't advance it further into danger.
            if (ctx.enemyThreats.isSet(piece.position)) {
                std::cerr << "    [Move] Piece " << piece.id << " already threatened, skip advance" << std::endl;
                continue;
            }

            auto it = ctx.selfMoves.find(piece.id);
            if (it == ctx.selfMoves.end()) continue;

            for (const auto& move : it->second) {
                bool safe = engine.isMoveSafe(move, ctx);
                // NEVER advance to a threatened square — piece will be
                // captured for free. Tactical captures (section 3) already
                // handle profitable trades; the advance section is purely
                // positional and must stay safe.
                if (!safe) continue;

                float dx = static_cast<float>(move.x - targetPos.x);
                float dy = static_cast<float>(move.y - targetPos.y);
                float dist = std::sqrt(dx * dx + dy * dy);
                float progress = currentDist - dist;

                // Base score: getting closer to enemy king
                float score = progress * 4.0f;
                int newCoverage = 0;
                int moveSectorLoad = 0;
                int slotDistance = 999;

                // === SURROUND BONUS ===
                if (enemyK) {
                    AssaultEval moveAssault = evaluateAssaultPosition(piece, move, self,
                        *enemyK, assaultSlots, sectorLoads, ctx);
                    score += (moveAssault.value - currentAssault.value) * 1.2f;
                    moveSectorLoad = moveAssault.sectorLoad;
                    slotDistance = moveAssault.slotDistance;

                    // Count escape squares this move would newly threaten
                    for (const auto& esc : escapeSquares) {
                        if (!ctx.selfThreats.isSet(esc)) {
                            int escDist = std::abs(move.x - esc.x) + std::abs(move.y - esc.y);
                            int threatRange = (piece.type == PieceType::Knight) ? 3 : 1;
                            if (piece.type == PieceType::Rook || piece.type == PieceType::Queen) {
                                if (move.x == esc.x || move.y == esc.y) threatRange = 8;
                            }
                            if (piece.type == PieceType::Bishop || piece.type == PieceType::Queen) {
                                if (std::abs(move.x - esc.x) == std::abs(move.y - esc.y)) threatRange = 8;
                            }
                            if (escDist <= threatRange) ++newCoverage;
                        }
                    }
                    score += newCoverage * 25.0f;

                    // Proximity bonus: getting very close to king is good
                    float moveMd = static_cast<float>(
                        std::abs(move.x - enemyK->position.x) + std::abs(move.y - enemyK->position.y));
                    if (moveMd <= 2.0f) score += 20.0f;
                }

                // Prefer strong pieces + pieces already near enemy
                score += AITacticalEngine::pieceValue(piece.type) * 0.01f;
                score += std::max(0.0f, 50.0f - currentDist) * 0.5f;

                // === LOOP DETECTION PENALTY ===
                // Check global move history — penalize any (piece, dest) that
                // has appeared recently. Scales with repetition count so
                // 2-turn, 3-turn, 4-turn loops all get caught.
                int repeats = countRecentOccurrences(piece.id, move);
                if (repeats > 0) {
                    score -= 200.0f * repeats;
                    std::cerr << "    [Move] LOOP penalty: piece=" << piece.id
                              << " to=(" << move.x << "," << move.y << ") repeats=" << repeats << std::endl;
                }

                // Army rotation
                if (piece.id == s_lastMovedPieceId) {
                    score -= 50.0f;
                }

                bool better = false;
                if (score > bestScore + 0.001f) {
                    better = true;
                } else if (std::abs(score - bestScore) <= 0.001f) {
                    if (newCoverage > bestNewCoverage) {
                        better = true;
                    } else if (newCoverage == bestNewCoverage && moveSectorLoad < bestSectorLoad) {
                        better = true;
                    } else if (newCoverage == bestNewCoverage && moveSectorLoad == bestSectorLoad
                               && slotDistance < bestSlotDistance) {
                        better = true;
                    } else if (newCoverage == bestNewCoverage && moveSectorLoad == bestSectorLoad
                               && slotDistance == bestSlotDistance && progress > bestProgress + 0.001f) {
                        better = true;
                    } else if (newCoverage == bestNewCoverage && moveSectorLoad == bestSectorLoad
                               && slotDistance == bestSlotDistance && std::abs(progress - bestProgress) <= 0.001f
                               && (bestPieceId < 0 || piece.id < bestPieceId)) {
                        better = true;
                    }
                }

                if (better) {
                    bestScore = score;
                    bestPieceId = piece.id;
                    bestMovePos = move;
                    bestNewCoverage = newCoverage;
                    bestSectorLoad = moveSectorLoad;
                    bestSlotDistance = slotDistance;
                    bestProgress = progress;
                }
            }
        }

        // In attack phases accept even small shuffles; in eco phases need real progress
        float minScore = (phase == AIPhase::AGGRESSION || phase == AIPhase::ENDGAME
                          || phase == AIPhase::MID_GAME) ? -5.0f : 0.0f;
        if (bestPieceId >= 0 && bestScore > minScore) {
            Piece* p = self.getPieceById(bestPieceId);
            if (p) {
                std::cerr << "    [Move] Advance: piece " << bestPieceId << " ("
                          << static_cast<int>(p->type) << ") to (" << bestMovePos.x << ","
                          << bestMovePos.y << ") score=" << bestScore << std::endl;
                commands.push_back(makeMoveCmd(bestPieceId, p->position, bestMovePos));
                return commands;
            }
        } else {
            std::cerr << "    [Move] Advance: no candidate (bestId=" << bestPieceId
                      << " score=" << bestScore << ")" << std::endl;
        }
    }

    // =========================================================================
    // 8. RESOURCE GATHERING (when we need more workers)
    // =========================================================================
    if (priorities.economy >= 0.3f && !ctx.freeResourceCells.empty()
        && piecesOnResources < desiredWorkers + 1) {
        std::cerr << "    [Move] Resource gathering" << std::endl;
        float bestDist = std::numeric_limits<float>::max();
        int bestPId = -1;
        sf::Vector2i bestMPos = {0, 0};

        for (const auto& piece : self.pieces) {
            if (piece.type == PieceType::King && nonKingPieces >= 2) continue;
            if (isOnResource(piece, board)) continue;

            auto it = ctx.selfMoves.find(piece.id);
            if (it == ctx.selfMoves.end()) continue;

            for (const auto& move : it->second) {
                if (!engine.isMoveSafe(move, ctx)) continue;
                for (const auto& res : ctx.freeResourceCells) {
                    float dx = static_cast<float>(move.x - res.x);
                    float dy = static_cast<float>(move.y - res.y);
                    float dist = std::sqrt(dx * dx + dy * dy);
                    if (dist < bestDist) {
                        bestDist = dist;
                        bestPId = piece.id;
                        bestMPos = move;
                    }
                }
            }
        }

        if (bestPId >= 0 && bestDist < 30.0f) {
            Piece* p = self.getPieceById(bestPId);
            if (p) {
                std::cerr << "    [Move] Resource: piece " << bestPId << " to ("
                          << bestMPos.x << "," << bestMPos.y << ")" << std::endl;
                commands.push_back(makeMoveCmd(bestPId, p->position, bestMPos));
                return commands;
            }
        }
    }

    // =========================================================================
    // 9. KING POSITIONING FALLBACK
    // =========================================================================
    if (king) {
        bool onResource = isOnResource(*king, board);
        bool safe = !ctx.enemyThreats.isSet(king->position);
        std::cerr << "    [Move] King fallback: onRes=" << onResource << " safe=" << safe << std::endl;

        if (onResource && safe && nonKingPieces >= 1) {
            std::cerr << "    [Move] King staying on resource" << std::endl;
            return commands;
        }

        // Lone king without mating material: prioritize reaching a resource
        if (!brain.hasSufficientMatingMaterial(self) && onResource && safe) {
            std::cerr << "    [Move] King staying on resource (no mating material)" << std::endl;
            return commands; // Stay put and farm
        }

        auto it = ctx.selfMoves.find(king->id);
        if (it != ctx.selfMoves.end() && !it->second.empty()) {
            // Lone king → advance toward enemy ONLY if we have mating material
            if (nonKingPieces == 0 && !enemy.pieces.empty() && brain.hasSufficientMatingMaterial(self)) {
                float currentDist = std::sqrt(static_cast<float>(
                    (king->position.x - targetPos.x) * (king->position.x - targetPos.x) +
                    (king->position.y - targetPos.y) * (king->position.y - targetPos.y)));
                float bestDist = currentDist;
                sf::Vector2i bestDest = king->position;
                for (const auto& move : it->second) {
                    if (ctx.enemyThreats.isSet(move)) continue;
                    float dx = static_cast<float>(move.x - targetPos.x);
                    float dy = static_cast<float>(move.y - targetPos.y);
                    float d = std::sqrt(dx * dx + dy * dy);
                    if (d < bestDist - 0.01f) {
                        bestDist = d;
                        bestDest = move;
                    }
                }
                if (bestDest != king->position) {
                    std::cerr << "    [Move] King advancing toward enemy" << std::endl;
                    commands.push_back(makeMoveCmd(king->id, king->position, bestDest));
                    return commands;
                }
            }

            // Move king toward nearest free resource
            if (!ctx.freeResourceCells.empty() && !onResource) {
                float bestDist = std::numeric_limits<float>::max();
                sf::Vector2i bestDest = king->position;
                float currentBestRes = std::numeric_limits<float>::max();
                for (const auto& res : ctx.freeResourceCells) {
                    float d = std::sqrt(static_cast<float>(
                        (king->position.x - res.x) * (king->position.x - res.x) +
                        (king->position.y - res.y) * (king->position.y - res.y)));
                    if (d < currentBestRes) currentBestRes = d;
                }
                for (const auto& move : it->second) {
                    if (ctx.enemyThreats.isSet(move)) continue;
                    for (const auto& res : ctx.freeResourceCells) {
                        float d = std::sqrt(static_cast<float>(
                            (move.x - res.x) * (move.x - res.x) +
                            (move.y - res.y) * (move.y - res.y)));
                        if (d < bestDist && d < currentBestRes - 0.01f) {
                            bestDist = d;
                            bestDest = move;
                        }
                    }
                }
                if (bestDest != king->position) {
                    commands.push_back(makeMoveCmd(king->id, king->position, bestDest));
                    return commands;
                }
            }

            // Move toward center
            int center = board.getRadius();
            float curCenterDist = std::sqrt(static_cast<float>(
                (king->position.x - center) * (king->position.x - center) +
                (king->position.y - center) * (king->position.y - center)));
            sf::Vector2i bestCenter = king->position;
            float bestCDist = curCenterDist;
            for (const auto& move : it->second) {
                if (ctx.enemyThreats.isSet(move)) continue;
                float d = std::sqrt(static_cast<float>(
                    (move.x - center) * (move.x - center) +
                    (move.y - center) * (move.y - center)));
                if (d < bestCDist - 0.01f) {
                    bestCDist = d;
                    bestCenter = move;
                }
            }
            if (bestCenter != king->position) {
                commands.push_back(makeMoveCmd(king->id, king->position, bestCenter));
                return commands;
            }
        }
    }

    std::cerr << "    [Move] NO MOVE PRODUCED" << std::endl;
    return commands;
}
