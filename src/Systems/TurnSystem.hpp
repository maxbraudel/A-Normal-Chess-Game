#pragma once
#include <set>
#include <cstdint>
#include <map>
#include <vector>

#include "Autonomous/AutonomousUnit.hpp"
#include "Core/GameplayNotification.hpp"
#include "Objects/MapObject.hpp"
#include "Systems/ChestSystem.hpp"
#include "Systems/InfernalSystem.hpp"
#include "Systems/XPTypes.hpp"

#include "Systems/TurnCommand.hpp"
#include "Systems/TurnValidationContext.hpp"
#include "Kingdom/KingdomId.hpp"

class Board;
class Building;
class Kingdom;
class GameConfig;
class EventLog;
class CombatSystem;
class EconomySystem;
class XPSystem;
class BuildSystem;
class ProductionSystem;
class MarriageSystem;
class CheckSystem;
class PieceFactory;
class BuildingFactory;

class TurnSystem {
public:
    TurnSystem();

    void setActiveKingdom(KingdomId id);
    void setTurnNumber(int turnNumber);
    KingdomId getActiveKingdom() const;
    int getTurnNumber() const;

    void syncPointBudget(const GameConfig& config, const Kingdom& activeKingdom);
    bool queueCommand(const TurnCommand& cmd,
                      const TurnValidationContext& context,
                      BuildingFactory* buildingFactory = nullptr);
    bool queueCommand(const TurnCommand& cmd,
                      const Board& board,
                      const Kingdom& activeKingdom,
                      const Kingdom& enemyKingdom,
                      const std::vector<Building>& publicBuildings,
                      const GameConfig& config,
                      BuildingFactory* buildingFactory = nullptr);
    void resetPendingCommands();
    bool cancelMoveCommand(int pieceId,
                           const TurnValidationContext& context);
    bool cancelMoveCommand(int pieceId,
                           const Board& board,
                           const Kingdom& activeKingdom,
                           const Kingdom& enemyKingdom,
                           const std::vector<Building>& publicBuildings,
                           const GameConfig& config);
    bool cancelBuildCommand(int buildId,
                            const TurnValidationContext& context);
    bool cancelBuildCommand(int buildId,
                            const Board& board,
                            const Kingdom& activeKingdom,
                            const Kingdom& enemyKingdom,
                            const std::vector<Building>& publicBuildings,
                            const GameConfig& config);
    bool replaceMoveCommand(const TurnCommand& moveCommand,
                            const TurnValidationContext& context);
    bool replaceMoveCommand(const TurnCommand& moveCommand,
                            const Board& board,
                            const Kingdom& activeKingdom,
                            const Kingdom& enemyKingdom,
                            const std::vector<Building>& publicBuildings,
                            const GameConfig& config);
        bool cancelProduceCommand(int barracksId,
                                                            const TurnValidationContext& context);
    bool cancelProduceCommand(int barracksId,
                              const Board& board,
                              const Kingdom& activeKingdom,
                              const Kingdom& enemyKingdom,
                              const std::vector<Building>& publicBuildings,
                              const GameConfig& config);
    bool cancelUpgradeCommand(int pieceId,
                              const TurnValidationContext& context);
    bool cancelUpgradeCommand(int pieceId,
                              const Board& board,
                              const Kingdom& activeKingdom,
                              const Kingdom& enemyKingdom,
                              const std::vector<Building>& publicBuildings,
                              const GameConfig& config);
    bool cancelDisbandCommand(int pieceId,
                              const TurnValidationContext& context);
    bool cancelDisbandCommand(int pieceId,
                              const Board& board,
                              const Kingdom& activeKingdom,
                              const Kingdom& enemyKingdom,
                              const std::vector<Building>& publicBuildings,
                              const GameConfig& config);
    const std::vector<TurnCommand>& getPendingCommands() const;
    const TurnCommand* getPendingMoveCommand(int pieceId) const;
    const TurnCommand* getPendingBuildCommand(int buildId) const;
    const TurnCommand* getPendingProduceCommand(int barracksId) const;
    const TurnCommand* getPendingUpgradeCommand(int pieceId) const;
    const TurnCommand* getPendingDisbandCommand(int pieceId) const;

    bool hasPendingMove() const;
    bool hasPendingBuild() const;
    bool hasPendingProduce() const;
    bool hasPendingMarriage() const;
    bool hasPendingMoveForPiece(int pieceId) const;
    bool hasPendingProduceForBarracks(int barracksId) const;
    bool hasPendingUpgradeForPiece(int pieceId) const;
    bool hasPendingDisbandForPiece(int pieceId) const;

    int getMovementPointsMax() const;
    int getMovementPointsRemaining() const;
    int getBuildPointsMax() const;
    int getBuildPointsRemaining() const;
    int getMoveCountForPiece(int pieceId) const;
    std::uint64_t getPendingStateRevision() const;

    void commitTurn(Board& board, Kingdom& activeKingdom, Kingdom& enemyKingdom,
                    std::vector<Building>& publicBuildings,
                    std::vector<MapObject>& mapObjects,
                    ChestSystemState& chestSystemState,
                    XPSystemState& xpSystemState,
                    std::vector<AutonomousUnit>& autonomousUnits,
                    InfernalSystemState& infernalSystemState,
                    std::uint32_t worldSeed,
                    const GameConfig& config, EventLog& log,
                    std::vector<GameplayNotification>& gameplayNotifications,
                    PieceFactory& pieceFactory, BuildingFactory& buildingFactory);
    void commitTurn(Board& board, Kingdom& activeKingdom, Kingdom& enemyKingdom,
                    std::vector<Building>& publicBuildings,
                    const GameConfig& config, EventLog& log,
                    PieceFactory& pieceFactory, BuildingFactory& buildingFactory) {
        std::vector<MapObject> mapObjects;
        ChestSystemState chestSystemState{};
        XPSystemState xpSystemState{};
        std::vector<AutonomousUnit> autonomousUnits;
        InfernalSystemState infernalSystemState{};
        std::vector<GameplayNotification> gameplayNotifications;
        commitTurn(board,
                   activeKingdom,
                   enemyKingdom,
                   publicBuildings,
                   mapObjects,
                   chestSystemState,
                   xpSystemState,
                   autonomousUnits,
                   infernalSystemState,
                   0,
                   config,
                   log,
                   gameplayNotifications,
                   pieceFactory,
                   buildingFactory);
    }

    void advanceTurn();

private:
    KingdomId m_activeKingdom;
    int m_turnNumber;
    std::vector<TurnCommand> m_pendingCommands;

    int m_movementPointsMax;
    int m_movementPointsRemaining;
    int m_buildPointsMax;
    int m_buildPointsRemaining;
    std::map<int, int> m_pieceMoveCounts;
    bool m_hasProduced;           // true if at least 1 production queued
    std::set<int> m_producedBarracks;  // barracks IDs that have a produce queued
    bool m_hasMarried;
    std::uint64_t m_pendingStateRevision;

    void rebuildQueuedSpecialState();
    void refreshProjectedBudgetState(const TurnValidationContext& context);
    void markPendingStateChanged();
};
