#include "AI/AIStrategyBuild.hpp"
#include "AI/AIBrain.hpp"
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

// Helper: find the direction toward the nearest enemy piece
static sf::Vector2i enemyDirection(const Kingdom& self, const Kingdom& enemy) {
    const Piece* king = self.getKing();
    if (!king || enemy.pieces.empty()) return {0, 0};

    float bestDist = 9999.0f;
    sf::Vector2i bestDir = {0, 0};
    for (const auto& ep : enemy.pieces) {
        float dx = static_cast<float>(ep.position.x - king->position.x);
        float dy = static_cast<float>(ep.position.y - king->position.y);
        float dist = std::sqrt(dx * dx + dy * dy);
        if (dist < bestDist) {
            bestDist = dist;
            bestDir = {dx > 0 ? 1 : (dx < 0 ? -1 : 0),
                       dy > 0 ? 1 : (dy < 0 ? -1 : 0)};
        }
    }
    return bestDir;
}

std::vector<TurnCommand> AIStrategyBuild::decide(const Board& board, Kingdom& self,
                                                   const Kingdom& enemy, const GameConfig& config,
                                                   const AIConfig& aiConfig, const AIBrain& brain,
                                                   bool hasBuilt) {
    std::vector<TurnCommand> commands;
    if (hasBuilt) return commands;

    const auto& priorities = brain.getPriorities();
    AIPhase phase = brain.getPhase();
    Piece* king = self.getKing();
    if (!king) return commands;

    // Don't build during crisis or if building priority is low
    if (phase == AIPhase::CRISIS || priorities.building < 0.2f) return commands;

    // Count existing buildings
    bool hasBarracks = false;
    bool hasArena = false;
    int wallCount = 0;
    for (const auto& b : self.buildings) {
        if (b.isDestroyed()) continue;
        if (b.type == BuildingType::Barracks) hasBarracks = true;
        if (b.type == BuildingType::Arena) hasArena = true;
        if (b.type == BuildingType::WoodWall || b.type == BuildingType::StoneWall) ++wallCount;
    }

    // Determine nearest enemy distance
    float nearestEnemyDist = 9999.0f;
    for (const auto& ep : enemy.pieces) {
        float dx = static_cast<float>(ep.position.x - king->position.x);
        float dy = static_cast<float>(ep.position.y - king->position.y);
        float dist = std::sqrt(dx * dx + dy * dy);
        if (dist < nearestEnemyDist) nearestEnemyDist = dist;
    }

    // Decide what to build
    BuildingType toBuild = BuildingType::Barracks;

    if (!hasBarracks && self.gold >= config.getBarracksCost()) {
        // Priority #1: always need a barracks
        toBuild = BuildingType::Barracks;
    } else if (nearestEnemyDist < static_cast<float>(aiConfig.wallDefenseRadius) &&
               priorities.defense >= 0.5f && wallCount < 4) {
        // Priority #2: defensive walls when enemy is close
        if (self.gold >= config.getStoneWallCost()) {
            toBuild = BuildingType::StoneWall;
        } else if (self.gold >= config.getWoodWallCost()) {
            toBuild = BuildingType::WoodWall;
        } else {
            return commands;
        }
    } else if (hasBarracks && !hasArena && self.gold >= config.getArenaCost() &&
               (phase == AIPhase::BUILD_UP || phase == AIPhase::MID_GAME)) {
        // Priority #3: arena for XP farming during build-up
        toBuild = BuildingType::Arena;
    } else {
        return commands; // Nothing worth building right now
    }

    // Place building: for walls, prefer placing between king and enemy
    sf::Vector2i dir = enemyDirection(self, enemy);

    // Try placing in the enemy direction first, then expand outward
    auto tryPlace = [&](int dx, int dy) -> bool {
        sf::Vector2i origin = king->position + sf::Vector2i(dx, dy);
        if (BuildSystem::canBuild(toBuild, origin, *king, board, self, config)) {
            TurnCommand cmd;
            cmd.type = TurnCommand::Build;
            cmd.buildingType = toBuild;
            cmd.buildOrigin = origin;
            commands.push_back(cmd);
            return true;
        }
        return false;
    };

    // Try enemy-facing direction first
    if (dir.x != 0 || dir.y != 0) {
        for (int dist = 1; dist <= 3; ++dist) {
            if (tryPlace(dir.x * dist, dir.y * dist)) return commands;
        }
    }

    // Then try all directions
    for (int radius = 1; radius <= 4; ++radius) {
        for (int dy = -radius; dy <= radius; ++dy) {
            for (int dx = -radius; dx <= radius; ++dx) {
                if (std::abs(dx) != radius && std::abs(dy) != radius) continue;
                if (tryPlace(dx, dy)) return commands;
            }
        }
    }

    return commands;
}
