#include "AI/AIBuildModule.hpp"
#include "AI/GameSnapshot.hpp"
#include "Config/GameConfig.hpp"
#include "Systems/BuildReachRules.hpp"
#include <cmath>
#include <algorithm>

// =========================================================================
//  canBuildBarracksAt — config-sized barracks, must be adjacent to king
// =========================================================================

bool AIBuildModule::canBuildBarracksAt(const GameSnapshot& s, sf::Vector2i pos,
                                       const std::vector<sf::Vector2i>& builderPositions,
                                       const GameConfig& config) {
    const int width = config.getBuildingWidth(BuildingType::Barracks);
    const int height = config.getBuildingHeight(BuildingType::Barracks);

    if (!footprintHasAdjacentBuilder(pos, width, height, builderPositions)) return false;

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
    const auto builderPositions = collectBuilderPositions(s.kingdom(k).pieces);
    if (builderPositions.empty()) return std::nullopt;

    auto* king = s.kingdom(k).getKing();
    const sf::Vector2i anchor = king ? king->position : builderPositions.front();

    // Try positions in expanding rings around idealPos
    for (int r = 0; r <= 5; ++r) {
        for (int dy = -r; dy <= r; ++dy) {
            for (int dx = -r; dx <= r; ++dx) {
                if (std::abs(dx) != r && std::abs(dy) != r) continue; // ring only
                sf::Vector2i pos = idealPos + sf::Vector2i{dx, dy};
                if (canBuildBarracksAt(s, pos, builderPositions, config))
                    return BuildAction{BuildingType::Barracks, pos};
            }
        }
    }

    // Fallback: anywhere near our builders, keeping king-first behavior when available.
    std::vector<sf::Vector2i> searchAnchors{anchor};
    for (const sf::Vector2i& builderPos : builderPositions) {
        if (builderPos != anchor) {
            searchAnchors.push_back(builderPos);
        }
    }

    for (const sf::Vector2i& searchAnchor : searchAnchors) {
        for (int r = 0; r <= 3; ++r) {
            for (int dy = -r; dy <= r; ++dy) {
                for (int dx = -r; dx <= r; ++dx) {
                    if (std::abs(dx) != r && std::abs(dy) != r) continue;
                    sf::Vector2i pos = searchAnchor + sf::Vector2i{dx, dy};
                    if (canBuildBarracksAt(s, pos, builderPositions, config))
                        return BuildAction{BuildingType::Barracks, pos};
                }
            }
        }
    }

    return std::nullopt;
}

// =========================================================================
//  buildWall — place defensive wall near our king
// =========================================================================

std::optional<BuildAction> AIBuildModule::buildWall(const GameSnapshot& s, KingdomId k) {
    const auto builderPositions = collectBuilderPositions(s.kingdom(k).pieces);
    if (builderPositions.empty()) return std::nullopt;

    // Try 1x1 wood wall in the 8 cells around any builder.
    for (const sf::Vector2i& builderPos : builderPositions) {
        for (int dy = -1; dy <= 1; ++dy) {
            for (int dx = -1; dx <= 1; ++dx) {
                if (dx == 0 && dy == 0) continue;
                sf::Vector2i pos = builderPos + sf::Vector2i{dx, dy};
                if (!s.isTraversable(pos.x, pos.y)) continue;
                if (s.pieceAt(pos)) continue;
                if (s.buildingAt(pos)) continue;
                return BuildAction{BuildingType::WoodWall, pos};
            }
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

    const auto builderPositions = collectBuilderPositions(s.kingdom(k).pieces);
    if (builderPositions.empty()) return std::nullopt;

    auto* king = s.kingdom(k).getKing();
    const sf::Vector2i anchor = king ? king->position : builderPositions.front();

    // PRIORITY 1: First barracks
    if (barracksCount == 0 && gold >= 50)
        return buildBarracks(s, k, anchor, config);

    // PRIORITY 2: Second barracks (if income sufficient)
    if (barracksCount == 1 && incomePerTurn >= 15 && gold >= 50 && turnNumber > 10)
        return buildBarracks(s, k, anchor, config);

    // PRIORITY 3: Third barracks (if cash-flow very good)
    if (barracksCount == 2 && incomePerTurn >= 30 && gold >= 80)
        return buildBarracks(s, k, anchor, config);

    // PRIORITY 4: Defensive walls if threatened
    if (objective == StrategicObjective::DEFEND_KING && gold >= 20)
        return buildWall(s, k);

    // Build infra objective: try more barracks
    if (objective == StrategicObjective::BUILD_INFRASTRUCTURE && gold >= 50)
        return buildBarracks(s, k, anchor, config);

    return std::nullopt;
}
