#pragma once
#include <vector>
#include "Systems/TurnCommand.hpp"

class Board;
class Kingdom;
class GameConfig;
class AIConfig;

class AIStrategyMove {
public:
    static std::vector<TurnCommand> decide(const Board& board, Kingdom& self,
                                            const Kingdom& enemy, const GameConfig& config,
                                            const AIConfig& aiConfig, bool hasMoved);
};
