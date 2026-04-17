#pragma once

#include "Buildings/BuildingType.hpp"
#include "Units/PieceType.hpp"

class GameConfig;

struct TurnPointBudget {
    int movementPointsMax = 0;
    int movementPointsRemaining = 0;
    int buildPointsMax = 0;
    int buildPointsRemaining = 0;
};

class TurnPointRules {
public:
    static TurnPointBudget makeBudget(const GameConfig& config,
                                      int movementPointsMaxBonus = 0,
                                      int buildPointsMaxBonus = 0);
    static int movementCost(PieceType type, const GameConfig& config);
    static int buildCost(BuildingType type, const GameConfig& config);
    static int moveAllowance(PieceType type, const GameConfig& config);
};