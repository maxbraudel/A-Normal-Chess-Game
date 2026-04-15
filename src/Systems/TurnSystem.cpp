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
#include "Units/PieceFactory.hpp"
#include "Buildings/BuildingFactory.hpp"

namespace {

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
      m_hasMoved(false), m_hasBuilt(false), m_hasProduced(false), m_hasMarried(false) {}

void TurnSystem::setActiveKingdom(KingdomId id) { m_activeKingdom = id; }
void TurnSystem::setTurnNumber(int turnNumber) { m_turnNumber = std::max(1, turnNumber); }
KingdomId TurnSystem::getActiveKingdom() const { return m_activeKingdom; }
int TurnSystem::getTurnNumber() const { return m_turnNumber; }

bool TurnSystem::queueCommand(const TurnCommand& cmd) {
    switch (cmd.type) {
        case TurnCommand::Move:
            if (m_hasMoved) return false;
            m_hasMoved = true;
            break;
        case TurnCommand::Build:
            if (m_hasBuilt) return false;
            m_hasBuilt = true;
            break;
        case TurnCommand::Produce:
            // Allow 1 production per barracks (instead of 1 global)
            if (cmd.barracksId >= 0 && m_producedBarracks.count(cmd.barracksId))
                return false; // this barracks already has a production queued
            if (cmd.barracksId >= 0)
                m_producedBarracks.insert(cmd.barracksId);
            m_hasProduced = true;
            break;
        case TurnCommand::Marry:
            if (m_hasMarried) return false;
            m_hasMarried = true;
            break;
        default:
            break;
    }
    m_pendingCommands.push_back(cmd);
    return true;
}

void TurnSystem::resetPendingCommands() {
    m_pendingCommands.clear();
    m_hasMoved = false;
    m_hasBuilt = false;
    m_hasProduced = false;
    m_producedBarracks.clear();
    m_hasMarried = false;
}

void TurnSystem::cancelMoveCommand() {
    auto it = std::remove_if(m_pendingCommands.begin(), m_pendingCommands.end(),
        [](const TurnCommand& c) { return c.type == TurnCommand::Move; });
    m_pendingCommands.erase(it, m_pendingCommands.end());
    m_hasMoved = false;
}

void TurnSystem::cancelBuildCommand() {
    auto it = std::remove_if(m_pendingCommands.begin(), m_pendingCommands.end(),
        [](const TurnCommand& c) { return c.type == TurnCommand::Build; });
    m_pendingCommands.erase(it, m_pendingCommands.end());
    m_hasBuilt = false;
}

const std::vector<TurnCommand>& TurnSystem::getPendingCommands() const {
    return m_pendingCommands;
}

const TurnCommand* TurnSystem::getPendingBuildCommand() const {
    for (const auto& cmd : m_pendingCommands) {
        if (cmd.type == TurnCommand::Build) {
            return &cmd;
        }
    }
    return nullptr;
}

bool TurnSystem::hasPendingMove() const { return m_hasMoved; }
bool TurnSystem::hasPendingBuild() const { return m_hasBuilt; }
bool TurnSystem::hasPendingProduce() const { return m_hasProduced; }
bool TurnSystem::hasPendingMarriage() const { return m_hasMarried; }

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
                oldCell.piece = nullptr;

                // Combat resolution
                CombatSystem::resolve(*piece, board, cmd.destination,
                                       activeKingdom, enemyKingdom, config, log, m_turnNumber);

                // Move
                piece->position = cmd.destination;
                Cell& newCell = board.getCell(cmd.destination.x, cmd.destination.y);
                newCell.piece = piece;

                log.log(m_turnNumber, m_activeKingdom, "Moved piece to (" +
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
                // Find pawn on church for marriage
                for (auto& b : publicBuildings) {
                    if (b.type == BuildingType::Church) {
                        MarriageSystem::performMarriage(activeKingdom, board, b, log, m_turnNumber);
                        break;
                    }
                }
                break;
            }
            default:
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
    m_hasMoved = false;
    m_hasBuilt = false;
    m_hasProduced = false;
    m_producedBarracks.clear();
    m_hasMarried = false;
}

void TurnSystem::advanceTurn() {
    if (m_activeKingdom == KingdomId::White)
        m_activeKingdom = KingdomId::Black;
    else {
        m_activeKingdom = KingdomId::White;
        ++m_turnNumber;
    }
}
