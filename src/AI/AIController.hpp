#pragma once
#include "Config/AIConfig.hpp"
#include "AI/AIBrain.hpp"
#include "AI/AITacticalEngine.hpp"
#include <SFML/System/Vector2.hpp>
#include <string>
#include <vector>

#include "Systems/TurnCommand.hpp"

class Board;
class Kingdom;
class TurnSystem;
class GameConfig;
class EventLog;
class Building;
class BuildingFactory;

struct AITurnPlan {
    std::vector<TurnCommand> commands;
    std::string phaseName;
    sf::Vector2i lastEnemyKingPos{-9999, -9999};
    int enemyKingStaticTurns = 0;
};

class AIController {
public:
    AIController();
    bool loadConfig(const std::string& filepath);

    AITurnPlan computeTurnPlan(Board& board, Kingdom& self, Kingdom& enemy,
                               const std::vector<Building>& publicBuildings,
                               int turnNumber, const GameConfig& config);
    void applyTurnPlanMetadata(const AITurnPlan& plan);

    void playTurn(Board& board, Kingdom& self, Kingdom& enemy,
                  const std::vector<Building>& publicBuildings,
                  TurnSystem& turnSystem, const GameConfig& config, EventLog& log,
                  BuildingFactory& buildingFactory);

    const AIBrain& getBrain() const;

private:
    AIConfig m_config;
    AIBrain m_brain;
    AITacticalEngine m_tacticalEngine;
    sf::Vector2i m_lastEnemyKingPos{-9999, -9999};
    int m_enemyKingStaticTurns = 0;
};
