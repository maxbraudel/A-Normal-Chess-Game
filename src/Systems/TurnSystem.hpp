#pragma once
#include <set>
#include <cstdint>
#include <map>
#include <vector>
#include "Systems/TurnCommand.hpp"
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

    void syncPointBudget(const GameConfig& config);
    bool queueCommand(const TurnCommand& cmd,
                      const Board& board,
                      const Kingdom& activeKingdom,
                      const Kingdom& enemyKingdom,
                      const std::vector<Building>& publicBuildings,
                      const GameConfig& config,
                      BuildingFactory* buildingFactory = nullptr);
    void resetPendingCommands();
    bool cancelMoveCommand(int pieceId,
                           const Board& board,
                           const Kingdom& activeKingdom,
                           const Kingdom& enemyKingdom,
                           const std::vector<Building>& publicBuildings,
                           const GameConfig& config);
    bool cancelBuildCommand(int buildId,
                            const Board& board,
                            const Kingdom& activeKingdom,
                            const Kingdom& enemyKingdom,
                            const std::vector<Building>& publicBuildings,
                            const GameConfig& config);
    bool replaceMoveCommand(const TurnCommand& moveCommand,
                            const Board& board,
                            const Kingdom& activeKingdom,
                            const Kingdom& enemyKingdom,
                            const std::vector<Building>& publicBuildings,
                            const GameConfig& config);
    bool cancelProduceCommand(int barracksId,
                              const Board& board,
                              const Kingdom& activeKingdom,
                              const Kingdom& enemyKingdom,
                              const std::vector<Building>& publicBuildings,
                              const GameConfig& config);
    bool cancelUpgradeCommand(int pieceId,
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

    bool hasPendingMove() const;
    bool hasPendingBuild() const;
    bool hasPendingProduce() const;
    bool hasPendingMarriage() const;
    bool hasPendingMoveForPiece(int pieceId) const;
    bool hasPendingProduceForBarracks(int barracksId) const;
    bool hasPendingUpgradeForPiece(int pieceId) const;

    int getMovementPointsMax() const;
    int getMovementPointsRemaining() const;
    int getBuildPointsMax() const;
    int getBuildPointsRemaining() const;
    int getMoveCountForPiece(int pieceId) const;
    std::uint64_t getPendingStateRevision() const;

    void commitTurn(Board& board, Kingdom& activeKingdom, Kingdom& enemyKingdom,
                    std::vector<Building>& publicBuildings,
                    const GameConfig& config, EventLog& log,
                    PieceFactory& pieceFactory, BuildingFactory& buildingFactory);

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
    void refreshProjectedBudgetState(const Board& board,
                                     const Kingdom& activeKingdom,
                                     const Kingdom& enemyKingdom,
                                     const std::vector<Building>& publicBuildings,
                                     const GameConfig& config);
    void markPendingStateChanged();
};
