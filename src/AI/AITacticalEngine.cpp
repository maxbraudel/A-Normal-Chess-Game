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
#include "Systems/CheckSystem.hpp"
#include "Config/GameConfig.hpp"
#include <algorithm>
#include <cmath>
#include <limits>

AITacticalEngine::AITacticalEngine() {}

float AITacticalEngine::pieceValue(PieceType type) {
    switch (type) {
        case PieceType::Pawn:   return 100.0f;
        case PieceType::Knight: return 320.0f;
        case PieceType::Bishop: return 330.0f;
        case PieceType::Rook:   return 500.0f;
        case PieceType::Queen:  return 900.0f;
        case PieceType::King:   return 20000.0f;
    }
    return 0.0f;
}

// === Core heuristic: score a single move candidate ===
float AITacticalEngine::scoreMove(const Piece& piece, sf::Vector2i dest,
                                   const Board& board, const Kingdom& self, const Kingdom& enemy,
                                   const GameConfig& config, const AITurnContext& ctx) const {
    float score = 0.0f;

    const Cell& destCell = board.getCell(dest.x, dest.y);

    // 1. CAPTURE VALUE — strongest signal (never score king captures — use checkmate)
    if (destCell.piece && destCell.piece->kingdom != self.id
        && destCell.piece->type != PieceType::King) {
        float captureVal = pieceValue(destCell.piece->type);
        float attackerVal = pieceValue(piece.type);
        // MVV-LVA: prefer capturing expensive pieces with cheap ones
        score += captureVal * 10.0f - attackerVal * 0.5f;
    }

    // 2. SAFETY — is destination under enemy attack? (O(1) lookup)
    bool destSafe = !ctx.enemyThreats.isSet(dest);
    if (!destSafe) {
        // Moving to an attacked square — penalize by piece value
        score -= pieceValue(piece.type) * 3.0f;
        // But if it's a profitable capture, it might still be worth it
        if (destCell.piece && destCell.piece->kingdom != self.id) {
            float captureVal = pieceValue(destCell.piece->type);
            if (captureVal >= pieceValue(piece.type)) {
                score += captureVal * 2.0f; // Winning trade is OK
            }
        }
    }

    // 3. RESOURCE CONTROL — net resource change (gain minus loss)
    if (destCell.building) {
        if (destCell.building->type == BuildingType::Mine) score += 150.0f;
        if (destCell.building->type == BuildingType::Farm) score += 75.0f;
    }
    const Cell& srcCell = board.getCell(piece.position.x, piece.position.y);
    if (srcCell.building) {
        if (srcCell.building->type == BuildingType::Mine) score -= 150.0f;
        if (srcCell.building->type == BuildingType::Farm) score -= 75.0f;
    }

    // 4. ENEMY KING PROXIMITY — closer to enemy king is better for non-king pieces
    const Piece* enemyKing = enemy.getKing();
    if (enemyKing && piece.type != PieceType::King) {
        float dx = static_cast<float>(dest.x - enemyKing->position.x);
        float dy = static_cast<float>(dest.y - enemyKing->position.y);
        float distToEnemyKing = std::abs(dx) + std::abs(dy); // Manhattan distance
        score += std::max(0.0f, 30.0f - distToEnemyKing * 2.0f);
    }

    // 5. KING SAFETY — for king moves, prefer safe squares away from threats
    if (piece.type == PieceType::King) {
        if (destSafe) score += 500.0f; // King safety is paramount
        // Count enemy threats around destination
        int adjacentThreats = 0;
        for (int dy = -1; dy <= 1; ++dy) {
            for (int dx = -1; dx <= 1; ++dx) {
                if (ctx.enemyThreats.isSet(dest.x + dx, dest.y + dy))
                    ++adjacentThreats;
            }
        }
        score -= adjacentThreats * 50.0f;
    }

    // 6. CENTER CONTROL — bonus for moving toward center
    int center = board.getRadius();
    float dx = static_cast<float>(dest.x - center);
    float dy = static_cast<float>(dest.y - center);
    float distToCenter = std::sqrt(dx * dx + dy * dy);
    score += std::max(0.0f, 10.0f - distToCenter * 0.5f);

    // 7. LEAVING ATTACKED SQUARE — bonus for escaping danger
    if (ctx.enemyThreats.isSet(piece.position) && destSafe) {
        score += pieceValue(piece.type) * 1.0f;
    }

    return score;
}

