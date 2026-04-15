#include "Systems/TurnPointRules.hpp"

#include "Config/GameConfig.hpp"

TurnPointBudget TurnPointRules::makeBudget(const GameConfig& config) {
    const int movementPoints = config.getMovementPointsPerTurn();
    const int buildPoints = config.getBuildPointsPerTurn();
    return {movementPoints, movementPoints, buildPoints, buildPoints};
}

int TurnPointRules::movementCost(PieceType type, const GameConfig& config) {
    return config.getMovePointCost(type);
}

int TurnPointRules::buildCost(BuildingType type, const GameConfig& config) {
    return config.getBuildPointCost(type);
}

int TurnPointRules::moveAllowance(PieceType type, const GameConfig& config) {
    return config.getMoveAllowancePerTurn(type);
}