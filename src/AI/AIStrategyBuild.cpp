#include "AI/AIStrategyBuild.hpp"
#include "Board/Board.hpp"
#include "Board/Cell.hpp"
#include "Kingdom/Kingdom.hpp"
#include "Units/Piece.hpp"
#include "Units/PieceType.hpp"
#include "Buildings/Building.hpp"
#include "Buildings/BuildingType.hpp"
#include "Systems/BuildSystem.hpp"
#include "Config/GameConfig.hpp"
#include "Config/AIConfig.hpp"
#include <cmath>

std::vector<TurnCommand> AIStrategyBuild::decide(const Board& board, Kingdom& self,
                                                   const Kingdom& enemy, const GameConfig& config,
                                                   const AIConfig& aiConfig, bool hasBuilt) {
    std::vector<TurnCommand> commands;
    if (hasBuilt) return commands;

    Piece* king = self.getKing();
    if (!king) return commands;

    // Check if we already have a barracks
    bool hasBarracks = false;
    for (const auto& b : self.buildings) {
        if (b.type == BuildingType::Barracks && !b.isDestroyed()) {
            hasBarracks = true;
            break;
        }
    }

    // Priority: defensive walls if enemy pieces are nearby
    float nearestEnemyDist = 9999.0f;
    for (const auto& ep : enemy.pieces) {
        float dx = static_cast<float>(ep.position.x - king->position.x);
        float dy = static_cast<float>(ep.position.y - king->position.y);
        float dist = std::sqrt(dx * dx + dy * dy);
        if (dist < nearestEnemyDist) nearestEnemyDist = dist;
    }

    BuildingType toBuild = BuildingType::Barracks;
    if (!hasBarracks && self.gold >= config.getBarracksCost()) {
        toBuild = BuildingType::Barracks;
    } else if (nearestEnemyDist < static_cast<float>(aiConfig.wallDefenseRadius) && self.gold >= config.getWoodWallCost()) {
        // Build a wall between king and enemy
        toBuild = BuildingType::WoodWall;
        if (self.gold >= config.getStoneWallCost()) {
            toBuild = BuildingType::StoneWall;
        }
    } else if (hasBarracks && self.gold >= config.getArenaCost()) {
        // Consider an arena for XP farming
        bool hasArena = false;
        for (const auto& b : self.buildings) {
            if (b.type == BuildingType::Arena && !b.isDestroyed()) {
                hasArena = true;
                break;
            }
        }
        if (!hasArena) {
            toBuild = BuildingType::Arena;
        } else {
            return commands; // Nothing to build
        }
    } else {
        return commands;
    }

    // Try to place around king
    for (int dy = -3; dy <= 3; ++dy) {
        for (int dx = -3; dx <= 3; ++dx) {
            if (dx == 0 && dy == 0) continue;
            sf::Vector2i origin = king->position + sf::Vector2i(dx, dy);
            if (BuildSystem::canBuild(toBuild, origin, *king, board, self, config)) {
                TurnCommand cmd;
                cmd.type = TurnCommand::Build;
                cmd.buildingType = toBuild;
                cmd.buildOrigin = origin;
                commands.push_back(cmd);
                return commands;
            }
        }
    }

    return commands;
}
