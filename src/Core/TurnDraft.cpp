#include "Core/TurnDraft.hpp"

#include <algorithm>

#include "Config/GameConfig.hpp"
#include "Core/GameEngine.hpp"
#include "Systems/StructureIntegrityRules.hpp"
#include "Systems/TurnCommand.hpp"

namespace {

Building makeUnderConstructionBuilding(const TurnCommand& command,
                                       KingdomId owner,
                                       const GameConfig& config) {
    Building building;
    building.id = command.buildId;
    building.type = command.buildingType;
    building.owner = owner;
    building.isNeutral = false;
    building.origin = command.buildOrigin;
    building.width = config.getBuildingWidth(command.buildingType);
    building.height = config.getBuildingHeight(command.buildingType);
    building.rotationQuarterTurns = command.buildRotationQuarterTurns;
    building.flipMask = 0;
    building.setConstructionState(BuildingState::UnderConstruction);
    building.isProducing = false;
    building.producingType = 0;
    building.turnsRemaining = 0;

    const int hp = StructureIntegrityRules::defaultCellHP(command.buildingType, config);
    building.cellHP.assign(building.width * building.height, hp);
    building.cellBreachState.assign(building.width * building.height, 0);
    return building;
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

} // namespace

bool TurnDraft::rebuild(const GameEngine& engine,
                        const GameConfig& config,
                        const std::vector<TurnCommand>& commands,
                        std::string* errorMessage) {
    m_board = engine.board();
    m_kingdoms = engine.kingdoms();
    m_publicBuildings = engine.publicBuildings();
    m_valid = true;
    m_errorMessage.clear();
    relinkBoardState(m_board, m_kingdoms, m_publicBuildings);

    const KingdomId activeKingdomId = engine.turnSystem().getActiveKingdom();
    for (const TurnCommand& command : commands) {
        switch (command.type) {
            case TurnCommand::Move:
                if (!applyMoveCommand(command, activeKingdomId)) {
                    setError("Unable to replay a queued move into the local turn draft.", errorMessage);
                    return false;
                }
                break;

            case TurnCommand::Build:
                if (!applyBuildCommand(command, activeKingdomId, config)) {
                    setError("Unable to replay a queued build into the local turn draft.", errorMessage);
                    return false;
                }
                break;

            case TurnCommand::Produce:
                applyProduceReservation(command, activeKingdomId, config);
                break;

            case TurnCommand::Upgrade:
                applyUpgradeReservation(command, activeKingdomId, config);
                break;

            case TurnCommand::Disband:
                if (!applyDisbandCommand(command, activeKingdomId)) {
                    setError("Unable to replay a queued piece sacrifice into the local turn draft.", errorMessage);
                    return false;
                }
                break;

            case TurnCommand::Marry:
            case TurnCommand::FormGroup:
            case TurnCommand::BreakGroup:
            default:
                break;
        }
    }

    relinkBoardState(m_board, m_kingdoms, m_publicBuildings);
    return true;
}

void TurnDraft::clear() {
    m_board = Board();
    m_kingdoms = {Kingdom(KingdomId::White), Kingdom(KingdomId::Black)};
    m_publicBuildings.clear();
    m_valid = false;
    m_errorMessage.clear();
}

bool TurnDraft::applyMoveCommand(const TurnCommand& command, KingdomId activeKingdomId) {
    Kingdom& activeKingdom = kingdom(activeKingdomId);
    Kingdom& enemyKingdom = kingdom(opponent(activeKingdomId));
    Piece* piece = activeKingdom.getPieceById(command.pieceId);
    if (!piece) {
        return false;
    }

    if (!m_board.isInBounds(command.origin.x, command.origin.y)
        || !m_board.isInBounds(command.destination.x, command.destination.y)) {
        return false;
    }

    Cell& originCell = m_board.getCell(command.origin.x, command.origin.y);
    if (originCell.piece == piece) {
        originCell.piece = nullptr;
    }

    Cell& destinationCell = m_board.getCell(command.destination.x, command.destination.y);
    if (destinationCell.piece && destinationCell.piece->kingdom != piece->kingdom) {
        enemyKingdom.removePiece(destinationCell.piece->id);
        destinationCell.piece = nullptr;
        relinkBoardState(m_board, m_kingdoms, m_publicBuildings);
    }

    piece = activeKingdom.getPieceById(command.pieceId);
    if (!piece) {
        return false;
    }

    piece->position = command.destination;
    destinationCell.piece = piece;
    return true;
}

bool TurnDraft::applyBuildCommand(const TurnCommand& command,
                                  KingdomId activeKingdomId,
                                  const GameConfig& config) {
    Kingdom& activeKingdom = kingdom(activeKingdomId);
    activeKingdom.gold = std::max(0, activeKingdom.gold - getBuildCost(command.buildingType, config));
    activeKingdom.addBuilding(makeUnderConstructionBuilding(command, activeKingdomId, config));
    relinkBoardState(m_board, m_kingdoms, m_publicBuildings);
    return true;
}

void TurnDraft::applyProduceReservation(const TurnCommand& command,
                                        KingdomId activeKingdomId,
                                        const GameConfig& config) {
    Kingdom& activeKingdom = kingdom(activeKingdomId);
    activeKingdom.gold = std::max(0, activeKingdom.gold - config.getRecruitCost(command.produceType));
}

void TurnDraft::applyUpgradeReservation(const TurnCommand& command,
                                        KingdomId activeKingdomId,
                                        const GameConfig& config) {
    Kingdom& activeKingdom = kingdom(activeKingdomId);
    const Piece* piece = activeKingdom.getPieceById(command.upgradePieceId);
    if (!piece) {
        return;
    }

    activeKingdom.gold = std::max(
        0,
        activeKingdom.gold - config.getUpgradeCost(piece->type, command.upgradeTarget));
}

bool TurnDraft::applyDisbandCommand(const TurnCommand& command,
                                    KingdomId activeKingdomId) {
    Kingdom& activeKingdom = kingdom(activeKingdomId);
    Piece* piece = activeKingdom.getPieceById(command.pieceId);
    if (!piece || piece->type == PieceType::King) {
        return false;
    }

    const sf::Vector2i piecePosition = piece->position;
    if (!m_board.isInBounds(piecePosition.x, piecePosition.y)) {
        return false;
    }

    Cell& pieceCell = m_board.getCell(piecePosition.x, piecePosition.y);
    if (pieceCell.piece == piece) {
        pieceCell.piece = nullptr;
    }

    activeKingdom.removePiece(piece->id);
    relinkBoardState(m_board, m_kingdoms, m_publicBuildings);
    return true;
}

void TurnDraft::setError(const std::string& message, std::string* errorMessage) {
    m_valid = false;
    m_errorMessage = message;
    if (errorMessage) {
        *errorMessage = message;
    }
}