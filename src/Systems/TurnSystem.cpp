#include "Systems/TurnSystem.hpp"
#include "Board/Board.hpp"
#include <algorithm>
#include "Board/Cell.hpp"
#include "Kingdom/Kingdom.hpp"
#include "Config/GameConfig.hpp"
#include "Systems/EventLog.hpp"
#include "Systems/CombatSystem.hpp"
#include "Systems/BuildReachRules.hpp"
#include "Systems/EconomySystem.hpp"
#include "Systems/XPSystem.hpp"
#include "Systems/BuildSystem.hpp"
#include "Systems/ProductionSystem.hpp"
#include "Systems/ProductionSpawnRules.hpp"
#include "Systems/StructureIntegrityRules.hpp"
#include "Systems/MarriageSystem.hpp"
#include "Systems/PendingTurnProjection.hpp"
#include "Systems/ChestSystem.hpp"
#include "Systems/InfernalSystem.hpp"
#include "Systems/TurnPointRules.hpp"
#include "Units/PieceFactory.hpp"
#include "Buildings/BuildingFactory.hpp"

namespace {

const char* pieceTypeDisplayName(PieceType type) {
    switch (type) {
        case PieceType::Pawn:
            return "Pawn";
        case PieceType::Knight:
            return "Knight";
        case PieceType::Bishop:
            return "Bishop";
        case PieceType::Rook:
            return "Rook";
        case PieceType::Queen:
            return "Queen";
        case PieceType::King:
            return "King";
        default:
            return "Piece";
    }
}

int getBuildCost(BuildingType type, const GameConfig& config) {
    switch (type) {
        case BuildingType::Barracks:
            return config.getBarracksCost();

        case BuildingType::WoodWall:
            return config.getWoodWallCost();

        case BuildingType::StoneWall:
            return config.getStoneWallCost();

        case BuildingType::Arena:
            return config.getArenaCost();

        default:
            return 0;
    }
}

void clearBoardBuildingLinks(Board& board) {
    auto& grid = board.getGrid();
    for (auto& row : grid) {
        for (auto& cell : row) {
            cell.building = nullptr;
        }
    }
}

template <typename BuildingContainer>
void relinkBuildingContainer(Board& board, BuildingContainer& buildings) {
    for (auto& building : buildings) {
        for (const sf::Vector2i& pos : building.getOccupiedCells()) {
            board.getCell(pos.x, pos.y).building = &building;
        }
    }
}

void relinkAllBuildings(Board& board,
                        Kingdom& activeKingdom,
                        Kingdom& enemyKingdom,
                        std::vector<Building>& publicBuildings) {
    clearBoardBuildingLinks(board);
    relinkBuildingContainer(board, publicBuildings);
    relinkBuildingContainer(board, activeKingdom.buildings);
    relinkBuildingContainer(board, enemyKingdom.buildings);
}

void removeAutonomousUnitAtCell(std::vector<AutonomousUnit>& autonomousUnits,
                                InfernalSystemState& infernalSystemState,
                                Cell& cell) {
    if (cell.autonomousUnit == nullptr) {
        return;
    }

    const int capturedUnitId = cell.autonomousUnit->id;
    cell.autonomousUnit = nullptr;
    autonomousUnits.erase(
        std::remove_if(autonomousUnits.begin(), autonomousUnits.end(),
            [capturedUnitId](const AutonomousUnit& unit) {
                return unit.id == capturedUnitId;
            }),
        autonomousUnits.end());

    if (infernalSystemState.activeInfernalUnitId == capturedUnitId) {
        InfernalSystem::clearActiveInfernal(infernalSystemState);
    }
}

int processEnemyStructureOccupancy(Board& board,
                                   Kingdom& activeKingdom,
                                   XPSystemState& xpSystemState,
                                   std::uint32_t worldSeed,
                                   const GameConfig& config,
                                   EventLog& log,
                                   int turnNumber) {
    int structureDamageEvents = 0;
    for (auto& piece : activeKingdom.pieces) {
        Cell& occupiedCell = board.getCell(piece.position.x, piece.position.y);
        Building* building = occupiedCell.building;
        if (!building || building->isNeutral || building->owner == activeKingdom.id) {
            continue;
        }

        const int localX = piece.position.x - building->origin.x;
        const int localY = piece.position.y - building->origin.y;
        const StructureOccupancyResult result = StructureIntegrityRules::applyEnemyOccupancy(
            *building, localX, localY, config);
        if (result == StructureOccupancyResult::None) {
            continue;
        }

        ++structureDamageEvents;
        XPSystem::grantBlockDestroyXP(piece, xpSystemState, worldSeed, config);

        switch (result) {
            case StructureOccupancyResult::Breached:
                log.log(turnNumber, activeKingdom.id, "Breached enemy stone wall!");
                break;

            case StructureOccupancyResult::Destroyed:
                log.log(turnNumber, activeKingdom.id, "Destroyed enemy structure cell!");
                break;

            case StructureOccupancyResult::Damaged:
                log.log(turnNumber, activeKingdom.id, "Damaged enemy building!");
                break;

            case StructureOccupancyResult::None:
                break;
        }
    }

    return structureDamageEvents;
}

void processFriendlyRepairs(Kingdom& activeKingdom,
                           const GameConfig& config,
                           EventLog& log,
                           int turnNumber) {
    for (auto& building : activeKingdom.buildings) {
        if (!StructureIntegrityRules::isRepairableOwnedStructureType(building.type)) {
            continue;
        }

        const int footprintWidth = building.getFootprintWidth();
        const int footprintHeight = building.getFootprintHeight();
        for (int localY = 0; localY < footprintHeight; ++localY) {
            for (int localX = 0; localX < footprintWidth; ++localX) {
                if (!building.isCellDestroyed(localX, localY)) {
                    continue;
                }

                const sf::Vector2i cellPos{building.origin.x + localX, building.origin.y + localY};
                const Piece* occupant = activeKingdom.getPieceAt(cellPos);
                if (!occupant || !isBuildSupportPieceType(occupant->type)) {
                    continue;
                }

                const int repairCost = StructureIntegrityRules::repairCostPerCell(building.type, config);
                if (activeKingdom.gold < repairCost) {
                    continue;
                }

                if (StructureIntegrityRules::repairDestroyedCell(building, localX, localY, config)) {
                    activeKingdom.gold -= repairCost;
                    log.log(turnNumber, activeKingdom.id, "Repaired a destroyed building cell!");
                }
            }
        }
    }
}

void removeDestroyedStructures(Kingdom& kingdom) {
    kingdom.buildings.erase(
        std::remove_if(kingdom.buildings.begin(), kingdom.buildings.end(),
                       [](const Building& building) {
                           return building.isDestroyed()
                               && StructureIntegrityRules::shouldRemoveWhenFullyDestroyed(building.type);
                       }),
        kingdom.buildings.end());
}

} // namespace

