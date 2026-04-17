#include "Systems/TurnPointRules.hpp"

#include "Config/GameConfig.hpp"

TurnPointBudget TurnPointRules::makeBudget(const GameConfig& config,
                                           int movementPointsMaxBonus,
                                           int buildPointsMaxBonus) {
    const int movementPoints = config.getMovementPointsPerTurn() + std::max(0, movementPointsMaxBonus);
    const int buildPoints = config.getBuildPointsPerTurn() + std::max(0, buildPointsMaxBonus);
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