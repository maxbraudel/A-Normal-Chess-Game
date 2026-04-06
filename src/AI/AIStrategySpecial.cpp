#include "AI/AIStrategySpecial.hpp"
#include "Board/Board.hpp"
#include "Kingdom/Kingdom.hpp"
#include "Units/Piece.hpp"
#include "Units/PieceType.hpp"
#include "Buildings/Building.hpp"
#include "Buildings/BuildingType.hpp"
#include "Systems/MarriageSystem.hpp"
#include "Systems/XPSystem.hpp"
#include "Config/GameConfig.hpp"
#include "Config/AIConfig.hpp"

std::vector<TurnCommand> AIStrategySpecial::decide(const Board& board, Kingdom& self,
                                                     const Kingdom& enemy,
                                                     const std::vector<Building>& publicBuildings,
                                                     const GameConfig& config, const AIConfig& aiConfig,
                                                     bool hasMarried) {
    std::vector<TurnCommand> commands;

    // 1. Upgrade pieces if possible
    for (auto& piece : self.pieces) {
        if (piece.type == PieceType::King || piece.type == PieceType::Queen) continue;

        // Determine upgrade path
        PieceType target = PieceType::Pawn;
        switch (piece.type) {
            case PieceType::Pawn:   target = PieceType::Knight; break;
            case PieceType::Knight: target = PieceType::Bishop; break;
            case PieceType::Bishop: target = PieceType::Rook;   break;
            case PieceType::Rook:   target = PieceType::Queen;  break;
            default: continue;
        }

        if (XPSystem::canUpgrade(piece, target, config)) {
            TurnCommand cmd;
            cmd.type = TurnCommand::Upgrade;
            cmd.upgradePieceId = piece.id;
            cmd.upgradeTarget = target;
            commands.push_back(cmd);
        }
    }

    // 2. Marriage if conditions met
    if (!hasMarried) {
        for (const auto& b : publicBuildings) {
            if (b.type == BuildingType::Church && !b.isDestroyed()) {
                if (MarriageSystem::canMarry(self, board, b)) {
                    TurnCommand cmd;
                    cmd.type = TurnCommand::Marry;
                    commands.push_back(cmd);
                    break;
                }
            }
        }
    }

    return commands;
}
