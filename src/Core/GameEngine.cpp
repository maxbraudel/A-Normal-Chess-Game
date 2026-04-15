#include "Core/GameEngine.hpp"

#include <algorithm>

#include "Board/BoardGenerator.hpp"
#include "Config/GameConfig.hpp"
#include "Core/GameStateValidator.hpp"

void relinkBoardState(Board& board,
                      std::array<Kingdom, kNumKingdoms>& kingdoms,
                      std::vector<Building>& publicBuildings) {
    const int diameter = board.getDiameter();
    for (int y = 0; y < diameter; ++y) {
        for (int x = 0; x < diameter; ++x) {
            Cell& cell = board.getCell(x, y);
            cell.piece = nullptr;
            cell.building = nullptr;
        }
    }

    for (auto& building : publicBuildings) {
        for (const auto& pos : building.getOccupiedCells()) {
            if (board.isInBounds(pos.x, pos.y)) {
                board.getCell(pos.x, pos.y).building = &building;
            }
        }
    }

    for (auto& kingdom : kingdoms) {
        for (auto& building : kingdom.buildings) {
            for (const auto& pos : building.getOccupiedCells()) {
                if (board.isInBounds(pos.x, pos.y)) {
                    board.getCell(pos.x, pos.y).building = &building;
                }
            }
        }

        for (auto& piece : kingdom.pieces) {
            if (board.isInBounds(piece.position.x, piece.position.y)) {
                board.getCell(piece.position.x, piece.position.y).piece = &piece;
            }
        }
    }
}

GameEngine::GameEngine()
    : m_kingdoms{Kingdom(KingdomId::White), Kingdom(KingdomId::Black)} {
}

bool GameEngine::startNewSession(const GameSessionConfig& session,
                                 const GameConfig& config,
                                 std::string* errorMessage) {
    if (!GameStateValidator::validateSessionConfig(session, errorMessage)) {
        return false;
    }

    m_sessionConfig = session;
    m_pieceFactory.reset();
    m_buildingFactory.reset();

    m_board.init(config.getMapRadius());
    m_publicBuildings.clear();
    const auto generation = BoardGenerator::generate(m_board, config, m_publicBuildings);

    for (int kingdomSlot = 0; kingdomSlot < kNumKingdoms; ++kingdomSlot) {
        const auto kingdomId = static_cast<KingdomId>(kingdomSlot);
        m_kingdoms[kingdomSlot] = Kingdom(kingdomId);
        m_kingdoms[kingdomSlot].gold = config.getStartingGold();
    }

    const Piece whiteKing = m_pieceFactory.createPiece(PieceType::King, KingdomId::White, generation.playerSpawn);
    const Piece blackKing = m_pieceFactory.createPiece(PieceType::King, KingdomId::Black, generation.aiSpawn);
    kingdom(KingdomId::White).addPiece(whiteKing);
    kingdom(KingdomId::Black).addPiece(blackKing);

    relinkBoardState(m_board, m_kingdoms, m_publicBuildings);

    m_turnSystem = TurnSystem();
    m_turnSystem.setActiveKingdom(KingdomId::White);
    m_turnSystem.setTurnNumber(1);

    m_eventLog.clear();
    m_eventLog.log(1, KingdomId::White,
        "Game started: " + m_sessionConfig.saveName + " [" + gameModeLabel(gameMode()) + "]");

    syncFactoryIds();
    return validate(errorMessage);
}

bool GameEngine::restoreFromSave(const SaveData& data,
                                 const GameConfig& config,
                                 std::string* errorMessage) {
    if (!GameStateValidator::validateSaveData(data, errorMessage)) {
        return false;
    }

    m_sessionConfig.saveName = data.gameName;
    m_sessionConfig.kingdoms = data.sessionKingdoms;
    m_sessionConfig.multiplayer = data.multiplayer;

    m_board.init(data.mapRadius);
    if (!data.grid.empty()) {
        auto& grid = m_board.getGrid();
        const int diameter = m_board.getDiameter();
        for (int y = 0; y < diameter && y < static_cast<int>(data.grid.size()); ++y) {
            for (int x = 0; x < diameter && x < static_cast<int>(data.grid[y].size()); ++x) {
                grid[y][x].type = data.grid[y][x].type;
                grid[y][x].isInCircle = data.grid[y][x].isInCircle;
            }
        }
    } else {
        std::vector<Building> generatedPublicBuildings;
        BoardGenerator::generate(m_board, config, generatedPublicBuildings);
    }

    for (int kingdomSlot = 0; kingdomSlot < kNumKingdoms; ++kingdomSlot) {
        const auto kingdomId = static_cast<KingdomId>(kingdomSlot);
        m_kingdoms[kingdomSlot] = Kingdom(kingdomId);
        m_kingdoms[kingdomSlot].gold = data.kingdoms[kingdomSlot].gold;
        for (const auto& piece : data.kingdoms[kingdomSlot].pieces) {
            m_kingdoms[kingdomSlot].addPiece(piece);
        }
        for (const auto& building : data.kingdoms[kingdomSlot].buildings) {
            m_kingdoms[kingdomSlot].addBuilding(building);
        }
    }

    m_publicBuildings = data.publicBuildings;

    relinkBoardState(m_board, m_kingdoms, m_publicBuildings);

    m_turnSystem = TurnSystem();
    m_turnSystem.setActiveKingdom(data.activeKingdom);
    m_turnSystem.setTurnNumber(data.turnNumber);

    m_eventLog.clear();
    for (const auto& event : data.events) {
        m_eventLog.log(event.turnNumber, event.kingdom, event.message);
    }

    syncFactoryIds();
    return validate(errorMessage);
}

