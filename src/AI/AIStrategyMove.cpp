#include "AI/AIStrategyMove.hpp"
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
#include "Config/AIConfig.hpp"
#include <cmath>
#include <limits>

static float targetValue(PieceType type) {
    switch (type) {
        case PieceType::Queen:  return 9.0f;
        case PieceType::Rook:   return 5.0f;
        case PieceType::Bishop: return 3.0f;
        case PieceType::Knight: return 3.0f;
        case PieceType::Pawn:   return 1.0f;
        case PieceType::King:   return 50.0f;
        default: return 0.0f;
    }
}

std::vector<TurnCommand> AIStrategyMove::decide(const Board& board, Kingdom& self,
                                                  const Kingdom& enemy, const GameConfig& config,
                                                  const AIConfig& aiConfig, bool hasMoved) {
    std::vector<TurnCommand> commands;
    if (hasMoved) return commands;

    Piece* king = self.getKing();

    // 1. If king is in check, mandatory survival move
    if (king && CheckSystem::isInCheck(self.id, board, config)) {
        auto kingMoves = MovementRules::getValidMoves(*king, board, config);
        for (const auto& move : kingMoves) {
            if (CheckSystem::isSafeSquare(move, self.id, board, config)) {
                TurnCommand cmd;
                cmd.type = TurnCommand::Move;
                cmd.pieceId = king->id;
                cmd.destination = move;
                commands.push_back(cmd);
                return commands;
            }
        }
        // If king can't escape, try blocking with another piece
        // (simplified: just move king to any valid square)
        if (!kingMoves.empty()) {
            TurnCommand cmd;
            cmd.type = TurnCommand::Move;
            cmd.pieceId = king->id;
            cmd.destination = kingMoves[0];
            commands.push_back(cmd);
            return commands;
        }
    }

    // 2. Attack high-value targets
    float bestAttackValue = 0.0f;
    int bestAttackPieceId = -1;
    sf::Vector2i bestAttackTarget = {0, 0};

    for (auto& piece : self.pieces) {
        auto moves = MovementRules::getValidMoves(piece, board, config);
        for (const auto& move : moves) {
            const Piece* target = enemy.getPieceAt(move);
            if (target) {
                float value = targetValue(target->type) * aiConfig.attackPriority;
                if (value > bestAttackValue) {
                    bestAttackValue = value;
                    bestAttackPieceId = piece.id;
                    bestAttackTarget = move;
                }
            }
        }
    }

    if (bestAttackValue >= aiConfig.attackPriority * 3.0f &&
        static_cast<int>(self.pieces.size()) >= aiConfig.minPiecesBeforeAttack) {
        TurnCommand cmd;
        cmd.type = TurnCommand::Move;
        cmd.pieceId = bestAttackPieceId;
        cmd.destination = bestAttackTarget;
        commands.push_back(cmd);
        return commands;
    }

    // 3. Defend the king: if enemy can reach king, move a defender
    if (king) {
        auto threatened = CheckSystem::getThreatenedSquares(enemy.id, board, config);
        bool kingThreatened = threatened.count(king->position) > 0;
        if (kingThreatened) {
            // Move king away or interpose a piece
            auto kingMoves = MovementRules::getValidMoves(*king, board, config);
            sf::Vector2i bestSafe = king->position;
            float bestSafeDist = 0.0f;
            for (const auto& move : kingMoves) {
                if (threatened.count(move) == 0) {
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
                TurnCommand cmd;
                cmd.type = TurnCommand::Move;
                cmd.pieceId = king->id;
                cmd.destination = bestSafe;
                commands.push_back(cmd);
                return commands;
            }
        }
    }

    // 4. Take a farm zone: move an idle piece toward a mine/farm
    {
        float bestDist = std::numeric_limits<float>::max();
        int bestPieceId = -1;
        sf::Vector2i bestMove = {0, 0};

        int diam = board.getDiameter();
        std::vector<sf::Vector2i> resourceCells;
        for (int y = 0; y < diam; ++y) {
            for (int x = 0; x < diam; ++x) {
                const Cell& cell = board.getCell(x, y);
                if (cell.isInCircle && cell.building && (cell.building->type == BuildingType::Mine || cell.building->type == BuildingType::Farm)) {
                    if (!self.getPieceAt({x, y})) {
                        resourceCells.push_back({x, y});
                    }
                }
            }
        }

        for (auto& piece : self.pieces) {
            if (piece.type == PieceType::King) continue;
            // Check if already on resource
            const Cell& curCell = board.getCell(piece.position.x, piece.position.y);
            if (curCell.building && (curCell.building->type == BuildingType::Mine || curCell.building->type == BuildingType::Farm)) continue;

            auto moves = MovementRules::getValidMoves(piece, board, config);
            for (const auto& move : moves) {
                for (const auto& res : resourceCells) {
                    float dx = static_cast<float>(move.x - res.x);
                    float dy = static_cast<float>(move.y - res.y);
                    float dist = std::sqrt(dx * dx + dy * dy);
                    if (dist < bestDist) {
                        bestDist = dist;
                        bestPieceId = piece.id;
                        bestMove = move;
                    }
                }
            }
        }

        if (bestPieceId >= 0 && bestDist < 20.0f) {
            TurnCommand cmd;
            cmd.type = TurnCommand::Move;
            cmd.pieceId = bestPieceId;
            cmd.destination = bestMove;
            commands.push_back(cmd);
            return commands;
        }
    }

    // 5. Advance toward enemy: move non-king piece toward enemy king
    if (!enemy.pieces.empty()) {
        const Piece* enemyKing = enemy.getKing();
        sf::Vector2i targetPos = enemyKing ? enemyKing->position : enemy.pieces.front().position;

        float bestDist = std::numeric_limits<float>::max();
        int bestPieceId = -1;
        sf::Vector2i bestMove = {0, 0};

        for (auto& piece : self.pieces) {
            if (piece.type == PieceType::King) continue;
            auto moves = MovementRules::getValidMoves(piece, board, config);
            for (const auto& move : moves) {
                float dx = static_cast<float>(move.x - targetPos.x);
                float dy = static_cast<float>(move.y - targetPos.y);
                float dist = std::sqrt(dx * dx + dy * dy);
                if (dist < bestDist) {
                    bestDist = dist;
                    bestPieceId = piece.id;
                    bestMove = move;
                }
            }
        }

        if (bestPieceId >= 0) {
            TurnCommand cmd;
            cmd.type = TurnCommand::Move;
            cmd.pieceId = bestPieceId;
            cmd.destination = bestMove;
            commands.push_back(cmd);
            return commands;
        }
    }

    return commands;
}
