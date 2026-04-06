#pragma once
#include <vector>
#include "Systems/TurnCommand.hpp"

class Board;
class Kingdom;
class GameConfig;
class AIConfig;
class AIBrain;

class AIStrategyBuild {
public:
    static std::vector<TurnCommand> decide(const Board& board, Kingdom& self,
                                            const Kingdom& enemy, const GameConfig& config,
                                            const AIConfig& aiConfig, const AIBrain& brain,
                                            bool hasBuilt);
};