SaveData GameEngine::createSaveData() const {
    SaveData data;
    data.gameName = m_sessionConfig.saveName;
    data.turnNumber = m_turnSystem.getTurnNumber();
    data.activeKingdom = m_turnSystem.getActiveKingdom();
    data.mapRadius = m_board.getRadius();
    data.sessionKingdoms = m_sessionConfig.kingdoms;
    data.multiplayer = m_sessionConfig.multiplayer;

    const int diameter = m_board.getDiameter();
    data.grid.resize(diameter);
    for (int y = 0; y < diameter; ++y) {
        data.grid[y].resize(diameter);
        for (int x = 0; x < diameter; ++x) {
            const Cell& cell = m_board.getCell(x, y);
            data.grid[y][x].type = cell.type;
            data.grid[y][x].isInCircle = cell.isInCircle;
        }
    }

    for (int kingdomSlot = 0; kingdomSlot < kNumKingdoms; ++kingdomSlot) {
        data.kingdoms[kingdomSlot].id = static_cast<KingdomId>(kingdomSlot);
        data.kingdoms[kingdomSlot].gold = m_kingdoms[kingdomSlot].gold;
        data.kingdoms[kingdomSlot].pieces.assign(m_kingdoms[kingdomSlot].pieces.begin(), m_kingdoms[kingdomSlot].pieces.end());
        data.kingdoms[kingdomSlot].buildings = m_kingdoms[kingdomSlot].buildings;
    }

    data.publicBuildings = m_publicBuildings;
    data.events = m_eventLog.getEvents();
    return data;
}

bool GameEngine::validate(std::string* errorMessage) const {
    return GameStateValidator::validateRuntimeState(
        m_board, m_kingdoms, m_publicBuildings, m_turnSystem, m_sessionConfig, errorMessage);
}

void GameEngine::resetPendingTurn() {
    m_turnSystem.resetPendingCommands();
}

ControllerType GameEngine::controller(KingdomId id) const {
    return kingdomParticipantConfig(m_sessionConfig.kingdoms, id).controller;
}

bool GameEngine::isHumanControlled(KingdomId id) const {
    return controller(id) == ControllerType::Human;
}

bool GameEngine::isActiveHuman() const {
    return isHumanControlled(m_turnSystem.getActiveKingdom());
}

bool GameEngine::isActiveAI() const {
    return !isActiveHuman();
}

KingdomId GameEngine::humanKingdomId() const {
    for (int kingdomSlot = 0; kingdomSlot < kNumKingdoms; ++kingdomSlot) {
        const auto kingdomId = static_cast<KingdomId>(kingdomSlot);
        if (isHumanControlled(kingdomId)) {
            return kingdomId;
        }
    }

    return KingdomId::White;
}

std::array<ControllerType, kNumKingdoms> GameEngine::controllers() const {
    return controllersFromParticipants(m_sessionConfig.kingdoms);
}

std::array<std::string, kNumKingdoms> GameEngine::participantNames() const {
    return participantNamesFromParticipants(m_sessionConfig.kingdoms);
}

std::string GameEngine::participantName(KingdomId id) const {
    const std::string& configuredName = kingdomParticipantConfig(m_sessionConfig.kingdoms, id).participantName;
    if (!configuredName.empty()) {
        return configuredName;
    }

    return (id == KingdomId::White) ? "White" : "Black";
}

std::string GameEngine::activeTurnLabel() const {
    const KingdomId activeId = m_turnSystem.getActiveKingdom();
    return participantName(activeId) + " - " + kingdomLabel(activeId)
        + " (" + controllerTypeLabel(controller(activeId)) + ")";
}

void GameEngine::syncFactoryIds() {
    m_pieceFactory.reset();
    m_buildingFactory.reset();

    for (const auto& kingdom : m_kingdoms) {
        for (const auto& piece : kingdom.pieces) {
            m_pieceFactory.observeExisting(piece.id);
        }
        for (const auto& building : kingdom.buildings) {
            m_buildingFactory.observeExisting(building.id);
        }
    }

    for (const auto& building : m_publicBuildings) {
        m_buildingFactory.observeExisting(building.id);
    }
}