TurnSystem::TurnSystem()
    : m_activeKingdom(KingdomId::White), m_turnNumber(1),
      m_movementPointsMax(0), m_movementPointsRemaining(0),
      m_buildPointsMax(0), m_buildPointsRemaining(0),
      m_hasProduced(false), m_hasMarried(false),
    m_pendingStateRevision(1) {}

void TurnSystem::setActiveKingdom(KingdomId id) {
    if (m_activeKingdom != id) {
        m_activeKingdom = id;
        markPendingStateChanged();
    }
}

void TurnSystem::setTurnNumber(int turnNumber) {
    const int normalizedTurnNumber = std::max(1, turnNumber);
    if (m_turnNumber != normalizedTurnNumber) {
        m_turnNumber = normalizedTurnNumber;
        markPendingStateChanged();
    }
}
KingdomId TurnSystem::getActiveKingdom() const { return m_activeKingdom; }
int TurnSystem::getTurnNumber() const { return m_turnNumber; }

void TurnSystem::markPendingStateChanged() {
    ++m_pendingStateRevision;
}

void TurnSystem::syncPointBudget(const GameConfig& config, const Kingdom& activeKingdom) {
    const TurnPointBudget budget = TurnPointRules::makeBudget(
        config,
        activeKingdom.movementPointsMaxBonus,
        activeKingdom.buildPointsMaxBonus);
    m_movementPointsMax = budget.movementPointsMax;
    m_buildPointsMax = budget.buildPointsMax;

    if (m_pendingCommands.empty()) {
        m_movementPointsRemaining = budget.movementPointsRemaining;
        m_buildPointsRemaining = budget.buildPointsRemaining;
        m_pieceMoveCounts.clear();
    } else {
        m_movementPointsRemaining = std::min(m_movementPointsRemaining, budget.movementPointsMax);
        m_buildPointsRemaining = std::min(m_buildPointsRemaining, budget.buildPointsMax);
    }
}

