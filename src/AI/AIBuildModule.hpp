#pragma once
#include <optional>
#include <SFML/System/Vector2.hpp>
#include "Buildings/BuildingType.hpp"
#include "AI/AIStrategy.hpp"
#include "Kingdom/KingdomId.hpp"

struct GameSnapshot;
struct AITurnContext;
class GameConfig;

/// A build action: type + origin position
struct BuildAction {
    BuildingType type = BuildingType::Barracks;
    sf::Vector2i position{0, 0};
};

/// Intelligent building placement module
class AIBuildModule {
public:
    AIBuildModule() = default;

    /// Suggest a building to construct this turn (if any).
    std::optional<BuildAction> suggestBuild(
        const GameSnapshot& s,
        KingdomId k,
        StrategicObjective objective,
        int turnNumber,
        int incomePerTurn,
        const GameConfig& config) const;

private:
    /// Try to find a valid barracks placement near a target position.
    static std::optional<BuildAction> buildBarracks(
        const GameSnapshot& s, KingdomId k, sf::Vector2i idealPos,
        const GameConfig& config);

    /// Try to find a valid wall placement near our king.
    static std::optional<BuildAction> buildWall(
        const GameSnapshot& s, KingdomId k);

    /// Check if a position is valid for a 2×2 barracks
    static bool canBuildBarracksAt(const GameSnapshot& s, sf::Vector2i pos,
                                    sf::Vector2i kingPos,
                                    const GameConfig& config);
};
