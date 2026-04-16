#include "Systems/XPSystem.hpp"
#include "Units/Piece.hpp"
#include "Kingdom/Kingdom.hpp"
#include "Board/Board.hpp"
#include "Board/Cell.hpp"
#include "Buildings/Building.hpp"
#include "Buildings/BuildingType.hpp"
#include "Config/GameConfig.hpp"

void XPSystem::grantKillXP(Piece& killer, PieceType victim, const GameConfig& config) {
    killer.xp += config.getKillXP(victim);
}

void XPSystem::grantBlockDestroyXP(Piece& destroyer, const GameConfig& config) {
    destroyer.xp += config.getDestroyBlockXP();
}

void XPSystem::grantArenaXP(Kingdom& kingdom, const Board& board,
                              const std::vector<Building>& buildings, const GameConfig& config) {
    for (const auto& b : buildings) {
        if (b.type != BuildingType::Arena) continue;
        if (b.owner != kingdom.id) continue;
        if (!b.isUsable()) continue;

        for (auto& pos : b.getOccupiedCells()) {
            const Cell& cell = board.getCell(pos.x, pos.y);
            if (cell.piece && cell.piece->kingdom == kingdom.id) {
                Piece* p = kingdom.getPieceById(cell.piece->id);
                if (p) p->xp += config.getArenaXPPerTurn();
            }
        }
    }
}

bool XPSystem::canUpgrade(const Piece& piece, PieceType target, const GameConfig& config) {
    return piece.canUpgradeTo(target, config);
}

void XPSystem::upgrade(Piece& piece, PieceType target) {
    piece.type = target;
}