void TurnSystem::rebuildQueuedSpecialState() {
    m_hasProduced = false;
    m_producedBarracks.clear();
    m_hasMarried = false;

    for (const TurnCommand& command : m_pendingCommands) {
        if (command.type == TurnCommand::Produce) {
            m_hasProduced = true;
            if (command.barracksId >= 0) {
                m_producedBarracks.insert(command.barracksId);
            }
        } else if (command.type == TurnCommand::Marry) {
            m_hasMarried = true;
        }
    }
}

void TurnSystem::refreshProjectedBudgetState(const TurnValidationContext& context) {
    syncPointBudget(context.config, context.activeKingdom);
    if (m_pendingCommands.empty()) {
        return;
    }

    const PendingTurnNormalizationResult projection = PendingTurnProjection::normalize(
        context,
        m_pendingCommands,
        PendingTurnInvalidCommandPolicy::DropInvalidBuilds);
    if (!projection.valid) {
        return;
    }

    if (!projection.droppedCommands.empty()) {
        m_pendingCommands = projection.normalizedCommands;
        rebuildQueuedSpecialState();
        markPendingStateChanged();
    }

    const SnapTurnBudget& budget = projection.snapshot.turnBudget(context.activeKingdom.id);
    m_movementPointsRemaining = budget.movementPointsRemaining;
    m_buildPointsRemaining = budget.buildPointsRemaining;
    m_pieceMoveCounts = budget.pieceMoveCounts;
}

bool TurnSystem::queueCommand(const TurnCommand& cmd,
                              const TurnValidationContext& context,
                              BuildingFactory* buildingFactory) {
    syncPointBudget(context.config, context.activeKingdom);

    TurnCommand queuedCommand = cmd;

    switch (queuedCommand.type) {
        case TurnCommand::Move:
            if (queuedCommand.pieceId >= 0
                && getPendingDisbandCommand(queuedCommand.pieceId) != nullptr) {
                return false;
            }
            break;

        case TurnCommand::Produce:
            if (queuedCommand.barracksId >= 0
                && m_producedBarracks.count(queuedCommand.barracksId)) {
                return false;
            }
            break;

        case TurnCommand::Upgrade:
            if (queuedCommand.upgradePieceId >= 0
                && (getPendingUpgradeCommand(queuedCommand.upgradePieceId) != nullptr
                    || getPendingDisbandCommand(queuedCommand.upgradePieceId) != nullptr)) {
                return false;
            }
            break;

        case TurnCommand::Disband:
            if (queuedCommand.pieceId < 0
                || getPendingDisbandCommand(queuedCommand.pieceId) != nullptr
                || getPendingMoveCommand(queuedCommand.pieceId) != nullptr
                || getPendingUpgradeCommand(queuedCommand.pieceId) != nullptr) {
                return false;
            }
            break;

        case TurnCommand::Marry:
            return false;

        default:
            break;
    }

    std::string errorMessage;
    if (!PendingTurnProjection::canAppendCommand(
            context, m_pendingCommands, queuedCommand, &errorMessage)) {
        return false;
    }

    if (queuedCommand.type == TurnCommand::Build) {
        if (!buildingFactory && queuedCommand.buildId < 0) {
            return false;
        }

        if (queuedCommand.buildId < 0) {
            queuedCommand.buildId = buildingFactory->reserveId();
        } else if (buildingFactory) {
            buildingFactory->observeExisting(queuedCommand.buildId);
        }
    }

    m_pendingCommands.push_back(queuedCommand);
    rebuildQueuedSpecialState();
    refreshProjectedBudgetState(context);
    markPendingStateChanged();
    return true;
}

bool TurnSystem::queueCommand(const TurnCommand& cmd,
                              const Board& board,
                              const Kingdom& activeKingdom,
                              const Kingdom& enemyKingdom,
                              const std::vector<Building>& publicBuildings,
                              const GameConfig& config,
                              BuildingFactory* buildingFactory) {
    return queueCommand(
        cmd,
        TurnValidationContext{board, activeKingdom, enemyKingdom, publicBuildings, m_turnNumber, config},
        buildingFactory);
}

