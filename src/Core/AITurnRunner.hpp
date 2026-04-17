#pragma once

#include <array>
#include <memory>
#include <mutex>
#include <optional>
#include <vector>

#include "AI/AIDirector.hpp"
#include "Kingdom/KingdomId.hpp"

class Board;
class AutonomousUnit;
class Building;
class GameConfig;
class Kingdom;
class MapObject;

class AITurnRunner {
public:
    struct CompletedTurn {
        KingdomId activeKingdom = KingdomId::White;
        int turnNumber = 0;
        AIDirectorPlan plan;
    };

    void cancel();
    bool isRunning() const;
    void start(const Board& board,
               const std::array<Kingdom, kNumKingdoms>& kingdoms,
               const std::vector<Building>& publicBuildings,
               const std::vector<MapObject>& mapObjects,
               const std::vector<AutonomousUnit>& autonomousUnits,
               KingdomId activeKingdom,
               int turnNumber,
               const GameConfig& config,
               const AIDirector& director);
    std::optional<CompletedTurn> poll();

private:
    struct TaskState {
        std::mutex mutex;
        bool ready = false;
        KingdomId activeKingdom = KingdomId::White;
        int turnNumber = 0;
        AIDirectorPlan plan;
    };

    std::shared_ptr<TaskState> m_task;
};