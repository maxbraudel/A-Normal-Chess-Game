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
#include <algorithm>

void AITurnContext::build(const Board& board, const Kingdom& self, const Kingdom& enemy,
                          const GameConfig& config) {
    enemyThreats.clear();
    selfThreats.clear();
    selfMoves.clear();
    enemyMoves.clear();
    freeResourceCells.clear();
    mineIncomePerCell = config.getMineIncomePerCellPerTurn();
    farmIncomePerCell = config.getFarmIncomePerCellPerTurn();

    // 1. Compute move lists and threat maps for our pieces
    const Piece* enemyKing = enemy.getKing();
    for (const auto& piece : self.pieces) {
        auto moves = MovementRules::getValidMoves(piece, board, config);
        for (const auto& m : moves) {
            selfThreats.mark(m);
        }
        // Filter out moves onto the enemy king's cell — kings can only be
        // eliminated via checkmate, never captured. Keeping these moves would
        // cause the AI to pick them (high score) then TurnSystem blocks them.
        if (enemyKing) {
            moves.erase(
                std::remove(moves.begin(), moves.end(), enemyKing->position),
                moves.end());
        }
        selfMoves[piece.id] = std::move(moves);
    }

    // 2. Compute move lists and threat maps for enemy pieces
    for (const auto& piece : enemy.pieces) {
        auto moves = MovementRules::getValidMoves(piece, board, config);
        for (const auto& m : moves) {
            enemyThreats.mark(m);
        }
        enemyMoves[piece.id] = std::move(moves);
    }

    // 3. Gather free resource cells (scan board once)
    int diam = board.getDiameter();
    for (int y = 0; y < diam; ++y) {
        for (int x = 0; x < diam; ++x) {
            const Cell& cell = board.getCell(x, y);
            if (cell.isInCircle && cell.building &&
                (cell.building->type == BuildingType::Mine || cell.building->type == BuildingType::Farm)) {
                if (!self.getPieceAt({x, y})) {
                    freeResourceCells.push_back({x, y});
                }
            }
        }
    }
}