void TurnSystem::resetPendingCommands() {
    m_pendingCommands.clear();
    m_pieceMoveCounts.clear();
    m_movementPointsRemaining = m_movementPointsMax;
    m_buildPointsRemaining = m_buildPointsMax;
    rebuildQueuedSpecialState();
    markPendingStateChanged();
}

bool TurnSystem::cancelMoveCommand(int pieceId,
                                   const TurnValidationContext& context) {
    const auto originalSize = m_pendingCommands.size();
    auto it = std::remove_if(m_pendingCommands.begin(), m_pendingCommands.end(),
        [pieceId](const TurnCommand& c) {
            return c.type == TurnCommand::Move && c.pieceId == pieceId;
        });
    m_pendingCommands.erase(it, m_pendingCommands.end());
    if (m_pendingCommands.size() == originalSize) {
        return false;
    }

    rebuildQueuedSpecialState();
    refreshProjectedBudgetState(context);
    markPendingStateChanged();
    return true;
}

bool TurnSystem::cancelMoveCommand(int pieceId,
                                   const Board& board,
                                   const Kingdom& activeKingdom,
                                   const Kingdom& enemyKingdom,
                                   const std::vector<Building>& publicBuildings,
                                   const GameConfig& config) {
    return cancelMoveCommand(
        pieceId,
        TurnValidationContext{board, activeKingdom, enemyKingdom, publicBuildings, m_turnNumber, config});
}

bool TurnSystem::cancelBuildCommand(int buildId,
                                    const TurnValidationContext& context) {
    const auto originalSize = m_pendingCommands.size();
    auto it = std::remove_if(m_pendingCommands.begin(), m_pendingCommands.end(),
        [buildId](const TurnCommand& c) {
            return c.type == TurnCommand::Build && c.buildId == buildId;
        });
    m_pendingCommands.erase(it, m_pendingCommands.end());
    if (m_pendingCommands.size() == originalSize) {
        return false;
    }

    rebuildQueuedSpecialState();
    refreshProjectedBudgetState(context);
    markPendingStateChanged();
    return true;
}

bool TurnSystem::cancelBuildCommand(int buildId,
                                    const Board& board,
                                    const Kingdom& activeKingdom,
                                    const Kingdom& enemyKingdom,
                                    const std::vector<Building>& publicBuildings,
                                    const GameConfig& config) {
    return cancelBuildCommand(
        buildId,
        TurnValidationContext{board, activeKingdom, enemyKingdom, publicBuildings, m_turnNumber, config});
}

bool TurnSystem::replaceMoveCommand(const TurnCommand& moveCommand,
                                    const TurnValidationContext& context) {
    if (moveCommand.type != TurnCommand::Move) {
        return false;
    }
    if (getPendingDisbandCommand(moveCommand.pieceId) != nullptr) {
        return false;
    }

    syncPointBudget(context.config, context.activeKingdom);

    std::vector<TurnCommand> candidateCommands = m_pendingCommands;
    bool replacedExistingMove = false;
    for (TurnCommand& pendingCommand : candidateCommands) {
        if (pendingCommand.type == TurnCommand::Move && pendingCommand.pieceId == moveCommand.pieceId) {
            pendingCommand = moveCommand;
            replacedExistingMove = true;
            break;
        }
    }
    if (!replacedExistingMove) {
        candidateCommands.push_back(moveCommand);
    }

    const PendingTurnNormalizationResult projection = PendingTurnProjection::normalize(
        context,
        candidateCommands,
        PendingTurnInvalidCommandPolicy::DropInvalidBuilds);
    if (!projection.valid) {
        return false;
    }

    m_pendingCommands = projection.normalizedCommands;
    rebuildQueuedSpecialState();

    const SnapTurnBudget& budget = projection.snapshot.turnBudget(context.activeKingdom.id);
    m_movementPointsRemaining = budget.movementPointsRemaining;
    m_buildPointsRemaining = budget.buildPointsRemaining;
    m_pieceMoveCounts = budget.pieceMoveCounts;
    markPendingStateChanged();
    return true;
}

