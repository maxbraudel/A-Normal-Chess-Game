#pragma once
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
    KingdomId getActiveKingdom() const;
    int getTurnNumber() const;

    bool queueCommand(const TurnCommand& cmd);
    void resetPendingCommands();
    void cancelMoveCommand();   // undo a live-applied move: removes the Move command only
    void cancelBuildCommand();  // removes the queued Build command only
    const std::vector<TurnCommand>& getPendingCommands() const;
    const TurnCommand* getPendingBuildCommand() const;

    bool hasPendingMove() const;
    bool hasPendingBuild() const;
    bool hasPendingProduce() const;
    bool hasPendingMarriage() const;

    void commitTurn(Board& board, Kingdom& activeKingdom, Kingdom& enemyKingdom,
                    std::vector<Building>& publicBuildings,
                    const GameConfig& config, EventLog& log,
                    PieceFactory& pieceFactory, BuildingFactory& buildingFactory);

    void advanceTurn();

private:
    KingdomId m_activeKingdom;
    int m_turnNumber;
    std::vector<TurnCommand> m_pendingCommands;

    bool m_hasMoved;
    bool m_hasBuilt;
    bool m_hasProduced;
    bool m_hasMarried;
};
