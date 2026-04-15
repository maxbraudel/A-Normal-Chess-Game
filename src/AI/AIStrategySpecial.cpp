#include "AI/AIStrategySpecial.hpp"
#include "AI/AIBrain.hpp"
#include "Board/Board.hpp"
#include "Kingdom/Kingdom.hpp"
#include "Units/Piece.hpp"
#include "Units/PieceType.hpp"
#include "Units/MovementRules.hpp"
#include "Buildings/Building.hpp"
#include "Buildings/BuildingType.hpp"
#include "Systems/MarriageSystem.hpp"
#include "Systems/XPSystem.hpp"
#include "Config/GameConfig.hpp"
#include "Config/AIConfig.hpp"
#include <cmath>
#include <limits>

std::vector<TurnCommand> AIStrategySpecial::decide(const Board& board, Kingdom& self,
                                                     const Kingdom& enemy,
                                                     const std::vector<Building>& publicBuildings,
                                                     const GameConfig& config, const AIConfig& aiConfig,
                                                     const AIBrain& brain, bool hasMarried) {
    std::vector<TurnCommand> commands;

    // =========================================================================
    // 1. Upgrades: upgrade pieces when possible (no phase restriction)
    // =========================================================================
    int knightCount = 0;
    int bishopCount = 0;
    for (const auto& piece : self.pieces) {
        if (piece.type == PieceType::Knight) ++knightCount;
        if (piece.type == PieceType::Bishop) ++bishopCount;
    }

    for (auto& piece : self.pieces) {
        if (piece.type == PieceType::King || piece.type == PieceType::Queen) continue;

        PieceType target = PieceType::Pawn;
        AIPhase phase = brain.getPhase();

        switch (piece.type) {
            case PieceType::Pawn:
                if (phase == AIPhase::AGGRESSION || phase == AIPhase::ENDGAME) {
                    target = PieceType::Knight;
                } else {
                    target = (knightCount <= bishopCount) ? PieceType::Knight : PieceType::Bishop;
                }
                break;
            case PieceType::Knight:
            case PieceType::Bishop:
                target = PieceType::Rook;
                break;
            default:
                continue;
        }

        if (XPSystem::canUpgrade(piece, target, config)) {
            int cost = config.getUpgradeCost(piece.type, target);
            if (self.gold >= cost) {
                TurnCommand cmd;
                cmd.type = TurnCommand::Upgrade;
                cmd.upgradePieceId = piece.id;
                cmd.upgradeTarget = target;
                commands.push_back(cmd);
                self.gold -= cost;
                if (piece.type == PieceType::Pawn) {
                    if (target == PieceType::Knight) ++knightCount;
                    if (target == PieceType::Bishop) ++bishopCount;
                }
            }
        }
    }

    // =========================================================================
    // 2. Marriage: actively pursue when strategic
    // =========================================================================
    if (!hasMarried && !self.hasQueen()) {
        for (const auto& b : publicBuildings) {
            if (b.type != BuildingType::Church || b.isDestroyed()) continue;

            // Check if marriage conditions are already met
            if (MarriageSystem::canMarry(self, board, b)) {
                TurnCommand cmd;
                cmd.type = TurnCommand::Marry;
                commands.push_back(cmd);
                break;
            }
        }
    }

    return commands;
}