bool TurnSystem::replaceMoveCommand(const TurnCommand& moveCommand,
                                    const Board& board,
                                    const Kingdom& activeKingdom,
                                    const Kingdom& enemyKingdom,
                                    const std::vector<Building>& publicBuildings,
                                    const GameConfig& config) {
    return replaceMoveCommand(
        moveCommand,
        TurnValidationContext{board, activeKingdom, enemyKingdom, publicBuildings, m_turnNumber, config});
}

bool TurnSystem::cancelProduceCommand(int barracksId,
                                      const TurnValidationContext& context) {
    const auto originalSize = m_pendingCommands.size();
    auto it = std::remove_if(m_pendingCommands.begin(), m_pendingCommands.end(),
        [barracksId](const TurnCommand& c) {
            return c.type == TurnCommand::Produce && c.barracksId == barracksId;
        });
    m_pendingCommands.erase(it, m_pendingCommands.end());
    if (m_pendingCommands.size() == originalSize) {
        return false;
    }

    rebuildQueuedSpecialState();
    refreshProjectedBudgetState(context);
    markPendingStateChanged();
    return true;
}

bool TurnSystem::cancelProduceCommand(int barracksId,
                                      const Board& board,
                                      const Kingdom& activeKingdom,
                                      const Kingdom& enemyKingdom,
                                      const std::vector<Building>& publicBuildings,
                                      const GameConfig& config) {
    return cancelProduceCommand(
        barracksId,
        TurnValidationContext{board, activeKingdom, enemyKingdom, publicBuildings, m_turnNumber, config});
}

bool TurnSystem::cancelUpgradeCommand(int pieceId,
                                      const TurnValidationContext& context) {
    const auto originalSize = m_pendingCommands.size();
    auto it = std::remove_if(m_pendingCommands.begin(), m_pendingCommands.end(),
        [pieceId](const TurnCommand& c) {
            return c.type == TurnCommand::Upgrade && c.upgradePieceId == pieceId;
        });
    m_pendingCommands.erase(it, m_pendingCommands.end());
    if (m_pendingCommands.size() == originalSize) {
        return false;
    }

    rebuildQueuedSpecialState();
    refreshProjectedBudgetState(context);
    markPendingStateChanged();
    return true;
}

bool TurnSystem::cancelUpgradeCommand(int pieceId,
                                      const Board& board,
                                      const Kingdom& activeKingdom,
                                      const Kingdom& enemyKingdom,
                                      const std::vector<Building>& publicBuildings,
                                      const GameConfig& config) {
    return cancelUpgradeCommand(
        pieceId,
        TurnValidationContext{board, activeKingdom, enemyKingdom, publicBuildings, m_turnNumber, config});
}

bool TurnSystem::cancelDisbandCommand(int pieceId,
                                      const TurnValidationContext& context) {
    const auto originalSize = m_pendingCommands.size();
    auto it = std::remove_if(m_pendingCommands.begin(), m_pendingCommands.end(),
        [pieceId](const TurnCommand& c) {
            return c.type == TurnCommand::Disband && c.pieceId == pieceId;
        });
    m_pendingCommands.erase(it, m_pendingCommands.end());
    if (m_pendingCommands.size() == originalSize) {
        return false;
    }

    rebuildQueuedSpecialState();
    refreshProjectedBudgetState(context);
    markPendingStateChanged();
    return true;
}

bool TurnSystem::cancelDisbandCommand(int pieceId,
                                      const Board& board,
                                      const Kingdom& activeKingdom,
                                      const Kingdom& enemyKingdom,
                                      const std::vector<Building>& publicBuildings,
                                      const GameConfig& config) {
    return cancelDisbandCommand(
        pieceId,
        TurnValidationContext{board, activeKingdom, enemyKingdom, publicBuildings, m_turnNumber, config});
}

const std::vector<TurnCommand>& TurnSystem::getPendingCommands() const {
    return m_pendingCommands;
}

const TurnCommand* TurnSystem::getPendingMoveCommand(int pieceId) const {
    for (const auto& cmd : m_pendingCommands) {
        if (cmd.type == TurnCommand::Move && cmd.pieceId == pieceId) {
            return &cmd;
        }
    }
    return nullptr;
}

const TurnCommand* TurnSystem::getPendingBuildCommand(int buildId) const {
    for (const auto& cmd : m_pendingCommands) {
        if (cmd.type == TurnCommand::Build && cmd.buildId == buildId) {
            return &cmd;
        }
    }

    return nullptr;
}

