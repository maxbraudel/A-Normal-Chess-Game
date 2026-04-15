#include "Systems/TurnSystem.hpp"
#include "Board/Board.hpp"
#include <algorithm>
#include "Board/Cell.hpp"
#include "Kingdom/Kingdom.hpp"
#include "Config/GameConfig.hpp"
#include "Systems/EventLog.hpp"
#include "Systems/CombatSystem.hpp"
#include "Systems/EconomySystem.hpp"
#include "Systems/XPSystem.hpp"
#include "Systems/BuildSystem.hpp"
#include "Systems/ProductionSystem.hpp"
#include "Systems/MarriageSystem.hpp"
#include "Units/PieceFactory.hpp"
#include "Buildings/BuildingFactory.hpp"

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
                Building building = buildingFactory.createBuilding(
                    cmd.buildingType, m_activeKingdom, cmd.buildOrigin, config);

                int cost = 0;
                switch (cmd.buildingType) {
                    case BuildingType::Barracks: cost = config.getBarracksCost(); break;
                    case BuildingType::WoodWall: cost = config.getWoodWallCost(); break;
                    case BuildingType::StoneWall: cost = config.getStoneWallCost(); break;
                    case BuildingType::Arena: cost = config.getArenaCost(); break;
                    default: break;
                }
                activeKingdom.gold -= cost;
                activeKingdom.addBuilding(building);

                // Update board cells
                Building* placed = &activeKingdom.buildings.back();
                for (auto& pos : placed->getOccupiedCells()) {
                    board.getCell(pos.x, pos.y).building = placed;
                }

                log.log(m_turnNumber, m_activeKingdom, "Built a building");
                break;
            }
            case TurnCommand::Produce: {
                Building* barracks = nullptr;
                for (auto& b : activeKingdom.buildings) {
                    if (b.id == cmd.barracksId) { barracks = &b; break; }
                }
                if (barracks) {
                    ProductionSystem::startProduction(*barracks, cmd.produceType, config);
                    int cost = config.getRecruitCost(cmd.produceType);
                    activeKingdom.gold -= cost;
                    log.log(m_turnNumber, m_activeKingdom, "Started production");
                }
                break;
            }
            case TurnCommand::Upgrade: {
                Piece* piece = activeKingdom.getPieceById(cmd.upgradePieceId);
                if (piece) {
                    int cost = config.getUpgradeCost(piece->type, cmd.upgradeTarget);
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

    // Advance all barracks production
    for (auto& b : activeKingdom.buildings) {
        if (b.type == BuildingType::Barracks && b.isProducing) {
            ProductionSystem::advanceProduction(b);
            if (ProductionSystem::isProductionComplete(b)) {
                sf::Vector2i spawnPos = ProductionSystem::findSpawnCell(b, board);
                if (spawnPos.x >= 0) {
                    PieceType pt = static_cast<PieceType>(b.producingType);
                    Piece newPiece = pieceFactory.createPiece(pt, m_activeKingdom, spawnPos);
                    activeKingdom.addPiece(newPiece);
                    board.getCell(spawnPos.x, spawnPos.y).piece = &activeKingdom.pieces.back();

                    log.log(m_turnNumber, m_activeKingdom, "Unit produced!");
                    b.isProducing = false;
                } else {
                    // Spawn cell blocked — keep trying each turn
                    // (turnsRemaining is already 0, so we'll retry next turn)
                }
            }
        }
    }

    // Remove destroyed buildings
    for (int i = static_cast<int>(activeKingdom.buildings.size()) - 1; i >= 0; --i) {
        if (activeKingdom.buildings[i].isDestroyed()) {
            auto cells = activeKingdom.buildings[i].getOccupiedCells();
            for (auto& pos : cells) {
                board.getCell(pos.x, pos.y).building = nullptr;
            }
            activeKingdom.buildings.erase(activeKingdom.buildings.begin() + i);
        }
    }

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