// === Find best move using heuristic scoring (no tree search) ===
ScoredMove AITacticalEngine::findBestMove(const Board& board, const Kingdom& self, const Kingdom& enemy,
                                           const GameConfig& config, const AITurnContext& ctx) {
    ScoredMove bestMove;
    bestMove.score = -std::numeric_limits<float>::max();

    for (const auto& piece : self.pieces) {
        // Use cached move list
        auto it = ctx.selfMoves.find(piece.id);
        if (it == ctx.selfMoves.end()) continue;

        for (const auto& dest : it->second) {
            const Cell& dc = board.getCell(dest.x, dest.y);

            // Never target the enemy king — kings can only be eliminated
            // via checkmate, not capture. CombatSystem blocks king captures
            // so generating them just wastes the move action.
            if (dc.piece && dc.piece->kingdom != self.id && dc.piece->type == PieceType::King)
                continue;

            // Skip non-capture king moves — king positioning is handled
            // by dedicated defense/economy logic, not tactical scoring.
            if (piece.type == PieceType::King) {
                if (!dc.piece || dc.piece->kingdom == self.id) continue;
            }

            float score = scoreMove(piece, dest, board, self, enemy, config, ctx);
            if (score > bestMove.score) {
                bestMove.pieceId = piece.id;
                bestMove.from = piece.position;
                bestMove.to = dest;
                bestMove.score = score;
            }
        }
    }

    return bestMove;
}

// === O(1) safety check using cached threat map ===
bool AITacticalEngine::isMoveSafe(sf::Vector2i destination, const AITurnContext& ctx) const {
    return !ctx.enemyThreats.isSet(destination);
}

// === Checkmate-in-1: try each move, see if it delivers checkmate ===
ScoredMove AITacticalEngine::findCheckmateIn1(Board& board, Kingdom& self, Kingdom& enemy,
                                               const GameConfig& config, const AITurnContext& ctx) {
    for (const auto& piece : self.pieces) {
        auto it = ctx.selfMoves.find(piece.id);
        if (it == ctx.selfMoves.end()) continue;

        for (const auto& dest : it->second) {
            // Quick pre-filter: only check moves within piece range of enemy king
            // Long-range pieces (rook/queen/bishop) can deliver mate from up to 8 cells away
            const Piece* ek = enemy.getKing();
            if (!ek) continue;
            int dkx = std::abs(dest.x - ek->position.x);
            int dky = std::abs(dest.y - ek->position.y);
            const Cell& destCell = board.getCell(dest.x, dest.y);
            bool isCapture = destCell.piece && destCell.piece->kingdom != self.id;
            int maxCheckDist = config.getGlobalMaxRange();
            if (dkx > maxCheckDist && dky > maxCheckDist && !isCapture) continue;

            // Simulate move
            sf::Vector2i oldPos = piece.position;
            Cell& oldCell = board.getCell(oldPos.x, oldPos.y);
            Cell& newCell = board.getCell(dest.x, dest.y);
            Piece* captured = newCell.piece;

            // Need non-const piece pointer for simulation
            Piece* mutablePiece = self.getPieceById(piece.id);
            if (!mutablePiece) continue;

            oldCell.piece = nullptr;
            mutablePiece->position = dest;
            newCell.piece = mutablePiece;

            bool isCheckmate = CheckSystem::isInCheckFast(enemy, self, board, config);
            if (isCheckmate) {
                // Verify it's actually checkmate (enemy has no escape)
                // Quick check: can any enemy piece move to block/escape?
                bool canEscape = false;
                for (const auto& ep : enemy.pieces) {
                    auto eMoves = MovementRules::getValidMoves(ep, board, config);
                    for (const auto& em : eMoves) {
                        // Simulate enemy escape
                        Piece* mutableEp = enemy.getPieceById(ep.id);
                        if (!mutableEp) continue;
                        sf::Vector2i epOld = mutableEp->position;
                        Cell& epOldCell = board.getCell(epOld.x, epOld.y);
                        Cell& epNewCell = board.getCell(em.x, em.y);
                        Piece* epCaptured = epNewCell.piece;

                        epOldCell.piece = nullptr;
                        mutableEp->position = em;
                        epNewCell.piece = mutableEp;

                        bool stillCheck = CheckSystem::isInCheckFast(enemy, self, board, config);

                        // Revert
                        mutableEp->position = epOld;
                        epNewCell.piece = epCaptured;
                        epOldCell.piece = mutableEp;

                        if (!stillCheck) { canEscape = true; break; }
                    }
                    if (canEscape) break;
                }

                if (!canEscape) {
                    // Revert our move
                    mutablePiece->position = oldPos;
                    newCell.piece = captured;
                    oldCell.piece = mutablePiece;

                    ScoredMove mate;
                    mate.pieceId = piece.id;
                    mate.from = oldPos;
                    mate.to = dest;
                    mate.score = 99999.0f;
                    return mate;
                }
            }

            // Revert our move
            mutablePiece->position = oldPos;
            newCell.piece = captured;
            oldCell.piece = mutablePiece;
        }
    }

    return {}; // No checkmate found
}