const TurnCommand* TurnSystem::getPendingProduceCommand(int barracksId) const {
    for (const auto& cmd : m_pendingCommands) {
        if (cmd.type == TurnCommand::Produce && cmd.barracksId == barracksId) {
            return &cmd;
        }
    }

    return nullptr;
}

const TurnCommand* TurnSystem::getPendingUpgradeCommand(int pieceId) const {
    for (const auto& cmd : m_pendingCommands) {
        if (cmd.type == TurnCommand::Upgrade && cmd.upgradePieceId == pieceId) {
            return &cmd;
        }
    }

    return nullptr;
}

const TurnCommand* TurnSystem::getPendingDisbandCommand(int pieceId) const {
    for (const auto& cmd : m_pendingCommands) {
        if (cmd.type == TurnCommand::Disband && cmd.pieceId == pieceId) {
            return &cmd;
        }
    }

    return nullptr;
}

bool TurnSystem::hasPendingMove() const {
    return std::any_of(m_pendingCommands.begin(), m_pendingCommands.end(),
        [](const TurnCommand& command) { return command.type == TurnCommand::Move; });
}
bool TurnSystem::hasPendingBuild() const {
    return std::any_of(m_pendingCommands.begin(), m_pendingCommands.end(),
        [](const TurnCommand& command) { return command.type == TurnCommand::Build; });
}
bool TurnSystem::hasPendingProduce() const { return m_hasProduced; }
bool TurnSystem::hasPendingMarriage() const { return m_hasMarried; }
bool TurnSystem::hasPendingMoveForPiece(int pieceId) const {
    return getPendingMoveCommand(pieceId) != nullptr;
}
bool TurnSystem::hasPendingProduceForBarracks(int barracksId) const {
    return getPendingProduceCommand(barracksId) != nullptr;
}
bool TurnSystem::hasPendingUpgradeForPiece(int pieceId) const {
    return getPendingUpgradeCommand(pieceId) != nullptr;
}
bool TurnSystem::hasPendingDisbandForPiece(int pieceId) const {
    return getPendingDisbandCommand(pieceId) != nullptr;
}

int TurnSystem::getMovementPointsMax() const { return m_movementPointsMax; }
int TurnSystem::getMovementPointsRemaining() const { return m_movementPointsRemaining; }
int TurnSystem::getBuildPointsMax() const { return m_buildPointsMax; }
int TurnSystem::getBuildPointsRemaining() const { return m_buildPointsRemaining; }
int TurnSystem::getMoveCountForPiece(int pieceId) const {
    const auto it = m_pieceMoveCounts.find(pieceId);
    return (it == m_pieceMoveCounts.end()) ? 0 : it->second;
}

std::uint64_t TurnSystem::getPendingStateRevision() const { return m_pendingStateRevision; }

