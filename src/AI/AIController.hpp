#pragma once
#include "Config/AIConfig.hpp"

class Board;
class Kingdom;
class TurnSystem;
class GameConfig;
class EventLog;
class Building;
#include <vector>

class AIController {
public:
    AIController();
    bool loadConfig(const std::string& filepath);

    void playTurn(Board& board, Kingdom& self, Kingdom& enemy,
                  const std::vector<Building>& publicBuildings,
                  TurnSystem& turnSystem, const GameConfig& config, EventLog& log);

private:
    AIConfig m_config;
};
