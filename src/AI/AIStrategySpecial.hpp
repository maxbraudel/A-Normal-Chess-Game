#pragma once
#include <vector>
#include "Systems/TurnCommand.hpp"

class Board;
class Building;
class Kingdom;
class GameConfig;
class AIConfig;
class AIBrain;

class AIStrategySpecial {
public:
    static std::vector<TurnCommand> decide(const Board& board, Kingdom& self,
                                            const Kingdom& enemy,
                                            const std::vector<Building>& publicBuildings,
                                            const GameConfig& config, const AIConfig& aiConfig,
                                            const AIBrain& brain, bool hasMarried);
};