void TurnSystem::commitTurn(Board& board, Kingdom& activeKingdom, Kingdom& enemyKingdom,
                             std::vector<Building>& publicBuildings,
                             std::vector<MapObject>& mapObjects,
                             ChestSystemState& chestSystemState,
                             XPSystemState& xpSystemState,
                             std::vector<AutonomousUnit>& autonomousUnits,
                             InfernalSystemState& infernalSystemState,
                             std::uint32_t worldSeed,
                             const GameConfig& config, EventLog& log,
                             std::vector<GameplayNotification>& gameplayNotifications,
                             PieceFactory& pieceFactory, BuildingFactory& buildingFactory) {
    std::vector<TurnCommand> deferredUpgrades;
    deferredUpgrades.reserve(m_pendingCommands.size());

    // Execute all pending commands
    for (const auto& cmd : m_pendingCommands) {
        switch (cmd.type) {
            case TurnCommand::Move: {
                Piece* piece = activeKingdom.getPieceById(cmd.pieceId);
                if (!piece) break;

                // Prevent moving onto enemy king — game should end on checkmate, not capture
                Cell& destCheck = board.getCell(cmd.destination.x, cmd.destination.y);
                if (destCheck.piece && destCheck.piece->kingdom != piece->kingdom
                    && destCheck.piece->type == PieceType::King) {
                    break; // Illegal move — skip entirely
                }

                // Clear the origin cell (use cmd.origin, not piece->position, because
                // the piece may have been pre-applied live by InputHandler already)
                Cell& oldCell = board.getCell(cmd.origin.x, cmd.origin.y);
                if (oldCell.piece == piece) {
                    oldCell.piece = nullptr;
                }

                // Combat resolution
                const CombatSystem::CombatResult combatResult = CombatSystem::resolve(
                    *piece,
                    board,
                    cmd.destination,
                    activeKingdom,
                    enemyKingdom,
                    xpSystemState,
                    worldSeed,
                    config,
                    log,
                    m_turnNumber);
                if (combatResult.occurred && combatResult.targetWasPiece) {
                    InfernalSystem::addBloodDebtForCapturedPiece(
                        infernalSystemState,
                        activeKingdom.id,
                        combatResult.capturedPieceType,
                        config.getInfernalBloodDebtForCapturedPiece(combatResult.capturedPieceType));
                }

                // Move
                piece->position = cmd.destination;
                Cell& newCell = board.getCell(cmd.destination.x, cmd.destination.y);
                newCell.piece = piece;

                if (newCell.autonomousUnit != nullptr) {
                    const std::string capturedName = autonomousUnitTypeDisplayName(newCell.autonomousUnit->type);
                    removeAutonomousUnitAtCell(autonomousUnits, infernalSystemState, newCell);
                    log.log(m_turnNumber,
                            m_activeKingdom,
                            "Captured " + capturedName + " at ("
                                + std::to_string(cmd.destination.x) + ","
                                + std::to_string(cmd.destination.y) + ")");
                }

                if (std::optional<ChestClaimResult> chestClaim = ChestSystem::collectChestAtPosition(
                        mapObjects,
                        chestSystemState,
                        cmd.destination,
                        activeKingdom)) {
                    newCell.mapObject = nullptr;
                    gameplayNotifications.push_back(chestClaim->notification);
                    log.log(m_turnNumber,
                            m_activeKingdom,
                            "Opened a chest and received " + describeChestReward(chestClaim->reward));
                }

                log.log(m_turnNumber, m_activeKingdom, "Moved " +
                    std::string(pieceTypeDisplayName(piece->type)) + " to (" +
                        std::to_string(cmd.destination.x) + "," + std::to_string(cmd.destination.y) + ")");
                break;
            }
            case TurnCommand::Build: {
                if (!BuildSystem::canBuild(cmd.buildingType,
                                           cmd.buildOrigin,
                                           board,
                                           activeKingdom,
                                           config,
                                           cmd.buildRotationQuarterTurns)) {
                    break;
                }

                const int cost = getBuildCost(cmd.buildingType, config);
                if (activeKingdom.gold < cost) {
                    break;
                }

                Building building = buildingFactory.createBuilding(
                    cmd.buildingType, m_activeKingdom, cmd.buildOrigin, config,
                    cmd.buildRotationQuarterTurns, cmd.buildId);

                activeKingdom.gold -= cost;
                activeKingdom.addBuilding(building);
                relinkAllBuildings(board, activeKingdom, enemyKingdom, publicBuildings);

                log.log(m_turnNumber, m_activeKingdom, "Built a building");
                break;
            }
            case TurnCommand::Produce: {
                Building* barracks = nullptr;
                for (auto& b : activeKingdom.buildings) {
                    if (b.id == cmd.barracksId) { barracks = &b; break; }
                }
                if (barracks) {
                    const int cost = config.getRecruitCost(cmd.produceType);
                    if (activeKingdom.gold < cost) {
                        break;
                    }
                    ProductionSystem::startProduction(*barracks, cmd.produceType, config);
                    activeKingdom.gold -= cost;
                    log.log(m_turnNumber, m_activeKingdom, "Started production");
                }
                break;
            }
            case TurnCommand::Upgrade: {
                Piece* piece = activeKingdom.getPieceById(cmd.upgradePieceId);
                if (piece) {
                    int cost = config.getUpgradeCost(piece->type, cmd.upgradeTarget);
                    if (activeKingdom.gold < cost || !XPSystem::canUpgrade(*piece, cmd.upgradeTarget, config)) {
                        break;
                    }
                    activeKingdom.gold -= cost;
                    deferredUpgrades.push_back(cmd);
                }
                break;
            }
            case TurnCommand::Disband: {
                Piece* piece = activeKingdom.getPieceById(cmd.pieceId);
                if (!piece || piece->type == PieceType::King) {
                    break;
                }

                const sf::Vector2i piecePosition = piece->position;
                if (board.isInBounds(piecePosition.x, piecePosition.y)) {
                    Cell& pieceCell = board.getCell(piecePosition.x, piecePosition.y);
                    if (pieceCell.piece == piece) {
                        pieceCell.piece = nullptr;
                    }
                }

                const PieceType removedType = piece->type;
                activeKingdom.removePiece(piece->id);
                log.log(m_turnNumber,
                        m_activeKingdom,
                        "Sacrificed " + std::string(pieceTypeDisplayName(removedType)));
                break;
            }
            case TurnCommand::Marry: {
                break;
            }
            default:
                break;
        }
    }

    for (const TurnCommand& cmd : deferredUpgrades) {
        Piece* piece = activeKingdom.getPieceById(cmd.upgradePieceId);
        if (!piece || !XPSystem::canUpgrade(*piece, cmd.upgradeTarget, config)) {
            continue;
        }

        XPSystem::upgrade(*piece, cmd.upgradeTarget);
        log.log(m_turnNumber, m_activeKingdom, "Upgraded a piece");
    }

    for (auto& building : publicBuildings) {
        if (building.type == BuildingType::Church) {
            MarriageSystem::performChurchCoronation(activeKingdom, board, building, log, m_turnNumber);
            break;
        }
    }

    relinkAllBuildings(board, activeKingdom, enemyKingdom, publicBuildings);

    const int structureDamageEvents = processEnemyStructureOccupancy(
        board,
        activeKingdom,
        xpSystemState,
        worldSeed,
        config,
        log,
        m_turnNumber);
    if (structureDamageEvents > 0) {
        InfernalSystem::addBloodDebtForStructureDamage(
            infernalSystemState,
            activeKingdom.id,
            structureDamageEvents * config.getInfernalBloodDebtForStructureDamage());
    }

    // Advance all barracks production
    for (auto& b : activeKingdom.buildings) {
        if (b.type == BuildingType::Barracks && b.isProducing && !b.isDestroyed()) {
            ProductionSystem::advanceProduction(b);
            if (ProductionSystem::isProductionComplete(b)) {
                const PieceType pt = static_cast<PieceType>(b.producingType);
                sf::Vector2i spawnPos = ProductionSystem::findSpawnCell(b, board, pt, activeKingdom);
                if (spawnPos.x >= 0) {
                    Piece newPiece = pieceFactory.createPiece(pt, m_activeKingdom, spawnPos);
                    activeKingdom.addPiece(newPiece);
                    board.getCell(spawnPos.x, spawnPos.y).piece = &activeKingdom.pieces.back();
                    if (pt == PieceType::Bishop) {
                        activeKingdom.recordSuccessfulBishopSpawnParity(
                            ProductionSpawnRules::squareColorParity(spawnPos));
                    }

                    log.log(m_turnNumber, m_activeKingdom, "Unit produced!");
                    b.isProducing = false;
                } else {
                    // Spawn cell blocked — keep trying each turn
                    // (turnsRemaining is already 0, so we'll retry next turn)
                }
            }
        }
    }

    processFriendlyRepairs(activeKingdom, config, log, m_turnNumber);

    removeDestroyedStructures(activeKingdom);
    removeDestroyedStructures(enemyKingdom);
    relinkAllBuildings(board, activeKingdom, enemyKingdom, publicBuildings);

    // Credit income
    EconomySystem::collectIncome(activeKingdom, board, publicBuildings, config, log, m_turnNumber);

    // Arena XP
    XPSystem::grantArenaXP(activeKingdom, board, activeKingdom.buildings, xpSystemState, worldSeed, config);

    m_pendingCommands.clear();
    m_pieceMoveCounts.clear();
    m_movementPointsRemaining = m_movementPointsMax;
    m_buildPointsRemaining = m_buildPointsMax;
    rebuildQueuedSpecialState();
    markPendingStateChanged();
}

void TurnSystem::advanceTurn() {
    if (m_activeKingdom == KingdomId::White)
        m_activeKingdom = KingdomId::Black;
    else {
        m_activeKingdom = KingdomId::White;
        ++m_turnNumber;
    }
    markPendingStateChanged();
}
