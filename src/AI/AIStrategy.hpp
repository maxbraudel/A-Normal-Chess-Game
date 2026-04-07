#pragma once
#include <vector>
#include <string>
#include "Units/PieceType.hpp"
#include "Buildings/BuildingType.hpp"

class AIConfig;

// Forward
struct AITurnContext;
struct GameSnapshot;
enum class KingdomId;

/// Strategic objectives the AI can pursue each turn
enum class StrategicObjective {
    RUSH_ATTACK,           // Attack immediately with current forces
    ECONOMY_EXPAND,        // Occupy mines/farms, accumulate gold
    BUILD_ARMY,            // Produce units in barracks
    BUILD_INFRASTRUCTURE,  // Construct barracks/walls
    PURSUE_QUEEN,          // Marriage at church
    DEFEND_KING,           // Protect our king
    CHECKMATE_PRESS,       // Encircle and mate the enemy king
    CONTEST_RESOURCES,     // Dispute a mine/farm with enemy
    RETREAT_REGROUP,       // Regroup scattered troops
};

/// Composite turn plan: primary movement objective + secondary actions
struct TurnPlan {
    StrategicObjective primaryObjective = StrategicObjective::ECONOMY_EXPAND;
    bool shouldProduce   = true;
    PieceType preferredProduction = PieceType::Pawn;
    bool shouldBuild     = false;
    BuildingType preferredBuilding = BuildingType::Barracks;
    bool shouldMarry     = false;
};

/// Utility AI — replaces AIBrain's rigid phase system
class AIStrategy {
public:
    AIStrategy() = default;

    /// Evaluate all objectives and build a composite TurnPlan.
    /// ctx provides pre-computed snapshot, threat maps, move lists, etc.
    TurnPlan computePlan(const AITurnContext& ctx,
                         const GameSnapshot& snapshot,
                         KingdomId aiKingdom,
                         int turnNumber,
                         const AIConfig& aiConfig);

    StrategicObjective getLastObjective() const { return m_lastObjective; }
    static std::string objectiveName(StrategicObjective obj);

private:
    StrategicObjective m_lastObjective = StrategicObjective::ECONOMY_EXPAND;

    // Individual objective scoring functions (all return 0..100)
    float scoreRushAttack(const AITurnContext& ctx, const GameSnapshot& s,
                          KingdomId k) const;
    float scoreEconomyExpand(const AITurnContext& ctx, const GameSnapshot& s,
                             KingdomId k) const;
    float scoreBuildArmy(const AITurnContext& ctx, const GameSnapshot& s,
                         KingdomId k) const;
    float scoreBuildInfra(const AITurnContext& ctx, const GameSnapshot& s,
                          KingdomId k) const;
    float scorePursueQueen(const AITurnContext& ctx, const GameSnapshot& s,
                           KingdomId k, int turnNumber) const;
    float scoreDefendKing(const AITurnContext& ctx, const GameSnapshot& s,
                          KingdomId k, int globalMaxRange) const;
    float scoreCheckmatePress(const AITurnContext& ctx, const GameSnapshot& s,
                              KingdomId k, int globalMaxRange) const;
    float scoreContestResources(const AITurnContext& ctx, const GameSnapshot& s,
                                KingdomId k) const;
    float scoreRetreatRegroup(const AITurnContext& ctx, const GameSnapshot& s,
                              KingdomId k, int globalMaxRange) const;

    // Helpers
    static int countFreeBarracks(const GameSnapshot& s, KingdomId k);
    static bool hasMatingMaterial(const GameSnapshot& s, KingdomId k);
    static int countPiecesNearEnemyKing(const GameSnapshot& s, KingdomId k, int radius);
    static int countEnemyKingEscapeSquares(const GameSnapshot& s, KingdomId k,
                                            int globalMaxRange);
    static int countEnemyPiecesNearMyKing(const GameSnapshot& s, KingdomId k, int radius);
    static int countSelfKingEscapeSquares(const GameSnapshot& s, KingdomId k,
                                           int globalMaxRange);
    static float avgDistanceToChurch(const GameSnapshot& s, KingdomId k);
};
