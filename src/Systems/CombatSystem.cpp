#include "Systems/CombatSystem.hpp"
#include "Units/Piece.hpp"
#include "Units/PieceType.hpp"
#include "Board/Board.hpp"
#include "Board/Cell.hpp"
#include "Kingdom/Kingdom.hpp"
#include "Buildings/Building.hpp"
#include "Buildings/BuildingType.hpp"
#include "Config/GameConfig.hpp"
#include "Systems/EventLog.hpp"

CombatSystem::CombatResult CombatSystem::resolve(
    Piece& attacker, Board& board, sf::Vector2i target,
    Kingdom& attackerKingdom, Kingdom& defenderKingdom,
    const GameConfig& config, EventLog& log, int turnNumber) {

    CombatResult result{false, false, 0};
    Cell& targetCell = board.getCell(target.x, target.y);

    // Check if target has an enemy piece
    if (targetCell.piece && targetCell.piece->kingdom != attacker.kingdom) {
        // Kings can never be captured — game ends on checkmate, not capture
        if (targetCell.piece->type == PieceType::King) {
            return result;
        }
        result.occurred = true;
        result.targetWasPiece = true;
        Piece* victim = targetCell.piece;
        int xp = config.getKillXP(victim->type);
        attacker.xp += xp;
        result.xpGained = xp;

        log.log(turnNumber, attacker.kingdom, "Captured enemy piece!");

        // Remove victim
        targetCell.piece = nullptr;
        defenderKingdom.removePiece(victim->id);
        return result;
    }
    return result;
}
