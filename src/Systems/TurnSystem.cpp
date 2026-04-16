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

void processEnemyStructureOccupancy(Board& board,
                                    Kingdom& activeKingdom,
                                    const GameConfig& config,
                                    EventLog& log,
                                    int turnNumber) {
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

        piece.xp += config.getDestroyBlockXP();

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
      m_hasProduced(false), m_hasMarried(false) {}

void TurnSystem::setActiveKingdom(KingdomId id) { m_activeKingdom = id; }
void TurnSystem::setTurnNumber(int turnNumber) { m_turnNumber = std::max(1, turnNumber); }
KingdomId TurnSystem::getActiveKingdom() const { return m_activeKingdom; }
int TurnSystem::getTurnNumber() const { return m_turnNumber; }

void TurnSystem::syncPointBudget(const GameConfig& config) {
    const TurnPointBudget budget = TurnPointRules::makeBudget(config);
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

void TurnSystem::refreshProjectedBudgetState(const Board& board,
                                             const Kingdom& activeKingdom,
                                             const Kingdom& enemyKingdom,
                                             const std::vector<Building>& publicBuildings,
                                             const GameConfig& config) {
    syncPointBudget(config);
    if (m_pendingCommands.empty()) {
        return;
    }

    const PendingTurnProjectionResult projection = PendingTurnProjection::project(
        board, activeKingdom, enemyKingdom, publicBuildings, m_turnNumber,
        m_pendingCommands, config);
    if (!projection.valid) {
        return;
    }

    const SnapTurnBudget& budget = projection.snapshot.turnBudget(activeKingdom.id);
    m_movementPointsRemaining = budget.movementPointsRemaining;
    m_buildPointsRemaining = budget.buildPointsRemaining;
    m_pieceMoveCounts = budget.pieceMoveCounts;
}

bool TurnSystem::queueCommand(const TurnCommand& cmd,
                              const Board& board,
                              const Kingdom& activeKingdom,
                              const Kingdom& enemyKingdom,
                              const std::vector<Building>& publicBuildings,
                              const GameConfig& config) {
    syncPointBudget(config);

    switch (cmd.type) {
        case TurnCommand::Produce:
            if (cmd.barracksId >= 0 && m_producedBarracks.count(cmd.barracksId)) {
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
            board, activeKingdom, enemyKingdom, publicBuildings,
            m_turnNumber, m_pendingCommands, cmd, config, &errorMessage)) {
        return false;
    }

    m_pendingCommands.push_back(cmd);
    rebuildQueuedSpecialState();
    refreshProjectedBudgetState(board, activeKingdom, enemyKingdom, publicBuildings, config);
    return true;
}

void TurnSystem::resetPendingCommands() {
    m_pendingCommands.clear();
    m_pieceMoveCounts.clear();
    m_movementPointsRemaining = m_movementPointsMax;
    m_buildPointsRemaining = m_buildPointsMax;
    rebuildQueuedSpecialState();
}

bool TurnSystem::cancelMoveCommand(int pieceId,
                                   const Board& board,
                                   const Kingdom& activeKingdom,
                                   const Kingdom& enemyKingdom,
                                   const std::vector<Building>& publicBuildings,
                                   const GameConfig& config) {
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
    refreshProjectedBudgetState(board, activeKingdom, enemyKingdom, publicBuildings, config);
    return true;
}

bool TurnSystem::cancelBuildCommand(BuildingType type,
                                    sf::Vector2i origin,
                                    int rotationQuarterTurns,
                                    const Board& board,
                                    const Kingdom& activeKingdom,
                                    const Kingdom& enemyKingdom,
                                    const std::vector<Building>& publicBuildings,
                                    const GameConfig& config) {
    const auto originalSize = m_pendingCommands.size();
    auto it = std::remove_if(m_pendingCommands.begin(), m_pendingCommands.end(),
        [type, origin, rotationQuarterTurns](const TurnCommand& c) {
            return c.type == TurnCommand::Build
                && c.buildingType == type
                && c.buildOrigin == origin
                && c.buildRotationQuarterTurns == rotationQuarterTurns;
        });
    m_pendingCommands.erase(it, m_pendingCommands.end());
    if (m_pendingCommands.size() == originalSize) {
        return false;
    }

    rebuildQueuedSpecialState();
    refreshProjectedBudgetState(board, activeKingdom, enemyKingdom, publicBuildings, config);
    return true;
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

int TurnSystem::getMovementPointsMax() const { return m_movementPointsMax; }
int TurnSystem::getMovementPointsRemaining() const { return m_movementPointsRemaining; }
int TurnSystem::getBuildPointsMax() const { return m_buildPointsMax; }
int TurnSystem::getBuildPointsRemaining() const { return m_buildPointsRemaining; }
int TurnSystem::getMoveCountForPiece(int pieceId) const {
    const auto it = m_pieceMoveCounts.find(pieceId);
    return (it == m_pieceMoveCounts.end()) ? 0 : it->second;
}

void TurnSystem::commitTurn(Board& board, Kingdom& activeKingdom, Kingdom& enemyKingdom,
                             std::vector<Building>& publicBuildings,
                             const GameConfig& config, EventLog& log,
                             PieceFactory& pieceFactory, BuildingFactory& buildingFactory) {
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
                CombatSystem::resolve(*piece, board, cmd.destination,
                                       activeKingdom, enemyKingdom, config, log, m_turnNumber);

                // Move
                piece->position = cmd.destination;
                Cell& newCell = board.getCell(cmd.destination.x, cmd.destination.y);
                newCell.piece = piece;

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
                    cmd.buildRotationQuarterTurns);

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
                    XPSystem::upgrade(*piece, cmd.upgradeTarget);
                    log.log(m_turnNumber, m_activeKingdom, "Upgraded a piece");
                }
                break;
            }
            case TurnCommand::Marry: {
                break;
            }
            default:
                break;
        }
    }

    for (auto& building : publicBuildings) {
        if (building.type == BuildingType::Church) {
            MarriageSystem::performChurchCoronation(activeKingdom, board, building, log, m_turnNumber);
            break;
        }
    }

    relinkAllBuildings(board, activeKingdom, enemyKingdom, publicBuildings);

    processEnemyStructureOccupancy(board, activeKingdom, config, log, m_turnNumber);

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
    XPSystem::grantArenaXP(activeKingdom, board, activeKingdom.buildings, config);

    m_pendingCommands.clear();
    m_pieceMoveCounts.clear();
    m_movementPointsRemaining = m_movementPointsMax;
    m_buildPointsRemaining = m_buildPointsMax;
    rebuildQueuedSpecialState();
}

void TurnSystem::advanceTurn() {
    if (m_activeKingdom == KingdomId::White)
        m_activeKingdom = KingdomId::Black;
    else {
        m_activeKingdom = KingdomId::White;
        ++m_turnNumber;
    }
}
