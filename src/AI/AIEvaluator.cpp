#include "AI/AIEvaluator.hpp"
#include "Board/Board.hpp"
#include "Board/Cell.hpp"
#include "Kingdom/Kingdom.hpp"
#include "Units/Piece.hpp"
#include "Units/PieceType.hpp"
#include "Units/MovementRules.hpp"
#include "Buildings/Building.hpp"
#include "Buildings/BuildingType.hpp"
#include "Config/GameConfig.hpp"
#include <cmath>

static float pieceValue(PieceType type) {
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
        score += pieceValue(p.type);
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
    // Count friendly pieces near king
    for (const auto& p : kingdom.pieces) {
        if (p.type == PieceType::King) continue;
        int dx = std::abs(p.position.x - king->position.x);
        int dy = std::abs(p.position.y - king->position.y);
        if (dx <= 3 && dy <= 3) {
            safety += 1.0f;
        }
    }
    // Check if king is near friendly buildings (walls)
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
        // Closer to center = better
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
    // Check how many of our pieces are under enemy attack
    for (const auto& enemyPiece : enemy.pieces) {
        auto moves = MovementRules::getValidMoves(enemyPiece, board, config);
        for (const auto& m : moves) {
            const Piece* ourPiece = kingdom.getPieceAt(m);
            if (ourPiece) {
                threat += pieceValue(ourPiece->type);
            }
        }
    }
    return threat;
}
