#pragma once
#include <vector>
#include <utility>
#include <SFML/System/Vector2.hpp>
#include "Units/PieceType.hpp"
#include "AI/AIStrategy.hpp"
#include "Kingdom/KingdomId.hpp"

struct GameSnapshot;
struct AITurnContext;

/// Assignment: pieceId → target resource cell
struct ResourcePlan {
    std::vector<std::pair<int, sf::Vector2i>> assignments;
    int expectedIncome = 0;
};

/// Production order: barracksId → piece type to produce
struct ProductionPlan {
    std::vector<std::pair<int, PieceType>> orders;
};

/// Economy and production planning module
class AIEconomyModule {
public:
    AIEconomyModule() = default;

    /// Greedy assignment of idle pieces to free resource cells.
    ResourcePlan planResourceGathering(const GameSnapshot& s,
                                       const AITurnContext& ctx,
                                       KingdomId k) const;

    /// Decide what to produce in each free barracks.
    ProductionPlan planProduction(const GameSnapshot& s,
                                  KingdomId k,
                                  StrategicObjective objective,
                                  int turnNumber) const;

    /// Choose the best unit type given current composition vs desired.
    static PieceType chooseBestUnit(const GameSnapshot& s, KingdomId k,
                                     StrategicObjective objective, int budget);

private:
    static int computeGoldReserve(int turnNumber, int barracksCount,
                                   StrategicObjective objective);
    static int recruitCost(PieceType type);
};
