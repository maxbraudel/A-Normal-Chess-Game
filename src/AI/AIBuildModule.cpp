#include "AI/AIBuildModule.hpp"
#include "AI/GameSnapshot.hpp"
#include "Config/GameConfig.hpp"
#include <cmath>
#include <algorithm>

// =========================================================================
//  canBuildBarracksAt — config-sized barracks, must be adjacent to king
// =========================================================================

bool AIBuildModule::canBuildBarracksAt(const GameSnapshot& s, sf::Vector2i pos,
                                         sf::Vector2i kingPos,
                                         const GameConfig& config) {
    const int width = config.getBuildingWidth(BuildingType::Barracks);
    const int height = config.getBuildingHeight(BuildingType::Barracks);

    // Check king adjacency: at least one cell of the barracks must be adjacent.
    bool nearKing = false;
    for (int dy = 0; dy < height; ++dy) {
        for (int dx = 0; dx < width; ++dx) {
            int cx = pos.x + dx, cy = pos.y + dy;
            int kdx = std::abs(cx - kingPos.x);
            int kdy = std::abs(cy - kingPos.y);
            if (kdx <= 1 && kdy <= 1 && (kdx + kdy > 0)) { nearKing = true; break; }
        }
        if (nearKing) break;
    }
    if (!nearKing) return false;

    // All cells must be traversable, not occupied, no existing building.
    for (int dy = 0; dy < height; ++dy) {
        for (int dx = 0; dx < width; ++dx) {
            int cx = pos.x + dx, cy = pos.y + dy;
            if (!s.isTraversable(cx, cy)) return false;
            if (s.pieceAt({cx, cy})) return false;
            if (s.buildingAt({cx, cy})) return false;
        }
    }
    return true;
}

// =========================================================================
//  buildBarracks — spiral search near ideal position
// =========================================================================

std::optional<BuildAction> AIBuildModule::buildBarracks(
    const GameSnapshot& s, KingdomId k, sf::Vector2i idealPos,
    const GameConfig& config)
{
    auto* king = s.kingdom(k).getKing();
    if (!king) return std::nullopt;
    sf::Vector2i kingPos = king->position;

    // Try positions in expanding rings around idealPos
    for (int r = 0; r <= 5; ++r) {
        for (int dy = -r; dy <= r; ++dy) {
            for (int dx = -r; dx <= r; ++dx) {
                if (std::abs(dx) != r && std::abs(dy) != r) continue; // ring only
                sf::Vector2i pos = idealPos + sf::Vector2i{dx, dy};
                if (canBuildBarracksAt(s, pos, kingPos, config))
                    return BuildAction{BuildingType::Barracks, pos};
            }
        }
    }

    // Fallback: anywhere near the king
    for (int r = 0; r <= 3; ++r) {
        for (int dy = -r; dy <= r; ++dy) {
            for (int dx = -r; dx <= r; ++dx) {
                if (std::abs(dx) != r && std::abs(dy) != r) continue;
                sf::Vector2i pos = kingPos + sf::Vector2i{dx, dy};
                if (canBuildBarracksAt(s, pos, kingPos, config))
                    return BuildAction{BuildingType::Barracks, pos};
            }
        }
    }

    return std::nullopt;
}

// =========================================================================
//  buildWall — place defensive wall near our king
// =========================================================================

std::optional<BuildAction> AIBuildModule::buildWall(const GameSnapshot& s, KingdomId k) {
    auto* king = s.kingdom(k).getKing();
    if (!king) return std::nullopt;

    // Try 1×1 wood wall in the 8 cells around king
    for (int dy = -1; dy <= 1; ++dy) {
        for (int dx = -1; dx <= 1; ++dx) {
            if (dx == 0 && dy == 0) continue;
            sf::Vector2i pos = king->position + sf::Vector2i{dx, dy};
            if (!s.isTraversable(pos.x, pos.y)) continue;
            if (s.pieceAt(pos)) continue;
            if (s.buildingAt(pos)) continue;
            return BuildAction{BuildingType::WoodWall, pos};
        }
    }
    return std::nullopt;
}

// =========================================================================
//  suggestBuild — main entry
// =========================================================================

std::optional<BuildAction> AIBuildModule::suggestBuild(
    const GameSnapshot& s,
    KingdomId k,
    StrategicObjective objective,
    int turnNumber,
    int incomePerTurn,
    const GameConfig& config) const
{
    int gold = s.kingdom(k).gold;
    int barracksCount = 0;
    for (auto& b : s.kingdom(k).buildings)
        if (b.type == BuildingType::Barracks && !b.isDestroyed()) ++barracksCount;

    auto* king = s.kingdom(k).getKing();
    if (!king) return std::nullopt;

    // PRIORITY 1: First barracks
    if (barracksCount == 0 && gold >= 50)
        return buildBarracks(s, k, king->position, config);

    // PRIORITY 2: Second barracks (if income sufficient)
    if (barracksCount == 1 && incomePerTurn >= 15 && gold >= 50 && turnNumber > 10)
        return buildBarracks(s, k, king->position, config);

    // PRIORITY 3: Third barracks (if cash-flow very good)
    if (barracksCount == 2 && incomePerTurn >= 30 && gold >= 80)
        return buildBarracks(s, k, king->position, config);

    // PRIORITY 4: Defensive walls if threatened
    if (objective == StrategicObjective::DEFEND_KING && gold >= 20)
        return buildWall(s, k);

    // Build infra objective: try more barracks
    if (objective == StrategicObjective::BUILD_INFRASTRUCTURE && gold >= 50)
        return buildBarracks(s, k, king->position, config);

    return std::nullopt;
}
