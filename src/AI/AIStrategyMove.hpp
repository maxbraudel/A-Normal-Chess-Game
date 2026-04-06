#pragma once
#include <vector>
#include "Systems/TurnCommand.hpp"

class Board;
class Kingdom;
class GameConfig;
class AIConfig;
class AIBrain;
class AITacticalEngine;
struct AITurnContext;
class Building;

class AIStrategyMove {
public:
    static std::vector<TurnCommand> decide(Board& board, Kingdom& self,
                                            Kingdom& enemy, const GameConfig& config,
                                            const AIConfig& aiConfig, const AIBrain& brain,
                                            AITacticalEngine& engine, const AITurnContext& ctx,
                                            const std::vector<Building>& publicBuildings,
                                            bool hasMoved);
};
