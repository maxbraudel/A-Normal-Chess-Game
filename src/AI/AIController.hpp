#pragma once
#include "Config/AIConfig.hpp"
#include "AI/AIBrain.hpp"
#include "AI/AITacticalEngine.hpp"

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

    const AIBrain& getBrain() const;

private:
    AIConfig m_config;
    AIBrain m_brain;
    AITacticalEngine m_tacticalEngine;
};
