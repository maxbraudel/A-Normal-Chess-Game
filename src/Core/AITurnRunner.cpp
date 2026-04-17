#include "Core/AITurnRunner.hpp"

#include <thread>

#include "Autonomous/AutonomousUnit.hpp"
#include "Board/Board.hpp"
#include "Buildings/Building.hpp"
#include "Config/GameConfig.hpp"
#include "Core/GameEngine.hpp"
#include "Kingdom/Kingdom.hpp"
#include "Objects/MapObject.hpp"

namespace {

struct AIWorldSnapshot {
    Board board;
    std::array<Kingdom, kNumKingdoms> kingdoms;
    std::vector<Building> publicBuildings;
    std::vector<MapObject> mapObjects;
    std::vector<AutonomousUnit> autonomousUnits;
};

AIWorldSnapshot makeAIWorldSnapshot(const Board& board,
                                    const std::array<Kingdom, kNumKingdoms>& kingdoms,
                                    const std::vector<Building>& publicBuildings,
                                    const std::vector<MapObject>& mapObjects,
                                    const std::vector<AutonomousUnit>& autonomousUnits) {
    AIWorldSnapshot snapshot{board, kingdoms, publicBuildings, mapObjects, autonomousUnits};
    relinkBoardState(snapshot.board,
                     snapshot.kingdoms,
                     snapshot.publicBuildings,
                     snapshot.mapObjects,
                     snapshot.autonomousUnits);
    return snapshot;
}

} // namespace

void AITurnRunner::cancel() {
    m_task.reset();
}

bool AITurnRunner::isRunning() const {
    return static_cast<bool>(m_task);
}

void AITurnRunner::start(const Board& board,
                         const std::array<Kingdom, kNumKingdoms>& kingdoms,
                         const std::vector<Building>& publicBuildings,
                         const std::vector<MapObject>& mapObjects,
                         const std::vector<AutonomousUnit>& autonomousUnits,
                         KingdomId activeKingdom,
                         int turnNumber,
                         std::uint32_t worldSeed,
                         XPSystemState xpSystemState,
                         const GameConfig& config,
                         const AIDirector& director) {
    if (m_task) {
        return;
    }

    auto task = std::make_shared<TaskState>();
    task->activeKingdom = activeKingdom;
    task->turnNumber = turnNumber;
    m_task = task;

    AIWorldSnapshot snapshot = makeAIWorldSnapshot(board,
                                                   kingdoms,
                                                   publicBuildings,
                                                   mapObjects,
                                                   autonomousUnits);
    GameConfig configCopy = config;
    AIDirector directorWorker = director;

    std::thread([task,
                 snapshot = std::move(snapshot),
                 directorWorker = std::move(directorWorker),
                 worldSeed,
                 xpSystemState,
                 configCopy]() mutable {
        Kingdom& self = snapshot.kingdoms[kingdomIndex(task->activeKingdom)];
        Kingdom& enemy = snapshot.kingdoms[kingdomIndex(opponent(task->activeKingdom))];
        AIDirectorPlan plan = directorWorker.computeTurn(snapshot.board,
                                                         self,
                                                         enemy,
                                                         snapshot.publicBuildings,
                                                         task->turnNumber,
                                                         configCopy,
                                                         worldSeed,
                                                         xpSystemState);

        std::scoped_lock lock(task->mutex);
        task->plan = std::move(plan);
        task->ready = true;
    }).detach();
}

std::optional<AITurnRunner::CompletedTurn> AITurnRunner::poll() {
    if (!m_task) {
        return std::nullopt;
    }

    bool ready = false;
    {
        std::scoped_lock lock(m_task->mutex);
        ready = m_task->ready;
    }
    if (!ready) {
        return std::nullopt;
    }

    std::shared_ptr<TaskState> task = m_task;
    m_task.reset();

    CompletedTurn completedTurn;
    {
        std::scoped_lock lock(task->mutex);
        completedTurn.activeKingdom = task->activeKingdom;
        completedTurn.turnNumber = task->turnNumber;
        completedTurn.plan = std::move(task->plan);
    }
    return completedTurn;
}